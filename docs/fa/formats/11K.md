# Audio -- Raw PCM (.11K / .5K / .8K)

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

## PCM Encoding

- Signed 8-bit (`int8_t`), range −128..127
- Mono (1 channel)
- No header, no footer — the file is raw samples from byte 0

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

## ft commands

```
ft audio info   <file.11K|.5K|.8K>              # sample rate, sample count, duration
ft audio unpack <file.11K|.5K|.8K> [-o out.wav] # raw PCM → WAV
ft audio pack   <in.wav> -o <out.11K|.5K|.8K>   # WAV → raw PCM
                            [-r 11025]            # override sample rate (default from ext)
```

The output extension determines the stored sample rate when packing.
Input WAV must be mono, 8-bit; `ft` rejects stereo or 16-bit input.

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

## AIL Runtime Integration (FA.EXE)

FA.EXE calls the **Miles Audio Interface Library (AIL)** API directly. The following API calls and globals were confirmed via Ghidra analysis of the FA.EXE main binary:

### Initialization sequence

```c
_AIL_startup_0();
_AIL_set_preference_8(4, 0x78);                              // MIDI preference
_AIL_midiOutOpen_12(&_musicDriverHandle, 0, 0xFFFFFFFF);     // open MIDI driver
_AIL_lock_0();
_AIL_set_XMIDI_master_volume_8(...);
_AIL_unlock_0();
_AIL_register_timer_4(&_PollMod__YGXK_Z) → _timerHandle;    // register poll callback
_AIL_set_timer_frequency_8(_timerHandle, 0x1e);              // 30 Hz
_AIL_start_timer_4(_timerHandle);

_AIL_set_preference_8(0xf, 0);
_AIL_set_preference_8(0xe, 0x4000);
_AIL_set_preference_8(0,   0x10);
_AIL_set_preference_8(2,   0x28f);
_AIL_waveOutOpen_16(&_soundDriverHandle, 0, 0, &fmt);        // 22050 Hz, stereo, 16-bit

// Allocate mix channels (loop):
_AIL_allocate_sample_handle_4(_soundDriverHandle) → _mixChanHandle[i]
```

### Runtime globals

| Global | Type | Role |
|--------|------|------|
| `_musicDriverHandle__3PAU_MDI_DRIVER__A` | MDI_DRIVER* | MIDI driver handle |
| `_soundDriverHandle__3PAU_DIG_DRIVER__A` | DIG_DRIVER* | Digital audio driver handle (22050 Hz stereo 16-bit) |
| `_timerHandle__3JA` | HTIMER | 30 Hz poll timer for `_PollMod` |
| `_mixChanHandle__3PAPAU_SAMPLE__A` | HSAMPLE[] | Per-channel digital audio handles |
| `_ailActive__3DA` | bool | AIL system active flag |

### Channel teardown

`_AIL_end_sample_4(channel)` — called per channel on shutdown.

The WAIL32.DLL and msapi.dll are the AIL wrapper DLLs loaded at runtime. Their import surface has not been traced (no Ghidra output available for the secondary project). IP.EXE audio path is also uncharted.

## Applications

Use `ft audio unpack` to convert to WAV, edit, then `ft audio pack` to re-encode.
Audacity can also import the raw PCM file directly without the `ft` step (*File →
Import → Raw Data*: signed 8-bit, mono, sample rate from extension).

- **Audacity** — free, cross-platform; raw import, noise reduction, pitch/tempo tools
- **Adobe Audition** `$` — paid; professional mastering and spectral repair
