# Campaign Description Text (.TXT)

FA_2.LIB contains 8 `.TXT` files (e.g. `BALTIC.TXT`). Each stores the short description shown on the campaign selection screen. Format is **plain ASCII text** using the same section/directive syntax as `.MT` mission briefing files, but with a simpler two-section structure.

## Format

Plain ASCII text, CRLF line endings. Uses the same `.section`, `.header`, `.body` directives as `.MT` files.

### Structure

```
.section 1
.header
<Campaign title>
.body
<Campaign description paragraph>
.section 2
END
```

Section 2 always appears to contain only `END` — likely a sentinel the engine checks for to detect end-of-file cleanly.

### Example — BALTIC.TXT (full file, 147 bytes)

```
.section 1
.header
The Baltics 2009
.body
Fly missions over Estonia, Latvia, and Lithuania, defending them from aggression.
.section 2
END
```

## Location

| LIB | Count |
|-----|-------|
| FA_2.LIB | 8 |

## TODO — Deep Dive

- Confirm all 8 files follow the same two-section pattern
- Clarify why there are 8 `.TXT` files for 6 campaigns (2 campaigns may have alternate/variant descriptions)

## Related

- [CAM.md](CAM.md) — campaign definition overlays shown alongside this text
- [MNU.md](MNU.md) — the campaign selection menu screen that displays this text
- [MT.md](MT.md) — mission briefing text using the same section/directive syntax
