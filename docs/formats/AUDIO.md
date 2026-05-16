# Audio -- Raw PCM (.11K / .5K / .8K)

> Format identified from FA audio file analysis. Sample rate confirmed against the
> [OpenFA project](https://gitlab.com/openfa/openfa).

---

## Overview

FA audio files are raw, headerless, signed 8-bit mono PCM. The sample rate is encoded
in the file extension:

| Extension | Sample rate | Notes |
|-----------|-------------|-------|
| `.5K` | 5512 Hz | |
| `.8K` | 8000 Hz | Confirmed via TOOLKIT LIBPTR cache |
| `.11K` | 11025 Hz | Most common |
| `.22K` | 22050 Hz | Supported by TOOLKIT; not observed in FA LIBs (may be ATF/USNF only) |

## Filename Prefix Conventions

The filename prefix (before any letters) is an engine convention, not a format difference:

| Prefix | Meaning | Example |
|--------|---------|---------|
| `&` | Looping ambient / cockpit sound | `&AFTB2.11K` |
| `^` | Voice / radio callout (one-shot) | `^ENGAGE.11K`, `^MLTRY-A.11K` |

`ft lib unpack` maps `&` and `^` to `_` on extraction because Windows rejects those
characters in filenames. The original names are preserved in memory for patching.

---

## PCM Encoding

- Signed 8-bit (`int8_t`), range −128..127
- Mono (1 channel)
- No header, no footer — the file is raw samples from byte 0

---

## WAV Conversion

WAV stores 8-bit audio as **unsigned** (0..255), not signed. Apply `^ 0x80` on both
directions:

```c
// FA raw → WAV sample
wav_byte = (uint8_t)((int8_t)fa_byte + 128);   // equivalently: fa_byte ^ 0x80

// WAV sample → FA raw
fa_byte = (int8_t)(wav_byte - 128);
```

WAV header (mono, 8-bit, sample rate `R`):

```
RIFF chunk:  "RIFF" + (file_size - 8) u32LE + "WAVE"
fmt  chunk:  "fmt " + 16 u32LE + 1 u16LE (PCM) + 1 u16LE (channels)
             + R u32LE (sample rate) + R u32LE (byte rate)
             + 1 u16LE (block align) + 8 u16LE (bits per sample)
data chunk:  "data" + sample_count u32LE + [samples]
```

---

## ft commands

```
ft audio info   <file.11K|.5K|.8K>              # sample rate, sample count, duration
ft audio unpack <file.11K|.5K|.8K> [-o out.wav] # raw PCM → WAV
ft audio pack   <in.wav> -o <out.11K|.5K|.8K>   # WAV → raw PCM
                            [-r 11025]            # override sample rate (default from ext)
```

The output extension determines the stored sample rate when packing.
Input WAV must be mono, 8-bit; `ft` rejects stereo or 16-bit input.

---

## .MUS -- Music Slot File

`.MUS` files are Phar Lap PE binaries (same container as `.SH`) that act as named
pointers to `.11K` audio files. The game has nine fixed music slots:

| Filename | Trigger | Referenced audio |
|----------|---------|-----------------|
| `M_air.MUS` | Dogfight | `dogf001.11K` |
| `M_danger.MUS` | Enemy detected | `dang001.11K` |
| `M_deck.MUS` | On the deck | `valk001.11K` |
| `M_eject.MUS` | Ejected | `norm001.11K` |
| `M_home.MUS` | Almost home | `valk001.11K` |
| `M_launch.MUS` | Takeoff | `slam001.11K` |
| `M_normal.MUS` | Normal flight | `norm001.11K` |
| `M_succ.MUS` | Success | `succ001.11K` |
| `M_valk.MUS` | Ctrl+V hidden track | `valk001.11K` |

### Internal structure

At a fixed offset in the code section there is a 6-byte record:

```
FF [name 4 bytes] 00 ...
```

The 4-byte name (e.g. `norm`, `dogf`, `slam`) is appended with `001.11K` to form
the audio filename the engine looks up in the LIB. To replace in-game music:

1. Prepare a mono 8-bit 11,025 Hz WAV and pack it to `.11K` with `ft audio pack`.
2. Rename the packed file to match the slot's expected name (e.g. `norm001.11K`).
3. Update `ITEM.DAT` (or `TK.TRN`) in the toolkit project to reference the new filename.
4. Patch both the `.MUS` file and the renamed `.11K` into the target `.LIB`.

To create a `.5K` variant: pack at 11025 Hz after doubling playback speed 2×, then
rename the extension to `.5K`. The engine plays it at half rate, yielding the correct
pitch with reduced quality.
