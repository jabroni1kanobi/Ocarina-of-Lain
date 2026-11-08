"""Self-test of tests/maxwell_warp_emu.py: hand-encoded Maxwell sequences against hand-computed results.

Part 1 runs Maxwell words encoded here from the field layouts of the translator's Decoders/InstDecoders.cs (integer and logic
ops, SHFL.IDX with clamp and segment masks, LDS/STS sizes, F2F, HFMA2, FFMA rounding, PRMT, predicates, EXIT, BRA).
Part 2, when build/xlate/nx2navi_xlate.exe exists, lowers SM86 HMMA.1688 / HMMA.16816, LDSM and MOVM with
nx2navi_xlate --lower and checks the emulated result against exactly representable reference values (small
integers, so every F16 rounding is exact and only the fragment layout can be wrong), and HFMA2.RELU as the translator
runs it (the translator drops PRMT, so AmpereLowering.cs Half must not use it). tests/tensor_lowering_check.py checks
the same lowerings against hardware results.

  python tests/maxwell_warp_emu_selftest.py
"""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import maxwell_warp_emu as emu  # noqa: E402

PT = 7 << 16          # guard field: predicate PT
LANES = 32
lane = np.arange(LANES, dtype=np.int64)
failures: list[str] = []
checks = 0


def check(name: str, got, want):
    global checks
    checks += 1
    got = np.asarray(got).astype(np.int64)
    want = np.broadcast_to(np.asarray(want).astype(np.int64), got.shape)
    if not np.array_equal(got, want):
        bad = np.nonzero(got != want)[0][:6]
        failures.append(f"{name}: " + ", ".join(f"lane {b} got {int(got[b]):#x} want {int(want[b]):#x}" for b in bad))


# ---- Maxwell encoders (InstDecoders.cs field positions) -----------------------------------------------------------

def guard(p: int, inv: bool = False) -> int:
    return (p | (8 if inv else 0)) << 16


def iadd32i(d, a, imm, g=PT): return 0x1C00000000000000 | (imm & 0xFFFFFFFF) << 20 | a << 8 | d | g
def mov32i(d, imm, g=PT): return 0x0100000000000000 | 0xF << 12 | (imm & 0xFFFFFFFF) << 20 | d | g
def mov(d, s, g=PT): return 0x5C98000000000000 | 0xF << 39 | s << 20 | d | g
def s2r(d, sr=0): return 0xF0C8000000000000 | sr << 20 | d | PT
def lop32i(d, a, imm, op): return 0x0400000000000000 | op << 53 | (imm & 0xFFFFFFFF) << 20 | a << 8 | d | PT
def lop3(d, a, b, c, lut): return 0x5BE0000000000000 | 7 << 48 | c << 39 | lut << 28 | b << 20 | a << 8 | d | PT
def iscadd(d, a, b, sh): return 0x5C18000000000000 | sh << 39 | b << 20 | a << 8 | d | PT
def iadd3(d, a, b, c, neg_b=False): return 0x5CC0000000000000 | c << 39 | b << 20 | a << 8 | d | (1 << 50 if neg_b else 0) | PT
def shl_i(d, a, n): return 0x3848000000000000 | n << 20 | a << 8 | d | PT
def shr_i(d, a, n, signed=False): return 0x3828000000000000 | (1 << 48 if signed else 0) | n << 20 | a << 8 | d | PT
def shr_r(d, a, b): return 0x5C28000000000000 | b << 20 | a << 8 | d | PT
def prmt(d, a, sel, c): return 0x5BC0000000000000 | c << 39 | sel << 20 | a << 8 | d | PT
def isetp_i(pd, a, imm, cmp, signed=False):   # ISETP.cmp Pd, PT, Ra, imm, PT (AND)
    return 0x3660000000000000 | cmp << 49 | (1 << 48 if signed else 0) | 7 << 39 | (imm & 0x7FFFF) << 20 | a << 8 | pd << 3 | 7 | PT
def shfl(d, a, b, c, mode=0, b_imm=False, c_imm=False, pd=7):
    return (0xEF10000000000000 | pd << 48 | (c << 34 if c_imm else c << 39) | (1 << 29 if c_imm else 0)
            | (1 << 28 if b_imm else 0) | mode << 30 | b << 20 | a << 8 | d | PT)
