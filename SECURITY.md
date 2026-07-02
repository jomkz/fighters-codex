# Security Policy

`fx_lib` and the tools built on it parse untrusted binary files (archives, images, video,
save files). Malformed-input handling is in scope for security reports — a crafted file
that crashes or corrupts memory in `fx`, `fx-gui`, or any `fx_lib` codec is a bug worth
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

Every PR runs the full test suite under ASan/UBSan, and CodeQL (security-extended)
analyzes all first-party C++ on PRs, pushes to `main`, and a weekly schedule. Parser
fuzzing (libFuzzer harnesses per codec) is tracked as Phase 4 of the
[roadmap](docs/roadmap.md) (epic #51). Local ASan/UBSan builds are available via
`cmake --preset asan-ubsan` (Linux).
