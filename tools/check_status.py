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
                  BOM, no mojibake), relative-link integrity (case-exact),
                  README index membership.
  2. claims:      every lib/cli/test/fuzz/gui pointer resolves to a real file;
                  every fx command token is a live dispatch literal.
  3. coverage:    (once all specs are converted) every codec, CLI command,
                  test, fuzz harness, and GUI editor is claimed by a spec.

Stdlib-only; Python 3.8+.
"""

import argparse
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

# Specs not yet restructured to the template (docs/spec-authoring.md). Each
# conversion batch removes its tokens; the mechanism is deleted when empty.
LEGACY = {
    "11K", "AI", "BI", "BRF", "CAM", "CB8", "CFG", "DAT", "DLG",
    "ECM", "FNT", "GAS", "HUD", "JT", "M", "MC",
    "MNU", "MT", "MUS", "NT", "OT", "P", "PT",
    "PTS", "RGN", "SEE", "SEQ", "SMS", "SSF", "TXT",
    "VDO", "XMI",
}

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


def iter_links(text):
    """Relative link targets outside code fences and inline code spans."""
    fence = False
    for line in text.splitlines():
        if line.lstrip().startswith("```"):
            fence = not fence
            continue
        if fence:
            continue
        line = re.sub(r"`[^`]*`", "", line)
        for m in LINK_RE.finditer(line):
            target = m.group(1)
            if re.match(r"^[a-z][a-z0-9+.-]*:", target):  # http:, mailto:, ...
                continue
            target = target.split("#", 1)[0]
            if target:
                yield target


def docs_files():
    files = sorted(ROOT.glob("*.md"))
    files += sorted(p for p in (ROOT / "docs").rglob("*.md"))
    return files


def check_docs_hygiene():
    errs = []
    for path in docs_files():
        rel = path.relative_to(ROOT)
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
        for target in iter_links(text):
            resolved = (path.parent / target).resolve()
            if not exists_case_exact(resolved):
                errs.append("%s: broken or wrong-case link: %s" % (rel, target))
    return errs


# ---------------------------------------------------------------------------
# Spec loading + repo-reality checks
# ---------------------------------------------------------------------------

def spec_paths():
    return sorted(p for p in FORMATS_DIR.glob("*.md") if p.name not in NON_SPEC)


def load_specs():
    """Return ({token: front-matter}, errors) for converted (non-LEGACY) specs."""
    specs = {}
    errs = []
    for path in spec_paths():
        stem = path.stem
        rel = path.relative_to(ROOT)
        text = path.read_bytes().decode("utf-8", errors="replace")
        has_fm = text.startswith("---\n")
        if stem in LEGACY:
            if has_fm:
                errs.append("%s: has front-matter but is still in LEGACY — remove "
                            "the token from LEGACY in tools/check_status.py" % rel)
            continue
        if not has_fm:
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

    # Reverse coverage: enforced once every spec is converted, advisory before.
    sink = errs if not LEGACY else warns
    def unclaimed(kind, paths):
        for p in sorted(paths):
            rel = str(p.relative_to(ROOT))
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
# Driver
# ---------------------------------------------------------------------------

def run_checks(write_matrix):
    specs, errs = load_specs()
    errs += check_docs_hygiene()
    errs += check_readme_index(specs)
    claim_errs, warns = check_claims(specs)
    errs += claim_errs

    matrix = generate_matrix(specs)
    if write_matrix:
        STATUS_MD.write_bytes(matrix.encode("utf-8"))  # LF-only on every platform
        print("wrote %s" % STATUS_MD.relative_to(ROOT))
    else:
        on_disk = STATUS_MD.read_text(encoding="utf-8") if STATUS_MD.exists() else ""
        if on_disk.replace("\r\n", "\n") != matrix:
            errs.append("docs/fa/formats/STATUS.md is stale — run "
                        "'python3 tools/check_status.py --write-matrix' and commit")

    for w in warns:
        print("note: %s" % w)
    for e in errs:
        print("error: %s" % e, file=sys.stderr)
    converted = len(specs)
    total = len(spec_paths())
    print("%d/%d specs converted; %d error(s), %d advisory note(s)"
          % (converted, total, len(errs), len(warns)))
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

    row = _matrix_row("ZZZ", fm)
    expect("round-trip (byte-identical)" in row and "re-static" in row,
           "matrix: row rendering")
    expect(_matrix_row("ZZZ", fm) == row, "matrix: deterministic")

    print("self-test: all checks passed")
    return 0


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