def lds(d, a, off, size): return 0xEF48000000000000 | size << 48 | (off & 0xFFFFFF) << 20 | a << 8 | d | PT
def sts(v, a, off, size): return 0xEF58000000000000 | size << 48 | (off & 0xFFFFFF) << 20 | a << 8 | v | PT
def f2f(d, b, dst, src, high=False): return 0x5CA8000000000000 | dst << 8 | src << 10 | b << 20 | (1 << 41 if high else 0) | d | PT
def hfma2(d, a, b, c, neg_c=False, sat=False):
    return 0x5D00000000000000 | c << 39 | (1 << 30 if neg_c else 0) | (1 << 32 if sat else 0) | b << 20 | a << 8 | d | PT
def ffma(d, a, b, c): return 0x5980000000000000 | c << 39 | b << 20 | a << 8 | d | PT
def bra(offset): return 0xE24000000000000F | (offset & 0xFFFFFF) << 20 | PT
EXIT = 0xE30000000007000F


def f32bits(x): return int(np.array([x], dtype=np.float32).view(np.uint32)[0])
def h16bits(x): return int(np.array([x], dtype=np.float16).view(np.uint16)[0])
def hpair(lo, hi): return h16bits(lo) | h16bits(hi) << 16


def regs_all(**regs):
    return {"*": {k: v for k, v in regs.items()}}


def per_lane(reg, values):
    return {l: {reg: int(v) & 0xFFFFFFFF} for l, v in enumerate(values)}


def merge(*specs):
    out: dict = {}
    for s in specs:
        for k, v in s.items():
            out.setdefault(k, {}).update(v)
    return out


# ---- part 1: hand-encoded sequences -------------------------------------------------------------------------------

