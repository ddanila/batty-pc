#!/usr/bin/env python3
"""Recursive-descent reachability trace from the game's entry point.

Walks every reachable JP/JR/CALL/DJNZ target plus fall-through, starting
at 0x6800. Anything not reached is either data, dead code, or only
reachable via computed jumps / runtime-patched operands (SMC).

Inputs:
  - z80dasm disassembly with --address (parsed for instruction lengths,
    mnemonics, and branch targets).

Outputs:
  build/reach.txt          one line per reachable basic block start
  build/calls.txt          unique CALL targets (= function entries)
  build/data_refs.txt      memory addresses referenced by reachable code
  build/unreached_code.txt z80dasm-decoded instructions never reached
                             — strong candidates for data ranges
"""
import re
import sys
from pathlib import Path

ORIGIN, END = 0x6800, 0xC274

# Tab-separated line layout:  \t<mnem...>\t;<addr>\t<bytes>\t<ascii>
LINE_RE = re.compile(
    r'^\s+([a-z][\S ]*?)\s*;([0-9a-fA-F]{4,5})\s+((?:[0-9a-fA-F]{2}\s+){0,3}[0-9a-fA-F]{2})'
)
LABEL_HEX_RE = re.compile(r'\b(?:l|sub_)([0-9a-fA-F]{4,5})h\b')   # z80dasm auto labels
PLAIN_HEX_RE = re.compile(r'\b0?([0-9a-fA-F]{1,4})h\b')


def _hex(s):
    s = s.rstrip('h').lstrip('0') or '0'
    return int(s, 16)


def extract_target(operand: str):
    """First numeric we find in the operand string. Strip the condition prefix."""
    if ',' in operand:
        operand = operand.split(',', 1)[1]
    m = LABEL_HEX_RE.search(operand)
    if m: return int(m.group(1), 16)
    m = PLAIN_HEX_RE.search(operand)
    if m: return int(m.group(1), 16)
    return None


def classify(mnem: str):
    """Return (kind, target). Kinds:
       seq        — falls through only
       terminator — current path ends (ret, jp (hl), jp (ix), jp (iy))
       uncond_br  — jp/jr to target; path ends after
       cond_br    — jp cc/jr cc/djnz; explore target AND fall through
       call       — call (cc) target; explore target AND fall through
    """
    m = mnem.strip()
    if m == 'ret' or m in ('reti', 'retn'): return ('terminator', None)
    if m.startswith('ret '):                 return ('seq',        None)  # ret cc: keep walking fall-through
    if m.startswith('jp ') or m.startswith('jp\t'):
        rest = m[3:].strip()
        if rest.startswith('('): return ('terminator', None)
        if ',' in rest:          return ('cond_br', extract_target(rest))
        return ('uncond_br', extract_target(rest))
    if m.startswith('jr '):
        rest = m[3:].strip()
        if ',' in rest:          return ('cond_br', extract_target(rest))
        return ('uncond_br', extract_target(rest))
    if m.startswith('call '):                return ('call',       extract_target(m[5:]))
    if m.startswith('djnz '):                return ('cond_br',    extract_target(m[5:]))
    if m.startswith('rst '):                 return ('call',       extract_target(m[4:]))
    return ('seq', None)


def parse(asm_path: Path):
    """addr -> (length, mnemonic, kind, target, is_data)"""
    out = {}
    for ln in asm_path.read_text().splitlines():
        m = LINE_RE.match(ln)
        if not m:
            continue
        mnem = m.group(1).strip()
        addr = int(m.group(2), 16)
        nbytes = len(m.group(3).split())
        is_data = mnem.startswith('defb') or mnem.startswith('defw')
        kind, target = (('data', None) if is_data else classify(mnem))
        out[addr] = (nbytes, mnem, kind, target, is_data)
    return out


