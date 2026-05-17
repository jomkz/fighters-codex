# EA Installer Script (.SSF)

`.SSF` files are plain-text EA installer scripts that drive the FA installation process. They list source files, destination paths, install options, and conditional logic for selecting between install configurations.

## Location

Found in the FA installer directory. Not packed into any LIB archive.

## Known Files

| File | Role |
|------|------|
| `FINSTALL.SSF` | Full install — copies all assets including FA_4B.LIB (digital audio) |
| `MINSTALL.SSF` | Minimal install — omits FA_4B.LIB; uses MIDI only (FA_4C.LIB on disc) |

## Format

Plain ASCII text. The EA installer script language is proprietary but resembles a simple DSL with:
- File copy directives (source → destination path)
- Conditional blocks (disc type, free disk space, user selection)
- Registry / INI write operations
- Reference to `.RGN` files for installer UI layout

## Key Distinction: Full vs Minimal Install

The primary difference between `FINSTALL.SSF` and `MINSTALL.SSF` is whether `FA_4B.LIB` is copied to the hard drive. `FA_4B.LIB` contains 77+ `.11K` digital audio files for in-game music. The minimal install skips this and relies on the MIDI sequences in `FA_4C.LIB` streamed from the CD.

## TODO

- Extract the complete file copy manifest from both SSF files to document which LIBs land on disk vs. remain CD-resident
- Identify the SSF grammar (keywords, operators, conditionals) for any future installer tooling

## Related

- [EALIB.md](EALIB.md) — LIB archive format; FA_4B.LIB is the key full-install-only LIB
- [RGN.md](RGN.md) — installer UI region maps referenced by the installer
