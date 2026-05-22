# Contributing

## Commit Messages

This project uses [Conventional Commits](https://www.conventionalcommits.org/). Commit
messages drive the automated changelog — `scripts/draft-changelog.ps1` parses them to
populate `CHANGELOG.md` before each release.

### Format

```
<type>[(<scope>)][!]: <description>
```

**Examples:**
```
feat(ft-gui): add dark/light theming toggle
fix(ft-lib): correct CB8 stride calculation for odd-width images
docs: document MUS opcode table
refactor(ft-cli): simplify extract command argument parsing
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
| `ft-lib` | `lib/` static library |
| `ft-cli` | `cli/` command-line tool |
| `ft-gui` | `gui/` GUI application |

**Omit the scope** when a change spans multiple components or is repo-wide. Do not
combine scopes (e.g. `feat(ft-lib,ft-cli):`) — either split into separate commits or
drop the scope entirely.

### Breaking Changes

Append `!` after the type/scope, or add a `BREAKING CHANGE:` footer:

```
feat(ft-lib)!: rename Lib::extract() to Lib::unpack()

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

---

For the full workflow — building, releasing, and the draft-changelog script — see
[docs/development.md](docs/development.md).
