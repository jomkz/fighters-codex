# SPDX-License-Identifier: MIT
#
# probe-savediff.ps1 - the classic differential-save probe for the PLT gap regions (#29), for when
# the live watcher is more than you need: snapshot every PLTnnn.P file, do a thing in-game (fly a
# mission, earn a medal, complete a fort assault), snapshot again, and report which gap-region
# bytes changed in which pilot file.
#
#   # before flying:
#   powershell -File C:\bench\probe-savediff.ps1 -Snapshot before
#   # ... fly, save, exit to the pilot screen ...
#   powershell -File C:\bench\probe-savediff.ps1 -Snapshot after
#   powershell -File C:\bench\probe-savediff.ps1 -Diff before after
#
# Gap regions are addressed by .P FILE OFFSET (not VA) - this reads files, not memory. Offsets are
# the same ones docs/fa/formats/P.md and the plt codec use.

[CmdletBinding()]
param(
    [ValidateSet('before','after')] [string] $Snapshot,
    [switch] $Diff,
    [string] $A = 'before',
    [string] $B = 'after',
    [string] $FaDir   = 'C:\JANES\Fighters Anthology',
    [string] $SnapDir = 'C:\bench\snapshots'
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# .P file-offset gap regions (P.md). The medal band 0x572-0x57C is mapped but still worth watching:
# a medal award must flip exactly one byte here (the static-read validation).
$Regions = @(
    @{ Name = 'gap1-rank-score';    Off = 0x00B0; Len = 0x12  }
    @{ Name = 'gap2-log-a';         Off = 0x00CF; Len = 0x4A3 }
    @{ Name = 'medal-band';         Off = 0x0572; Len = 0x0B  }
    @{ Name = 'gap2-log-b';         Off = 0x057D; Len = 0x32  }
    @{ Name = 'service-record';     Off = 0x05AF; Len = 0x1D0 }
    # CONTROL (not a gap): confirmed mission/kill counters. A completed mission must change this;
    # if it is flat between snapshots, the in-game action did not persist and a "no gap change"
    # result is meaningless - re-fly before trusting it.
    @{ Name = 'control-stats-block';Off = 0x1F80; Len = 0x98  }
    @{ Name = 'gap3-kill-subcats';  Off = 0x2018; Len = 0xA0  }
    @{ Name = 'gap4-fort-stats';    Off = 0x21F8; Len = 0x3E8 }
)

function Snap([string]$tag) {
    $dest = Join-Path $SnapDir $tag
    New-Item -ItemType Directory -Force -Path $dest | Out-Null
    $files = Get-ChildItem -Path $FaDir -Filter 'PLT*.P' -File
    if (-not $files) { $files = Get-ChildItem -Path $FaDir -Filter '*.P' -File }
    foreach ($f in $files) { Copy-Item $f.FullName (Join-Path $dest $f.Name) -Force }
    Write-Host ("snapshot '{0}': {1} pilot file(s) -> {2}" -f $tag, $files.Count, $dest)
}

function DiffSnaps([string]$aTag, [string]$bTag) {
    $aDir = Join-Path $SnapDir $aTag
    $bDir = Join-Path $SnapDir $bTag
    if (-not (Test-Path $aDir)) { throw "no snapshot '$aTag' (run -Snapshot $aTag first)" }
    if (-not (Test-Path $bDir)) { throw "no snapshot '$bTag' (run -Snapshot $bTag first)" }

    $any = $false
    foreach ($bf in Get-ChildItem -Path $bDir -Filter '*.P' -File) {
        $af = Join-Path $aDir $bf.Name
        if (-not (Test-Path $af)) { Write-Host "NEW pilot file: $($bf.Name)"; $any = $true; continue }
        $a = [IO.File]::ReadAllBytes($af)
        $b = [IO.File]::ReadAllBytes($bf.FullName)
        foreach ($r in $Regions) {
            $end = [Math]::Min($r.Off + $r.Len, [Math]::Min($a.Length, $b.Length))
            $changes = @()
            for ($i = $r.Off; $i -lt $end -and $changes.Count -lt 24; $i++) {
                if ($a[$i] -ne $b[$i]) { $changes += ('+0x{0:X4} {1:X2}->{2:X2}' -f $i, $a[$i], $b[$i]) }
            }
            if ($changes.Count) {
                $any = $true
                Write-Host ("{0}  [{1}]  {2}" -f $bf.Name, $r.Name, ($changes -join '  '))
            }
        }
    }
    if (-not $any) { Write-Host "no gap-region changes between '$aTag' and '$bTag'." }
}

if ($Snapshot) { Snap $Snapshot }
elseif ($Diff) { DiffSnaps $A $B }
else { Write-Host "usage: -Snapshot before|after   |   -Diff -A before -B after" }