def part1():
    # IADD32I, MOV32I, IADD3 with a negated operand, ISCADD.
    r = emu.run([mov32i(1, 0x7FFFFFFF), iadd32i(2, 0, 5), iadd32i(3, 1, 1), iadd32i(4, 0, -3),
                 iadd3(5, 0, 2, 1, neg_b=True), iscadd(6, 0, 2, 4), EXIT], per_lane("R0", lane * 3))
    check("IADD32I", r[:, 2], lane * 3 + 5)
    check("IADD32I wraps", r[:, 3], 0x80000000)
    check("IADD32I negative", r[:, 4], (lane.astype(np.int64) * 3 - 3) & 0xFFFFFFFF)
    check("IADD3 -B", r[:, 5], (lane.astype(np.int64) * 3 - (lane * 3 + 5) + 0x7FFFFFFF) & 0xFFFFFFFF)
    check("ISCADD", r[:, 6], ((lane.astype(np.int64) * 3) << 4) + lane * 3 + 5)
    check("RZ stays 0", r[:, 255], 0)

    # S2R lane id, LOP32I, SHL/SHR, LOP3, SHR by register.
    r = emu.run([s2r(1), lop32i(2, 1, 3, 0), shl_i(3, 1, 3), shr_i(4, 1, 2), lop32i(5, 1, 0xF0, 1),
                 lop32i(6, 1, 0x5, 2), lop3(7, 1, 2, 3, 0x96), mov32i(8, 0x80000000), shr_i(9, 8, 4, signed=True),
                 shr_r(10, 3, 2), EXIT], {})
    check("S2R SR_LANEID", r[:, 1], lane)
    check("LOP32I.AND", r[:, 2], lane & 3)
    check("SHL imm", r[:, 3], lane << 3)
    check("SHR imm", r[:, 4], lane >> 2)
    check("LOP32I.OR", r[:, 5], lane | 0xF0)
    check("LOP32I.XOR", r[:, 6], lane ^ 5)
    check("LOP3 0x96 (xor3)", r[:, 7], lane ^ (lane & 3) ^ (lane << 3))
    check("SHR.S32", r[:, 9], 0xF8000000)
    check("SHR by register", r[:, 10], (lane << 3) >> (lane & 3))

    # SHFL.IDX: the whole warp by a lane register; a segment of 4 lanes; out of range lanes keep their own value.
    vals = lane * 0x101 + 7
    seq = [s2r(1), iadd32i(2, 1, 1), lop32i(2, 2, 31, 0),
           shfl(3, 0, 2, 31, c_imm=True),                         # (lane + 1) % 32
           shfl(4, 0, 1, 0x1C03, b_imm=True, c_imm=True),          # segments of 4: lane (t & ~3) | 1
           shfl(5, 0, 5, 0x1803, b_imm=True, c_imm=True, pd=0),    # segments of 8, clamp 3: index 5 invalid
           shfl(6, 0, 2, 0x1803, b_imm=True, c_imm=True, pd=1),    # index 2 valid
           shfl(7, 0, 1, 0x1F, mode=3, b_imm=True, c_imm=True),     # BFLY 1
           shfl(8, 0, 2, 0x00, mode=2, b_imm=True, c_imm=True),     # DOWN 2, clamp 0: lanes > 0... invalid
           shfl(9, 0, 2, 0x1F, mode=2, b_imm=True, c_imm=True),     # DOWN 2
           shfl(10, 0, 1, 0, mode=1, b_imm=True, c_imm=True),      # UP 1
           EXIT]
    w = emu.run_warp(seq, per_lane("R0", vals))
    r = w.r
    check("SHFL.IDX lane register", r[:, 3], vals[(lane + 1) % 32])
    check("SHFL.IDX segment 4", r[:, 4], vals[(lane & ~3) | 1])
    check("SHFL.IDX clamp invalid keeps own value", r[:, 5], vals)
    check("SHFL.IDX clamp invalid predicate", w.p[:, 0], False)
    check("SHFL.IDX in clamp", r[:, 6], vals[(lane & ~7) | 2])
    check("SHFL.IDX valid predicate", w.p[:, 1], True)
    check("SHFL.BFLY", r[:, 7], vals[lane ^ 1])
    check("SHFL.DOWN clamp 0", r[:, 8], vals)
    check("SHFL.DOWN", r[:, 9], np.where(lane + 2 <= 31, vals[np.minimum(lane + 2, 31)], vals))
    check("SHFL.UP", r[:, 10], np.where(lane >= 1, vals[np.maximum(lane.astype(np.int64) - 1, 0)], vals))

    # STS / LDS: 32, 16 and 8 bits, sign extension, 64 and 128 bits, the immediate offset.
    seq = [s2r(1), shl_i(2, 1, 2),                                  # R2 = 4 * lane
           sts(1, 2, 0x100, 4),                                    # word [0x100 + 4t] = t
           lds(3, 2, 0x100, 4),
           lds(4, 2, 0x100, 2), lds(5, 2, 0x102, 2), lds(6, 2, 0x101, 0),
           mov32i(7, 0x8001F0F0), sts(7, 255, 0x40, 4), lds(8, 255, 0x40, 3), lds(9, 255, 0x42, 3),
           lds(10, 255, 0x40, 1), lds(11, 255, 0x41, 0),
           lds(12, 255, 0x100, 6),                                 # R12-R15 = words 0..3 of the table
           mov32i(16, 0xABCD), sts(16, 255, 0x82, 2), lds(17, 255, 0x80, 4),
           EXIT]
    r = emu.run(seq, {})
    check("LDS.32", r[:, 3], lane)
    check("LDS.U16", r[:, 4], lane)
    check("LDS.U16 high half", r[:, 5], 0)
    check("LDS.U8 byte 1", r[:, 6], 0)
    check("LDS.S16", r[:, 8], 0xFFFFF0F0)
    check("LDS.S16 high", r[:, 9], 0xFFFF8001)
    check("LDS.S8", r[:, 10], 0xFFFFFFF0)
    check("LDS.U8", r[:, 11], 0xF0)
    for k in range(4):
        check(f"LDS.128 word {k}", r[:, 12 + k], k)
    check("STS.U16 at +2", r[:, 17], 0xABCD0000)

    # F2F: F32 from either F16 half (exact), F16 from F32 (round to nearest even, high half zero).
    src = hpair(1.5, -0.333251953125)
    seq = [mov32i(1, src), f2f(2, 1, 2, 1), f2f(3, 1, 2, 1, high=True),
           mov32i(4, f32bits(1 / 3)), f2f(5, 4, 1, 2),
           mov32i(6, f32bits(1 + 2 ** -11)), f2f(7, 6, 1, 2),             # tie: to even (1.0)
           mov32i(8, f32bits(1 + 3 * 2 ** -11)), f2f(9, 8, 1, 2),         # tie: to even (1 + 2^-9)
           mov32i(10, f32bits(70000.0)), f2f(11, 10, 1, 2),                # overflow: inf
           EXIT]
    r = emu.run(seq, {})
    check("F2F.F32.F16 H0", r[:, 2], f32bits(1.5))
    check("F2F.F32.F16 H1", r[:, 3], f32bits(-0.333251953125))
    check("F2F.F16.F32 1/3", r[:, 5], 0x3555)
    check("F2F.F16.F32 tie down", r[:, 7], 0x3C00)
    check("F2F.F16.F32 tie up", r[:, 9], 0x3C02)
    check("F2F.F16.F32 overflow", r[:, 11], 0x7C00)

    # HFMA2: per half F32 FMA then F16; neg C; saturate.
    seq = [mov32i(1, hpair(1.5, 2.0)), mov32i(2, hpair(2.0, -0.5)), mov32i(3, hpair(0.25, 1.0)),
           hfma2(4, 1, 2, 3), hfma2(5, 1, 2, 3, neg_c=True), hfma2(6, 1, 2, 3, sat=True),
           mov32i(7, hpair(1 + 2 ** -10, 1.0)), mov32i(8, hpair(1 + 2 ** -10, 2 ** -11)), mov32i(9, hpair(0.0, 1.0)),
           hfma2(10, 7, 8, 9), EXIT]
    r = emu.run(seq, {})
    check("HFMA2", r[:, 4], hpair(3.25, 0.0))
    check("HFMA2 -C", r[:, 5], hpair(2.75, -2.0))
    check("HFMA2.SAT", r[:, 6], hpair(1.0, 0.0))
    # (1 + 2^-10)^2 = 1 + 2^-9 + 2^-20 -> F16 1 + 2^-9; 1 + 2^-11 is an F16 tie -> 1.0.
    check("HFMA2 rounding", r[:, 10], hpair(1 + 2 ** -9, 1.0))

    # The translator's Optimizer.PropagatePack: an F16 result unpacked again without an integer operation in between is the
    # unrounded F32 value (directly or through a MOV); LOP in between, or a guarded pack, keeps the F16 rounding.
    third, third16 = f32bits(1 / 3), f32bits(float(np.float16(1 / 3)))
    seq = [mov32i(1, third), f2f(2, 1, 1, 2), f2f(3, 2, 2, 1),               # pack, unpack: folded
           mov(4, 2), f2f(5, 4, 2, 1),                                       # through a MOV: folded
           lop32i(6, 2, 0xFFFFFFFF, 0), f2f(7, 6, 2, 1),                     # through LOP: rounded
           s2r(8), isetp_i(0, 8, 32, 1),                                     # P0 = lane < 32, true everywhere
           f2f(9, 1, 1, 2) & ~(0xF << 16) | guard(0), f2f(10, 9, 2, 1),       # guarded pack: rounded
           mov32i(11, hpair(40000.0, 1.0)), mov32i(12, hpair(2.0, 2.0)), mov32i(13, 0), mov32i(16, hpair(0.5, 0.5)),
           hfma2(14, 11, 12, 13), hfma2(15, 14, 16, 13),                     # 40000 * 2 = inf as F16, * 0.5 again
           EXIT]
    r = emu.run(seq, {})
    check("F2F.F16 then F2F.F32: unrounded (PropagatePack)", r[:, 3], third)
    check("... through MOV: unrounded", r[:, 5], third)
    check("... through LOP: rounded", r[:, 7], third16)
    check("... guarded pack: rounded", r[:, 10], third16)
    check("HFMA2 overflow stored as F16 inf", r[:, 14], hpair(np.inf, 2.0))
    check("HFMA2 into HFMA2: the unrounded 80000 halved", r[:, 15], hpair(40000.0, 1.0))

    # FFMA: rounded once. Exact a*b + c lies just below an F32 halfway point; F64 alone would round to the tie and
    # then to even (0x3f800002), the fused result is 0x3f800001.
    seq = [mov32i(1, 0x39800001), mov32i(2, 0x397FFFFE), mov32i(3, 0x3F800001), ffma(4, 1, 2, 3),
           mov32i(5, f32bits(3.0)), mov32i(6, f32bits(0.5)), ffma(7, 5, 6, 5), EXIT]
    r = emu.run(seq, {})
    check("FFMA single rounding at a tie", r[:, 4], 0x3F800001)
    check("FFMA", r[:, 7], f32bits(4.5))

    # PRMT: a no-op as the translator translates it, the Maxwell byte permute otherwise (0xbb99: sign of bytes 1 and 3).
    seq = [mov32i(1, 0x7F80C000), mov32i(2, 0xBB99), mov32i(3, 0x11111111), prmt(3, 1, 2, 255),
           mov32i(4, 0x3210), mov32i(5, 0x22222222), prmt(5, 1, 4, 255), EXIT]
    r = emu.run(seq, {}, prmt="ryujinx")
    check("PRMT (Ryujinx: no-op)", r[:, 3], 0x11111111)
    r = emu.run(seq, {}, prmt="maxwell")
    check("PRMT sign replicate (Maxwell)", r[:, 3], 0x0000FFFF)
    check("PRMT identity (Maxwell)", r[:, 5], 0x7F80C000)

    # Predicates: ISETP, a guarded write, an inverted guard, EXIT of some lanes, a uniform BRA over one instruction.
    seq = [s2r(1), isetp_i(0, 1, 16, 1),                           # P0 = lane < 16
           mov32i(2, 0xAAAA, g=guard(0)), mov32i(3, 0xBBBB, g=guard(0, inv=True)),
           bra(emu.slot_address(6) - emu.slot_address(4) - 8), mov32i(4, 0xDEAD),   # skipped
           mov32i(5, 0x5555),
           (EXIT & ~(0xF << 16)) | guard(0, inv=True),             # lanes >= 16 exit
           mov32i(6, 0x6666), EXIT]
    w = emu.run_warp(seq, regs_all(R2=1, R3=1, R6=1))
    check("ISETP.LT", w.p[:, 0], lane < 16)
    check("guarded write", w.r[:, 2], np.where(lane < 16, 0xAAAA, 1))
    check("inverted guard", w.r[:, 3], np.where(lane < 16, 1, 0xBBBB))
    check("BRA skips", w.r[:, 4], 0)
    check("after BRA", w.r[:, 5], 0x5555)
    check("guarded EXIT", w.r[:, 6], np.where(lane < 16, 0x6666, 1))

    # Unknown opcodes raise with the word.
    global checks
    checks += 1
    try:
        emu.run([0xFFFFFFFFFFFFFFFF], {})
        failures.append("unknown opcode did not raise")
    except emu.UnknownOpcode as e:
        if "0xffffffffffffffff" not in str(e):
            failures.append(f"unknown opcode message: {e}")


