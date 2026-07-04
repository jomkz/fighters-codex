# External imports — MS / third-party DLL boundary

FA.EXE and its companion binaries link a set of **Microsoft and third-party redistributable
DLLs**. This page documents the FA-side **boundary** — which functions each binary imports from
each external DLL — built mechanically from the PE import tables. Per the license rule (MIT
repo), the external code itself is **not** reverse-engineered; only the interface FA calls is
recorded. FA's own companion binaries — [WAIL32.DLL](wail32.md) (Miles/AIL) and the Cdrv comms
suite ([CDRVDL32](comms.md) / [modem](comms-modem.md) / [transfer](comms-transfer.md) /
[terminal](comms-screen.md)) — are documented as their own subsystems, not here.

## External DLLs

| DLL | Kind | Role | Imported by | Boundary (representative) |
|-----|------|------|-------------|---------------------------|
| `DDRAW.dll` | MS DirectX | DirectDraw 2D surface / blit | FA.EXE, IP.EXE | `DirectDrawCreate` (1) — the rest of the surface API is via the returned COM vtable (see [render-core.md](render-core.md)) |
| `DSOUND.dll` | MS DirectX | DirectSound | IP.EXE (diagnostics) | `DirectSoundCreate` |
| `WINMM.dll` | MS Win32 | multimedia — joystick + timers + wave/MIDI | FA.EXE · WAIL32 · IP.EXE · comms | FA.EXE: `joyGetPosEx`/`joyGetPos`/`joyGetDevCapsA` (input), `timeGetTime`; WAIL32: `timeSetEvent`/`waveOut*`; IP.EXE: `midi*`/`wave*`/`mixer*` DevCaps (diagnostics) |
| `MSAPI.dll` | EA / third-party | **multiplayer match / mission-server client** | FA.EXE | `initializeMS`, `connectMS`, `sendMSresults`, `getMSdatafilesize`, `getMSdatafile`, `closeMS` |
| `MAPI32.dll` | MS | Simple MAPI e-mail | IP.EXE | (the support-report e-mail path) |
| `WSOCK32.dll` | MS | Winsock — IP config query | IP.EXE | (the network-config section of the support report) |
| `ADVAPI32.dll` | MS Win32 | registry | FA.EXE · IP.EXE · … | FA.EXE: `RegOpenKeyExA`/`RegQueryValueExA`/`RegEnumKeyExA`/`RegCloseKey` |
| `KERNEL32` / `USER32` / `GDI32` | MS Win32 | OS core · windowing · GDI | all binaries | ubiquitous OS surface (files, memory, threads, messages, DC/blit) |
| `VERSION` · `comdlg32` · `WINSPOOL.DRV` · `SHELL32` · `COMCTL32` | MS Win32 | version info · common dialogs · printing · shell | IP.EXE | the support tool's file/print/shell UI (`GetOpenFileNameA`, `OpenPrinterA`, `ShellExecuteExA`, …) |

## Notes

- **`MSAPI.dll` is the one genuinely game-specific external** — an EA multiplayer service client
  (`connectMS` / `sendMSresults` / `getMSdatafile`), i.e. the match / mission-server API FA.EXE's
  networking calls into. If `MSAPI.dll` ships in the install it warrants its own boundary or
  reconstruction pass; the FA-side import surface is the six functions above.
- **DirectDraw** is a 1-function link (`DirectDrawCreate`); FA drives the surface through the
  returned COM interface, traced in [render-core.md](render-core.md) / [renderer.md](renderer.md).
- **WINMM** is FA.EXE's joystick source ([input.md](input.md)) and a timer source, and WAIL32's
  wave/timer backend — the same DLL crosses several subsystems.

## Related

- [wail32.md](wail32.md) · [comms.md](comms.md) — FA's companion binaries (documented as subsystems).
- [architecture.md](architecture.md) — the overall binary/overlay architecture.
- [network.md](network.md) — FA.EXE's multiplayer, which uses `MSAPI.dll` + DirectPlay.
