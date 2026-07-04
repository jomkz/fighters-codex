#!/usr/bin/env python3
"""Validate format-spec front-matter and generate the per-format status matrix.

Usage:
  python3 tools/check_status.py --self-test     run the embedded test suite
  python3 tools/check_status.py --check         validate docs + claims; fail if
                                                docs/fa/formats/STATUS.md is stale
  python3 tools/check_status.py --write-matrix  validate, then regenerate STATUS.md

Every format spec in docs/fa/formats/ carries YAML front-matter describing its
status and pointing at the code that validates it (docs/spec-authoring.md).
This script is the single source of truth for the canonical vocabularies, and
it checks three layers:

  1. docs-only:   front-matter schema, section skeleton, encoding (UTF-8, no
                  BOM, no mojibake), relative-link integrity (case-exact,
                  and links in docs/ must stay inside the docs tree so the
                  published site renders them), repo blob/tree URLs point at
                  real main-branch paths, README index membership.
  2. claims:      every lib/cli/test/fuzz/gui pointer resolves to a real file;
                  every fx command token is a live dispatch literal.
  3. coverage:    every codec, CLI command, test, fuzz harness, and GUI
                  editor in the repository is claimed by a spec.

It also validates the FA.EXE reconstruction program (epic #209): the db/ symbol
database (manifest + per-subsystem symbol files + committed Ghidra inventory),
per-subsystem coverage for completed subsystems (every in-scope function named,
every referenced global named or waived), subsystem-doc structure, and the
generated docs/fa/reconstruction.md matrix. See db/README.md.

Stdlib-only; Python 3.8+.
"""

import argparse
import csv
import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FORMATS_DIR = ROOT / "docs" / "fa" / "formats"
STATUS_MD = FORMATS_DIR / "STATUS.md"
MANIFEST = ROOT / "tests" / "integration" / "fa-extract.sha256"
CLI_MAIN = ROOT / "cli" / "main.cpp"
CLI_DOC = ROOT / "docs" / "cli.md"
NON_SPEC = {"README.md", "STATUS.md"}

# Reconstruction program (epic #209): machine-readable symbol database under db/,
# its generated matrix, and the enforced vocabularies. See db/README.md.
DB_DIR = ROOT / "db"
SUBSYSTEMS_CSV = DB_DIR / "subsystems.csv"
SYMBOLS_DIR = DB_DIR / "symbols"
INVENTORY_DIR = DB_DIR / "inventory"
RECON_MD = ROOT / "docs" / "fa" / "reconstruction.md"
SYMBOLS_MD = ROOT / "docs" / "fa" / "symbols.md"
GLOBALS_MD = ROOT / "docs" / "fa" / "globals.md"
RECON_STATUS = {"planned", "active", "complete"}
SYMBOL_KIND = {"func", "data"}
SYMBOL_SOURCE = {"sms", "re", "waiver"}
SYMBOL_CONFIDENCE = {"confirmed", "inferred"}
BLOB = "https://github.com/jomkz/fighters-codex/blob/main"

# Canonical H2 set, in required relative order.
CANONICAL_H2 = [
    "Tools",
    "File Layout",
    "File Inventory",
    "Engine Notes",
    "Round-Trip Notes",
    "Open Questions",
    "Related",
]

# category enum -> H2 heading in docs/fa/formats/README.md
CATEGORIES = {
    "archive": "Archive",
    "graphics": "Graphics & Images",
    "terrain": "Terrain & Maps",
    "3d": "3D & Scene",
    "audio": "Audio",
    "video": "Video & Cutscenes",
    "mission": "Mission & Campaign",
    "typedef": "Type Definitions (BRF DSL)",
    "ui-overlay": "UI & Win32 Overlays",
    "system": "System & Config",
    "installer": "Installer",
    "text": "Text",
}

ENDIANNESS = {"little", "big", "mixed", "none"}
SPEC_STATUS = {"complete", "partial", "stub"}
GAP_KINDS = {"re-static", "re-gameplay", "unrecoverable"}
DIRECTIONS = {"none", "read", "write", "round-trip"}

TOKEN_RE = re.compile(r"^[A-Z0-9]+$")
EXT_RE = re.compile(r"^\.[A-Z0-9]+$")
KEY_RE = re.compile(r"^([A-Za-z_][A-Za-z0-9_-]*):(.*)$")
LINK_RE = re.compile(r"\[[^\]]*\]\(([^)\s]+)\)")
# File/directory links into this repository (docs/ uses these instead of
# relative links that would escape the published site's docs tree).
REPO_FILE_URL_RE = re.compile(
    r"^https://github\.com/jomkz/fighters-codex/(?:blob|tree)/([^/#?]+)/([^#?]*)"
)
STRCMP_RE = re.compile(r'strcmp\(cmd,\s*"(\w+)"\)\s*==\s*0')
RETURN_CMD_RE = re.compile(r"return\s+(cmd_\w+)\(")


# ---------------------------------------------------------------------------
# YAML subset parser
#
# Grammar (2-space indent, <=2 nesting levels, no anchors/flow-maps/multiline):
#   scalar        bare token | "quoted string" | integer | true | false
#   inline list   [a, b, c] of scalars (no commas inside quotes)
#   block map     key: <scalar|inline list>  |  key: <nested block>
#   block list    - key: value ...  (flat maps)  |  - scalar
# ---------------------------------------------------------------------------

class YamlError(ValueError):
    pass


def _parse_scalar(text):
    text = text.strip()
    if text.startswith('"'):
        if not text.endswith('"') or len(text) < 2:
            raise YamlError("unterminated quoted string: %s" % text)
        inner = text[1:-1]
        if '"' in inner:
            raise YamlError("embedded quote unsupported: %s" % text)
        return inner
    if text in ("true", "false"):
        return text == "true"
    if re.match(r"^-?\d+$", text):
        return int(text)
    if text.startswith("[") or "#" in text:
        raise YamlError("bad scalar: %s" % text)
    return text


def _parse_value(text):
    text = text.strip()
    if text.startswith("["):
        if not text.endswith("]"):
            raise YamlError("unterminated inline list: %s" % text)
        inner = text[1:-1].strip()
        if not inner:
            return []
        return [_parse_scalar(part) for part in inner.split(",")]
    return _parse_scalar(text)


def _indent_of(line):
    n = len(line) - len(line.lstrip(" "))
    if line[n:n + 1] == "\t" or "\t" in line[:n]:
        raise YamlError("tab indentation")
    return n


def parse_yaml_subset(text):
    """Parse the front-matter subset into nested dicts/lists/scalars."""
    lines = [
        ln for ln in text.splitlines()
        if ln.strip() and not ln.strip().startswith("#")
    ]

    def parse_map(i, indent, depth):
        out = {}
        while i < len(lines):
            ind = _indent_of(lines[i])
            if ind < indent:
                break
            if ind > indent:
                raise YamlError("unexpected indent: %s" % lines[i].strip())
            content = lines[i][indent:]
            m = KEY_RE.match(content)
            if not m:
                raise YamlError("expected 'key:': %s" % content)
            key, rest = m.group(1), m.group(2)
            if key in out:
                raise YamlError("duplicate key: %s" % key)
            if rest.strip():
                out[key] = _parse_value(rest)
                i += 1
                continue
            i += 1
            if depth >= 2:
                raise YamlError("nesting too deep at key: %s" % key)
            if i >= len(lines) or _indent_of(lines[i]) != indent + 2:
                raise YamlError("empty value for key: %s" % key)
            if lines[i][indent + 2:].startswith("- "):
                out[key], i = parse_list(i, indent + 2)
            else:
                out[key], i = parse_map(i, indent + 2, depth + 1)
        return out, i

    def parse_list(i, indent):
        items = []
        while i < len(lines):
            ind = _indent_of(lines[i])
            if ind < indent:
                break
            content = lines[i][indent:]
            if ind == indent and content.startswith("- "):
                first = content[2:]
                if first.lstrip().startswith('"') or not KEY_RE.match(first):
                    items.append(_parse_scalar(first))
                    i += 1
                    continue
                item, i = parse_flat_map_item(i, indent)
                items.append(item)
            else:
                raise YamlError("expected '- ' list item: %s" % content)
        return items, i

    def parse_flat_map_item(i, indent):
        item = {}
        first = lines[i][indent + 2:]
        m = KEY_RE.match(first)
        if not m or not m.group(2).strip():
            raise YamlError("list item must be 'key: value': %s" % first)
        item[m.group(1)] = _parse_value(m.group(2))
        i += 1
        while i < len(lines) and _indent_of(lines[i]) == indent + 2:
            content = lines[i][indent + 2:]
            m = KEY_RE.match(content)
            if not m or not m.group(2).strip():
                raise YamlError("list item field must be 'key: value': %s" % content)
            if m.group(1) in item:
                raise YamlError("duplicate key in list item: %s" % m.group(1))
            item[m.group(1)] = _parse_value(m.group(2))
            i += 1
        return item, i

    obj, i = parse_map(0, 0, 0)
    if i != len(lines):
        raise YamlError("trailing content: %s" % lines[i].strip())
    return obj


def split_front_matter(text):
    """Return (front_matter_text, body_text) or raise YamlError."""
    if not text.startswith("---\n"):
        raise YamlError("missing front-matter opening '---'")
    end = text.find("\n---\n", 4)
    if end < 0:
        raise YamlError("missing front-matter closing '---'")
    return text[4:end + 1], text[end + 5:]


# ---------------------------------------------------------------------------
# Schema validation
# ---------------------------------------------------------------------------

TOP_REQUIRED = ["format", "name", "extensions", "category", "endianness",
                "spec", "codec", "related"]
TOP_OPTIONAL = ["variants", "family", "credits"]
CODEC_KEYS = {"direction", "byte_identical", "rationale", "issue", "lib",
              "commands", "tests", "fuzz", "fixtures", "gui"}


