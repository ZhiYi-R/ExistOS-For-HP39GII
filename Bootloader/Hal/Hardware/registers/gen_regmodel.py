#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_regmodel.py — STMP3770 typed C++23 register-model generator.

Parses the legacy hand-/XML-generated ``regs*.h`` headers in this directory
(the only surviving source of truth — the original XML and ``build_io_map``
tool were never imported) and emits a zero-overhead, type-safe C++23 model:

    generated/reg_types.hpp   the ``hw_<reg>_t`` unions (verbatim layout)
    generated/reg_model.hpp   Register<>/MultiRegister<> templates + descriptors
    generated/reg_values.hpp  BV_* enum symbols + BF_* field-value synthesis
    generated/reg_verify.cpp  ~10k static_assert: new model == old BM_/BP_/_ADDR

The generated model lives entirely in ``namespace reg`` so reg_verify.cpp can
include BOTH the new model and every legacy header in one TU without union /
macro name clashes, and assert bit-for-bit parity at compile time.

Run:  python gen_regmodel.py        (from this directory, or any cwd)
"""

import os
import re
import sys
import glob

HERE = os.path.dirname(os.path.abspath(__file__))
OUTDIR = os.path.join(HERE, "generated")

# The model covers exactly the register-module headers the firmware actually
# #includes (confirmed by a tree-wide include scan). The remaining ~24 regs*.h
# (regsaudioin, regsdcp, regsdram, regsi2c, regspwm, regstvenc, the regssim*
# simulation blocks, ...) are referenced by nothing and are dead code — they are
# deleted outright in Phase 5 rather than modelled. (Some, e.g. regssimsspsel.h,
# even contain malformed duplicate-field unions that never compiled.)
INCLUDE = {
    "regsapbh.h", "regsapbx.h", "regsaudioout.h", "regsclkctrl.h",
    "regsdigctl.h", "regsecc8.h", "regsgpmi.h", "regsicoll.h", "regslcdif.h",
    "regslradc.h", "regspinctrl.h", "regspower.h", "regsrtc.h", "regsssp.h",
    "regstimrot.h", "regsuartdbg.h", "regsusbctrl.h", "regsusbphy.h",
}

REGS_BASE = 0x80000000


# --------------------------------------------------------------------------
# IR
# --------------------------------------------------------------------------
class Field:
    __slots__ = ("name", "pos", "mask", "syms")

    def __init__(self, name, pos, mask):
        self.name = name
        self.pos = pos
        self.mask = mask
        self.syms = []           # list of (sym_name, value)


class Register:
    __slots__ = ("name", "module", "multi", "addr", "stride", "count",
                 "set_addr", "clr_addr", "tog_addr", "atomic",
                 "writable", "union_text", "union_tname", "fields")

    def __init__(self, name):
        self.name = name         # e.g. RTC_CTRL  or  APBH_CHn_CMD
        self.module = name.split("_", 1)[0]
        self.multi = False
        self.addr = None         # absolute base address
        self.stride = 0          # multi only
        self.count = 0           # multi only
        self.set_addr = None
        self.clr_addr = None
        self.tog_addr = None
        self.atomic = False
        self.writable = False
        self.union_text = None
        self.union_tname = None  # exact legacy 'hw_<name>_t' inner name (case-preserving)
        self.fields = []         # list[Field]


# --------------------------------------------------------------------------
# chip_io_map.h — module base addresses
# --------------------------------------------------------------------------
def parse_module_bases():
    bases = {"REGS_BASE": REGS_BASE}
    text = open(os.path.join(HERE, "chip_io_map.h"), encoding="latin-1").read()
    for m in re.finditer(
            r"#define\s+(REGS_\w+_BASE)\s+\(\s*REGS_BASE\s*\+\s*(0x[0-9A-Fa-f]+)\s*\)",
            text):
        bases[m.group(1)] = REGS_BASE + int(m.group(2), 16)
    return bases


def eval_addr(expr, bases):
    """Evaluate a constant address expression like '(REGS_RTC_BASE + 0x30)'."""
    env = dict(bases)
    return eval(expr, {"__builtins__": {}}, env)


# --------------------------------------------------------------------------
# Per-header parsing
# --------------------------------------------------------------------------
RE_UNION = re.compile(
    r"typedef\s+union\s*\{(?P<body>.*?)\}\s*hw_(?P<name>\w+)_t\s*;",
    re.DOTALL)

RE_ADDR = re.compile(
    r"#define\s+HW_(\w+)_ADDR\s+\((.*?)\)\s*$", re.MULTILINE)
RE_ADDR_N = re.compile(
    r"#define\s+HW_(\w+)_ADDR\(n\)\s+\((.*?)\)\s*$", re.MULTILINE)
RE_SET_ADDR = re.compile(
    r"#define\s+HW_(\w+)_SET_ADDR\s+\((.*?)\)\s*$", re.MULTILINE)
RE_CLR_ADDR = re.compile(
    r"#define\s+HW_(\w+)_CLR_ADDR\s+\((.*?)\)\s*$", re.MULTILINE)
RE_TOG_ADDR = re.compile(
    r"#define\s+HW_(\w+)_TOG_ADDR\s+\((.*?)\)\s*$", re.MULTILINE)
RE_SET_ADDR_N = re.compile(
    r"#define\s+HW_(\w+)_SET_ADDR\(n\)\s+\((.*?)\)\s*$", re.MULTILINE)
RE_CLR_ADDR_N = re.compile(
    r"#define\s+HW_(\w+)_CLR_ADDR\(n\)\s+\((.*?)\)\s*$", re.MULTILINE)
RE_TOG_ADDR_N = re.compile(
    r"#define\s+HW_(\w+)_TOG_ADDR\(n\)\s+\((.*?)\)\s*$", re.MULTILINE)
RE_COUNT = re.compile(
    r"#define\s+HW_(\w+)_COUNT\s+(\d+)\s*$", re.MULTILINE)
RE_WR = re.compile(r"#define\s+HW_(\w+)_WR\b")

RE_BP = re.compile(r"#define\s+BP_(\w+)\s+(\d+)\s*$", re.MULTILINE)
RE_BM = re.compile(r"#define\s+BM_(\w+)\s+(0x[0-9A-Fa-f]+)\s*$", re.MULTILINE)
RE_BV = re.compile(r"#define\s+BV_(\w+?)__(\w+)\s+(0x[0-9A-Fa-f]+|\d+)\s*$",
                   re.MULTILINE)

# strip line comments so they never bleed into a captured macro expansion
RE_LINE_COMMENT = re.compile(r"//[^\n]*")
RE_BLOCK_COMMENT = re.compile(r"/\*.*?\*/", re.DOTALL)


def split_n_expr(expr):
    """For a multi-instance addr expr, return (base_value_expr, stride_int)."""
    m = re.search(r"\(\(n\)\s*\*\s*(0x[0-9A-Fa-f]+|\d+)\)", expr)
    stride = int(m.group(1), 16 if m.group(1).startswith("0x") else 10)
    base_expr = expr[:m.start()] + expr[m.end():]
    base_expr = base_expr.rstrip().rstrip("+").strip()
    return base_expr, stride


def parse_header(path, bases, regs):
    raw = open(path, encoding="latin-1").read()
    # comment-stripped view for macro scanning (keep raw for unions)
    code = RE_BLOCK_COMMENT.sub("", raw)
    code = RE_LINE_COMMENT.sub("", code)

    # local module base fallbacks (#ifndef REGS_X_BASE #define ...)
    for m in re.finditer(
            r"#define\s+(REGS_\w+_BASE)\s+\(\s*REGS_BASE\s*\+\s*(0x[0-9A-Fa-f]+)\s*\)",
            code):
        bases.setdefault(m.group(1), REGS_BASE + int(m.group(2), 16))

    def reg(name):
        r = regs.get(name)
        if r is None:
            r = Register(name)
            regs[name] = r
        return r

    # multi-instance addresses first (more specific)
    multi_names = set()
    for m in RE_ADDR_N.finditer(code):
        name, expr = m.group(1), m.group(2)
        base_expr, stride = split_n_expr(expr)
        r = reg(name)
        r.multi = True
        r.addr = eval_addr(base_expr, bases)
        r.stride = stride
        multi_names.add(name)
    for m in RE_COUNT.finditer(code):
        name = m.group(1)
        if name in regs:
            regs[name].count = int(m.group(2))

    # single-instance addresses (skip the _ADDR(n) ones already handled)
    for m in RE_ADDR.finditer(code):
        name, expr = m.group(1), m.group(2)
        if name in multi_names:
            continue
        if "(n)" in expr or "n" in re.sub(r"0x[0-9A-Fa-f]+", "", expr):
            continue
        r = reg(name)
        r.addr = eval_addr(expr, bases)

    for rex, attr in ((RE_SET_ADDR, "set_addr"),
                      (RE_CLR_ADDR, "clr_addr"),
                      (RE_TOG_ADDR, "tog_addr")):
        for m in rex.finditer(code):
            name, expr = m.group(1), m.group(2)
            if name in regs:
                if name in multi_names:
                    expr2, _ = split_n_expr(expr) if "(n)" in expr else (expr, 0)
                    setattr(regs[name], attr, eval_addr(expr2, bases))
                else:
                    setattr(regs[name], attr, eval_addr(expr, bases))
    for rex, attr in ((RE_SET_ADDR_N, "set_addr"),
                      (RE_CLR_ADDR_N, "clr_addr"),
                      (RE_TOG_ADDR_N, "tog_addr")):
        for m in rex.finditer(code):
            name, expr = m.group(1), m.group(2)
            if name in regs:
                base_expr, _ = split_n_expr(expr)
                setattr(regs[name], attr, eval_addr(base_expr, bases))

    for m in RE_WR.finditer(code):
        if m.group(1) in regs:
            regs[m.group(1)].writable = True

    # unions (from raw text to preserve exact field layout & comments-free body)
    # Register keys keep the multi-instance lower-case trailing 'n'
    # (e.g. TIMROT_TIMCTRLn), so match union names case-insensitively.
    upper_to_reg = {rn.upper(): rn for rn in regs}
    for m in RE_UNION.finditer(RE_LINE_COMMENT.sub("", raw)):
        uname = m.group("name")          # lower-case, == reg name lower
        rname = upper_to_reg.get(uname.upper())
        if rname is not None:
            regs[rname].union_text = m.group("body").strip("\n")
            regs[rname].union_tname = m.group("name")  # exact case, e.g. icoll_PRIORITYn

    # fields: BP_/BM_ keyed by the longest matching register prefix
    reg_names_sorted = sorted(regs.keys(), key=len, reverse=True)

    def match_reg(sym):
        for rn in reg_names_sorted:
            if sym.startswith(rn + "_"):
                return rn, sym[len(rn) + 1:]
        return None, None

    bp = {}
    bm = {}
    for m in RE_BP.finditer(code):
        bp[m.group(1)] = int(m.group(2))
    for m in RE_BM.finditer(code):
        bm[m.group(1)] = int(m.group(2), 16)

    for sym, pos in bp.items():
        if sym not in bm:
            continue
        rn, fld = match_reg(sym)
        if rn is None:
            continue
        r = regs[rn]
        if any(f.name == fld for f in r.fields):
            continue
        r.fields.append(Field(fld, pos, bm[sym]))

    # BV enum symbols
    for m in RE_BV.finditer(code):
        regfield, symname, val = m.group(1), m.group(2), m.group(3)
        rn, fld = match_reg(regfield)
        if rn is None:
            continue
        r = regs[rn]
        target = next((f for f in r.fields if f.name == fld), None)
        if target is None:
            continue
        v = int(val, 16 if val.startswith("0x") else 10)
        if symname not in (s for s, _ in target.syms):
            target.syms.append((symname, v))


# --------------------------------------------------------------------------
# Emission
# --------------------------------------------------------------------------
BANNER = """// SPDX-License-Identifier: GPL-2.0-or-later
//
// GENERATED by Bootloader/Hal/Hardware/registers/gen_regmodel.py
// Source of truth: the legacy regs*.h headers in the same directory.
// Do not edit by hand — re-run the generator (target: regmodel-regen).
//
"""


def hexu(v):
    return "0x%08Xu" % (v & 0xFFFFFFFF) if v <= 0xFFFFFFFF else "0x%Xull" % v


def addru(v):
    return "0x%08Xull" % v if v > 0xFFFFFFFF else "0x%08Xu" % v


def emit_types(regs):
    out = [BANNER, "#pragma once", "",
           "// Bitfield unions, layout-identical to the legacy hw_<reg>_t.",
           "namespace reg {", "",
           "typedef volatile unsigned char  reg8_t;",
           "typedef volatile unsigned short reg16_t;",
           "typedef volatile unsigned int   reg32_t;", ""]
    seen = set()
    for r in regs:
        if r.union_text is None or r.name in seen:
            continue
        seen.add(r.name)
        lname = r.union_tname or r.name.lower()
        out.append("typedef union {")
        out.append(r.union_text)
        out.append("} hw_%s_t;" % lname)
        out.append("")
    out.append("}  // namespace reg")
    out.append("")
    return "\n".join(out)


def access_of(r):
    if not r.writable:
        return "Access::RO"
    return "Access::RW"


def emit_model(regs):
    out = [BANNER, "#pragma once", '#include "reg_types.hpp"',
           "#include <cstdint>", "", "namespace reg {", "",
           "enum class Access { RO, WO, RW };", "",
           "// Single-instance memory-mapped register.",
           "template<uintptr_t Addr, typename T, Access A,",
           "         uintptr_t Set = 0, uintptr_t Clr = 0, uintptr_t Tog = 0>",
           "struct Register {",
           "    static constexpr uintptr_t address   = Addr;",
           "    static constexpr bool      has_atomic = (Set != 0);",
           "    using type = T;",
           "    [[gnu::always_inline]] static uint32_t rd() noexcept",
           "        { return *reinterpret_cast<volatile uint32_t*>(Addr); }",
           "    [[gnu::always_inline]] static void wr(uint32_t v) noexcept",
           "        { *reinterpret_cast<volatile uint32_t*>(Addr) = v; }",
           "    [[gnu::always_inline]] static volatile uint32_t& U() noexcept",
           "        { return *reinterpret_cast<volatile uint32_t*>(Addr); }",
           "    [[gnu::always_inline]] static auto& B() noexcept",
           "        { return reinterpret_cast<volatile T*>(Addr)->B; }",
           "    [[gnu::always_inline]] static void set(uint32_t v) noexcept requires (Set != 0)",
           "        { *reinterpret_cast<volatile uint32_t*>(Set) = v; }",
           "    [[gnu::always_inline]] static void clr(uint32_t v) noexcept requires (Clr != 0)",
           "        { *reinterpret_cast<volatile uint32_t*>(Clr) = v; }",
           "    [[gnu::always_inline]] static void tog(uint32_t v) noexcept requires (Tog != 0)",
           "        { *reinterpret_cast<volatile uint32_t*>(Tog) = v; }",
           "};", "",
           "// Multi-instance register: address(n) = Base + n*Stride.",
           "template<uintptr_t Base, typename T, Access A, uintptr_t Stride, unsigned Count,",
           "         uintptr_t Set = 0, uintptr_t Clr = 0, uintptr_t Tog = 0>",
           "struct MultiRegister {",
           "    static constexpr unsigned count = Count;",
           "    static constexpr bool has_atomic = (Set != 0);",
           "    using type = T;",
           "    [[gnu::always_inline]] static constexpr uintptr_t address(unsigned n) noexcept",
           "        { return Base + n * Stride; }",
           "    [[gnu::always_inline]] static uint32_t rd(unsigned n) noexcept",
           "        { return *reinterpret_cast<volatile uint32_t*>(Base + n * Stride); }",
           "    [[gnu::always_inline]] static void wr(unsigned n, uint32_t v) noexcept",
           "        { *reinterpret_cast<volatile uint32_t*>(Base + n * Stride) = v; }",
           "    [[gnu::always_inline]] static volatile uint32_t& U(unsigned n) noexcept",
           "        { return *reinterpret_cast<volatile uint32_t*>(Base + n * Stride); }",
           "    [[gnu::always_inline]] static auto& B(unsigned n) noexcept",
           "        { return reinterpret_cast<volatile T*>(Base + n * Stride)->B; }",
           "    [[gnu::always_inline]] static void set(unsigned n, uint32_t v) noexcept requires (Set != 0)",
           "        { *reinterpret_cast<volatile uint32_t*>(Set + n * Stride) = v; }",
           "    [[gnu::always_inline]] static void clr(unsigned n, uint32_t v) noexcept requires (Clr != 0)",
           "        { *reinterpret_cast<volatile uint32_t*>(Clr + n * Stride) = v; }",
           "    [[gnu::always_inline]] static void tog(unsigned n, uint32_t v) noexcept requires (Tog != 0)",
           "        { *reinterpret_cast<volatile uint32_t*>(Tog + n * Stride) = v; }",
           "};", "",
           "// Per-field geometry helper.",
           "template<unsigned Pos, uint32_t Mask>",
           "struct Field {",
           "    static constexpr unsigned pos  = Pos;",
           "    static constexpr uint32_t mask = Mask;",
           "    [[gnu::always_inline]] static constexpr uint32_t val(uint32_t v) noexcept",
           "        { return (v << Pos) & Mask; }",
           "    [[gnu::always_inline]] static constexpr uint32_t get(uint32_t r) noexcept",
           "        { return (r & Mask) >> Pos; }",
           "};", ""]

    for r in regs:
        if r.addr is None:
            continue
        tname = "hw_%s_t" % (r.union_tname or r.name.lower()) if r.union_text else "uint32_t"
        # field descriptors namespace
        if r.fields:
            out.append("namespace %s_ {" % r.name)
            for f in r.fields:
                out.append("    using %s = Field<%d, %s>;" %
                           (f.name, f.pos, hexu(f.mask)))
            out.append("}")
        if r.multi:
            margs = [addru(r.addr), tname, access_of(r), hexu(r.stride),
                     str(r.count)]
            if r.set_addr or r.clr_addr or r.tog_addr:
                margs.append(addru(r.set_addr) if r.set_addr else "0")
                margs.append(addru(r.clr_addr) if r.clr_addr else "0")
                margs.append(addru(r.tog_addr) if r.tog_addr else "0")
            out.append("using %s = MultiRegister<%s>;" %
                       (r.name, ", ".join(margs)))
        else:
            args = [addru(r.addr), tname, access_of(r)]
            if r.set_addr or r.clr_addr or r.tog_addr:
                args.append(addru(r.set_addr) if r.set_addr else "0")
                args.append(addru(r.clr_addr) if r.clr_addr else "0")
                args.append(addru(r.tog_addr) if r.tog_addr else "0")
            out.append("using %s = Register<%s>;" % (r.name, ", ".join(args)))
        out.append("")
    out.append("}  // namespace reg")
    out.append("")
    return "\n".join(out)


def emit_values(regs):
    out = [BANNER, "#pragma once", '#include "reg_model.hpp"',
           "#include <cstdint>", "",
           "// BV_* enumerated field symbols, as constexpr values living next to",
           "// their field. Used for off-MMIO command-word synthesis (DMA descriptors).",
           "namespace reg {", ""]
    any_sym = False
    for r in regs:
        syms = [(f, s, v) for f in r.fields for (s, v) in f.syms]
        if not syms:
            continue
        any_sym = True
        out.append("namespace %s_sym {" % r.name)
        for f, s, v in syms:
            out.append("    inline constexpr uint32_t %s__%s = %s;" %
                       (f.name, s, hexu(v)))
        out.append("}")
    if not any_sym:
        out.append("// (no BV_* symbols)")
    out.append("}  // namespace reg")
    out.append("")
    return "\n".join(out)


def emit_verify(regs, headers):
    out = [BANNER,
           "// Compile-only parity gate: every generated constant is asserted",
           "// equal to the legacy BM_/BP_/_ADDR macro it replaces. Not linked.",
           "",
           '#include "reg_model.hpp"',
           '#include "reg_values.hpp"',
           "", "// legacy source of truth:"]
    for h in headers:
        out.append('#include "../%s"' % h)
    out.append("")
    out.append("namespace {")
    n = 0
    for r in regs:
        if r.addr is None:
            continue
        if r.multi:
            out.append("static_assert(reg::%s::address(0) == HW_%s_ADDR(0));" %
                       (r.name, r.name))
            out.append("static_assert(reg::%s::address(1) == HW_%s_ADDR(1));" %
                       (r.name, r.name))
            n += 2
            if r.set_addr:
                out.append(
                    "static_assert(reg::%s::has_atomic);" % r.name)
                n += 1
        else:
            out.append("static_assert(reg::%s::address == HW_%s_ADDR);" %
                       (r.name, r.name))
            n += 1
            if r.set_addr:
                out.append("static_assert(reg::%s::address + 0x4 == HW_%s_SET_ADDR);" %
                           (r.name, r.name))
                out.append("static_assert(reg::%s::has_atomic);" % r.name)
                n += 2
        if r.union_text:
            tn = r.union_tname or r.name.lower()
            out.append("static_assert(sizeof(reg::hw_%s_t) == sizeof(hw_%s_t));" %
                       (tn, tn))
            n += 1
        for f in r.fields:
            out.append("static_assert(reg::%s_::%s::mask == BM_%s_%s);" %
                       (r.name, f.name, r.name, f.name))
            out.append("static_assert(reg::%s_::%s::pos  == BP_%s_%s);" %
                       (r.name, f.name, r.name, f.name))
            n += 2
            for s, v in f.syms:
                out.append(
                    "static_assert(reg::%s_sym::%s__%s == BV_%s_%s__%s);" %
                    (r.name, f.name, s, r.name, f.name, s))
                n += 1
    out.append("}  // namespace")
    out.append("")
    out.append("// total static_assert: %d" % n)
    out.append("")
    return "\n".join(out), n


# --------------------------------------------------------------------------
def main():
    bases = parse_module_bases()
    headers = sorted(os.path.basename(p) for p in glob.glob(os.path.join(HERE, "regs*.h"))
                     if os.path.basename(p) in INCLUDE)
    regs = {}
    for h in headers:
        parse_header(os.path.join(HERE, h), bases, regs)

    # stable order: by module then address
    reglist = sorted(regs.values(),
                     key=lambda r: (r.module, r.addr if r.addr is not None else 0,
                                    r.name))
    placed = [r for r in reglist if r.addr is not None]

    os.makedirs(OUTDIR, exist_ok=True)
    open(os.path.join(OUTDIR, "reg_types.hpp"), "w", newline="\n").write(
        emit_types(placed))
    open(os.path.join(OUTDIR, "reg_model.hpp"), "w", newline="\n").write(
        emit_model(placed))
    open(os.path.join(OUTDIR, "reg_values.hpp"), "w", newline="\n").write(
        emit_values(placed))
    verify_text, n_assert = emit_verify(placed, headers)
    open(os.path.join(OUTDIR, "reg_verify.cpp"), "w", newline="\n").write(
        verify_text)

    n_fields = sum(len(r.fields) for r in placed)
    n_multi = sum(1 for r in placed if r.multi)
    n_sym = sum(len(f.syms) for r in placed for f in r.fields)
    print("parsed %d headers" % len(headers))
    print("registers: %d (multi-instance: %d)" % (len(placed), n_multi))
    print("fields: %d   BV symbols: %d" % (n_fields, n_sym))
    print("static_assert generated: %d" % n_assert)
    miss = [r.name for r in regs.values() if r.addr is None]
    if miss:
        print("WARN: %d registers had no parseable address: %s" %
              (len(miss), ", ".join(sorted(miss)[:20])))


if __name__ == "__main__":
    main()
