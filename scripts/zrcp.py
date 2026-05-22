#!/usr/bin/env python3
"""Small ZEsarUX Remote Command Protocol client.

The reverse-engineering scripts need ZRCP for original-game captures.
Keep this client checked in so those scripts do not depend on a
machine-local helper outside the repository.
"""
from __future__ import annotations

import re
import select
import socket
import subprocess
import time
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Tuple


PROMPT_RE = re.compile(br"command(?:@cpu-step)?> ")


class ZrcpError(RuntimeError):
    pass


class ZrcpClient:
    def __init__(self, host: str = "127.0.0.1", port: int = 10000,
                 timeout: float = 5.0):
        self.host = host
        self.port = port
        self.timeout = timeout
        self.sock: Optional[socket.socket] = None
        self._buffer = b""

    def __enter__(self) -> "ZrcpClient":
        self.connect()
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        self.close()

    def connect(self) -> None:
        if self.sock is not None:
            return
        self.sock = socket.create_connection((self.host, self.port),
                                             timeout=self.timeout)
        self.sock.setblocking(False)
        self._read_until_prompt()

    def close(self) -> None:
        if self.sock is not None:
            try:
                self.sock.close()
            finally:
                self.sock = None
                self._buffer = b""

    def command(self, line: str, timeout: Optional[float] = None,
                allow_error: bool = False) -> str:
        if self.sock is None:
            self.connect()
        assert self.sock is not None
        self.sock.sendall((line + "\n").encode("utf-8"))
        raw = self._read_until_prompt(timeout=timeout)
        text = raw.decode("latin1", errors="replace")
        text = re.sub(r"command(?:@cpu-step)?> $", "", text)
        text = text.rstrip("\r\n")
        # ZEsarUX signals failure by prefixing the response with "Error.";
        # the body of the response is the error message. Without this
        # check, callers silently drop the failure on the floor and the
        # downstream symptom (e.g. a breakpoint that never trips) is
        # hard to trace back. Set allow_error=True if you need to read
        # the error string yourself.
        if not allow_error and text.startswith("Error."):
            raise ZrcpError(f"ZRCP command failed: {line!r} -> {text!r}")
        return text

    def _read_until_prompt(self, timeout: Optional[float] = None) -> bytes:
        if self.sock is None:
            raise ZrcpError("not connected")
        deadline = time.monotonic() + (self.timeout if timeout is None else timeout)
        match = PROMPT_RE.search(self._buffer)
        while match is None:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError("timed out waiting for ZRCP prompt")
            readable, _, _ = select.select([self.sock], [], [], remaining)
            if not readable:
                continue
            chunk = self.sock.recv(65536)
            if not chunk:
                raise ZrcpError("ZRCP connection closed")
            self._buffer += chunk
            match = PROMPT_RE.search(self._buffer)
        pos = match.end()
        out = self._buffer[:pos]
        self._buffer = self._buffer[pos:]
        return out

    def get_version(self) -> str:
        return self.command("get-version").strip()

    def get_registers(self) -> Dict[str, int]:
        text = self.command("get-registers")
        regs: Dict[str, int] = {}
        for key, value in re.findall(r"([A-Z][A-Z0-9']*)=([0-9A-Fa-f]+)", text):
            try:
                regs[key] = int(value, 16)
            except ValueError:
                pass
        return regs

    def read_memory(self, address: int, length: int) -> bytes:
        if length < 0:
            raise ValueError("length must be non-negative")
        out = bytearray()
        # ZRCP returns a contiguous hex string. Chunk reads keep socket
        # responses small enough to parse cheaply.
        chunk_size = 4096
        for offset in range(0, length, chunk_size):
            n = min(chunk_size, length - offset)
            text = self.command(f"read-memory {address + offset} {n}",
                                timeout=max(self.timeout, 10.0))
            hexdigits = "".join(re.findall(r"[0-9A-Fa-f]", text))
            expected = n * 2
            if len(hexdigits) < expected:
                raise ZrcpError(
                    f"short read-memory response at 0x{address+offset:04X}: "
                    f"{len(hexdigits)} hex digits, expected {expected}")
            out.extend(bytes.fromhex(hexdigits[:expected]))
        return bytes(out)

    def write_memory(self, address: int, data: bytes) -> None:
        if not data:
            return
        values = " ".join(f"{b:02X}H" for b in data)
        self.command(f"write-memory {address} {values}")

    def set_register(self, register: str, value: int) -> None:
        self.command(f"set-register {register}={value:04X}H")

    def enter_cpu_step(self) -> None:
        self.command("enter-cpu-step", timeout=max(self.timeout, 15.0))

    def exit_cpu_step(self) -> None:
        self.command("exit-cpu-step", timeout=max(self.timeout, 15.0))

    def run(self, opcodes: int, *, verbose: bool = False,
            no_stop_on_data: bool = False, timeout: Optional[float] = None) -> str:
        parts: List[str] = ["run"]
        if verbose:
            parts.append("verbose")
        parts.append(str(opcodes))
        if no_stop_on_data:
            parts.append("no-stop-on-data")
        return self.command(" ".join(parts), timeout=timeout)

    def snapshot_load(self, path: str) -> str:
        return self.command(f"snapshot-load {path}", timeout=max(self.timeout, 10.0))

    def save_screen(self, path: str) -> str:
        return self.command(f"save-screen {path}", timeout=max(self.timeout, 10.0))

    def set_breakpoint(self, index: int, condition: str) -> None:
        self.command(f"set-breakpoint {index} {condition}")

    def enable_breakpoints(self) -> None:
        self.command("enable-breakpoints")

    def disassemble(self, address: int, lines: int = 1) -> str:
        return self.command(f"disassemble {address:04X}H {lines}")

    def send_key_event(self, key: int, pressed: bool, *, nomenu: bool = True) -> None:
        event = 1 if pressed else 0
        flag = 1 if nomenu else 0
        self.command(f"send-keys-event {key} {event} {flag}")

    def exit_emulator(self) -> None:
        try:
            self.command("exit-emulator", timeout=2.0)
        except (OSError, TimeoutError, ZrcpError):
            pass


def launch_emulator(zesarux: str, machine: str = "48k",
                    extra_args: Optional[Iterable[str]] = None,
                    port: int = 10000, headless: bool = True,
                    cwd: Optional[Path] = None) -> Tuple[subprocess.Popen, ZrcpClient]:
    args = [
        zesarux,
        "--noconfigfile",
        "--machine", machine,
        "--ao", "null",
        "--enable-remoteprotocol",
        "--remoteprotocol-port", str(port),
    ]
    if headless:
        args.extend(["--vo", "null"])
    if extra_args:
        args.extend(str(a) for a in extra_args)

    proc = subprocess.Popen(args, cwd=str(cwd) if cwd else None,
                            stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL)
    client = ZrcpClient(port=port)
    deadline = time.monotonic() + 10.0
    while True:
        try:
            client.connect()
            return proc, client
        except OSError:
            if proc.poll() is not None:
                raise ZrcpError(f"ZEsarUX exited early with {proc.returncode}")
            if time.monotonic() >= deadline:
                proc.terminate()
                raise TimeoutError("timed out connecting to ZEsarUX ZRCP")
            time.sleep(0.1)