def _want(errs, cond, msg):
    if not cond:
        errs.append(msg)
    return cond


def _str_list(errs, obj, name):
    if not isinstance(obj, list) or any(not isinstance(x, str) or not x for x in obj):
        errs.append("%s: must be a list of non-empty strings" % name)
        return []
    return obj


def validate_schema(fm, stem):
    """Validate parsed front-matter against the strict schema."""
    errs = []
    if not isinstance(fm, dict):
        return ["front-matter is not a map"]

    for key in fm:
        if key not in TOP_REQUIRED and key not in TOP_OPTIONAL:
            errs.append("unknown key: %s" % key)
    for key in TOP_REQUIRED:
        if key not in fm:
            errs.append("missing required key: %s" % key)
    if errs:
        return errs

    fmt = fm["format"]
    if not (isinstance(fmt, str) and TOKEN_RE.match(fmt)):
        errs.append("format: must be an uppercase token")
    elif fmt != stem:
        errs.append("format: %r does not match filename stem %r" % (fmt, stem))

    _want(errs, isinstance(fm["name"], str) and fm["name"], "name: must be a non-empty string")

    exts = _str_list(errs, fm["extensions"], "extensions")
    for ext in exts:
        _want(errs, EXT_RE.match(ext) is not None,
              "extensions: %r must look like '.EXT'" % ext)
    if not exts and fm.get("family") != fm.get("format"):
        errs.append("extensions: may be empty only for a family-overview doc "
                    "(family == format)")

    if "variants" in fm:
        _str_list(errs, fm["variants"], "variants")
    _want(errs, fm["category"] in CATEGORIES,
          "category: %r not in %s" % (fm["category"], sorted(CATEGORIES)))
    if "family" in fm:
        _want(errs, isinstance(fm["family"], str) and TOKEN_RE.match(fm["family"] or ""),
              "family: must be an uppercase token")
    _want(errs, fm["endianness"] in ENDIANNESS,
          "endianness: %r not in %s" % (fm["endianness"], sorted(ENDIANNESS)))

    # --- spec ---
    spec = fm["spec"]
    if not isinstance(spec, dict):
        errs.append("spec: must be a map")
    else:
        for key in spec:
            _want(errs, key in ("status", "gaps"), "spec: unknown key %s" % key)
        status = spec.get("status")
        _want(errs, status in SPEC_STATUS,
              "spec.status: %r not in %s" % (status, sorted(SPEC_STATUS)))
        gaps = spec.get("gaps")
        if status == "complete":
            _want(errs, gaps is None, "spec.gaps: forbidden when status is complete")
        elif status in SPEC_STATUS:
            _want(errs, isinstance(gaps, list) and gaps,
                  "spec.gaps: required (non-empty) when status is %s" % status)
        for n, gap in enumerate(gaps or []):
            where = "spec.gaps[%d]" % n
            if not isinstance(gap, dict):
                errs.append("%s: must be a map" % where)
                continue
            for key in gap:
                _want(errs, key in ("kind", "issue", "note"),
                      "%s: unknown key %s" % (where, key))
            kind = gap.get("kind")
            _want(errs, kind in GAP_KINDS,
                  "%s.kind: %r not in %s" % (where, kind, sorted(GAP_KINDS)))
            _want(errs, isinstance(gap.get("note"), str) and gap.get("note"),
                  "%s.note: required non-empty string" % where)
            if kind == "unrecoverable":
                _want(errs, "issue" not in gap, "%s.issue: forbidden for unrecoverable" % where)
            elif kind in GAP_KINDS:
                _want(errs, isinstance(gap.get("issue"), int),
                      "%s.issue: required integer for %s" % (where, kind))

    # --- codec ---
    codec = fm["codec"]
    if not isinstance(codec, dict):
        errs.append("codec: must be a map")
        return errs
    for key in codec:
        _want(errs, key in CODEC_KEYS, "codec: unknown key %s" % key)
    direction = codec.get("direction")
    _want(errs, direction in DIRECTIONS,
          "codec.direction: %r not in %s" % (direction, sorted(DIRECTIONS)))

    if direction == "round-trip":
        _want(errs, isinstance(codec.get("byte_identical"), bool),
              "codec.byte_identical: required bool for round-trip")
    else:
        _want(errs, "byte_identical" not in codec,
              "codec.byte_identical: only valid for direction round-trip")

    solved = direction == "round-trip" and codec.get("byte_identical") is True
    has_rationale = isinstance(codec.get("rationale"), str) and codec.get("rationale")
    has_issue = isinstance(codec.get("issue"), int)
    if solved:
        _want(errs, "rationale" not in codec and "issue" not in codec,
              "codec: rationale/issue forbidden once byte-identical round-trip")
    else:
        _want(errs, bool(has_rationale) != bool(has_issue),
              "codec: exactly one of rationale (terminal) or issue (planned) "
              "required while not byte-identical round-trip")

    lib = _str_list(errs, codec.get("lib", []), "codec.lib")
    commands = _str_list(errs, codec.get("commands", []), "codec.commands")
    tests = _str_list(errs, codec.get("tests", []), "codec.tests")
    _str_list(errs, codec.get("fuzz", []), "codec.fuzz")
    _str_list(errs, codec.get("gui", []), "codec.gui")

    if direction == "none":
        _want(errs, not lib and not commands,
              "codec: lib/commands must be empty when direction is none")
    elif direction in DIRECTIONS:
        _want(errs, lib or commands,
              "codec: lib or commands required when direction is %s" % direction)
    if codec.get("byte_identical") is True:
        _want(errs, bool(tests),
              "codec.tests: required when byte_identical is true (the claim "
              "must name the test that proves it)")

    fixtures = codec.get("fixtures")
    if not isinstance(fixtures, dict):
        errs.append("codec.fixtures: required map")
    else:
        for key in fixtures:
            _want(errs, key in ("synthetic", "real_manifest"),
                  "codec.fixtures: unknown key %s" % key)
        for key in ("synthetic", "real_manifest"):
            _want(errs, isinstance(fixtures.get(key), bool),
                  "codec.fixtures.%s: required bool" % key)
        if fixtures.get("synthetic") is True:
            _want(errs, bool(tests), "codec.tests: required when fixtures.synthetic")

    related = _str_list(errs, fm["related"], "related")
    for token in related:
        _want(errs, TOKEN_RE.match(token) is not None,
              "related: %r must be an uppercase token" % token)
    if "credits" in fm:
        _str_list(errs, fm["credits"], "credits")
    return errs


# ---------------------------------------------------------------------------
# Body (template skeleton) validation
# ---------------------------------------------------------------------------

def body_headings(body):
    """H1/H2 headings outside fenced code blocks, as (level, text) pairs."""
    out = []
    fence = False
    for line in body.splitlines():
        if line.lstrip().startswith("```"):
            fence = not fence
            continue
        if fence:
            continue
        if line.startswith("## "):
            out.append((2, line[3:].strip()))
        elif line.startswith("# "):
            out.append((1, line[2:].strip()))
    return out


def expected_h1(fm):
    title = "# %s — %s" % (fm["format"], fm["name"])
    if fm["extensions"]:
        title += " (%s)" % " / ".join(fm["extensions"])
    return title


def validate_body(fm, body):
    errs = []
    headings = body_headings(body)
    h1s = [text for lvl, text in headings if lvl == 1]
    h2s = [text for lvl, text in headings if lvl == 2]

    want_h1 = expected_h1(fm)[2:]
    if len(h1s) != 1:
        errs.append("exactly one H1 required (found %d)" % len(h1s))
    elif h1s[0] != want_h1:
        errs.append("H1 mismatch: want %r, have %r" % (want_h1, h1s[0]))

    unknown = [h for h in h2s if h not in CANONICAL_H2]
    for h in unknown:
        errs.append("non-canonical H2: %r (allowed: %s)" % (h, ", ".join(CANONICAL_H2)))
    for h in set(h2s):
        if h2s.count(h) > 1:
            errs.append("duplicate H2: %r" % h)
    known = [h for h in h2s if h in CANONICAL_H2]
    order = [CANONICAL_H2.index(h) for h in known]
    if order != sorted(order):
        errs.append("H2 order must follow the canonical order: %s" % ", ".join(CANONICAL_H2))

    for required in ("File Layout", "Related"):
        if required not in h2s:
            errs.append("missing required section: ## %s" % required)

    gaps = (fm.get("spec") or {}).get("gaps") or []
    if gaps and "Open Questions" not in h2s:
        errs.append("spec.gaps present but no ## Open Questions section")
    if not gaps and "Open Questions" in h2s:
        errs.append("## Open Questions present but spec.gaps is empty")

    commands = (fm.get("codec") or {}).get("commands") or []
    if commands and "Tools" not in h2s:
        errs.append("codec.commands present but no ## Tools section")

    # Intro prose: non-empty content between H1 and the first H2.
    m = re.search(r"^# .*$", body, re.M)
    first_h2 = re.search(r"^## ", body, re.M)
    if m:
        intro = body[m.end():first_h2.start() if first_h2 else len(body)]
        if not intro.strip():
            errs.append("missing intro prose between the H1 and the first section")

    for token in fm.get("related") or []:
        if "](%s.md" % token not in body:
            errs.append("related token %s is not linked in the body" % token)

    if "Open Questions" in h2s:
        section = body.split("## Open Questions", 1)[1]
        nxt = re.search(r"^## ", section, re.M)
        section = section[:nxt.start()] if nxt else section
        for gap in gaps:
            issue = gap.get("issue")
            if isinstance(issue, int) and ("#%d" % issue) not in section:
                errs.append("gap issue #%d not referenced under ## Open Questions" % issue)
    return errs


# ---------------------------------------------------------------------------
# Encoding + link checks (all docs)
# ---------------------------------------------------------------------------

