# SPDX-License-Identifier: MIT
#
# provision.ps1 - places the FA install and its registry footprint in the bench guest (#56).
# Called by Vagrant. Installs NO toolchain (this VM runs the game, it does not build). Idempotent:
# every step checks its own result first, so `vagrant provision` re-runs cheaply.
#
# LICENSING. The FA bits are John's own licensed install, uploaded from the host (FX_FA_SRC) - not
# redistributed and not committed. The guest is a Microsoft evaluation image (see README for the
# rearm/rebuild paths). Nothing here is fetched from the network.

[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$InstallDir = 'C:\JANES\Fighters Anthology'
$Staging    = 'C:\fa-staging'

# -- 1. Place the FA install --------------------------------------------------
# The install is copied to the canonical path the #551 footprint records, so the registry keys
# below point at real files and any path-probing code in the game resolves. Keyed off what is
# actually on the guest disk (the C:\fa-staging upload), NOT an env var: `vagrant provision` does
# not re-thread FX_FA_SRC, so gating on it would skip placement on any re-provision.
if (Test-Path (Join-Path $InstallDir 'FA.EXE')) {
    Write-Host "FA already present at $InstallDir - skipping copy."
} elseif (Test-Path (Join-Path $Staging 'FA.EXE')) {
    Write-Host "Placing FA install at $InstallDir ..."
    # Clear any stale non-directory sitting at the install path first: Copy-Item of a multi-item
    # wildcard into a MISSING leaf dir collapses the whole set into a single file with that name, so
    # a prior failed run can leave 'Fighters Anthology' as a file. Then create the FULL leaf dir
    # (not just its parent) so the staged tree copies *inside* it.
    if ((Test-Path -LiteralPath $InstallDir) -and -not (Get-Item -LiteralPath $InstallDir).PSIsContainer) {
        Remove-Item -LiteralPath $InstallDir -Force
    }
    New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
    # The uploaded folder lands as C:\fa-staging; copy its contents into the canonical path.
    Copy-Item -Path (Join-Path $Staging '*') -Destination $InstallDir -Recurse -Force
    Remove-Item -Path $Staging -Recurse -Force -ErrorAction SilentlyContinue
    Write-Host "FA placed. FA.EXE present: $(Test-Path (Join-Path $InstallDir 'FA.EXE'))"
} else {
    Write-Warning "No FA install found: neither '$InstallDir\FA.EXE' nor '$Staging\FA.EXE' exists."
    Write-Warning "Set FX_FA_SRC to your licensed install dir and run 'vagrant up' (which uploads it"
    Write-Warning "to C:\fa-staging), or copy FA into '$InstallDir' by hand (see README)."
}

# -- 2. Registry footprint (docs/fa/formats/ESA.md sec. Install Footprint, #551) -
# The retail installer writes exactly three keys. We reproduce them so the install looks native to
# anything that reads them; the game itself keeps runtime settings in EA.CFG, not the registry.
# On this 64-bit guest the vendor + Uninstall keys live under the WOW6432Node view (32-bit
# installer), and App Paths\FA.EXE was observed in the native view.
function Ensure-Key($path)  { if (-not (Test-Path $path)) { New-Item -Path $path -Force | Out-Null } }

$vendor    = 'HKLM:\SOFTWARE\WOW6432Node\Jane''s Combat Simulations\Fighters Anthology'
$uninstall = 'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\Fighters Anthology'
$apppaths  = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\FA.EXE'

Ensure-Key $vendor      # created empty - no values, no subkeys (as the installer leaves it)

Ensure-Key $uninstall
Set-ItemProperty -Path $uninstall -Name 'DisplayName'     -Value 'Fighters Anthology'
Set-ItemProperty -Path $uninstall -Name 'UninstallString' `
    -Value 'C:\WINDOWS\system32\EAREMOVE.EXE C:\WINDOWS\system32\EA1.UIL'

Ensure-Key $apppaths
Set-ItemProperty -Path $apppaths -Name '(default)' -Value (Join-Path $InstallDir 'FA.EXE')
Write-Host "Registry footprint written (3 keys)."

# -- 3. Audio device ----------------------------------------------------------
# The Vagrantfile gives the guest an emulated HDA card; Windows Server leaves the Audio service
# disabled. Start it so the game finds a device.
$svc = Get-Service -Name 'Audiosrv' -ErrorAction SilentlyContinue
if ($svc) {
    Set-Service -Name 'Audiosrv' -StartupType Automatic
    if ($svc.Status -ne 'Running') { Start-Service -Name 'Audiosrv' }
    Write-Host "Windows Audio service enabled."
}

# -- 4. Display for a 1998 title ----------------------------------------------
# FA wants an 8-bit (256-color) palettized mode for some screens. Nothing to force here - the game
# sets its own DirectDraw mode - but 16-bit desktop color avoids a few palette-manager quirks.
# (Left as a note; QXL honours the mode the game requests.)

# -- 5. Probe kit -------------------------------------------------------------
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

# -- 6. Render + display stack so FA actually runs on QXL (no 3D GPU) ---------
# CODIFIES THE MANUALLY-VERIFIED bring-up (2026-08-20). Unlike the FA/game content (never
# downloaded), this DOES fetch two guest-runtime tools from the network -- a dedicated FA bench
# cannot render without them. Each step is idempotent. NOTE: the QXL-driver and DEP changes need
# ONE reboot to take effect ('vagrant reload' after the first 'up'). Validate on the next fresh
# rebuild -- this block was assembled from steps proven by hand, not yet from a clean provision.
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

# 6a. SPICE guest tools: QXL display driver + spice-vdagent (seamless mouse, window resize, and a
# real display driver instead of the Basic adapter). Pair with the USB tablet in the Vagrantfile.
if (-not (Get-CimInstance Win32_VideoController | Where-Object { $_.Name -match 'QXL' })) {
    try {
        $sgt = Join-Path $env:TEMP 'spice-guest-tools.exe'
        Invoke-WebRequest 'https://www.spice-space.org/download/windows/spice-guest-tools/spice-guest-tools-latest.exe' -OutFile $sgt -UseBasicParsing
        Start-Process $sgt -ArgumentList '/S' -Wait
        Write-Host "SPICE guest tools installed (reboot required)."
    } catch { Write-Warning "SPICE guest tools install failed: $_" }
}
# Ensure the SPICE agent service actually runs (seamless/absolute mouse, resize); it can land
# installed-but-stopped or Manual after the driver reboot.
$vd = Get-Service vdservice -ErrorAction SilentlyContinue
if ($vd) {
    Set-Service vdservice -StartupType Automatic
    if ($vd.Status -ne 'Running') { Start-Service vdservice -ErrorAction SilentlyContinue }
    Write-Host "vdservice: $((Get-Service vdservice).Status)"
}

# 6b. Rendering: FA needs an 8-bit 640x480 DirectDraw mode QXL's WDDM driver cannot expose. The
# shipped dgVoodoo wrapper needs Direct3D 11 (QXL has none) and crashes; cnc-ddraw's GDI software
# renderer provides the mode with no GPU.
$ddraw = Join-Path $InstallDir 'ddraw.dll'
$isCnc = (Test-Path $ddraw) -and ((Get-Item $ddraw).VersionInfo.ProductName -match 'cnc-ddraw')
if ((Test-Path (Join-Path $InstallDir 'FA.EXE')) -and -not $isCnc) {
    if ((Test-Path $ddraw) -and ((Get-Item $ddraw).VersionInfo.ProductName -match 'dgVoodoo')) {
        Rename-Item $ddraw ($ddraw + '.dgvoodoo-off') -Force
        Write-Host "Disabled the shipped dgVoodoo ddraw.dll (needs a 3D GPU)."
    }
    try {
        $zip = Join-Path $env:TEMP 'cnc-ddraw.zip'; $tmp = Join-Path $env:TEMP 'cnc-ddraw'
        Invoke-WebRequest 'https://github.com/FunkyFr3sh/cnc-ddraw/releases/latest/download/cnc-ddraw.zip' -OutFile $zip -UseBasicParsing
        Expand-Archive $zip $tmp -Force
        Copy-Item (Join-Path $tmp 'ddraw.dll') $InstallDir -Force
        @"
[ddraw]
renderer=gdi
fullscreen=false
windowed=true
border=true
resizable=true
maintas=true
nonexclusive=true
adjmouse=true
singlecpu=true
savesettings=0
"@ | Set-Content (Join-Path $InstallDir 'ddraw.ini') -Encoding ASCII
        Write-Host "cnc-ddraw installed (GDI software renderer)."
    } catch { Write-Warning "cnc-ddraw install failed: $_" }
}

# 6c. DEP off: FA executes generated code (its SH interpreter / renderer). DEP kills it with a BEX
# crash the instant a mission starts (fault always ends ...3832, module 'unknown' = a data page).
# Per-app DisableNX compat shims did NOT stick; system-wide is the reliable switch.
if ((bcdedit /enum '{current}' | Select-String 'nx' | Out-String) -notmatch 'AlwaysOff') {
    bcdedit /set '{current}' nx AlwaysOff | Out-Null
    Write-Host "DEP disabled (nx AlwaysOff) -- reboot required."
}

# 6d. Game audio: the FA install ships DSOAL (a DirectSound->OpenAL wrapper) as dsound.dll +
# alsoft.ini. DSOAL needs OpenAL Soft (soft_oal.dll), which isn't present here, so FA's audio init
# fails silently -- the game is mute while plain Windows sounds (which bypass DSOAL) still play.
# Disable the wrapper so FA uses the OS DirectSound, which works with the emulated card. (If
# soft_oal.dll is ever dropped in alongside it, DSOAL can work and this leaves it alone.)
$dsoal = Join-Path $InstallDir 'dsound.dll'
if ((Test-Path $dsoal) -and -not (Test-Path (Join-Path $InstallDir 'soft_oal.dll'))) {
    Rename-Item $dsoal ($dsoal + '.dsoal-off') -Force
    Write-Host "Disabled the DSOAL dsound.dll wrapper (no OpenAL Soft present); FA uses OS DirectSound."
}

Write-Host ""
Write-Host "=== Bench guest ready. ==="
Write-Host "Open the console (host: tools/bench-vm/run-bench.sh console), launch Fighters"
Write-Host "Anthology, then run 'FA Bench' to start the live memory watcher. See the README for"
Write-Host "the probe workflow and the #29 / #142 checklist."