def walk(instr, entry):
    reached = set()
    call_targets = set()
    branch_targets = set()
    data_refs = []           # (from_addr, target_addr, mnemonic)
    unknown_branches = []    # (from_addr, mnemonic)  — jp (hl) etc.
    work = [entry]
    while work:
        pc = work.pop()
        while pc in instr and pc not in reached:
            length, mnem, kind, target, is_data = instr[pc]
            if is_data:
                break
            reached.add(pc)
            # Collect data-ref candidates (any numeric in operand that points into RAM).
            for hx in PLAIN_HEX_RE.findall(mnem):
                v = int(hx, 16)
                if 0x4000 <= v < 0x10000 and v != target:
                    data_refs.append((pc, v, mnem))
            if kind == 'terminator':
                if 'jp\t' in mnem or 'jp ' in mnem:
                    unknown_branches.append((pc, mnem))
                break
            if kind in ('uncond_br', 'cond_br', 'call') and target is not None:
                if kind == 'call':
                    call_targets.add(target)
                else:
                    branch_targets.add(target)
                work.append(target)
            if kind == 'uncond_br':
                break
            pc += length
    return reached, call_targets, branch_targets, data_refs, unknown_branches


def runs(addrs):
    """Compact a sorted set of addresses into contiguous runs."""
    if not addrs: return []
    a = sorted(addrs)
    out, s, p = [], a[0], a[0]
    for x in a[1:]:
        if x == p + 1: p = x
        else: out.append((s, p)); s = p = x
    out.append((s, p))
    return out


def main():
    asm = Path('original/blocks/03_main.asm')   # no-blockdef version
    out_dir = Path('build'); out_dir.mkdir(exist_ok=True)
    instr = parse(asm)
    reached, calls, branches, refs, unks = walk(instr, ORIGIN)

    # Per-byte reachability: an instruction at addr length L covers addr..addr+L-1.
    reached_bytes = set()
    for addr in reached:
        for i in range(instr[addr][0]):
            reached_bytes.add(addr + i)
    all_decoded_bytes = set()
    for addr, (length, _m, _k, _t, _d) in instr.items():
        for i in range(length):
            all_decoded_bytes.add(addr + i)
    unreached_bytes = all_decoded_bytes - reached_bytes

    (out_dir / 'reach.txt').write_text(
        f"# reachable from 0x{ORIGIN:04X}\n"
        f"# {len(reached)} instructions, {len(reached_bytes)} bytes covered\n"
        + "\n".join(f"0x{a:04X}..0x{b:04X}  ({b-a+1:>5} B)"
                    for a, b in runs(reached_bytes))
        + "\n"
    )
    (out_dir / 'calls.txt').write_text(
        f"# {len(calls)} unique CALL targets (function entries)\n"
        + "\n".join(f"0x{a:04X}" for a in sorted(calls)) + "\n"
    )
    (out_dir / 'unreached_code.txt').write_text(
        f"# bytes z80dasm decoded as code but never reached from 0x{ORIGIN:04X}\n"
        f"# = candidate data ranges, or only-via-SMC code\n"
        f"# {len(unreached_bytes)} bytes in {len(runs(unreached_bytes))} runs\n"
        + "\n".join(f"0x{a:04X}..0x{b:04X}  ({b-a+1:>5} B)"
                    for a, b in runs(unreached_bytes))
        + "\n"
    )
    # Group data refs by target address.
    by_target = {}
    for src, tgt, mnem in refs:
        by_target.setdefault(tgt, []).append((src, mnem))
    (out_dir / 'data_refs.txt').write_text(
        f"# {len(by_target)} unique data targets, {len(refs)} total refs\n"
        + "\n".join(
            f"0x{t:04X}  ({len(srcs)}x)  from {', '.join(f'0x{s:04X}' for s,_ in srcs[:4])}"
            + ("..." if len(srcs) > 4 else "")
            for t, srcs in sorted(by_target.items())
        ) + "\n"
    )
    if unks:
        (out_dir / 'unknown_branches.txt').write_text(
            "\n".join(f"0x{a:04X}  {m}" for a, m in unks) + "\n")

    print(f"reachable instructions: {len(reached)}  ({len(reached_bytes)} bytes)")
    print(f"call targets (entries): {len(calls)}")
    print(f"branch targets:         {len(branches)}")
    print(f"data refs:              {len(refs)}  ({len(by_target)} unique)")
    print(f"unknown branches:       {len(unks)}")
    print(f"unreached code bytes:   {len(unreached_bytes)}")
    print("wrote build/{reach,calls,unreached_code,data_refs}.txt")


if __name__ == '__main__':
    main()
