#Requires -Version 5.1
<#
.SYNOPSIS
    Prepares a release: bumps the version in CMakeLists.txt, rotates CHANGELOG.md,
    commits both files, and creates the release tag.

.PARAMETER Version
    The new version in X.Y.Z format (e.g. 0.2.0).

.EXAMPLE
    .\scripts\release.ps1 0.2.0
#>
param(
    [Parameter(Mandatory)]
    [string]$Version
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Validate semver
if ($Version -notmatch '^\d+\.\d+\.\d+$') {
    Write-Error "Version must be in X.Y.Z format (e.g. 0.2.0). Got: '$Version'"
    exit 1
}

$tag     = "v$Version"
$repoUrl = 'https://github.com/jomkz/fighters-toolkit'
$root    = Split-Path $PSScriptRoot -Parent
$cmake   = Join-Path $root 'CMakeLists.txt'
$chlog   = Join-Path $root 'CHANGELOG.md'

# Confirm working tree is clean (excluding untracked files)
$dirty = git -C $root status --porcelain --untracked-files=no
if ($dirty) {
    Write-Error "Working tree has uncommitted changes. Commit or stash them before releasing."
    exit 1
}

# Confirm tag doesn't already exist
if (git -C $root tag --list $tag) {
    Write-Error "Tag '$tag' already exists."
    exit 1
}

$today = (Get-Date).ToString('yyyy-MM-dd')

# --- CMakeLists.txt ---
$cmakeContent = Get-Content $cmake -Raw
$newCmake = $cmakeContent -replace 'VERSION \d+\.\d+\.\d+', "VERSION $Version"
if ($newCmake -eq $cmakeContent) {
    Write-Error "Could not find 'VERSION x.y.z' pattern in CMakeLists.txt"
    exit 1
}
Set-Content $cmake $newCmake -NoNewline -Encoding UTF8

# --- CHANGELOG.md ---
$chlogContent = Get-Content $chlog -Raw

# Rotate [Unreleased] -> versioned section
$newChlog = $chlogContent -replace '## \[Unreleased\]', "## [Unreleased]`n`n## [$Version] - $today"

# Update [Unreleased] comparison link
$newChlog = $newChlog -replace '\[Unreleased\]: .+', "[Unreleased]: $repoUrl/compare/$tag...HEAD"

# Insert versioned release link before the first existing version link (the line starting [x.y.z]:)
$newChlog = $newChlog -replace '(?m)^(\[\d+\.\d+\.\d+\]:)', "[$Version]: $repoUrl/releases/tag/$tag`n`$1"

if ($newChlog -eq $chlogContent) {
    Write-Error "CHANGELOG.md does not contain an '## [Unreleased]' section."
    exit 1
}
Set-Content $chlog $newChlog -NoNewline -Encoding UTF8

# --- Commit and tag ---
git -C $root add $cmake $chlog
git -C $root commit -m "chore: release $tag"
git -C $root tag $tag

Write-Host ""
Write-Host "Release $tag prepared. Review the commit, then push:"
Write-Host "  git push origin main --tags"