def _misdecoded_byte(ch):
    """Byte this char would have come from under a cp1252 misdecode."""
    try:
        b = ch.encode("cp1252")
        return b[0] if len(b) == 1 else None
    except UnicodeError:
        cp = ord(ch)
        return cp if 0x80 <= cp <= 0x9F else None


def find_mojibake(text):
    """Runs of chars that inverse-map (cp1252) to valid multi-byte UTF-8."""
    hits = []
    i, n = 0, len(text)
    while i < n:
        b = _misdecoded_byte(text[i])
        if b is None or b < 0x80:
            i += 1
            continue
        j = i
        run = bytearray()
        while j < n:
            b2 = _misdecoded_byte(text[j])
            if b2 is None or b2 < 0x80:
                break
            run.append(b2)
            j += 1
        if j - i > 1:
            try:
                run.decode("utf-8")
                hits.append(text[i:j])
            except UnicodeDecodeError:
                pass
        i = j
    return hits


def exists_case_exact(path):
    """True if path exists with exact-case components (Windows/macOS-safe)."""
    path = Path(path)
    if not path.exists():
        return False
    probe = Path(path.anchor) if path.is_absolute() else Path(".")
    for part in path.parts if not path.is_absolute() else path.parts[1:]:
        if part == "..":
            probe = probe.parent
            continue
        if part == ".":
            continue
        try:
            entries = os.listdir(probe)
        except OSError:
            return False
        if part not in entries:
            return False
        probe = probe / part
    return True


def _iter_link_targets(text):
    """Raw link targets outside code fences and inline code spans."""
    fence = False
    for line in text.splitlines():
        if line.lstrip().startswith("```"):
            fence = not fence
            continue
        if fence:
            continue
        line = re.sub(r"`[^`]*`", "", line)
        for m in LINK_RE.finditer(line):
            yield m.group(1)


def iter_links(text):
    """Relative link targets outside code fences and inline code spans."""
    for target in _iter_link_targets(text):
        if re.match(r"^[a-z][a-z0-9+.-]*:", target):  # http:, mailto:, ...
            continue
        target = target.split("#", 1)[0]
        if target:
            yield target


def iter_repo_file_urls(text):
    """(ref, repo-relative path) per blob/tree URL into this repository."""
    for target in _iter_link_targets(text):
        m = REPO_FILE_URL_RE.match(target)
        if m:
            path = m.group(2).split("#", 1)[0].split("?", 1)[0]
            yield m.group(1), path.rstrip("/")


def docs_files():
    files = sorted(ROOT.glob("*.md"))
    files += sorted(p for p in (ROOT / "docs").rglob("*.md"))
    files += sorted(DB_DIR.glob("*.md"))
    return files


def check_docs_hygiene():
    errs = []
    docs_dir = (ROOT / "docs").resolve()
    for path in docs_files():
        rel = _rel(path)
        raw = path.read_bytes()
        if raw.startswith(b"\xef\xbb\xbf"):
            errs.append("%s: UTF-8 BOM (strip it)" % rel)
            raw = raw[3:]
        try:
            text = raw.decode("utf-8")
        except UnicodeDecodeError as exc:
            errs.append("%s: not valid UTF-8 (%s)" % (rel, exc))
            continue
        for hit in find_mojibake(text):
            errs.append("%s: mojibake %r (cp1252 double-encoding)" % (rel, hit))
        in_docs = rel.split("/")[0] == "docs"
        for target in iter_links(text):
            resolved = (path.parent / target).resolve()
            if not exists_case_exact(resolved):
                errs.append("%s: broken or wrong-case link: %s" % (rel, target))
            elif in_docs and not _under(resolved, docs_dir):
                # Escaping links 404 on the published docs site; point at
                # https://github.com/jomkz/fighters-codex/blob/main/... instead.
                errs.append("%s: link escapes docs/ (breaks the docs site; "
                            "use a repo blob/tree URL): %s" % (rel, target))
        for ref, target in iter_repo_file_urls(text):
            if ref != "main":
                errs.append("%s: repo file URL pins ref %r (link main): %s"
                            % (rel, ref, target))
            elif not target or not exists_case_exact(ROOT / target):
                errs.append("%s: repo file URL points at a missing or "
                            "wrong-case path: %s" % (rel, target))
    return errs


def _under(path, ancestor):
    """True if resolved path lies under ancestor (Python 3.8-safe)."""
    try:
        path.relative_to(ancestor)
        return True
    except ValueError:
        return False


# ---------------------------------------------------------------------------
# Spec loading + repo-reality checks
# ---------------------------------------------------------------------------

def spec_paths():
    return sorted(p for p in FORMATS_DIR.glob("*.md") if p.name not in NON_SPEC)


def load_specs():
    """Return ({token: front-matter}, errors) for every spec."""
    specs = {}
    errs = []
    for path in spec_paths():
        stem = path.stem
        rel = _rel(path)
        text = path.read_bytes().decode("utf-8", errors="replace")
        if not text.startswith("---\n"):
            errs.append("%s: missing front-matter (see docs/spec-authoring.md)" % rel)
            continue
        try:
            fm_text, body = split_front_matter(text)
            fm = parse_yaml_subset(fm_text)
        except YamlError as exc:
            errs.append("%s: front-matter: %s" % (rel, exc))
            continue
        schema_errs = validate_schema(fm, stem)
        errs += ["%s: %s" % (rel, e) for e in schema_errs]
        if schema_errs:
            continue  # malformed front-matter: skip body/claim checks
        errs += ["%s: %s" % (rel, e) for e in validate_body(fm, body)]
        specs[stem] = fm
    return specs, errs


def cli_dispatch_map(main_cpp_text):
    """token -> cmd function, from the strcmp dispatch chain in cli/main.cpp."""
    mapping = {}
    pending = []
    events = []
    for m in STRCMP_RE.finditer(main_cpp_text):
        events.append((m.start(), "token", m.group(1)))
    for m in RETURN_CMD_RE.finditer(main_cpp_text):
        events.append((m.start(), "return", m.group(1)))
    for _, kind, value in sorted(events):
        if kind == "token":
            pending.append(value)
        else:
            for token in pending:
                mapping[token] = value
            pending = []
    return mapping


def check_claims(specs):
    """Stage 2: front-matter claims vs. repository reality."""
    errs = []
    warns = []
    main_text = CLI_MAIN.read_text(encoding="utf-8")
    cli_doc = CLI_DOC.read_text(encoding="utf-8")
    dispatch = cli_dispatch_map(main_text)
    manifest_exts = set()
    if MANIFEST.exists():
        for line in MANIFEST.read_text(encoding="utf-8").splitlines():
            parts = line.split()
            if len(parts) >= 3:
                for comp in parts[2].split("/"):
                    dot = comp.rfind(".")
                    if dot > 0:
                        manifest_exts.add(comp[dot:].upper())

    claimed = {"lib": set(), "tests": set(), "fuzz": set(), "gui": set()}
    claimed_tokens = set()
    all_stems = {p.stem for p in spec_paths()}

    for token, fm in sorted(specs.items()):
        where = "docs/fa/formats/%s.md" % token
        codec = fm["codec"]
        for field in ("lib", "tests", "fuzz", "gui"):
            for rel in codec.get(field) or []:
                claimed[field].add(rel)
                if not exists_case_exact(ROOT / rel):
                    errs.append("%s: codec.%s path does not exist: %s" % (where, field, rel))
        for cmd in codec.get("commands") or []:
            claimed_tokens.add(cmd)
            if cmd not in dispatch:
                errs.append("%s: command %r is not a dispatch literal in cli/main.cpp"
                            % (where, cmd))
            if ("fx %s" % cmd) not in cli_doc:
                errs.append("%s: command %r is not documented in docs/cli.md" % (where, cmd))
        if "family" in fm and fm["family"] not in all_stems:
            errs.append("%s: family %r has no spec file" % (where, fm["family"]))
        for rel_token in fm.get("related") or []:
            if rel_token not in all_stems:
                errs.append("%s: related token %r has no spec file" % (where, rel_token))
        want_real = any(ext.upper() in manifest_exts for ext in fm["extensions"])
        have_real = codec["fixtures"]["real_manifest"]
        if manifest_exts and want_real != have_real:
            errs.append("%s: fixtures.real_manifest is %s but the extract manifest "
                        "says %s" % (where, have_real, want_real))

    # Reverse coverage: every codec/test/fuzz/GUI file must be claimed by a spec.
    sink = errs
    def unclaimed(kind, paths):
        for p in sorted(paths):
            # as_posix(): front-matter claims always use forward slashes, and
            # relative_to() yields backslashes on Windows (caught by the msvc
            # ctest leg the first time reverse coverage became a hard error).
            rel = p.relative_to(ROOT).as_posix()
            if rel not in claimed[kind]:
                sink.append("unclaimed %s (no spec's codec.%s lists it): %s"
                            % (kind, kind, rel))

    unclaimed("lib", (ROOT / "lib" / "src").glob("*.cpp"))
    unclaimed("tests", (ROOT / "tests").glob("test_*.cpp"))
    unclaimed("fuzz", (ROOT / "fuzz").glob("fuzz_*.cpp"))
    unclaimed("gui", (ROOT / "gui" / "src" / "editors").glob("*.cpp"))
    for path in sorted((ROOT / "cli").glob("cmd_*.cpp")):
        fn = path.stem
        tokens = [t for t, f in dispatch.items() if f == fn]
        if not any(t in claimed_tokens for t in tokens):
            sink.append("unclaimed CLI command (no spec's codec.commands reaches it): "
                        "cli/%s" % path.name)
    return errs, warns


# ---------------------------------------------------------------------------
# Status matrix
# ---------------------------------------------------------------------------

