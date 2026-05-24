#!/usr/bin/env python3
"""Run deterministic replay specs against the DOS port and/or original.

Replay specs are JSON files with timestamped key events and captures.
The harness keeps runner-specific mechanics here so scenario files stay
small and reviewable.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Sequence, Tuple

sys.path.insert(0, str(Path(__file__).parent))
from extract_scr import decode as decode_scr
from test_visual import (PALETTE_RGB, PLAYFIELD_H, PLAYFIELD_W,
                         ppm_inner_to_indices, run_qemu)
from zrcp import ZrcpClient, launch_emulator


ZESARUX_KEY = {
    "space": 128,
    "enter": 129,
    "left": 136,
    "right": 137,
    "down": 138,
    "up": 139,
    "esc": 159,
}

QEMU_KEY = {
    "space": "spc",
    "enter": "ret",
    "left": "left",
    "right": "right",
    "down": "down",
    "up": "up",
    "esc": "esc",
}


@dataclass(frozen=True)
class KeyEvent:
    at: float
    key: str
    kind: str = "tap"
    duration: float = 0.10
    note: str = ""


@dataclass(frozen=True)
class Capture:
    at: float
    name: str
    roi: Optional[Tuple[int, int, int, int]] = None


@dataclass(frozen=True)
class ReplaySpec:
    path: Path
    name: str
    description: str
    events: Tuple[KeyEvent, ...]
    captures: Tuple[Capture, ...]
    port: Dict[str, object]
    original: Dict[str, object]
    comparison: Dict[str, object]
    state_probe: Dict[str, object]

    @staticmethod
    def load(path: Path) -> "ReplaySpec":
        data = json.loads(path.read_text())
        unit = data.get("timebase", {}).get("unit", "seconds")
        if unit != "seconds":
            raise ValueError(f"{path}: unsupported timebase {unit!r}")
        events = []
        for raw in data.get("events", []):
            events.append(KeyEvent(
                at=float(raw["at"]),
                key=str(raw["key"]),
                kind=str(raw.get("kind", "tap")),
                duration=float(raw.get("duration", 0.10)),
                note=str(raw.get("note", "")),
            ))
        captures = []
        for raw in data.get("captures", []):
            roi = raw.get("roi")
            captures.append(Capture(
                at=float(raw["at"]),
                name=str(raw["name"]),
                roi=tuple(int(v) for v in roi) if roi else None,
            ))
        return ReplaySpec(
            path=path,
            name=str(data["name"]),
            description=str(data.get("description", "")),
            events=tuple(sorted(events, key=lambda e: e.at)),
            captures=tuple(sorted(captures, key=lambda c: c.at)),
            port=dict(data.get("port", {})),
            original=dict(data.get("original", {})),
            comparison=dict(data.get("comparison", {})),
            state_probe=dict(data.get("state_probe", {})),
        )

    @property
    def end_time(self) -> float:
        points = [e.at + (e.duration if e.kind == "hold" else 0.0)
                  for e in self.events]
        points.extend(c.at for c in self.captures)
        return max(points, default=0.0)


def validate_spec(spec: ReplaySpec) -> None:
    names = set()
    for cap in spec.captures:
        if cap.name in names:
            raise ValueError(f"duplicate capture name {cap.name!r}")
        names.add(cap.name)
        if cap.roi:
            x0, y0, x1, y1 = cap.roi
            if not (0 <= x0 < x1 <= PLAYFIELD_W and 0 <= y0 < y1 <= PLAYFIELD_H):
                raise ValueError(f"invalid roi for {cap.name}: {cap.roi}")
    for event in spec.events:
        if event.key not in QEMU_KEY or event.key not in ZESARUX_KEY:
            raise ValueError(f"unsupported key {event.key!r}")
        if event.kind not in ("tap", "hold", "resume"):
            raise ValueError(f"unsupported event kind {event.kind!r}")


def parse_int(value: object) -> int:
    if isinstance(value, int):
        return value
    if isinstance(value, str):
        return int(value, 0)
    raise TypeError(f"expected integer-like value, got {value!r}")


def ensure_sna_from_snapshot_dir(snapshot_dir: Path) -> Path:
    sna = snapshot_dir.parent / f"{snapshot_dir.name}.sna"
    ram = snapshot_dir / "ram_4000_FFFF.bin"
    if not ram.exists():
        raise FileNotFoundError(ram)
    if not sna.exists() or sna.stat().st_mtime < ram.stat().st_mtime:
        subprocess.run([
            "python3", str(Path(__file__).parent / "snap_to_sna.py"),
            str(snapshot_dir), str(sna),
        ], check=True)
    return sna


def write_indices(path: Path, idx: bytes) -> None:
    path.write_bytes(idx)


def read_indices(path: Path) -> bytes:
    return path.read_bytes()


def hex_bytes(data: bytes) -> str:
    return data.hex().upper()


def roi_bytes(idx: bytes, roi: Optional[Tuple[int, int, int, int]]) -> bytes:
    if roi is None:
        return idx
    x0, y0, x1, y1 = roi
    out = bytearray()
    for y in range(y0, y1):
        row = y * PLAYFIELD_W
        out.extend(idx[row + x0:row + x1])
    return bytes(out)


def write_probe_report(out_dir: Path, side: str, rows: Sequence[Tuple[str, str]]) -> None:
    lines = [f"# {side} state probe", ""]
    for name, value in rows:
        lines.append(f"{name}: {value}")
    lines.append("")
    (out_dir / "state_probe.txt").write_text("\n".join(lines))
    print(f"  state probe ({side})")
    for name, value in rows:
        print(f"    {name}: {value}")


def read_probe_report(out_dir: Path) -> Dict[str, str]:
    path = out_dir / "state_probe.txt"
    values: Dict[str, str] = {}
    if not path.exists():
        return values
    for line in path.read_text(errors="replace").splitlines():
        if line.startswith("#") or ":" not in line:
            continue
        name, value = line.split(":", 1)
        values[name.strip()] = value.strip()
    return values


def probe_assertion_passed(actual: str, assertion: Dict[str, object]) -> bool:
    op = str(assertion.get("op", "equals"))
    expected = str(assertion.get("value", ""))
    if op == "equals":
        return actual == expected
    if op == "not_equals":
        return actual != expected
    if op == "contains":
        return expected in actual
    if op == "not_contains":
        return expected not in actual
    if op == "lt_hex":
        return int(actual, 16) < int(expected, 16)
    if op == "le_hex":
        return int(actual, 16) <= int(expected, 16)
    if op == "gt_hex":
        return int(actual, 16) > int(expected, 16)
    if op == "ge_hex":
        return int(actual, 16) >= int(expected, 16)
    if op == "gt_dec":
        return int(actual, 10) > int(expected, 10)
    if op == "nonzero_hex_string":
        return any(ch != "0" for ch in actual)
    raise ValueError(f"unsupported probe assertion op {op!r}")


def preview_value(value: str, limit: int = 48) -> str:
    return value if len(value) <= limit else value[:limit] + "..."


def run_port_state_probe(spec: ReplaySpec, out_dir: Path) -> None:
    rows = []
    probe_file = spec.port.get("probe_file")
    if probe_file:
        extracted = out_dir / str(probe_file)
        subprocess.run([
            "mcopy", "-i", str(spec.port.get("floppy", "build/batty-test.img")),
            f"::{probe_file}", str(extracted),
        ], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if extracted.exists():
            for line in extracted.read_text(errors="replace").splitlines():
                if "=" in line:
                    name, value = line.split("=", 1)
                    rows.append((name.strip(), value.strip()))
    for probe in spec.state_probe.get("port", []):
        source = probe.get("source")
        name = str(probe.get("name", source))
        if source == "screen_hash":
            cap = str(probe["capture"])
            idx = read_indices(out_dir / f"{cap}.idx")
            roi = probe.get("roi")
            data = roi_bytes(idx, tuple(int(v) for v in roi) if roi else None)
            rows.append((name, hashlib.sha256(data).hexdigest()))
        else:
            raise ValueError(f"unsupported port probe source {source!r}")
    if rows:
        write_probe_report(out_dir, "port", rows)


def run_port(spec: ReplaySpec, out_dir: Path) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    floppy = Path(str(spec.port.get("floppy", "build/batty-test.img")))
    boot_wait = float(spec.port.get("boot_wait", 0.0))
    actions = []
    for event in spec.events:
        actions.append(("event", event.at, event))
    for capture in spec.captures:
        actions.append(("capture", capture.at, capture))
    actions.sort(key=lambda item: (item[1], 0 if item[0] == "event" else 1))

    script: List[str] = [f"SLEEP {boot_wait}"]
    cursor = 0.0
    for kind, at, payload in actions:
        if at < cursor:
            raise ValueError(f"non-monotonic replay action at {at}")
        if at > cursor:
            script.append(f"SLEEP {at - cursor:.3f}")
            cursor = at
        if kind == "event":
            event = payload
            qkey = QEMU_KEY[event.key]
            if event.kind == "hold":
                script.append(f"sendkey {qkey} {int(event.duration * 1000)}")
            else:
                script.append(f"sendkey {qkey}")
        else:
            capture = payload
            script.append(f"screendump {out_dir / (capture.name + '.ppm')}")
            script.append("SLEEP 0.2")
    script.append("sendkey esc")

    run_qemu(floppy, script, out_dir / "qemu.log")
    for capture in spec.captures:
        idx = ppm_inner_to_indices(out_dir / f"{capture.name}.ppm")
        write_indices(out_dir / f"{capture.name}.idx", idx)
    run_port_state_probe(spec, out_dir)


def apply_original_setup(zc: ZrcpClient, setup: Sequence[Dict[str, object]]) -> None:
    for step in setup:
        op = step.get("op")
        if op == "sleep":
            time.sleep(float(step.get("seconds", 0.0)))
        elif op == "run":
            # Step N opcodes — bit-exact across runs, unlike sleep which
            # advances at wall-clock speed and lands the Z80 at a
            # non-deterministic PC.
            zc.run(parse_int(step["opcodes"]),
                   no_stop_on_data=True,
                   timeout=max(10.0, parse_int(step["opcodes"]) / 50000.0))
        elif op == "run_until_pc":
            # Run until the Z80 reaches a specific PC, bounded by a
            # max_opcodes safety stop. Used to land the original side at
            # the exact same game-state point the port pauses at (e.g.
            # main-loop entry after the brick shimmer animation).
            #
            # ZEsarUX requires `enable-breakpoints` to run *before*
            # `set-breakpoint`; otherwise the set silently fails with
            # "you must enable breakpoints first".
            target_pc = parse_int(step["pc"])
            max_opcodes = parse_int(step.get("max_opcodes", 5000000))
            zc.enable_breakpoints()
            zc.set_breakpoint(1, f"PC={target_pc:04X}H")
            zc.run(max_opcodes,
                   no_stop_on_data=True,
                   timeout=max(15.0, max_opcodes / 50000.0))
            zc.clear_breakpoint(1)
            zc.disable_breakpoints()
            zc.enter_cpu_step()
            regs = zc.get_registers()
            actual_pc = regs.get("PC", -1)
            if actual_pc != target_pc:
                raise RuntimeError(
                    f"run_until_pc: expected PC=${target_pc:04X}, "
                    f"got PC=${actual_pc:04X} after {max_opcodes} opcodes")
        elif op == "enter_cpu_step":
            zc.enter_cpu_step()
        elif op == "exit_cpu_step":
            zc.exit_cpu_step()
        elif op == "write_memory":
            address = parse_int(step["address"])
            data = bytes(parse_int(v) for v in step.get("bytes", []))
            zc.write_memory(address, data)
        elif op == "set_register":
            zc.set_register(str(step["register"]), parse_int(step["value"]))
        elif op == "command":
            zc.command(str(step["value"]))
        else:
            raise ValueError(f"unsupported original setup op {op!r}")


def run_original_state_probe(spec: ReplaySpec, out_dir: Path, zc: ZrcpClient) -> None:
    rows = []
    for probe in spec.state_probe.get("original", []):
        name = str(probe["name"])
        address = parse_int(probe["address"])
        length = parse_int(probe["length"])
        data = zc.read_memory(address, length)
        rows.append((name, hex_bytes(data)))
    if rows:
        write_probe_report(out_dir, "original", rows)


def run_original(spec: ReplaySpec, out_dir: Path, zesarux: str, port: int) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    snapshot = spec.original.get("snapshot")
    if not snapshot and spec.original.get("snapshot_from"):
        snapshot = str(ensure_sna_from_snapshot_dir(Path(str(spec.original["snapshot_from"]))))
    if not snapshot:
        raise ValueError(f"{spec.path}: original.snapshot or original.snapshot_from is required")
    boot_wait = float(spec.original.get("boot_wait", 0.0))
    proc, zc = launch_emulator(zesarux, machine="48k",
                               extra_args=[],
                               port=port, headless=True)
    try:
        zc.snapshot_load(str(Path(str(snapshot)).resolve()))
        time.sleep(boot_wait)
        apply_original_setup(zc, spec.original.get("setup", []))
        probe_timing = str(spec.state_probe.get("original_timing", "before"))
        if probe_timing == "before":
            run_original_state_probe(spec, out_dir, zc)
        actions = []
        for event in spec.events:
            actions.append(("event", event.at, event))
            if event.kind == "tap":
                actions.append(("release", event.at + event.duration, event))
            elif event.kind == "hold":
                actions.append(("release", event.at + event.duration, event))
        for capture in spec.captures:
            actions.append(("capture", capture.at, capture))
        actions.sort(key=lambda item: (item[1], 0 if item[0] == "event" else 1))

        start = time.monotonic()
        for kind, at, payload in actions:
            delay = start + at - time.monotonic()
            if delay > 0:
                time.sleep(delay)
            if kind == "event":
                event = payload
                if event.kind == "resume":
                    zc.exit_cpu_step()
                else:
                    zc.send_key_event(ZESARUX_KEY[event.key], True)
            elif kind == "release":
                event = payload
                zc.send_key_event(ZESARUX_KEY[event.key], False)
            else:
                capture = payload
                scr = out_dir / f"{capture.name}.scr"
                zc.save_screen(str(scr.resolve()))
                write_indices(out_dir / f"{capture.name}.idx", decode_scr(scr.read_bytes()))
        if probe_timing == "after":
            run_original_state_probe(spec, out_dir, zc)
        elif probe_timing != "before":
            raise ValueError(f"unsupported original probe timing {probe_timing!r}")
    finally:
        zc.exit_emulator()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()


def diff_capture(actual: bytes, expected: bytes,
                 roi: Optional[Tuple[int, int, int, int]]
                 ) -> Tuple[int, int, Optional[Tuple[int, int, int, int]]]:
    if roi is None:
        roi = (0, 0, PLAYFIELD_W, PLAYFIELD_H)
    x0, y0, x1, y1 = roi
    diff = 0
    min_x = PLAYFIELD_W
    min_y = PLAYFIELD_H
    max_x = -1
    max_y = -1
    for y in range(y0, y1):
        row = y * PLAYFIELD_W
        for x in range(x0, x1):
            if PALETTE_RGB[actual[row + x]] != PALETTE_RGB[expected[row + x]]:
                diff += 1
                min_x = min(min_x, x)
                min_y = min(min_y, y)
                max_x = max(max_x, x + 1)
                max_y = max(max_y, y + 1)
    bounds = None if diff == 0 else (min_x, min_y, max_x, max_y)
    return diff, (x1 - x0) * (y1 - y0), bounds


def write_diff_png(actual: bytes, expected: bytes, out_path: Path,
                   roi: Optional[Tuple[int, int, int, int]]) -> bool:
    try:
        from PIL import Image
    except ImportError:
        return False
    if roi is None:
        roi = (0, 0, PLAYFIELD_W, PLAYFIELD_H)
    x0, y0, x1, y1 = roi
    img = Image.new("RGB", (x1 - x0, y1 - y0))
    px = img.load()
    for y in range(y0, y1):
        row = y * PLAYFIELD_W
        for x in range(x0, x1):
            i = row + x
            dx = x - x0
            dy = y - y0
            if PALETTE_RGB[actual[i]] == PALETTE_RGB[expected[i]]:
                r, g, b = PALETTE_RGB[expected[i]]
                px[dx, dy] = (r // 3, g // 3, b // 3)
            else:
                px[dx, dy] = (255, 0, 0)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    img.save(out_path)
    return True


def compare_outputs(spec: ReplaySpec, port_dir: Path, original_dir: Path,
                    fail_on_diff: bool) -> int:
    compare_dir = port_dir.parent / "compare"
    aligned = bool(spec.comparison.get("aligned_start", False))
    if fail_on_diff and not aligned:
        raise ValueError(
            f"{spec.path}: refusing --fail-on-diff because comparison.aligned_start is false")
    if not aligned:
        note = spec.comparison.get("note")
        print("  INFO comparison is not a parity gate: start states are not marked aligned")
        if note:
            print(f"       {note}")
    failures = 0
    port_state = read_probe_report(port_dir)
    original_state = read_probe_report(original_dir)
    common = sorted(set(port_state) & set(original_state))
    required_probe_rows = set(str(v) for v in spec.comparison.get("required_probe_rows", []))
    required_captures = set(str(v) for v in spec.comparison.get("required_captures", []))
    capture_max_diff_pixels = {
        str(name): int(limit)
        for name, limit in dict(spec.comparison.get("capture_max_diff_pixels", {})).items()
    }
    summary = {
        "spec": spec.name,
        "aligned_start": aligned,
        "fail_on_diff": fail_on_diff,
        "required_probe_rows": sorted(required_probe_rows),
        "required_captures": sorted(required_captures),
        "capture_max_diff_pixels": capture_max_diff_pixels,
        "probe_rows": [],
        "probe_assertions": [],
        "port_only_probe_rows": sorted(set(port_state) - set(original_state)),
        "original_only_probe_rows": sorted(set(original_state) - set(port_state)),
        "captures": [],
    }
    if common:
        print("  state probe comparison")
        for name in common:
            port_val = port_state[name]
            orig_val = original_state[name]
            same = port_val == orig_val
            required = name in required_probe_rows
            summary["probe_rows"].append({
                "name": name,
                "match": same,
                "required": required,
                "port": port_val,
                "original": orig_val,
            })
            tag = "PASS" if same else ("FAIL" if required else "INFO")
            if same:
                # PASS lines only need name + value; dumping the value
                # twice (especially long hex like current_level_copy)
                # buries the actual signal in CI logs.
                print(f"    {tag} {name}: {preview_value(port_val)}")
            else:
                print(f"    {tag} {name}: port={port_val} original={orig_val}")
            if not same and (fail_on_diff or required):
                failures += 1
    missing_required = sorted(required_probe_rows - set(common))
    for name in missing_required:
        print(f"    FAIL {name}: required probe row missing from one side")
        summary["probe_rows"].append({
            "name": name,
            "match": False,
            "required": True,
            "missing": True,
            "port": port_state.get(name),
            "original": original_state.get(name),
        })
        failures += 1
    probe_assertions = spec.comparison.get("probe_assertions", [])
    if probe_assertions:
        print("  probe assertions")
    for assertion in probe_assertions:
        side = str(assertion["side"])
        name = str(assertion["name"])
        state = port_state if side == "port" else original_state if side == "original" else None
        if state is None:
            raise ValueError(f"unsupported probe assertion side {side!r}")
        actual = state.get(name)
        passed = actual is not None and probe_assertion_passed(actual, assertion)
        op = str(assertion.get("op", "equals"))
        expected = str(assertion.get("value", ""))
        tag = "PASS" if passed else "FAIL"
        if actual is None:
            detail = "missing"
        else:
            detail = f"{preview_value(actual)} {op} {expected}".rstrip()
        print(f"    {tag} {side}.{name}: {detail}")
        summary["probe_assertions"].append({
            "side": side,
            "name": name,
            "op": op,
            "value": expected,
            "actual": actual,
            "match": passed,
        })
        if not passed:
            failures += 1
    for capture in spec.captures:
        actual = read_indices(port_dir / f"{capture.name}.idx")
        expected = read_indices(original_dir / f"{capture.name}.idx")
        diff, total, bounds = diff_capture(actual, expected, capture.roi)
        pct = 100.0 * diff / total
        required = capture.name in required_captures
        max_diff = capture_max_diff_pixels.get(capture.name)
        over_limit = max_diff is not None and diff > max_diff
        if diff == 0 or (max_diff is not None and not over_limit):
            tag = "PASS"
        elif fail_on_diff or required or over_limit:
            tag = "FAIL"
        else:
            tag = "INFO"
        roi = f" roi={capture.roi}" if capture.roi else ""
        bounds_text = f" bounds={bounds}" if bounds else ""
        print(f"  {tag} {capture.name}: {diff}/{total} px differ ({pct:.2f}%){roi}{bounds_text}")
        artifact = None
        if diff:
            diff_path = compare_dir / f"{capture.name}-diff.png"
            if write_diff_png(actual, expected, diff_path, capture.roi):
                artifact = str(diff_path)
                print(f"       diff artifact: {diff_path}")
        summary["captures"].append({
            "name": capture.name,
            "roi": list(capture.roi) if capture.roi else None,
            "diff_pixels": diff,
            "total_pixels": total,
            "diff_percent": pct,
            "required": required,
            "max_diff_pixels": max_diff,
            "bounds": list(bounds) if bounds else None,
            "diff_artifact": artifact,
        })
        if diff and (fail_on_diff or required or over_limit):
            failures += 1
    compare_dir.mkdir(parents=True, exist_ok=True)
    summary_path = compare_dir / "summary.json"
    summary_path.write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n")
    print(f"  comparison summary: {summary_path}")
    return failures


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("spec", type=Path)
    ap.add_argument("--out", type=Path, default=Path("build/replay"))
    ap.add_argument("--side", choices=("port", "original", "both"),
                    default="port")
    ap.add_argument("--compare", action="store_true")
    ap.add_argument("--fail-on-diff", action="store_true")
    ap.add_argument("--zesarux", default="tools/zesarux/src/zesarux")
    ap.add_argument("--zrcp-port", type=int, default=10000)
    args = ap.parse_args()

    spec = ReplaySpec.load(args.spec)
    validate_spec(spec)
    root = args.out / spec.name
    port_dir = root / "port"
    original_dir = root / "original"

    print(f"replay {spec.name}: {spec.description}")
    if args.side in ("port", "both"):
        print(f"running DOS port -> {port_dir}")
        run_port(spec, port_dir)
    if args.side in ("original", "both"):
        print(f"running original -> {original_dir}")
        run_original(spec, original_dir, args.zesarux, args.zrcp_port)
    if args.compare:
        print("comparing port vs original")
        failures = compare_outputs(spec, port_dir, original_dir, args.fail_on_diff)
        # One-line summary on the last line so CI / humans can grep the
        # final status without scanning the per-probe block above.
        has_gates = bool(args.fail_on_diff
                         or spec.comparison.get("required_probe_rows")
                         or spec.comparison.get("required_captures")
                         or spec.comparison.get("probe_assertions"))
        gated = "fail-gated" if has_gates else "informational"
        status = "PASS" if failures == 0 else f"FAIL ({failures} diff{'s' if failures != 1 else ''})"
        print(f"{status} replay {spec.name} ({gated})")
        return failures
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
