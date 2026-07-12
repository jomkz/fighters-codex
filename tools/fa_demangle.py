#!/usr/bin/env python3
"""MSVC name demangling for the recovered FA.SMS symbols (#452).

The FA.SMS names in db/symbols/ are *decorated* by the 1990s MSVC toolchain, and the
decoration encodes a signature we never parsed. This module turns a decorated name back
into a C prototype so the fxe generator -- and CI, which has no Ghidra -- can consume it
as a reviewable fact instead of relying on Ghidra's in-project demangling.

Two decoration schemes appear, carrying different amounts of truth:

    ?COLFlatGround@@YIDJPAUF24_POINT3@@00@Z   C++ mangling -- the COMPLETE signature:
                                              convention, return type, parameter types.
    _FMFuelConsumption@4                      C decoration -- convention (stdcall) and the
    @FMBurnNPCFuel@4                          argument byte count (fastcall). No types.
    _CTVarDiff                                C decoration -- convention (cdecl) only.

Every `@N` in the corpus is divisible by 4: all arguments are 4-byte dwords, so N/4 is the
arity exactly. For those we emit `undefined4` parameters -- Ghidra's idiom for "4 bytes,
type unrecovered". That is the honest encoding of what the name proves: the callee pops N
bytes. It does not invent a type (see #451: a wrong datatype is worse than none).

A cdecl name with no `@N` proves a convention but no arity, and a C prototype cannot say
"unknown parameters" without lying about the count -- so those yield nothing here and are
left to the per-subsystem recovery in #453.

STRICTNESS: every parse is all-or-nothing. Anything this module does not fully understand
returns None rather than a guess. A missing signature costs us a row; a wrong one silently
corrupts everything the generator emits downstream.
"""

import re

# --- MSVC type codes ---------------------------------------------------------------
# Primitives are single-character (or `_`-prefixed) and are NOT entered into the
# back-reference table; compound types are.
PRIMITIVE = {
    "X": "void",
    "C": "signed char",
    "D": "char",
    "E": "unsigned char",
    "F": "short",
    "G": "unsigned short",
    "H": "int",
    "I": "unsigned int",
    "J": "long",
    "K": "unsigned long",
    "M": "float",
    "N": "double",
    "O": "long double",
    "_N": "bool",
    "_J": "__int64",
    "_K": "unsigned __int64",
    "_W": "wchar_t",
}

# Calling-convention codes, as they appear after the `Y` (free-function) marker.
CALLCONV = {
    "A": "__cdecl",
    "B": "__cdecl",
    "C": "__pascal",
    "E": "__thiscall",
    "G": "__stdcall",
    "I": "__fastcall",
}

# Aggregate tags: U=struct, V=class, T=union, W=enum (enums carry an extra size digit).
AGGREGATE = {"U": "struct", "V": "class", "T": "union"}

# Pointer CV modifiers.
CV = {"A": "", "B": "const ", "C": "volatile ", "D": "const volatile "}

_IDENT_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")


class _Cursor:
    """A parse position over the mangled string, with the back-reference table."""

    def __init__(self, s):
        self.s = s
        self.i = 0
        self.backrefs = []

    def peek(self):
        return self.s[self.i] if self.i < len(self.s) else ""

    def take(self):
        c = self.peek()
        if not c:
            raise _Unparsed("ran off the end")
        self.i += 1
        return c

    def expect(self, c):
        if self.take() != c:
            raise _Unparsed("expected %r" % c)


class _Unparsed(Exception):
    """This module does not understand the construct. Yield nothing, never a guess."""


def _read_name(cur):
    """Read an `@`-terminated identifier fragment."""
    start = cur.i
    j = cur.s.find("@", start)
    if j < 0:
        raise _Unparsed("unterminated name")
    name = cur.s[start:j]
    cur.i = j + 1
    if not _IDENT_RE.match(name):
        raise _Unparsed("not a plain identifier: %r" % name)
    return name