def _matrix_row(token, fm):
    if fm is None:
        return "| [%s](%s.md) | — | *not yet converted* | | | | | | | |" % (token, token)
    spec = fm["spec"]
    codec = fm["codec"]
    gaps = spec.get("gaps") or []
    gap_cell = "<br>".join(
        "%s%s" % (g["kind"], " [#%d](https://github.com/jomkz/fighters-codex/issues/%d)"
                  % (g["issue"], g["issue"]) if "issue" in g else "")
        for g in gaps) or "—"
    direction = codec["direction"]
    if direction == "round-trip":
        codec_cell = "round-trip (byte-identical)" if codec.get("byte_identical") \
            else "round-trip"
    elif direction == "none":
        codec_cell = "none"
    else:
        codec_cell = direction + ("-only" if direction in ("read", "write") else "")
    if not (direction == "round-trip" and codec.get("byte_identical")):
        if isinstance(codec.get("issue"), int):
            codec_cell += " [#%d](https://github.com/jomkz/fighters-codex/issues/%d)" \
                % (codec["issue"], codec["issue"])
        elif codec.get("rationale"):
            codec_cell += " (by design)"
    fixtures = codec["fixtures"]
    fixture_cell = "/".join(
        name for name, on in (("syn", fixtures["synthetic"]),
                              ("real", fixtures["real_manifest"])) if on) or "—"

    def files_cell(field):
        vals = codec.get(field) or []
        return "<br>".join("`%s`" % v for v in vals) or "—"

    cmd_cell = "<br>".join("`fx %s`" % c for c in codec.get("commands") or []) or "—"
    return "| [%s](%s.md) | %s | %s | %s | %s | %s | %s | %s | %s | %s |" % (
        token, token, fm["category"], spec["status"], gap_cell, codec_cell,
        cmd_cell, files_cell("tests"), fixture_cell, files_cell("fuzz"),
        files_cell("gui"))


def generate_matrix(specs):
    lines = [
        "# Format Status Matrix",
        "",
        "<!-- Generated by tools/check_status.py --write-matrix. Do not edit. -->",
        "",
        "One row per format spec in this directory, generated from each spec's",
        "front-matter and verified against the repository by CI (`--check`).",
        "See [docs/spec-authoring.md](../../spec-authoring.md) for the vocabulary.",
        "",
        "| Format | Category | Spec | Gaps | Codec | Commands | Tests | Fixtures | Fuzz | GUI |",
        "|---|---|---|---|---|---|---|---|---|---|",
    ]
    for path in spec_paths():
        lines.append(_matrix_row(path.stem, specs.get(path.stem)))
    return "\n".join(lines) + "\n"


# ---------------------------------------------------------------------------
# README index sync
# ---------------------------------------------------------------------------

def check_readme_index(specs):
    errs = []
    readme = FORMATS_DIR / "README.md"
    text = readme.read_text(encoding="utf-8")
    rel = readme.relative_to(ROOT)

    section = None
    seen = {}
    fence = False
    for line in text.splitlines():
        if line.lstrip().startswith("```"):
            fence = not fence
            continue
        if fence:
            continue
        if line.startswith("## "):
            section = line[3:].strip()
        for m in re.finditer(r"\]\(([A-Z0-9]+)\.md\)", line):
            seen.setdefault(m.group(1), []).append(section)

    for path in spec_paths():
        token = path.stem
        sections = seen.get(token, [])
        if len(sections) != 1:
            errs.append("%s: %s.md must be linked exactly once (found %d)"
                        % (rel, token, len(sections)))
            continue
        fm = specs.get(token)
        if fm is not None:
            want = CATEGORIES[fm["category"]]
            if sections[0] != want:
                errs.append("%s: %s.md is under %r but its category maps to %r"
                            % (rel, token, sections[0], want))
    for token in seen:
        if not (FORMATS_DIR / ("%s.md" % token)).exists():
            errs.append("%s: links to nonexistent spec %s.md" % (rel, token))
    return errs


# ---------------------------------------------------------------------------
# Reconstruction program (epic #209): db/ symbol database
# ---------------------------------------------------------------------------

VA8_RE = re.compile(r"^0x[0-9A-F]{8}$")
RANGE_RE = re.compile(r"^0x[0-9A-Fa-f]{4,8}-0x[0-9A-Fa-f]{4,8}$")
ISSUE_URL = "https://github.com/jomkz/fighters-codex/issues"


def _rel(path):
    """Repo-relative posix path, or the bare name for out-of-tree self-test fixtures."""
    try:
        return path.relative_to(ROOT).as_posix()
    except ValueError:
        return path.name


def _read_csv(path):
    with path.open(encoding="utf-8", newline="") as f:
        rows = list(csv.reader(f))
    if not rows:
        return [], []
    return rows[0], rows[1:]


def load_manifest(path):
    """Parse subsystems.csv -> (list of subsystem dicts, errors)."""
    errs = []
    rel = _rel(path)
    if not path.exists():
        return [], ["%s: missing" % rel]
    header, rows = _read_csv(path)
    want = ["slug", "title", "binary", "ranges", "status", "doc", "issue"]
    if header != want:
        return [], ["%s: header must be %s" % (rel, ",".join(want))]
    subs = []
    seen = set()
    for i, r in enumerate(rows, start=2):
        if len(r) != len(want):
            errs.append("%s:%d: expected %d columns, got %d" % (rel, i, len(want), len(r)))
            continue
        slug, title, binary, ranges, status, doc, issue = r
        if not re.match(r"^[a-z0-9-]+$", slug):
            errs.append("%s:%d: bad slug %r" % (rel, i, slug))
        if slug in seen:
            errs.append("%s:%d: duplicate slug %r" % (rel, i, slug))
        seen.add(slug)
        if not binary:
            errs.append("%s:%d: empty binary" % (rel, i))
        parsed = []
        for chunk in ranges.split(";"):
            if not RANGE_RE.match(chunk):
                errs.append("%s:%d: bad range %r" % (rel, i, chunk))
                continue
            lo, hi = (int(x, 16) for x in chunk.split("-"))
            if lo >= hi:
                errs.append("%s:%d: range %r is not ascending" % (rel, i, chunk))
            parsed.append((lo, hi))
        if status not in RECON_STATUS:
            errs.append("%s:%d: status %r not in %s" % (rel, i, status, sorted(RECON_STATUS)))
        if not issue.lstrip("-").isdigit():
            errs.append("%s:%d: issue %r is not an integer" % (rel, i, issue))
            issue_n = 0
        else:
            issue_n = int(issue)
        if status != "planned":
            if not doc or not (ROOT / doc).exists():
                errs.append("%s:%d: doc %r must exist when status=%s" % (rel, i, doc, status))
            if issue_n <= 0:
                errs.append("%s:%d: %s subsystem must reference a real issue (got %s)"
                            % (rel, i, status, issue))
        subs.append({"slug": slug, "title": title, "binary": binary, "ranges": parsed,
                     "status": status, "doc": doc, "issue": issue_n, "line": i})
    return subs, errs


def load_symbols(symbols_dir, manifest):
    """Parse db/symbols/*.csv -> (dict slug->rows, errors). Enforces schema,
    ascending+unique VAs within a file, per-binary VA uniqueness, and manifest match.

    VAs are unique only within a binary: two images (e.g. FA.EXE and IP.EXE both
    based at 0x00400000) legitimately name different symbols at the same VA, so the
    uniqueness key is (binary, va), not va alone."""
    errs = []
    by_slug = {}
    slugs = {s["slug"] for s in manifest}
    slug_to_binary = {s["slug"]: s.get("binary") for s in manifest}
    global_va = {}   # (binary, va) -> "rel"
    global_name = {} # (binary, name) -> "rel:line"
    want = ["va", "kind", "name", "display", "source", "confidence", "notes", "type"]
    files = sorted(symbols_dir.glob("*.csv")) if symbols_dir.exists() else []
    for path in files:
        rel = _rel(path)
        slug = path.stem
        if slug not in slugs:
            errs.append("%s: no matching row in subsystems.csv" % rel)
        binary = slug_to_binary.get(slug)
        header, rows = _read_csv(path)
        if header != want:
            errs.append("%s: header must be %s" % (rel, ",".join(want)))
            continue
        parsed = []
        last = -1
        for i, r in enumerate(rows, start=2):
            if len(r) != len(want):
                errs.append("%s:%d: expected %d columns, got %d" % (rel, i, len(want), len(r)))
                continue
            va, kind, name, display, source, confidence, notes, ctype = r
            if not VA8_RE.match(va):
                errs.append("%s:%d: bad va %r (want 0x00XXXXXX)" % (rel, i, va))
                continue
            n = int(va, 16)
            if n <= last:
                errs.append("%s:%d: va %s not ascending" % (rel, i, va))
            last = n
            if (binary, n) in global_va:
                errs.append("%s:%d: va %s already defined in %s" % (rel, i, va, global_va[(binary, n)]))
            global_va[(binary, n)] = rel
            if kind not in SYMBOL_KIND:
                errs.append("%s:%d: kind %r invalid" % (rel, i, kind))
            if source not in SYMBOL_SOURCE:
                errs.append("%s:%d: source %r invalid" % (rel, i, source))
            if confidence not in SYMBOL_CONFIDENCE:
                errs.append("%s:%d: confidence %r invalid" % (rel, i, confidence))
            if source == "waiver":
                if not notes.strip():
                    errs.append("%s:%d: waiver requires a notes rationale" % (rel, i))
                if ctype.strip():
                    errs.append("%s:%d: waiver row must not carry a type" % (rel, i))
            else:
                if not name.strip():
                    errs.append("%s:%d: non-waiver row needs a name" % (rel, i))
                if (binary, name) in global_name:
                    errs.append("note-dup: %s:%d name %r also at %s"
                                % (rel, i, name, global_name[(binary, name)]))
                global_name[(binary, name)] = "%s:%d" % (rel, i)
            parsed.append({"va": n, "kind": kind, "name": name, "display": display,
                           "source": source, "confidence": confidence, "notes": notes,
                           "type": ctype})
        by_slug[slug] = parsed
    return by_slug, errs


