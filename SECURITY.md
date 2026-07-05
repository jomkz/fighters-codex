# Security Policy

`fx_lib` and the tools built on it parse untrusted binary files (archives, images, video,
save files). Malformed-input handling is in scope for security reports — a crafted file
that crashes or corrupts memory in `fx`, `fxs`, or any `fx_lib` codec is a bug worth
reporting even without a demonstrated exploit.

## Supported versions

Only the latest release and `main` receive fixes.

## Reporting a vulnerability

Use [GitHub private vulnerability reporting](https://github.com/jomkz/fighters-codex/security/advisories/new)
for anything you believe is exploitable (memory corruption, out-of-bounds reads/writes).
For plain crashes or hangs on malformed input, a regular
[bug report](https://github.com/jomkz/fighters-codex/issues/new?template=bug_report.yml)
with the offending file (or a minimized reproducer) attached is fine — please don't attach
copyrighted game assets; minimize to a synthetic reproducer where possible.

Expect an acknowledgement within a week. There is no bounty program; this is a
documentation-first reverse-engineering project.

## Hardening roadmap

Every PR runs the full test suite under ASan/UBSan, a 60-second libFuzzer smoke run of
each fuzz harness (the first covers the LIB container and DCL decompressor), and CodeQL
(security-extended) over all first-party C++; CodeQL also runs weekly. The per-parser
fuzzing rollout is tracked as Phase 4 of the [roadmap](docs/roadmap.md) (epic #51).
Local sanitizer and fuzzing builds are available via `cmake --preset asan-ubsan` and
`cmake --preset fuzz` (Linux; see [docs/development.md](docs/development.md#fuzzing)).
