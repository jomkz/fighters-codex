# Game compatibility — ATF Gold & USNF'97

The Fighters Anthology formats were reverse-engineered from FA (1998). FA is the last and richest
member of a family built on one continuously-evolved engine, so its predecessors were expected to
share most formats. This page records what happens when `fx` is actually run against real retail
installs of the two nearest relatives, resolving the "Confirmation needed" note in the README
([#57](https://github.com/jomkz/fighters-codex/issues/57)).

**Titles verified**

| Title | Retail version | Media |
|---|---|---|
| Advanced Tactical Fighters — Gold | 1.00F (disc label `ATFG_1_00F`) | `atf_*.lib` + `setup.esa` |
| U.S. Navy Fighters '97 | 1.04F (disc label `USNF_1_04F`) | `usnf_*.lib` + `setup.esa` |

**Headline result:** every format `fx` implements decodes on both titles, and both container formats
round-trip **byte-identically**. The engine is shared closely enough that the FA format specs
describe ATF and USNF as well, with only minor, self-describing structural deltas (below). The one
format `fx` does not cover on *any* title is `.XMI` (Miles XMIDI music) — that is external-converter
territory, not an FA-family codec, and its absence is not an ATF/USNF-specific gap.

## Method

Reproducible from the retail discs with no game install:

```
# mount the disc image read-only (loop), then, e.g. for ATF:
fx esa ls   "$DISC/setup.esa"                 # installer payload: EXE, SMS, inner LIBs, DLLs
fx esa extract "$DISC/setup.esa" ATF_1.LIB -o out
fx lib ls   out/ATF_1.LIB                      # the installed content archive
fx lib extract out/ATF_1.LIB A10.SH -o out     # any entry
fx sh  unpack out/A10.SH -o A10.obj             # run the codec
```

The disc-resident `*.lib` archives hold the CD-streamed media (audio, images, and — on USNF — the
`.VDO`/`.FBC` cutscenes); the executables, 3D shapes, missions, type records, palettes and terrain
live in `setup.esa`'s inner LIBs (`ATF_1/2/4B.LIB`, `USNF_1/2.LIB`), exactly as FA splits its own
content. Both titles ship their own game executable and symbol map (`ATF.EXE`+`ATF.SMS`,
`USNF.EXE`+`USNF.SMS`) and the same overlay-DLL family as FA (`WAIL32.DLL`, `CDRVDL32/HF32/XF32.DLL`,
`COMMSC32.DLL`).

## Containers — byte-identical round-trip

The strongest proof: the container codecs re-emit both titles' archives byte-for-byte.

| Format | `fx` command | ATF | USNF | Notes |
|---|---|---|---|---|
| LIB | `fx lib repack` | ✅ byte-identical | ✅ byte-identical | same DCL/LZSS/PxPk entry compression as FA |
| ESA | `fx esa repack` | ✅ byte-identical | ✅ byte-identical | installer payload; PKWARE-compressed + stored entries |

## Content formats — decoded on both titles

Every format below was extracted from a real ATF **and** USNF install and run through its `fx`
codec. ✅ = decodes/parses correctly; where a codec round-trips on FA it round-trips here too (the
byte logic is title-independent).

| Format | `fx` command | ATF | USNF | Spec |
|---|---|---|---|---|
| PIC (image) | `pic unpack` | ✅ decodes to PNG | ✅ decodes to PNG | [PIC](formats/PIC.md) |
| PAL (palette) | `pal info` | ✅ | ✅ | [PAL](formats/PAL.md) |
| SH (3D shape) | `sh unpack` | ✅ → OBJ | ✅ → OBJ | [SH](formats/SH.md) |
| CB8 (video) | `cb8 info` | ✅ | ✅ | [CB8](formats/CB8.md) |
| VDO / FBC (cutscene) | `vdo`/`fbc` | — (not shipped) | ✅ | [VDO](formats/VDO.md) |
| 5K / 11K / 8K (audio) | `audio info` | ✅ | ✅ | [11K](formats/11K.md) |
| INF (tech sheet) | `inf dump` | ✅ | ✅ | [INF](formats/INF.md) |
| M / MM / MC (mission) | `mission info` | ✅ | ✅ | [M](formats/M.md) |
| T2 (terrain) | `t2 info` | ✅ | ✅ | [T2](formats/T2.md) |
| OT/NT/PT/JT/SEE/ECM/GAS (BRF types) | `ot info` … | ✅ | ✅ | [BRF](formats/BRF.md) |
| SEQ (cutscene timeline) | `seq dump` | ✅ | ✅ | [SEQ](formats/SEQ.md) |
| HUD (cockpit layout) | `hud dump` | ✅ | ✅ | [HUD](formats/HUD.md) |
| LAY (sky/atmosphere) | `lay dump` | ✅ | ✅ | [LAY](formats/LAY.md) |
| FNT (bitmap font) | `fnt info` | ✅ | ✅ | [FNT](formats/FNT.md) |
| MUS (music playlist) | `mus dump` | ✅ | ✅ | [MUS](formats/MUS.md) |
| BI / AI (AI bytecode) | `bi dump` / `ai compile` | ✅ | ✅ | [BI](formats/BI.md) |
| CAM (campaign) | `cam info` | ✅ | ✅ | [CAM](formats/CAM.md) |
| DLG / MNU (UI overlay) | `dlg`/`mnu info` | ✅ | ✅ | [DLG](formats/DLG.md) |
| PTS / HGR / BIN (data) | `pts`/`hgr`/`bin info` | ✅ | ✅ | [PTS](formats/PTS.md) |
| TXT / MT (text) | `txt`/`mt info` | ✅ | ✅ | [TXT](formats/TXT.md) |
| SMS (symbol map) | `sms dump` | ✅ (3,615 syms) | ✅ (3,440 syms) | — |
| XMI (Miles XMIDI music) | — | ❌ no codec | ❌ no codec | not an FA-family format |

## Observed deltas

None of these required a code change — the formats are self-describing, so one codec reads every
variant:

- **BRF type records carry different field/block counts per title.** The same aircraft's `.PT`
  decodes to 219 fields / 20 blocks on ATF but 218 / 18 on USNF; `.NT` is 73/7 vs 72/7; `.OT` 64/2
  vs 63/2. Each record states its own layout (the BRF header enumerates its fields), so `fx`'s
  parser follows the record rather than a hard-coded FA schema, and every variant decodes. This is
  the mechanism that makes the type formats title-independent.
- **Cutscene media differs by title.** USNF ships `.VDO`/`.FBC` full-motion cutscenes on the disc;
  ATF Gold ships `.CB8` block-video. Both formats decode; the two titles simply carry different
  content.
- **Each title has its own executable and symbol map.** `ATF.EXE`/`USNF.EXE` are distinct binaries
  from `FA.EXE`; their `.SMS` maps parse with `fx sms dump` (ATF 3,615 symbols, USNF 3,440). This
  page verifies the *asset formats*, not those executables — a per-title reconstruction is out of
  scope for the verification pass.

## Scope and status

This was a timeboxed verification pass ([#57](https://github.com/jomkz/fighters-codex/issues/57)):
its job is to state exactly what is confirmed per title, which is what the table above does. It is
not a commitment to maintain ATF/USNF as first-class targets — the FA specs remain the authority,
and these titles are documented as verified-compatible rather than separately specified.

Not attempted here (honest markers, available as follow-ups): wiring the ATF/USNF discs into the
`FX_FA_ROOT`-style integration test harness for CI-durable proof, and any reconstruction of the
`ATF.EXE`/`USNF.EXE` executables.