def load_inventory(inv_dir):
    """Parse one binary's db/inventory/<binary>/*.csv -> (dict, errors)."""
    errs = []
    inv = {"functions": {}, "globals": [], "ranges": []}
    fpath = inv_dir / "functions.csv"
    if fpath.exists():
        _, rows = _read_csv(fpath)
        for r in rows:
            if len(r) >= 3 and VA8_RE.match(r[0]):
                inv["functions"][int(r[0], 16)] = {"name": r[1], "size": r[2]}
    else:
        errs.append("%s: missing (run export_inventory.sh)" % _rel(fpath))
    gpath = inv_dir / "globals.csv"
    if gpath.exists():
        _, rows = _read_csv(gpath)
        for r in rows:
            if len(r) >= 4 and VA8_RE.match(r[0]):
                inv["globals"].append({"va": int(r[0], 16), "name": r[1],
                                       "xref": r[2], "subs": r[3].split(";") if r[3] else []})
    else:
        errs.append("%s: missing (run export_inventory.sh)" % _rel(gpath))
    rpath = inv_dir / "ranges.csv"
    if rpath.exists():
        _, rows = _read_csv(rpath)
        for r in rows:
            if len(r) >= 5:
                inv["ranges"].append(r)
    return inv, errs


def load_inventories(inv_root, binaries):
    """Load one inventory per binary from db/inventory/<binary>/ -> ({binary: inv}, errors).

    Inventories are keyed by the exact `binary` value from subsystems.csv (the Ghidra
    program name / imported filename, e.g. FA.EXE, WAIL32.DLL)."""
    errs = []
    invs = {}
    for binary in binaries:
        inv, ierrs = load_inventory(inv_root / binary)
        invs[binary] = inv
        errs += ierrs
    return invs, errs


def _claims_by_binary(symbols, slug_to_binary):
    """{binary: {va: slug}} for every symbol row (explicit membership, overrides ranges).
    Scoped per binary because VAs are only unique within a binary."""
    out = {}
    for slug, rows in symbols.items():
        binary = slug_to_binary.get(slug)
        for row in rows:
            out.setdefault(binary, {})[row["va"]] = slug
    return out


def _in_ranges(va, sub):
    return any(lo <= va < hi for lo, hi in sub["ranges"])


def check_coverage(sub, symbols, inventory, claims, binary_slugs):
    """For a complete subsystem: 100% of in-scope functions named, and every
    referenced global named or waived. `inventory`/`claims` are the subsystem's
    binary's; `binary_slugs` scopes the shared-global rule to that binary."""
    errs = []
    slug = sub["slug"]
    rel = ("db/symbols/%s.csv" % slug)
    rows = symbols.get(slug, [])
    by_va = {r["va"]: r for r in rows}

    # Functions: every claimed inventory function is covered and named.
    for va, info in inventory["functions"].items():
        owner = claims.get(va)
        belongs = owner == slug or (owner is None and _in_ranges(va, sub))
        if not belongs:
            continue
        row = by_va.get(va)
        if row is None:
            errs.append("%s: function 0x%08X (%s) is in-scope but absent from the DB"
                        % (rel, va, info["name"]))
        elif row["source"] != "waiver":
            if info["name"].startswith("FUN_"):
                errs.append("%s: 0x%08X still unnamed in the project (FUN_)" % (rel, va))
            elif info["name"] != row["name"]:
                errs.append("%s: 0x%08X name drift — project %r vs DB %r"
                            % (rel, va, info["name"], row["name"]))
    # Every func-kind DB row must correspond to a real function.
    for row in rows:
        if row["kind"] == "func" and row["source"] != "waiver" \
                and row["va"] not in inventory["functions"]:
            errs.append("%s: 0x%08X named as a function but not in the inventory" % (rel, row["va"]))

    # Data: every referenced global tagged with this subsystem must be named or
    # waived SOMEWHERE in the same binary's DB. Globals are frequently shared (a
    # struct/array interior read by many subsystems), and VAs are unique within a
    # binary, so a global is documented once — in whichever subsystem owns its
    # base — and that covers every subsystem of that binary which references it.
    db_data_covered = {r["va"] for s in binary_slugs for r in symbols.get(s, [])
                       if r["kind"] == "data"}
    for g in inventory["globals"]:
        if slug not in g["subs"]:
            continue
        unnamed = g["name"] == "<unnamed>" or g["name"].startswith("DAT_")
        if unnamed and g["va"] not in db_data_covered:
            errs.append("%s: referenced global 0x%08X is unnamed and unwaived "
                        "(name or waive it in some db/symbols/*.csv)" % (rel, g["va"]))
    return errs


def _section(text, heading_re):
    """Return the body lines of the first H2 whose text matches heading_re."""
    lines = text.splitlines()
    out, capturing = [], False
    for ln in lines:
        if ln.startswith("## "):
            if capturing:
                break
            capturing = bool(re.match(heading_re, ln))
            continue
        if capturing:
            out.append(ln)
    return out if capturing or out else None


def check_subsystem_doc(sub, symbols):
    """Structure checks for a subsystem doc (status in {active, complete})."""
    doc = ROOT / sub["doc"]
    if not doc.exists():
        return ["%s: subsystem doc missing" % sub["doc"]]
    return check_doc_structure(sub["doc"], doc.read_text(encoding="utf-8"),
                               doc.parent, symbols.get(sub["slug"], []))


def check_doc_structure(rel, text, doc_dir, rows):
    """Text-level structure checks (shared by the file check and the self-test)."""
    errs = []
    if not re.search(r"(?m)^>\s*\*\*Provenance:\*\*", text):
        errs.append("%s: missing '> **Provenance:**' blockquote" % rel)
    if not re.search(r"(?m)^##\s+Related\b", text):
        errs.append("%s: missing '## Related' section" % rel)

    oq = _section(text, r"^##\s+Open [Qq]uestions\b")
    if oq is None:
        errs.append("%s: missing '## Open Questions' section" % rel)
    elif not any("*Status:" in ln for ln in oq):
        errs.append("%s: Open Questions entries must end with a *Status: …* line" % rel)

    # Functions table cross-checked against the DB (kills doc/DB drift).
    fn = _section(text, r"^##\s+Functions\b")
    if fn is None:
        errs.append("%s: missing '## Functions' section" % rel)
    else:
        names = {r["va"]: {r["name"], r["display"]} - {""} for r in rows}
        for ln in fn:
            if not ln.strip().startswith("|"):
                continue
            cells = [c.strip() for c in ln.strip().strip("|").split("|")]
            if len(cells) < 2 or set(cells[0]) <= set("- :"):
                continue
            vas = re.findall(r"0x[0-9A-Fa-f]{6,8}", cells[0])
            if not vas:
                continue
            token_m = re.search(r"[`*]*([\w@?$]+)", cells[1])
            token = token_m.group(1) if token_m else ""
            for va in vas:
                n = int(va, 16)
                if n not in names:
                    errs.append("%s: Functions row 0x%08X not in the DB" % (rel, n))
                elif token and token not in names[n]:
                    errs.append("%s: Functions row 0x%08X shows %r; DB has %s"
                                % (rel, n, token, sorted(names[n])))

    imgs = re.findall(r"!\[[^\]]*\]\((diagrams/[^)]+\.svg)\)", text)
    if not imgs:
        errs.append("%s: needs at least one ![…](diagrams/*.svg) flow diagram" % rel)
    for img in imgs:
        svg = doc_dir / img
        if not svg.exists():
            errs.append("%s: diagram %s does not exist" % (rel, img))
            continue
        body = svg.read_text(encoding="utf-8")
        if "prefers-color-scheme: dark" not in body or "data-theme" not in body:
            errs.append("%s: %s must be theme-aware (prefers-color-scheme + data-theme)"
                        % (rel, img))
    return errs


def recon_stats(sub, symbols, inventory, claims):
    """Per-subsystem numbers for the matrix."""
    slug = sub["slug"]
    rows = symbols.get(slug, [])
    func_named = sum(1 for r in rows if r["kind"] == "func" and r["source"] != "waiver")
    func_waived = sum(1 for r in rows if r["kind"] == "func" and r["source"] == "waiver")
    func_total = 0
    for va in inventory["functions"]:
        owner = claims.get(va)
        if owner == slug or (owner is None and _in_ranges(va, sub)):
            func_total += 1
    g_named = sum(1 for r in rows if r["kind"] == "data" and r["source"] != "waiver")
    g_waived = sum(1 for r in rows if r["kind"] == "data" and r["source"] == "waiver")
    g_total = sum(1 for g in inventory["globals"] if slug in g["subs"])
    return {"func_named": func_named + func_waived, "func_total": func_total,
            "g_named": g_named, "g_waived": g_waived, "g_total": g_total}


def _ordered_binaries(manifest):
    """Distinct binaries in first-appearance (manifest) order — FA.EXE leads."""
    out = []
    for s in manifest:
        if s["binary"] not in out:
            out.append(s["binary"])
    return out


