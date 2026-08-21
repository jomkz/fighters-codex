# Bench VM — running Fighters Anthology for the gameplay-gated RE

A Vagrant/libvirt VM that runs the real, licensed Jane's Fighters Anthology on this Fedora host,
so the **gameplay-gated RE probes** ([#56](https://github.com/jomkz/fighters-codex/issues/56) /
[#29](https://github.com/jomkz/fighters-codex/issues/29) /
[#142](https://github.com/jomkz/fighters-codex/issues/142)) can be flown and observed here instead
of on the physical Windows bench.

**Why a VM beats the bench for these probes.** Everything #29 needs is a *differential observation*
— fly a thing, see which PLT gap bytes change. The VM adds two instruments the bench cannot:

- **Live memory watching.** [`plt-watch.ps1`](plt-watch.ps1) attaches to the running `FA.EXE` and
  polls `_campaignPilot` (`0x4F8BB8`) while you fly, printing a byte-level diff the instant a gap
  region changes. That tells you *when* and *under what action* a byte moves — not just what ends
  up in the saved `.P` file.
- **Snapshots.** `run-bench.sh snapshot` / `restore` give a perfectly repeatable pre-mission state,
  so a save-diff isolates exactly one variable (one mission, one medal, one fort assault).

It is a **correctness/observation environment, not a measurement one** — sized generously off the
host, software-rendered. Timings taken here are meaningless.

> Status: **validated end-to-end on the primary Fedora host (2026-08-20) — FA boots, renders, and
> flies in the VM.** The first `vagrant up` was the real test and surfaced a chain of issues, all
> fixed here: guest **audio device** (raw `hda-output` qemuargs aborted QEMU → libvirt
> `sound_type`), **provisioner encoding** (non-ASCII in the `.ps1` scripts broke PowerShell 5.1 over
> WinRM → ASCII only), **FA placement** (env-var gated → keyed off staged files) plus a **Copy-Item
> leaf-dir** collapse, the **SPICE console** (needs `--attach`, XWayland, and a USB tablet for a
> usable mouse/cursor), and the **render + DEP** stack (§ Rendering, input & sound). The guest boots,
> installs FA to `C:\JANES\Fighters Anthology`, and comes up game-ready — **FA renders, flies, and has
> sound** (validated on a clean `vagrant up`). Flying the probes is still a human console session.

## Host prerequisites (one-time)

Already satisfied on the primary Fedora box (verified 2026-08-20): KVM (`i9-12900K`, VT-x),
`qemu-kvm`, `virt-manager`/`virt-viewer`, `vagrant` **2.4.9** with the **vagrant-libvirt** plugin
(0.12.2), and `libvirtd`. If `libvirtd` is inactive: `sudo systemctl enable --now libvirtd`. The
user must be in the `libvirt` group.

## Guest OS: Server 2022 first, XP SP3 as the fallback

The default guest is the **Windows Server 2022 evaluation** box
(`peru/windows-server-2022-standard-x64-eval`) — the same one `windows-env` uses. It is
license-clean for this (180-day eval, rebuild to refresh), WinRM-driveable so the FA install +
registry step is fully scripted, and avoids the TPM/SecureBoot emulation a Windows 11 guest needs
under KVM. FA is a 1998 DirectDraw title, but it runs on modern Windows (the physical bench is
Windows 11), so software DirectDraw under QXL is expected to work.

### XP SP3 fallback

If the DirectDraw / 8-bit-palette path misbehaves under Server 2022, rebuild the guest on **Windows
XP SP3** — the era-correct target, where FA's video path is exactly what it was written for. XP is
the fallback rather than the default because it cannot be WinRM-automated (Vagrant cannot drive it),
so the install is hand-driven:

1. Create the VM in `virt-manager` from the XP SP3 ISO (`~/iso/…` — you have one on hand). Give it
   an IDE disk, an `ich9-intel-hda` or AC'97 sound device, and a QXL display.
2. Install XP, then copy the FA install into `C:\JANES\Fighters Anthology` (see *Staging* below).
3. Apply the three registry keys from [`provision.ps1`](provision.ps1) by hand (or import a `.reg`).
4. Drop [`plt-watch.ps1`](plt-watch.ps1), [`probe-savediff.ps1`](probe-savediff.ps1) and
   [`probes.psd1`](probes.psd1) into `C:\bench`. The watcher needs PowerShell 2.0+ (built into
   XP SP3 once WMF is installed) — if `Import-PowerShellDataFile` is missing on that PS version,
   read `probes.psd1` with `Invoke-Expression (Get-Content -Raw …)`.

The probe scripts themselves are guest-version-agnostic; only the *provisioning* differs.

## Staging the FA install

The guest gets FA from **your licensed install**, uploaded at `vagrant up` — nothing FA is
committed or downloaded. Point `FX_FA_SRC` at the install directory:

```
FX_FA_SRC="/run/media/john/Windows Disk/JANES/Fighters Anthology" \
  tools/bench-vm/run-bench.sh up
```

`provision.ps1` copies it to `C:\JANES\Fighters Anthology` (the path the #551 footprint records) and
writes the registry keys. The install on that Windows disk is already patched to **1.02F**, so no
disc install or RTPatch step is needed. (Alternatively, install from the disc ISOs in `~/iso/` and
run the 1.02 patch — but copying the ready install is faster and byte-for-byte known.)

## Rendering, input & sound

FA is a 1998 8-bit DirectDraw title; QXL's modern WDDM driver can't run it as shipped. `provision.ps1`
step 6 codifies the working setup (proven by hand 2026-08-20, validate on the next fresh rebuild):

- **Display driver + mouse.** The SPICE guest tools (QXL DoD driver + `spice-vdagent`) plus a USB
  tablet (in the `Vagrantfile`) give a real display driver, a seamless **absolute** mouse, and
  window resize. Without them the guest falls back to the Microsoft Basic Display Adapter with a
  grabbed pointer.
- **cnc-ddraw (GDI renderer).** FA needs an 8-bit 640×480 mode QXL cannot expose. The FA install
  ships **dgVoodoo** as `ddraw.dll`, which needs Direct3D 11 that QXL lacks and crashes on mission
  start; provisioning renames it `ddraw.dll.dgvoodoo-off` and drops in **cnc-ddraw**'s `ddraw.dll`
  with a `ddraw.ini` set to `renderer=gdi`, windowed. That pure-software path renders FA with no GPU.
- **DEP off.** FA executes generated code (its SH interpreter/renderer); DEP kills it with a `BEX`
  crash the instant a mission starts (fault offset always ends `…3832`, faulting module "unknown").
  `bcdedit /set nx AlwaysOff` (system-wide — per-app `DisableNX` shims did **not** stick). Needs one
  reboot to take effect.
- **Console.** `run-bench.sh console` runs `virt-viewer --attach` under `GDK_BACKEND=x11` (XWayland),
  so a fractional-scaled 4K desktop doesn't blow the cursor up to 141×141. `Shift+F12` releases the
  mouse; `Shift+F11` toggles fullscreen; inside cnc-ddraw's window, `Tab` releases the cursor and
  `Enter` toggles its own fullscreen.
- **Sound.** Works over the `--attach` connection — spice-gtk carries the SPICE audio channel to the
  host (GStreamer → PipeWire). The one catch is FA-specific: the install ships **DSOAL** (a
  DirectSound→OpenAL wrapper) as `dsound.dll` + `alsoft.ini`, but with no OpenAL Soft (`soft_oal.dll`)
  present, FA's audio init fails silently while plain Windows sounds still play. `provision.ps1`
  disables the DSOAL wrapper so FA uses the OS DirectSound. (Drop `soft_oal.dll` in beside it if you
  want DSOAL's EAX/3D-positional audio back.)

## The workflow

```
# 1. bring the guest up (first run downloads the box + provisions; expect a while)
FX_FA_SRC="…/Fighters Anthology" tools/bench-vm/run-bench.sh up

# 2. open the game screen
tools/bench-vm/run-bench.sh console

# 3. (optional) take a clean base for repeatable save-diffs
tools/bench-vm/run-bench.sh snapshot pre-mission

# 4. in the guest: launch "Fighters Anthology", then run "FA Bench" (starts plt-watch.ps1)

# 5. fly the probe. The watcher prints every gap-region change live; for a file-level
#    before/after instead, use probe-savediff.ps1 in the guest.

# 6. pull the watch log back to the host
tools/bench-vm/run-bench.sh fetch-log
```

### Live memory (`plt-watch.ps1`)

Attaches to `FA.EXE` and watches the regions in [`probes.psd1`](probes.psd1) — the four #29 gaps,
the medal band, and the service record — addressed by absolute VA. FA.EXE is a non-relocatable 1998
PE (`ImageBase 0x400000`, no reloc table), so `_campaignPilot`'s VA `0x4F8BB8` is valid directly;
the script still reads the live module base and rebases defensively. `_campaignPilot` maps 1:1 to
the `.P` file image, so file offset *N* in [`P.md`](../../docs/fa/formats/P.md) is VA `0x4F8BB8 + N`.

For a host-only path that never touches the guest, bring the VM up with `FX_BENCH_GDB=1` and QEMU
exposes a gdbstub on `localhost:1234`; a host debugger can read guest memory there. The in-guest
watcher is the primary tool because it already has the process's virtual address space — the gdbstub
path has to walk the guest page tables from `CR3` to translate a VA, which is only worth it if the
in-guest route is ever blocked.

### Differential saves (`probe-savediff.ps1`)

`-Snapshot before` → fly → `-Snapshot after` → `-Diff` reports which gap-region bytes changed in
which `PLTnnn.P`. Simpler than the live watcher when you only care about the persisted result.

## The probe checklist (what to fly)

From [#29](https://github.com/jomkz/fighters-codex/issues/29) and
[#142](https://github.com/jomkz/fighters-codex/issues/142):

| Probe | Region | Action to fly | Expected writer |
|---|---|---|---|
| **control** | `0x1F80`–`0x2017` | any completed mission | `_EndOfMissionStats` flush (`FUN_00485380`) — **must move** |
| gap 1 | `0xB0`–`0xC1` | advance rank / score | rank-index or score-tier write (unknown) |
| gap 2 | `0xCF`–`0x571` + `0x57D`–`0x5AE` | fly 3–5 missions | mission-log growth |
| **medal band** | `0x572`–`0x57C` | **earn a medal → exactly one band byte must flip** | `_AwardMedal` (`0x467110`) via the per-campaign `*Medals` pass |
| gap 3 | `0x2018`–`0x20B7` | fly with kills | `_EndOfMissionStats` flush, if written at all |
| gap 4 | `0x21F8`–`0x25DF` | complete a **fort-assault** mission | `_EndOfFortMissionStats` (`0x485040`) |
| HUD `+0x238` | HUD struct | (see #142) | needs the HUD struct base VA from `db/symbols` — add it to `probes.psd1` |
| HUD flag bit 14 | HUD flags word | the single-player state transition that sets it | — |
| CB8 palette-half | — | the CB8 screen that exercises it | — |
| wingman `wm_control` | — | the formation-tightness range | — |

**Read the control row first — it is a measurement-integrity guard, not a probe to solve.** The
`0x1F80`–`0x2017` block (confirmed mission/kill counters, [P.md](../../docs/fa/formats/P.md) §
Stats counters) is written by the same end-of-mission flush that would populate the upper gaps. If
you complete a mission and the **control block does not change**, the watcher is not attached, the
save did not commit, or the module rebased — so a "gap 3/4 did not change" reading is meaningless.
Trust a negative on the gaps only when the control moved in the same run.

Static RE already narrows what to expect: `_ConvertPilotFiles` (`0x485AE0`) zero-fills the whole
`0x25E0` record and migrates legacy fields only through `+0x2010`, so gaps 3 and 4 are **runtime-only**
— zero until gameplay writes them (see [P.md](../../docs/fa/formats/P.md) gaps 3–4 and
[game-compatibility.md](../../docs/fa/game-compatibility.md)). That is why the fresh-save baseline
reads all-zero there, and why the fort-assault path is the specific trigger for gap 4.

The medal-band probe is the cheapest win: the codec already decodes and edits that band
([#567](https://github.com/jomkz/fighters-codex/pull/567)), so confirming the live write closes the
loop between the static map and the running game. Feed everything else into the P/PLT codec
completion ([#143](https://github.com/jomkz/fighters-codex/issues/143)).

## Files

- `Vagrantfile` — the guest definition (libvirt, Windows Server 2022, QXL/SPICE, emulated audio,
  optional `FX_BENCH_GDB` gdbstub).
- `provision.ps1` — places the FA install, writes the #551 registry footprint, enables audio, drops
  the probe shortcuts. Idempotent.
- `plt-watch.ps1` — the live `_campaignPilot` memory watcher.
- `probe-savediff.ps1` — the file-level before/after gap differ.
- `probes.psd1` — the watch table (VAs + lengths + notes); fill the #142 HUD VAs before that campaign.
- `run-bench.sh` — host entry point: up / console / snapshot / restore / fetch-log / halt.

## Licensing

The guest is a Microsoft evaluation image; the FA bits are John's own licensed install, uploaded
from the host and never redistributed or committed. Both are used, not shipped.
