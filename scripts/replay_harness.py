#!/usr/bin/env python3
"""Run deterministic replay specs against the DOS port and/or original.

Replay specs are JSON files with timestamped key events and captures.
The harness keeps runner-specific mechanics here so scenario files stay
small and reviewable.
"""
from __future__ import annotations

import argparse
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
        if event.kind not in ("tap", "hold"):
            raise ValueError(f"unsupported event kind {event.kind!r}")


def write_indices(path: Path, idx: bytes) -> None:
    path.write_bytes(idx)


def read_indices(path: Path) -> bytes:
    return path.read_bytes()


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


def run_original(spec: ReplaySpec, out_dir: Path, zesarux: str, port: int) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    snapshot = spec.original.get("snapshot")
    if not snapshot:
        raise ValueError(f"{spec.path}: original.snapshot is required")
    boot_wait = float(spec.original.get("boot_wait", 0.0))
    proc, zc = launch_emulator(zesarux, machine="48k",
                               extra_args=["--snap", str(snapshot)],
                               port=port, headless=True)
    try:
        time.sleep(boot_wait)
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
                zc.send_key_event(ZESARUX_KEY[event.key], True)
            elif kind == "release":
                event = payload
                zc.send_key_event(ZESARUX_KEY[event.key], False)
            else:
                capture = payload
                scr = out_dir / f"{capture.name}.scr"
                zc.save_screen(str(scr.resolve()))
                write_indices(out_dir / f"{capture.name}.idx", decode_scr(scr.read_bytes()))
    finally:
        zc.exit_emulator()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()


def diff_capture(actual: bytes, expected: bytes,
                 roi: Optional[Tuple[int, int, int, int]]) -> Tuple[int, int]:
    if roi is None:
        total = PLAYFIELD_W * PLAYFIELD_H
        diff = sum(1 for a, e in zip(actual, expected)
                   if PALETTE_RGB[a] != PALETTE_RGB[e])
        return diff, total
    x0, y0, x1, y1 = roi
    diff = 0
    for y in range(y0, y1):
        row = y * PLAYFIELD_W
        for x in range(x0, x1):
            if PALETTE_RGB[actual[row + x]] != PALETTE_RGB[expected[row + x]]:
                diff += 1
    return diff, (x1 - x0) * (y1 - y0)


def compare_outputs(spec: ReplaySpec, port_dir: Path, original_dir: Path,
                    fail_on_diff: bool) -> int:
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
    for capture in spec.captures:
        actual = read_indices(port_dir / f"{capture.name}.idx")
        expected = read_indices(original_dir / f"{capture.name}.idx")
        diff, total = diff_capture(actual, expected, capture.roi)
        pct = 100.0 * diff / total
        tag = "PASS" if diff == 0 else ("FAIL" if fail_on_diff else "INFO")
        roi = f" roi={capture.roi}" if capture.roi else ""
        print(f"  {tag} {capture.name}: {diff}/{total} px differ ({pct:.2f}%){roi}")
        if diff and fail_on_diff:
            failures += 1
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
        return compare_outputs(spec, port_dir, original_dir, args.fail_on_diff)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
