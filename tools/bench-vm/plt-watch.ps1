# SPDX-License-Identifier: MIT
#
# plt-watch.ps1 - live memory watcher for the FA gameplay-gated RE probes (#56).
#
# Polls FA.EXE's `_campaignPilot` gap regions (and any other VAs in probes.psd1) via
# ReadProcessMemory while the game runs, and prints a timestamped line every time a watched region
# changes - with a byte-level diff. This is what the physical bench cannot do: it sees WHEN a gap
# byte changes and under what in-game action, not just what ends up in the saved .P file.
#
# Run it in the guest console AFTER launching Fighters Anthology (the "FA Bench" desktop shortcut
# does exactly this). It attaches to the running FA.EXE, so start the game first.
#
#   powershell -ExecutionPolicy Bypass -File C:\bench\plt-watch.ps1
#   ... -IntervalMs 250 -LogPath C:\bench\watch.log
#
# The probe table lives in probes.psd1 next to this script. FA.EXE is a non-relocatable 1998 PE
# (ImageBase 0x400000), so the VAs there are valid directly; this script still reads the live
# module base and rebases, so a future OS loading FA elsewhere would not silently read garbage.

[CmdletBinding()]
param(
    [int]    $IntervalMs = 250,
    [string] $ProbeFile  = (Join-Path $PSScriptRoot 'probes.psd1'),
    [string] $LogPath    = (Join-Path $PSScriptRoot 'watch.log'),
    [string] $ProcName   = 'FA'
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# --- Win32 memory access via P/Invoke ---------------------------------------------------------
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class Mem {
    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern IntPtr OpenProcess(int access, bool inherit, int pid);
    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern bool ReadProcessMemory(IntPtr h, IntPtr addr, byte[] buf, int size, out int read);
    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern bool CloseHandle(IntPtr h);
}
'@
$PROCESS_VM_READ = 0x0010
$PROCESS_QUERY_INFORMATION = 0x0400

function Get-FaProcess {
    Get-Process -Name $ProcName -ErrorAction SilentlyContinue | Select-Object -First 1
}

function Read-Region([IntPtr]$h, [long]$va, [int]$len) {
    $buf = New-Object byte[] $len
    $read = 0
    if ([Mem]::ReadProcessMemory($h, [IntPtr]$va, $buf, $len, [ref]$read) -and $read -eq $len) {
        return $buf
    }
    return $null
}

function Format-Diff([byte[]]$old, [byte[]]$new, [long]$baseVa, [int]$maxBytes = 24) {
    $changes = @()
    for ($i = 0; $i -lt $new.Length -and $changes.Count -lt $maxBytes; $i++) {
        if ($null -eq $old -or $old[$i] -ne $new[$i]) {
            $ov = if ($old) { '{0:X2}' -f $old[$i] } else { '??' }
            $changes += ('+0x{0:X3} {1}->{2:X2}' -f $i, $ov, $new[$i])
        }
    }
    $suffix = if ($new.Length -gt $maxBytes -and $changes.Count -ge $maxBytes) { ' ...' } else { '' }
    return ($changes -join '  ') + $suffix
}

# --- load the probe table ---------------------------------------------------------------------
if (-not (Test-Path $ProbeFile)) { throw "probe table not found: $ProbeFile" }
$probes = (Import-PowerShellDataFile $ProbeFile).Watch |
          Where-Object { $_.VA -ne 0 }   # skip #142 placeholders whose VA is not filled in yet
if (-not $probes) { throw "no probes with a non-zero VA in $ProbeFile" }

function Log($msg) {
    $line = ('{0:HH:mm:ss.fff}  {1}' -f (Get-Date), $msg)
    Write-Host $line
    Add-Content -Path $LogPath -Value $line
}

Log ("watching {0} region(s); interval {1} ms; log {2}" -f $probes.Count, $IntervalMs, $LogPath)
Log  "waiting for FA.EXE ..."

$handle = [IntPtr]::Zero
$rebase = 0
$state  = @{}   # probe name -> last bytes

while ($true) {
    $proc = Get-FaProcess
    if (-not $proc) {
        if ($handle -ne [IntPtr]::Zero) { [void][Mem]::CloseHandle($handle); $handle = [IntPtr]::Zero; $state.Clear(); Log "FA.EXE exited - waiting for relaunch ..." }
        Start-Sleep -Milliseconds 500
        continue
    }
    if ($handle -eq [IntPtr]::Zero) {
        $handle = [Mem]::OpenProcess(($PROCESS_VM_READ -bor $PROCESS_QUERY_INFORMATION), $false, $proc.Id)
        if ($handle -eq [IntPtr]::Zero) { Log "OpenProcess failed (run as Administrator) - retrying"; Start-Sleep 1; continue }
        # FA.EXE is non-relocatable, so it loads at 0x400000 and rebase is 0. Confirm off the live
        # module base when we can - but reading a 32-bit process's MainModule from 64-bit
        # PowerShell can throw, so treat 0x400000 as the trusted default and only refine on success.
        $rebase = 0
        try {
            $modBase = $proc.MainModule.BaseAddress.ToInt64()
            $rebase  = $modBase - 0x400000
            Log ("attached to FA.EXE pid {0}; module base 0x{1:X8}{2}" -f $proc.Id, $modBase,
                 ($(if ($rebase -ne 0) { ' (rebased {0:+0;-0} bytes)' -f $rebase } else { '' })))
        } catch {
            Log ("attached to FA.EXE pid {0}; assuming ImageBase 0x400000 (MainModule unreadable: {1})" -f $proc.Id, $_.Exception.Message)
        }
    }

    foreach ($p in $probes) {
        $bytes = Read-Region $handle ($p.VA + $rebase) $p.Len
        if ($null -eq $bytes) { continue }   # region not paged in yet (no campaign loaded)
        $prev = $state[$p.Name]
        $changed = $false
        if ($null -eq $prev) { $changed = $true }
        else { for ($i = 0; $i -lt $bytes.Length; $i++) { if ($bytes[$i] -ne $prev[$i]) { $changed = $true; break } } }
        if ($changed) {
            if ($null -eq $prev) {
                $nz = ($bytes | Where-Object { $_ -ne 0 }).Count
                Log ("[{0}] first read - {1}/{2} non-zero  ({3})" -f $p.Name, $nz, $p.Len, $p.Note)
            } else {
                Log ("[{0}] {1}   ({2})" -f $p.Name, (Format-Diff $prev $bytes $p.VA), $p.Note)
            }
            $state[$p.Name] = $bytes
        }
    }
    Start-Sleep -Milliseconds $IntervalMs
}