def _read_type(cur):
    """Read one type.

    Back-references are resolved but NOT recorded here: MSVC's table holds each complete
    top-level *argument* type, not the components nested inside one. Recording a pointee
    would make `PAUF24_POINT3@@` followed by `0` resolve to `F24_POINT3` instead of
    `F24_POINT3 *`. The argument loop does the recording.
    """
    c = cur.take()

    # Back-reference into the arg table: digits repeat an earlier COMPOUND argument type.
    if c.isdigit():
        idx = int(c)
        if idx >= len(cur.backrefs):
            raise _Unparsed("back-reference %d out of range" % idx)
        return cur.backrefs[idx]

    # `_`-prefixed primitives (bool, __int64, ...).
    if c == "_":
        code = "_" + cur.take()
        if code not in PRIMITIVE:
            raise _Unparsed("unknown extended primitive %r" % code)
        return PRIMITIVE[code]

    if c in PRIMITIVE:
        return PRIMITIVE[c]

    # Pointers (P) and references (A).
    if c in ("P", "A"):
        # `P6` is a pointer to function: P6<cc><ret><params>@Z.
        if c == "P" and cur.peek() == "6":
            cur.take()
            cc = cur.take()
            if cc not in CALLCONV:
                raise _Unparsed("unknown convention %r in function pointer" % cc)
            ret = _read_type(cur)
            params = _read_arglist(cur)
            cur.expect("Z")
            args = ", ".join(params) if params else "void"
            return "%s (%s *)(%s)" % (ret, CALLCONV[cc], args)

        cvc = cur.take()
        if cvc not in CV:
            raise _Unparsed("unknown CV modifier %r" % cvc)
        pointee = _read_type(cur)
        sigil = "*" if c == "P" else "&"
        return "%s%s %s" % (CV[cvc], pointee, sigil)

    # Aggregates: struct/class/union <name>@@.
    if c in AGGREGATE:
        name = _read_name(cur)
        cur.expect("@")
        return name

    # Enums: W<size-digit><name>@@. The digit encodes the underlying integer type; the
    # enum's own name is what matters to us.
    if c == "W":
        cur.take()  # underlying-type digit (4 = int, the only one FA uses)
        name = _read_name(cur)
        cur.expect("@")
        return name

    raise _Unparsed("unhandled type code %r" % c)


def _read_arglist(cur):
    """Read an argument list, recording each compound top-level type as a back-reference.

    Terminated by `@`, except that a lone `X` (void) means "no arguments" and carries no
    terminator -- it runs straight into the trailing `Z`.
    """
    params = []
    if cur.peek() == "X":
        cur.take()
        return params
    while cur.peek() and cur.peek() != "@":
        start = cur.i
        t = _read_type(cur)
        # Only a type whose encoding ran longer than one character enters the table;
        # a bare back-reference digit does not re-enter it either.
        if cur.i - start > 1:
            cur.backrefs.append(t)
        params.append(t)
    cur.expect("@")
    return params


def demangle_cpp(mangled):
    """Demangle a `?name@@Y<cc><ret><params>@Z` free function.

    Returns (name, convention, return_type, [param_types]) or None if anything about
    the name is not fully understood.
    """
    if not mangled.startswith("?"):
        return None
    cur = _Cursor(mangled)
    try:
        cur.expect("?")
        name = _read_name(cur)
        cur.expect("@")          # `@@` closes the (empty) qualification list

        if cur.take() != "Y":    # free function; member functions are out of scope
            raise _Unparsed("not a free function")
        cc = cur.take()
        if cc not in CALLCONV:
            raise _Unparsed("unknown calling convention %r" % cc)
        conv = CALLCONV[cc]

        # `?A` prefixes a return type that is a UDT returned by value (an enum, here).
        if cur.peek() == "?":
            cur.take()
            cur.expect("A")
        ret = _read_type(cur)

        params = _read_arglist(cur)
        cur.expect("Z")
        if cur.i != len(cur.s):
            raise _Unparsed("trailing garbage")
    except _Unparsed:
        return None
    return name, conv, ret, params


def demangle_c(decorated):
    """Read an MSVC *C* decoration: `_name@N` (stdcall), `@name@N` (fastcall), `_name`
    (cdecl).

    Returns (name, convention, arg_bytes) with arg_bytes None when the decoration proves
    a convention but not an arity. Returns None for an undecorated name (one we coined
    ourselves), which proves nothing.
    """
    m = re.match(r"^([_@])([A-Za-z_][A-Za-z0-9_]*)@(\d+)$", decorated)
    if m:
        sigil, name, nbytes = m.groups()
        conv = "__stdcall" if sigil == "_" else "__fastcall"
        return name, conv, int(nbytes)
    m = re.match(r"^_([A-Za-z_][A-Za-z0-9_]*)$", decorated)
    if m:
        return m.group(1), "__cdecl", None
    return None


def _fmt_param(t):
    return t.replace("* *", "**").rstrip()


def prototype(mangled):
    """Turn a decorated name into a C prototype for the db/ `type` column, or None.

    The prototype is what Ghidra's FunctionSignatureParser consumes (ApplyTypes.java) and
    what the fxe generator reads. Unnamed parameters keep it a statement about types, not
    a claim about argument names we never recovered.
    """
    got = demangle_cpp(mangled)
    if got:
        name, conv, ret, params = got
        args = ", ".join(_fmt_param(p) for p in params) if params else "void"
        return "%s %s %s(%s)" % (ret, conv, name, args)

    got = demangle_c(mangled)
    if got:
        name, conv, nbytes = got
        if nbytes is None:
            # cdecl with no arity: a convention is not a signature. Leave it to #453
            # rather than assert an argument count the name never proved.
            return None
        if nbytes % 4:
            # Every `@N` in the corpus is a multiple of 4. A non-multiple would mean our
            # all-args-are-dwords premise is wrong -- refuse rather than round.
            return None
        n = nbytes // 4
        args = ", ".join(["undefined4"] * n) if n else "void"
        return "undefined4 %s %s(%s)" % (conv, name, args)

    return None