# ---- part 2: lowered SM86 tensor instructions ---------------------------------------------------------------------

def hmma_case(k16: bool, rng):
    """HMMA.{1688,16816}.F16 R4, R8, R12, R16 with small integers; returns (sm86, regs_in, expected D regs)."""
    K = 16 if k16 else 8
    A = rng.integers(-3, 4, size=(16, K)).astype(np.float64)
    B = rng.integers(-3, 4, size=(K, 8)).astype(np.float64)
    C = rng.integers(-8, 9, size=(16, 8)).astype(np.float64)
    D = A @ B + C
    regs_in, want = {}, {}
    for t in range(LANES):
        g, q = t // 4, t % 4
        regs = {}
        for j in range(K // 8):
            for r in range(2):
                regs[f"R{8 + 2 * j + r}"] = hpair(A[g + 8 * r, 8 * j + 2 * q], A[g + 8 * r, 8 * j + 2 * q + 1])
            regs[f"R{12 + j}"] = hpair(B[8 * j + 2 * q, g], B[8 * j + 2 * q + 1, g])
        for r in range(2):
            regs[f"R{16 + r}"] = hpair(C[g + 8 * r, 2 * q], C[g + 8 * r, 2 * q + 1])
        regs_in[t] = regs
        want[t] = {4 + r: hpair(D[g + 8 * r, 2 * q], D[g + 8 * r, 2 * q + 1]) for r in range(2)}
    w0 = 0x23C | 7 << 12 | 4 << 16 | 8 << 24
    sm86 = [w0, 12, 16 | (1 << 11 if k16 else 0), 0]
    return sm86, regs_in, want


def part2(exe: Path):
    rng = np.random.default_rng(86)
    for k16 in (False, True):
        sm86, regs_in, want = hmma_case(k16, rng)
        words, urbase, _ = emu.lower(sm86, exe)
        r = emu.run(words, regs_in)
        for reg in (4, 5):
            check(f"HMMA.{'16816' if k16 else '1688'} D R{reg} (lowered, {len(words)} words)",
                  r[:, reg], [want[t][reg] for t in range(LANES)])

    # LDSM.16.M88.4 R4, [R1] and LDSM.16.MT88.1 R8, [R1]: matrix m row r at 0x200 * m + 0x20 * r (16 bytes used),
    # element (m, row, col) = 0x1000 * m + 0x100 * row + col.
    smem = bytearray(0x1000)
    for m in range(4):
        for row in range(8):
            for col in range(8):
                at = 0x200 * m + 0x20 * row + 2 * col
                smem[at:at + 2] = (0x1000 * m + 0x100 * row + col).to_bytes(2, "little")
    addr = {t: {"R1": 0x200 * (t // 8) + 0x20 * (t % 8)} for t in range(LANES)}
    el = lambda m, row, col: 0x1000 * m + 0x100 * row + col
    for transpose, size, dst in ((False, 2, 4), (True, 0, 8), (True, 2, 12), (False, 0, 16)):
        sm86 = [0x83B | 7 << 12 | dst << 16 | 1 << 24, 0, size << 8 | (0x4000 if transpose else 0), 0]
        words, _, _ = emu.lower(sm86, exe)
        r = emu.run(words, addr, bytes(smem))
        for m in range(1 << size):
            if transpose:   # column t/4 of rows 2(t%4) (low) and 2(t%4)+1 (high)
                want = [el(m, 2 * (t % 4), t // 4) | el(m, 2 * (t % 4) + 1, t // 4) << 16 for t in range(LANES)]
            else:           # row t/4, columns 2(t%4), 2(t%4)+1
                want = [el(m, t // 4, 2 * (t % 4)) | el(m, t // 4, 2 * (t % 4) + 1) << 16 for t in range(LANES)]
            check(f"LDSM.16.{'MT88' if transpose else 'M88'}.{1 << size} matrix {m}", r[:, dst + m], want)

    # MOVM.16.MT88 R2, R1: element (row, col) = 8 * row + col at lane 4 row + col / 2, half col % 2.
    regs_in = {t: {"R1": (8 * (t // 4) + 2 * (t % 4)) | (8 * (t // 4) + 2 * (t % 4) + 1) << 16} for t in range(LANES)}
    words, _, _ = emu.lower([0x23A | 7 << 12 | 2 << 16 | 1 << 24, 0, 0, 0], exe)
    r = emu.run(words, regs_in)
    want = [(8 * (2 * (t % 4)) + t // 4) | (8 * (2 * (t % 4) + 1) + t // 4) << 16 for t in range(LANES)]
    check("MOVM.16.MT88", r[:, 2], want)

    # HFMA2.RELU R0, R1, R2, R3 (PT) as the translator runs it (no PRMT: AmpereLowering.cs Half builds the mask with
    # integer operations), whatever the scratch registers held before: negative sums become +0, positive ones stay.
    sm86 = [0x231 | 7 << 12 | 0 << 16 | 1 << 24, 2, 3 | 0x8000 | 0x03800000, 0]
    words, _, _ = emu.lower(sm86, exe)
    for a, b, c, want in (((1.0, -2.0), (-3.0, 0.5), (1.0, 0.25), (0.0, 0.0)),       # sums -2.0, -0.75
                          ((1.0, 2.0), (3.0, -0.5), (1.0, 0.25), (4.0, 0.0))):      # sums 4.0, -0.75
        regs_in = regs_all(R1=hpair(*a), R2=hpair(*b), R3=hpair(*c))
        for fill in (0, 0x7FC0DEAD):
            w = emu.run_warp(words, regs_in, prmt="ryujinx", fill=fill)
            check(f"HFMA2.RELU -> {want} as translated (fill {fill:#x})", w.r[:, 0], hpair(*want))
            check(f"HFMA2.RELU uses no PRMT (fill {fill:#x})", [len(w.warnings)], 0)


def main() -> int:
    part1()
    exe = emu.DEFAULT_XLATE
    if exe.exists():
        part2(exe)
    else:
        print(f"  skip: part 2 needs {exe}")
    if failures:
        print("\n".join("FAIL " + f for f in failures))
    print(f"{checks - len(failures)}/{checks} checks passed")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
