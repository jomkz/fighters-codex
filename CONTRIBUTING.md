# Contributing

This file covers commit message and branch naming conventions — the things that affect
everyone working on the repo. For the full developer reference (building, IDE setup,
project structure, and release workflow) see [docs/development.md](docs/development.md).
For what to work on and how work is organized (milestones = phases, `epic`-labeled
issues + sub-issues), see [docs/roadmap.md](docs/roadmap.md).

## Commit Messages

This project uses [Conventional Commits](https://www.conventionalcommits.org/). Commit
messages drive the automated changelog — `scripts/draft-changelog.py` parses them to
populate `CHANGELOG.md` before each release.

### Format

```
<type>[(<scope>)][!]: <description>
```

**Examples:**
```
feat(fx-gui): add dark/light theming toggle
fix(fx-lib): correct CB8 stride calculation for odd-width images
docs: document MUS opcode table
refactor(fx-cli): simplify extract command argument parsing
feat!: change LIB archive header magic — breaks existing files
```

### Types

| Type | Changelog section | When to use |
|---|---|---|
| `feat` | Added | New user-facing functionality |
| `fix` | Fixed | Bug fixes |
| `docs` | Changed | Documentation only |
| `refactor` | Changed | Code restructuring, no behaviour change |
| `perf` | Changed | Performance improvements |
| `chore` | *(omitted)* | Maintenance, dependency bumps |
| `ci` | *(omitted)* | CI/CD changes |
| `build` | *(omitted)* | Build system changes |
| `test` | *(omitted)* | Adding or updating tests |
| `style` | *(omitted)* | Formatting, whitespace |

### Scopes

Use a scope when the change is isolated to one component:

| Scope | Targets |
|---|---|
| `fx-lib` | `lib/` static library |
| `fx-cli` | `cli/` command-line tool |
| `fx-gui` | `gui/` GUI application |

**Omit the scope** when a change spans multiple components or is repo-wide. Do not
combine scopes (e.g. `feat(fx-lib,fx-cli):`) — either split into separate commits or
drop the scope entirely.

### Breaking Changes

Append `!` after the type/scope, or add a `BREAKING CHANGE:` footer:

```
feat(fx-lib)!: rename Lib::extract() to Lib::unpack()

BREAKING CHANGE: all callers must update to the new method name.
```

Breaking commits appear prominently in the changelog and signal a semver major bump.

### Branch Names

```
<type>/<short-kebab-description>
```

Use the same type prefix as your commit (`feat/`, `fix/`, `docs/`, `refactor/`, `chore/`),
followed by a short lowercase kebab-case description. See
[docs/development.md](docs/development.md#branch-names) for the full table and rules.

## Pull Request Checks

Every PR runs the CI workflow — a build-and-test matrix across Linux and Windows plus
analysis legs. See [docs/development.md](docs/development.md#continuous-integration) for
the current leg list and what each check proves. PRs are expected to be green across the
matrix before merge; if a leg is red, fix the change rather than the check.

---

For the full workflow — building, releasing, and the draft-changelog script — see
[docs/development.md](docs/development.md).