def _recon_row(sub, st):
    rng = "<br>".join("`0x%06X–0x%06X`" % (lo, hi) for lo, hi in sub["ranges"])
    if sub["status"] == "planned":
        funcs = globs = "—"
    else:
        pct = (100 * st["func_named"] // st["func_total"]) if st["func_total"] else 100
        funcs = "%d/%d (%d%%)" % (st["func_named"], st["func_total"], pct)
        globs = "%d named · %d waived" % (st["g_named"], st["g_waived"])
    has_sym = (SYMBOLS_DIR / ("%s.csv" % sub["slug"])).exists()
    doc_cell = "[doc](%s)" % Path(sub["doc"]).name if sub["status"] != "planned" else "—"
    diag = "✓" if sub["status"] == "complete" else ("—" if not has_sym else "·")
    issue_cell = "[#%d](%s/%d)" % (sub["issue"], ISSUE_URL, sub["issue"]) \
        if sub["issue"] > 0 else "—"
    return "| %s | %s | %s | %s | %s | %s | %s | %s |" % (
        sub["title"], rng, funcs, globs, doc_cell, diag, issue_cell, sub["status"])


def generate_recon_matrix(manifest, symbols, inventories, claims_by_binary):
    """Multi-binary matrix: one section per binary (FA.EXE + each overlay), each
    verified against its own db/inventory/<binary>/ ground truth."""
    lines = [
        "# Reconstruction Matrix",
        "",
        "<!-- Generated by tools/check_status.py --write-matrix. Do not edit. -->",
        "",
        "Progress of the FA reconstruction programs — the",
        "[game-executable program](../roadmap.md#program-game-executable-reconstruction) (epic [#209](%s/209))" % ISSUE_URL,
        "and the [overlay-binary program](../roadmap.md#program-overlay-reconstruction) (epic [#247](%s/247))" % ISSUE_URL,
        "— one section per binary, one row per subsystem, generated from the",
        "[symbol database](%s/db/README.md) and verified against the committed per-binary" % BLOB,
        "Ghidra inventory by CI (`--check`).",
        "",
    ]
    p_named = p_funcs = p_gn = p_gt = p_done = p_subs = 0
    for binary in _ordered_binaries(manifest):
        bsubs = [s for s in manifest if s["binary"] == binary]
        inv = inventories.get(binary, {"functions": {}, "globals": [], "ranges": []})
        claims = claims_by_binary.get(binary, {})
        lines += [
            "## %s" % binary,
            "",
            "| Subsystem | Range(s) | Funcs named | Ref. globals | Doc | Diagram | Issue | Status |",
            "|---|---|---|---|---|---|---|---|",
        ]
        tot_named = tot_funcs = tot_gn = tot_gt = 0
        for sub in bsubs:
            st = recon_stats(sub, symbols, inv, claims)
            tot_named += st["func_named"]; tot_funcs += st["func_total"]
            tot_gn += st["g_named"] + st["g_waived"]; tot_gt += st["g_total"]
            lines.append(_recon_row(sub, st))
        done = sum(1 for s in bsubs if s["status"] == "complete")
        lines += [
            "",
            "**%s totals:** %d/%d subsystems complete; %d/%d in-scope functions named; "
            "%d/%d referenced globals resolved."
            % (binary, done, len(bsubs), tot_named, tot_funcs, tot_gn, tot_gt),
            "",
        ]
        p_named += tot_named; p_funcs += tot_funcs; p_gn += tot_gn; p_gt += tot_gt
        p_done += done; p_subs += len(bsubs)
    lines += [
        "**Program totals (all binaries):** %d/%d subsystems complete; %d/%d in-scope "
        "functions named; %d/%d referenced globals resolved."
        % (p_done, p_subs, p_named, p_funcs, p_gn, p_gt),
        "",
    ]
    return "\n".join(lines) + "\n"


# Generated registry regions injected into the hand-written symbols.md / globals.md
# (marker-delimited, so the intro/format prose stays hand-authored while the
# per-subsystem symbol tables come from the DB and cannot drift).
REGISTRY_TAGS = {"func": "symbol-registry", "data": "globals-registry"}


def generate_registry(manifest, symbols, kind):
    """Per-subsystem table of *named* symbols (kind 'func' or 'data') for the
    generated region of symbols.md / globals.md. Waivers are the DB's bookkeeping,
    not the human registry, so only sms/re rows appear. Grouped by binary (a label
    is emitted only when more than one binary is present, so the single-binary
    output is unchanged), address-ordered within each binary."""
    complete = [s for s in manifest if s["status"] == "complete"]
    binaries = _ordered_binaries(complete)
    multi = len(binaries) > 1
    what = "functions" if kind == "func" else "referenced globals"
    out = ["<!-- Generated by tools/check_status.py --write-matrix. Do not edit. -->",
           "",
           "_Generated from [`db/symbols/`](%s/db/symbols/); each subsystem's detailed "
           "prose lives on its own page._" % BLOB, ""]
    ordered = []
    for binary in binaries:
        ordered.append((binary, sorted((s for s in complete if s["binary"] == binary),
                                       key=lambda s: s["ranges"][0][0])))
    for binary, subs in ordered:
        if multi:
            out += ["**Binary: `%s`**" % binary, ""]
        for sub in subs:
            rows = sorted((r for r in symbols.get(sub["slug"], [])
                           if r["kind"] == kind and r["source"] in ("sms", "re")),
                          key=lambda r: r["va"])
            if not rows:
                continue
            out += ["### %s" % sub["title"], "",
                    "[`%s.csv`](%s/db/symbols/%s.csv) · [page](%s) — %d named %s"
                    % (sub["slug"], BLOB, sub["slug"], Path(sub["doc"]).name,
                       len(rows), what),
                    "", "| VA | Symbol | Src | Role |", "|----|--------|-----|------|"]
            for r in rows:
                sym = r["display"] or r["name"]
                note = (r["notes"] or "").replace("|", "\\|").strip()
                out.append("| `0x%08X` | `%s` | %s | %s |"
                           % (r["va"], sym, r["source"], note))
            out.append("")
    return "\n".join(out).rstrip() + "\n"


def _region_markers(tag):
    return "<!-- BEGIN GENERATED: %s -->" % tag, "<!-- END GENERATED: %s -->" % tag


def _region_body(block):
    """Canonical text placed between the markers (write and check must agree)."""
    return "\n\n" + block + "\n"


def write_registry_region(path, tag, block):
    text = path.read_text(encoding="utf-8").replace("\r\n", "\n")
    begin, end = _region_markers(tag)
    i, j = text.find(begin), text.find(end)
    if i == -1 or j == -1:
        return False
    new = text[:i + len(begin)] + _region_body(block) + text[j:]
    path.write_bytes(new.encode("utf-8"))
    return True


def registry_currency(path, tag, block, errs):
    try:
        rel = path.relative_to(ROOT).as_posix()
    except ValueError:
        rel = path.name
    text = path.read_text(encoding="utf-8").replace("\r\n", "\n") if path.exists() else ""
    begin, end = _region_markers(tag)
    i, j = text.find(begin), text.find(end)
    if i == -1 or j == -1:
        errs.append("%s: missing generated markers for '%s' (%s … %s)"
                    % (rel, tag, begin, end))
    elif text[i + len(begin):j] != _region_body(block):
        errs.append("%s generated region '%s' is stale — run 'python3 "
                    "tools/check_status.py --write-matrix' and commit" % (rel, tag))


def check_reconstruction(db_dir=DB_DIR):
    """Validate the symbol DB and return (errors, matrix-or-None, registries).
    registries maps (doc-path, marker-tag) -> generated block text."""
    manifest, errs = load_manifest(db_dir / "subsystems.csv")
    if not manifest:
        return errs, None, {}
    symbols, serrs = load_symbols(db_dir / "symbols", manifest)
    binaries = _ordered_binaries(manifest)
    inventories, ierrs = load_inventories(db_dir / "inventory", binaries)
    errs = errs + serrs + ierrs
    # Surface cross-file duplicate-name notes as warnings, not hard errors.
    warns = [e[len("note-dup: "):] for e in errs if e.startswith("note-dup: ")]
    errs = [e for e in errs if not e.startswith("note-dup: ")]
    slug_to_binary = {s["slug"]: s["binary"] for s in manifest}
    claims_by_binary = _claims_by_binary(symbols, slug_to_binary)
    slugs_by_binary = {}
    for slug, binary in slug_to_binary.items():
        slugs_by_binary.setdefault(binary, set()).add(slug)
    for sub in manifest:
        binary = sub["binary"]
        inv = inventories.get(binary, {"functions": {}, "globals": [], "ranges": []})
        claims = claims_by_binary.get(binary, {})
        if sub["status"] in ("active", "complete"):
            errs += check_subsystem_doc(sub, symbols)
        if sub["status"] == "complete":
            errs += check_coverage(sub, symbols, inv, claims,
                                   slugs_by_binary.get(binary, set()))
    matrix = generate_recon_matrix(manifest, symbols, inventories, claims_by_binary)
    registries = {
        (SYMBOLS_MD, REGISTRY_TAGS["func"]): generate_registry(manifest, symbols, "func"),
        (GLOBALS_MD, REGISTRY_TAGS["data"]): generate_registry(manifest, symbols, "data"),
    }
    for w in warns:
        print("note: %s" % w)
    return errs, matrix, registries


# ---------------------------------------------------------------------------
# Driver
# ---------------------------------------------------------------------------

def _matrix_currency(path, generated, errs):
    on_disk = path.read_text(encoding="utf-8") if path.exists() else ""
    if on_disk.replace("\r\n", "\n") != generated:
        errs.append("%s is stale — run 'python3 tools/check_status.py "
                    "--write-matrix' and commit" % path.relative_to(ROOT).as_posix())


def run_checks(write_matrix):
    specs, errs = load_specs()

    # Reconstruction DB + matrix. Generate (and, in --write-matrix, write) both
    # matrices before hygiene so db/README's link to reconstruction.md resolves.
    recon_errs, recon_matrix, registries = check_reconstruction()
    matrix = generate_matrix(specs)
    if write_matrix:
        STATUS_MD.write_bytes(matrix.encode("utf-8"))  # LF-only on every platform
        print("wrote %s" % STATUS_MD.relative_to(ROOT))
        if recon_matrix is not None:
            RECON_MD.write_bytes(recon_matrix.encode("utf-8"))
            print("wrote %s" % RECON_MD.relative_to(ROOT))
            for (path, tag), block in registries.items():
                if write_registry_region(path, tag, block):
                    print("wrote %s [%s]" % (path.relative_to(ROOT), tag))
                else:
                    errs.append("%s: missing generated markers for '%s'"
                                % (path.relative_to(ROOT).as_posix(), tag))
    else:
        _matrix_currency(STATUS_MD, matrix, errs)
        if recon_matrix is not None:
            _matrix_currency(RECON_MD, recon_matrix, errs)
            for (path, tag), block in registries.items():
                registry_currency(path, tag, block, errs)

    errs += check_docs_hygiene()
    errs += check_readme_index(specs)
    claim_errs, warns = check_claims(specs)
    errs += claim_errs
    errs += recon_errs

    for w in warns:
        print("note: %s" % w)
    for e in errs:
        print("error: %s" % e, file=sys.stderr)
    print("%d/%d specs valid; %d error(s)"
          % (len(specs), len(spec_paths()), len(errs)))
    return 1 if errs else 0


# ---------------------------------------------------------------------------
# Self-test
# ---------------------------------------------------------------------------

GOOD_FM = """\
format: ZZZ
name: Test Format
extensions: [".ZZZ"]
category: system
endianness: little
spec:
  status: partial
  gaps:
    - kind: re-static
      issue: 54
      note: "unknown trailer"
codec:
  direction: round-trip
  byte_identical: true
  lib: [lib/src/zzz.cpp]
  commands: [zzz]
  tests: [tests/test_zzz.cpp]
  fuzz: []
  fixtures:
    synthetic: true
    real_manifest: false
related: [LIB]
"""

GOOD_BODY = """\
# ZZZ — Test Format (.ZZZ)

Intro prose.

## Tools

### fx

## File Layout

All multi-byte integers are little-endian unless noted.

## Open Questions

### 1. Unknown trailer

*Status: open — re-static (#54)*

## Related

**Formats:** [LIB](LIB.md)
"""


def self_test():
    def expect(cond, label):
        if not cond:
            raise AssertionError("self-test failed: %s" % label)

    fm = parse_yaml_subset(GOOD_FM)
    expect(fm["format"] == "ZZZ" and fm["codec"]["byte_identical"] is True,
           "parser: scalars and nesting")
    expect(fm["spec"]["gaps"][0]["issue"] == 54, "parser: block list of maps")
    expect(fm["extensions"] == [".ZZZ"], "parser: quoted inline list")
    expect(parse_yaml_subset("a: []\n")["a"] == [], "parser: empty inline list")
    expect(parse_yaml_subset('c:\n  - "x: y"\n')["c"] == ["x: y"],
           "parser: quoted scalar list item")

    for bad in ("a: [1,\n", "a:\n", "\ta: 1\n", "a: 1\na: 2\n",
                "a:\n  b:\n    c:\n      d: 1\n"):
        try:
            parse_yaml_subset(bad)
            raise AssertionError("self-test failed: parser accepted %r" % bad)
        except YamlError:
            pass

    expect(validate_schema(fm, "ZZZ") == [], "schema: good front-matter")
    expect(validate_schema(fm, "YYY") != [], "schema: stem mismatch")

    import copy
    bad = copy.deepcopy(fm)
    bad["nope"] = 1
    expect(any("unknown key" in e for e in validate_schema(bad, "ZZZ")),
           "schema: unknown key rejected")
    bad = copy.deepcopy(fm)
    bad["spec"]["status"] = "complete"
    expect(any("forbidden" in e for e in validate_schema(bad, "ZZZ")),
           "schema: gaps forbidden when complete")
    bad = copy.deepcopy(fm)
    bad["codec"]["tests"] = []
    expect(any("byte_identical" in e for e in validate_schema(bad, "ZZZ")),
           "schema: byte_identical requires tests")
    bad = copy.deepcopy(fm)
    bad["codec"]["direction"] = "read"
    del bad["codec"]["byte_identical"]
    expect(any("exactly one of rationale" in e for e in validate_schema(bad, "ZZZ")),
           "schema: read-only needs rationale or issue")
    bad["codec"]["issue"] = 48
    expect(validate_schema(bad, "ZZZ") == [], "schema: read + issue is valid")
    bad = copy.deepcopy(fm)
    bad["codec"]["direction"] = "none"
    del bad["codec"]["byte_identical"]
    bad["codec"]["issue"] = 49
    expect(any("must be empty" in e for e in validate_schema(bad, "ZZZ")),
           "schema: none forbids lib/commands")

    expect(validate_body(fm, GOOD_BODY) == [], "body: good skeleton")
    expect(any("H1 mismatch" in e
               for e in validate_body(fm, GOOD_BODY.replace("(.ZZZ)", "(.YYY)"))),
           "body: H1 consistency")
    expect(any("order" in e for e in validate_body(
        fm, GOOD_BODY.replace("## Tools\n\n### fx\n\n", "")
                     .replace("## Related", "## Related\n\n## Tools"))),
           "body: H2 order")
    expect(any("#54" in e for e in validate_body(fm, GOOD_BODY.replace("#54", "#99"))),
           "body: gap issue must appear in Open Questions")

    moji = "size is 512Ã—384 pixels"  # 'Ã—'
    expect(find_mojibake(moji) == ["Ã—"], "mojibake: detects double-encoding")
    expect(find_mojibake("legit ½× and — dashes") == [],
           "mojibake: leaves legitimate text alone")

    expect(exists_case_exact(Path(__file__)), "case-exact: this file")
    wrong = Path(__file__).parent / Path(__file__).name.upper()
    expect(not exists_case_exact(wrong), "case-exact: wrong case rejected")

    snippet = (
        'if (strcmp(cmd, "lib") == 0) return cmd_lib(a, b);\n'
        'if (strcmp(cmd, "mission") == 0 || strcmp(cmd, "mm") == 0)\n'
        '    return cmd_mission(a, b);\n'
        'if (strcmp(cmd, "ot") == 0 || strcmp(cmd, "pt") == 0)\n'
        '    return cmd_ot(a, b, cmd);\n'
    )
    mapping = cli_dispatch_map(snippet)
    expect(mapping == {"lib": "cmd_lib", "mission": "cmd_mission",
                       "mm": "cmd_mission", "ot": "cmd_ot", "pt": "cmd_ot"},
           "dispatch: alias mapping")

    links = list(iter_links("[a](B.md) [b](http://x) ![i](img.png#frag) `[c](skip.md)`"))
    expect(links == ["B.md", "img.png"], "links: scheme/anchor/code-span handling")

    urls = list(iter_repo_file_urls(
        "[a](https://github.com/jomkz/fighters-codex/blob/main/README.md) "
        "[b](https://github.com/jomkz/fighters-codex/tree/main/fuzz/) "
        "[c](https://github.com/jomkz/fighters-codex/blob/v0.3.0/README.md#L5) "
        "[d](https://github.com/jomkz/fighters-codex/issues/45) "
        "`[e](https://github.com/jomkz/fighters-codex/blob/main/skip.md)`"))
    expect(urls == [("main", "README.md"), ("main", "fuzz"),
                    ("v0.3.0", "README.md")],
           "repo-urls: blob/tree parsing, ref/fragment/code-span handling")

    docs_dir = (ROOT / "docs").resolve()
    expect(_under((ROOT / "docs" / "cli.md").resolve(), docs_dir),
           "containment: docs file is under docs/")
    expect(not _under((ROOT / "README.md").resolve(), docs_dir),
           "containment: root file is not under docs/")

    row = _matrix_row("ZZZ", fm)
    expect("round-trip (byte-identical)" in row and "re-static" in row,
           "matrix: row rendering")
    expect(_matrix_row("ZZZ", fm) == row, "matrix: deterministic")

    _recon_self_test(expect)

    print("self-test: all checks passed")
    return 0


THEMED_SVG = ('<svg xmlns="http://www.w3.org/2000/svg">'
              '<style>@media (prefers-color-scheme: dark){.b{fill:#111}}'
              ':root[data-theme="dark"] .b{fill:#111}</style></svg>\n')

GOOD_DOC = """\
# Objects

Intro.

> **Provenance:** Ghidra static analysis. Markers per spec-authoring.

## Functions

| VA | Symbol | Role |
|---|---|---|
| `0x00462600` | `InitChain` | reset chains |
| `0x004A7200` / `0x004A7220` | `SetupNT` | init types |

![flow](diagrams/objects.svg)

## Open Questions

### 1. Something

*Status: open — re-static.*

## Related

[shape](shape-selection.md)
"""


def _recon_self_test(expect, tmpdir=None):
    import tempfile
    with tempfile.TemporaryDirectory() as td:
        base = Path(td)
        (base / "symbols").mkdir()
        (base / "inventory").mkdir()
        (base / "diagrams").mkdir()

        def write(rel, text):
            p = base / rel
            p.write_text(text, encoding="utf-8")
            return p

        # doc points at an existing file so load_manifest's existence check passes;
        # doc *structure* is exercised directly via check_doc_structure below.
        header = "slug,title,binary,ranges,status,doc,issue\n"
        write("subsystems.csv", header +
              "obj,Objects,FA.EXE,0x462600-0x462700,complete,db/README.md,210\n")
        good_syms = ("va,kind,name,display,source,confidence,notes,type\n"
                     "0x00462600,func,InitChain,,re,confirmed,head,\n"
                     "0x00462640,func,,,waiver,confirmed,thunk,\n"
                     "0x004EB600,data,_flag,,re,inferred,state,int\n")
        write("symbols/obj.csv", good_syms)
        manifest, merrs = load_manifest(base / "subsystems.csv")
        expect(merrs == [], "recon: clean manifest")
        symbols, serrs = load_symbols(base / "symbols", manifest)
        expect(serrs == [], "recon: clean symbols")

        # Manifest failures
        for bad, label in [
            ("obj,O,FA.EXE,0x462700-0x462600,planned,,0\n", "descending range"),
            ("obj,O,FA.EXE,zzz,planned,,0\n", "bad range syntax"),
            ("obj,O,FA.EXE,0x1-0x2,frozen,,0\n", "unknown status"),
            ("obj,O,FA.EXE,0x1-0x2,complete,nope.md,210\n", "complete needs real doc"),
            ("obj,O,FA.EXE,0x1-0x2,active,db/x.md,0\n", "active needs real issue"),
        ]:
            _, e = load_manifest(write("m.csv", header + bad))
            expect(e != [], "recon: manifest rejects %s" % label)

        # Symbol-schema failures
        for bad, label in [
            ("0x462600,func,X,,re,confirmed,n,\n", "short va"),
            ("0x00462600,func,X,,re,confirmed,n,\n0x00462500,func,Y,,re,confirmed,n,\n",
             "unsorted va"),
            ("0x00462600,func,X,,re,confirmed,n,\n0x00462600,data,Y,,re,confirmed,n,\n",
             "duplicate va"),
            ("0x00462600,thing,X,,re,confirmed,n,\n", "bad kind"),
            ("0x00462600,func,X,,guess,confirmed,n,\n", "bad source"),
            ("0x00462600,func,X,,re,maybe,n,\n", "bad confidence"),
            ("0x00462600,data,,,waiver,confirmed,,\n", "waiver without notes"),
            ("0x00462600,func,,,re,confirmed,n,\n", "non-waiver without name"),
            ("0x00462600,data,,,waiver,confirmed,note,int\n", "waiver carrying a type"),
            ("0x00462600,func,X,,re,confirmed,n\n", "wrong column count"),
        ]:
            wf = base / "symbols" / "m.csv"
            wf.write_text("va,kind,name,display,source,confidence,notes,type\n" + bad,
                          encoding="utf-8")
            _, e = load_symbols(base / "symbols", manifest + [{"slug": "m"}])
            expect(e != [], "recon: symbols reject %s" % label)
            wf.unlink()

        # Symbol file with no manifest row
        (base / "symbols" / "orphan.csv").write_text(good_syms, encoding="utf-8")
        _, e = load_symbols(base / "symbols", manifest)
        expect(any("no matching row" in x for x in e), "recon: orphan symbol file")
        (base / "symbols" / "orphan.csv").unlink()

        # Coverage: functions
        inv_ok = {"functions": {0x462600: {"name": "InitChain", "size": "10"},
                                0x462640: {"name": "FUN_00462640", "size": "5"}},
                  "globals": [{"va": 0x4EB600, "name": "_flag", "xref": "3", "subs": ["obj"]}],
                  "ranges": []}
        sub = manifest[0]
        s2b = {s["slug"]: s["binary"] for s in manifest}
        claims = _claims_by_binary(symbols, s2b).get("FA.EXE", {})
        bslugs = {sl for sl, b in s2b.items() if b == "FA.EXE"}
        expect(check_coverage(sub, symbols, inv_ok, claims, bslugs) == [], "recon: coverage clean")

        inv_funly = dict(inv_ok, functions={0x462600: {"name": "FUN_00462600", "size": "1"}})
        expect(any("FUN_" in x for x in check_coverage(sub, symbols, inv_funly, claims, bslugs)),
               "recon: coverage catches leftover FUN_")
        inv_gap = dict(inv_ok, functions={0x462600: {"name": "InitChain", "size": "1"},
                                          0x4626F0: {"name": "FUN_x", "size": "1"}})
        expect(any("in-scope but absent" in x
                   for x in check_coverage(sub, symbols, inv_gap, claims, bslugs)),
               "recon: coverage catches in-range function missing from DB")
        inv_grf = dict(inv_ok, globals=[{"va": 0x4EB601, "name": "<unnamed>",
                                         "xref": "2", "subs": ["obj"]}])
        expect(any("unnamed and unwaived" in x
                   for x in check_coverage(sub, symbols, inv_grf, claims, bslugs)),
               "recon: coverage catches unwaived referenced global")

        # --- Multi-binary (#252): VAs are unique only within a binary -----------
        # FA.EXE and IP.EXE both name 0x00462600 (their own images) — a collision
        # that MUST be accepted; the same VA twice within one binary is rejected.
        mb_manifest = manifest + [{"slug": "ipx", "title": "IP", "binary": "IP.EXE",
                                   "ranges": [(0x401000, 0x401100)], "status": "complete",
                                   "doc": "db/README.md", "issue": 254, "line": 3}]
        write("symbols/ipx.csv",
              "va,kind,name,display,source,confidence,notes,type\n"
              "0x00462600,func,IPXStart,,re,confirmed,ip entry (own image),\n")
        try:
            mb_syms, mb_e = load_symbols(base / "symbols", mb_manifest)
            expect(not any("already defined" in x for x in mb_e),
                   "recon: same VA in two binaries is accepted")
            s2b_mb = {s["slug"]: s["binary"] for s in mb_manifest}
            cbb = _claims_by_binary(mb_syms, s2b_mb)
            ip_sub = mb_manifest[-1]
            # IP.EXE coverage runs against IP.EXE's OWN inventory (0x462600 = IPXStart,
            # claimed by ipx), independent of FA.EXE naming the same VA InitChain.
            ip_inv = {"functions": {0x462600: {"name": "IPXStart", "size": "4"}},
                      "globals": [], "ranges": []}
            ip_slugs = {sl for sl, b in s2b_mb.items() if b == "IP.EXE"}
            expect(check_coverage(ip_sub, mb_syms, ip_inv, cbb.get("IP.EXE", {}),
                                  ip_slugs) == [], "recon: IP.EXE coverage clean (own inventory)")
            # matrix groups by binary
            mtx = generate_recon_matrix(mb_manifest, mb_syms,
                                        {"FA.EXE": inv_ok, "IP.EXE": ip_inv}, cbb)
            expect("## FA.EXE" in mtx and "## IP.EXE" in mtx,
                   "recon: matrix has one section per binary")
        finally:
            (base / "symbols" / "ipx.csv").unlink()
        # Same VA twice within ONE binary is still an error:
        write("symbols/obj.csv", good_syms.rstrip("\n") +
              "\n0x00462600,data,Dup,,re,confirmed,dup,int\n")
        _, dup_e = load_symbols(base / "symbols", manifest)
        expect(any("already defined" in x for x in dup_e),
               "recon: same VA twice in one binary is rejected")
        write("symbols/obj.csv", good_syms)

        # Doc structure
        write("x.md", GOOD_DOC)
        drows = symbols["obj"] + [{"va": 0x4A7200, "kind": "func", "name": "_SetupNT@4",
                                   "display": "SetupNT", "source": "sms",
                                   "confidence": "confirmed", "notes": ""},
                                  {"va": 0x4A7220, "kind": "func", "name": "_SetupPT@4",
                                   "display": "SetupNT", "source": "sms",
                                   "confidence": "confirmed", "notes": ""}]
        write("diagrams/objects.svg", THEMED_SVG)
        expect(check_doc_structure("x.md", GOOD_DOC, base, drows) == [],
               "recon: clean subsystem doc")
        expect(any("Provenance" in x for x in check_doc_structure(
            "x.md", GOOD_DOC.replace("> **Provenance:**", "Provenance"), base, drows)),
            "recon: doc needs provenance")
        expect(any("not in the DB" in x for x in check_doc_structure(
            "x.md", GOOD_DOC.replace("0x00462600", "0x00999999"), base, drows)),
            "recon: Functions VA must be in DB")
        expect(any("Status" in x for x in check_doc_structure(
            "x.md", GOOD_DOC.replace("*Status: open — re-static.*", "done"), base, drows)),
            "recon: open question needs Status line")
        expect(any("theme-aware" in x for x in check_doc_structure(
            "x.md", GOOD_DOC, base,
            drows)) is False, "recon: themed svg accepted")
        write("diagrams/objects.svg", "<svg></svg>\n")
        expect(any("theme-aware" in x
                   for x in check_doc_structure("x.md", GOOD_DOC, base, drows)),
               "recon: non-theme svg rejected")
        expect(any("diagram" in x for x in check_doc_structure(
            "x.md", GOOD_DOC.replace("diagrams/objects.svg", "diagrams/missing.svg"),
            base, drows)), "recon: missing diagram rejected")

        # Generated registry regions (symbols.md / globals.md).
        fblock = generate_registry(manifest, symbols, "func")
        expect("InitChain" in fblock and "thunk" not in fblock,
               "recon: registry lists named funcs, not waivers")
        expect("_flag" in generate_registry(manifest, symbols, "data"),
               "recon: registry lists named globals")
        rp = write("reg.md", "x\n\n<!-- BEGIN GENERATED: symbol-registry -->\n"
                   "<!-- END GENERATED: symbol-registry -->\n")
        expect(write_registry_region(rp, "symbol-registry", fblock),
               "recon: registry region written into markers")
        rerrs = []
        registry_currency(rp, "symbol-registry", fblock, rerrs)
        expect(rerrs == [], "recon: written registry region is current")
        registry_currency(rp, "symbol-registry", fblock + "x\n", rerrs)
        expect(any("stale" in e for e in rerrs),
               "recon: mutated registry region is stale")
        expect(write_registry_region(write("nomark.md", "no markers\n"),
                                     "symbol-registry", fblock) is False,
               "recon: missing markers reported, not written")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--check", action="store_true",
                       help="validate docs and claims; fail if STATUS.md is stale")
    group.add_argument("--write-matrix", action="store_true",
                       help="validate, then regenerate docs/fa/formats/STATUS.md")
    group.add_argument("--self-test", action="store_true",
                       help="run the embedded test suite")
    args = parser.parse_args()
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(errors="replace")
        sys.stderr.reconfigure(errors="replace")
    if args.self_test:
        return self_test()
    return run_checks(write_matrix=args.write_matrix)


if __name__ == "__main__":
    sys.exit(main())
