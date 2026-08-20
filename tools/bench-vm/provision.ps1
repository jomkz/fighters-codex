# SPDX-License-Identifier: MIT
#
# provision.ps1 — places the FA install and its registry footprint in the bench guest (#56).
# Called by Vagrant. Installs NO toolchain (this VM runs the game, it does not build). Idempotent:
# every step checks its own result first, so `vagrant provision` re-runs cheaply.
#
# LICENSING. The FA bits are John's own licensed install, uploaded from the host (FX_FA_SRC) — not
# redistributed and not committed. The guest is a Microsoft evaluation image (see README for the
# rearm/rebuild paths). Nothing here is fetched from the network.

[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$InstallDir = 'C:\JANES\Fighters Anthology'
$Staging    = 'C:\fa-staging'

# ── 1. Place the FA install ──────────────────────────────────────────────────
# The install is copied to the canonical path the #551 footprint records, so the registry keys
# below point at real files and any path-probing code in the game resolves.
if ($env:FX_FA_STAGED -ne "1") {
    Write-Warning "FX_FA_SRC was not set at 'vagrant up': no FA install was uploaded."
    Write-Warning "Set it to your licensed install dir and re-run 'vagrant provision', or copy FA"
    Write-Warning "into '$InstallDir' by hand (see README § 'Staging the FA install')."
} elseif (-not (Test-Path (Join-Path $InstallDir 'FA.EXE'))) {
    Write-Host "Placing FA install at $InstallDir ..."
    New-Item -ItemType Directory -Force -Path (Split-Path $InstallDir) | Out-Null
    # The uploaded folder lands as C:\fa-staging; move its contents into the canonical path.
    Copy-Item -Path (Join-Path $Staging '*') -Destination $InstallDir -Recurse -Force
    Remove-Item -Path $Staging -Recurse -Force -ErrorAction SilentlyContinue
    Write-Host "FA placed. FA.EXE present: $(Test-Path (Join-Path $InstallDir 'FA.EXE'))"
} else {
    Write-Host "FA already present at $InstallDir — skipping copy."
}

# ── 2. Registry footprint (docs/fa/formats/ESA.md § Install Footprint, #551) ─
# The retail installer writes exactly three keys. We reproduce them so the install looks native to
# anything that reads them; the game itself keeps runtime settings in EA.CFG, not the registry.
# On this 64-bit guest the vendor + Uninstall keys live under the WOW6432Node view (32-bit
# installer), and App Paths\FA.EXE was observed in the native view.
function Ensure-Key($path)  { if (-not (Test-Path $path)) { New-Item -Path $path -Force | Out-Null } }

$vendor    = 'HKLM:\SOFTWARE\WOW6432Node\Jane''s Combat Simulations\Fighters Anthology'
$uninstall = 'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\Fighters Anthology'
$apppaths  = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\FA.EXE'

Ensure-Key $vendor      # created empty — no values, no subkeys (as the installer leaves it)

Ensure-Key $uninstall
Set-ItemProperty -Path $uninstall -Name 'DisplayName'     -Value 'Fighters Anthology'
Set-ItemProperty -Path $uninstall -Name 'UninstallString' `
    -Value 'C:\WINDOWS\system32\EAREMOVE.EXE C:\WINDOWS\system32\EA1.UIL'

Ensure-Key $apppaths
Set-ItemProperty -Path $apppaths -Name '(default)' -Value (Join-Path $InstallDir 'FA.EXE')
Write-Host "Registry footprint written (3 keys)."

# ── 3. Audio device ──────────────────────────────────────────────────────────
# The Vagrantfile gives the guest an emulated HDA card; Windows Server leaves the Audio service
# disabled. Start it so the game finds a device.
$svc = Get-Service -Name 'Audiosrv' -ErrorAction SilentlyContinue
if ($svc) {
    Set-Service -Name 'Audiosrv' -StartupType Automatic
    if ($svc.Status -ne 'Running') { Start-Service -Name 'Audiosrv' }
    Write-Host "Windows Audio service enabled."
}

# ── 4. Display for a 1998 title ──────────────────────────────────────────────
# FA wants an 8-bit (256-color) palettized mode for some screens. Nothing to force here — the game
# sets its own DirectDraw mode — but 16-bit desktop color avoids a few palette-manager quirks.
# (Left as a note; QXL honours the mode the game requests.)

# ── 5. Probe kit ─────────────────────────────────────────────────────────────
# plt-watch.ps1 / probe-savediff.ps1 / probes.psd1 were uploaded to C:\bench. Drop a Start-menu
# shortcut so they are one click away in the console.
$desktop = [Environment]::GetFolderPath('CommonDesktopDirectory')
$sc = (New-Object -ComObject WScript.Shell).CreateShortcut((Join-Path $desktop 'FA Bench.lnk'))
$sc.TargetPath = 'powershell.exe'
$sc.Arguments  = '-NoExit -ExecutionPolicy Bypass -File C:\bench\plt-watch.ps1'
$sc.WorkingDirectory = 'C:\bench'
$sc.Save()

# A desktop shortcut to launch the game itself.
if (Test-Path (Join-Path $InstallDir 'FA.EXE')) {
    $fa = (New-Object -ComObject WScript.Shell).CreateShortcut((Join-Path $desktop 'Fighters Anthology.lnk'))
    $fa.TargetPath = (Join-Path $InstallDir 'FA.EXE')
    $fa.WorkingDirectory = $InstallDir
    $fa.Save()
}

Write-Host ""
Write-Host "=== Bench guest ready. ==="
Write-Host "Open the console (host: tools/bench-vm/run-bench.sh console), launch Fighters"
Write-Host "Anthology, then run 'FA Bench' to start the live memory watcher. See the README for"
Write-Host "the probe workflow and the #29 / #142 checklist."
