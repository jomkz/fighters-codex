#Requires -Version 5.1
<#
.SYNOPSIS
    Drafts CHANGELOG.md entries under [Unreleased] from conventional commits
    since the last git tag.

.PARAMETER Since
    Override the base tag/ref to compare from (default: last tag via git describe).

.EXAMPLE
    .\scripts\draft-changelog.ps1
    .\scripts\draft-changelog.ps1 -Since v0.1.0
#>
param(
    [string]$Since = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$root  = Split-Path $PSScriptRoot -Parent
$chlog = Join-Path $root 'CHANGELOG.md'

# --- Resolve base ref ---
if (-not $Since) {
    try {
        $Since = (git -C $root describe --tags --abbrev=0 2>$null).Trim()
    } catch {}
}
if (-not $Since) {
    Write-Error "No tags found and -Since not provided. Cannot determine commit range."
    exit 1
}

Write-Host "Drafting changelog entries since $Since ..."

# --- Collect commits ---
$subjects = git -C $root log "$Since..HEAD" --pretty=format:"%s" 2>$null
if (-not $subjects) {
    Write-Host "No commits found since $Since. Nothing to draft."
    exit 0
}

# --- Type → section and omit lists ---
$sectionMap = @{
    'feat'     = 'Added'
    'fix'      = 'Fixed'
    'docs'     = 'Changed'
    'refactor' = 'Changed'
    'perf'     = 'Changed'
}
$omitTypes = @('chore','ci','build','test','style','revert')

# Parsed entries keyed by section
$entries = @{ Added = [System.Collections.Generic.List[string]]::new()
              Fixed = [System.Collections.Generic.List[string]]::new()
              Changed = [System.Collections.Generic.List[string]]::new()
              Breaking = [System.Collections.Generic.List[string]]::new() }

$pattern = '^(?<type>[a-z]+)(\((?<scope>[^)]+)\))?(?<breaking>!)?:\s+(?<desc>.+)$'

foreach ($subject in ($subjects -split "`n")) {
    $subject = $subject.Trim()
    if (-not $subject) { continue }

    if ($subject -match $pattern) {
        $type     = $Matches['type']
        $scope    = $Matches['scope']
        $breaking = $Matches['breaking']
        $desc     = $Matches['desc']

        if ($omitTypes -contains $type) { continue }

        $qualifier = if ($scope) { "**$scope** " } else { '' }
        $line = "- $qualifier$desc"

        if ($breaking) {
            $entries['Breaking'].Add("- **BREAKING** $qualifier$desc")
        } elseif ($sectionMap.ContainsKey($type)) {
            $entries[$sectionMap[$type]].Add($line)
        } else {
            # Unknown types default to Added
            $entries['Added'].Add($line)
        }
    }
    # Non-conventional commits are silently skipped
}

$totalNew = ($entries.Values | ForEach-Object { $_.Count } | Measure-Object -Sum).Sum
if ($totalNew -eq 0) {
    Write-Host "No conventional commits found since $Since. Nothing to draft."
    Write-Host "(Non-conventional commits are skipped — see docs/development.md#commit-messages)"
    exit 0
}

# --- Load and update CHANGELOG.md ---
$content = Get-Content $chlog -Raw

# Find the [Unreleased] block: from heading to next ## [ heading
$blockPattern = '(?s)(## \[Unreleased\]\s*)(.*?)(?=\r?\n## \[|\z)'
$blockMatch   = [regex]::Match($content, $blockPattern)
if (-not $blockMatch.Success) {
    Write-Error "Could not find '## [Unreleased]' section in CHANGELOG.md"
    exit 1
}

$heading      = $blockMatch.Groups[1].Value
$existingBody = $blockMatch.Groups[2].Value

# Helper: insert entries into a section block, skipping duplicates
function Merge-Section {
    param([string]$body, [string]$section, [System.Collections.Generic.List[string]]$newEntries)

    if ($newEntries.Count -eq 0) { return $body }

    $added   = 0
    $skipped = 0
    $toAdd   = [System.Collections.Generic.List[string]]::new()

    foreach ($entry in $newEntries) {
        # Idempotency: skip if the description text is already present
        $descText = $entry -replace '^- (\*\*[^*]+\*\* )?(\*\*BREAKING\*\* (\*\*[^*]+\*\* )?)?', ''
        if ($body -like "*$descText*") {
            $skipped++
        } else {
            $toAdd.Add($entry)
            $added++
        }
    }

    if ($toAdd.Count -eq 0) { return $body }

    $sectionHeader = "### $section"
    if ($body -match "(?m)^### $section") {
        # Append under existing heading
        $body = $body -replace "(?m)(^### $section\s*)", "`$1$($toAdd -join "`n")`n"
    } else {
        # Prepend new heading + entries
        $newBlock = "$sectionHeader`n$($toAdd -join "`n")`n"
        $body = "$newBlock`n$body"
    }

    return $body
}

$newBody = $existingBody

# Breaking changes go first
$newBody = Merge-Section $newBody 'Changed' $entries['Breaking']
$newBody = Merge-Section $newBody 'Added'   $entries['Added']
$newBody = Merge-Section $newBody 'Fixed'   $entries['Fixed']
$newBody = Merge-Section $newBody 'Changed' $entries['Changed']

$newBlock   = "$heading$newBody"
$oldBlock   = $blockMatch.Value
$newContent = $content.Replace($oldBlock, $newBlock)

Set-Content $chlog $newContent -NoNewline -Encoding UTF8

# --- Summary ---
Write-Host ""
Write-Host "Done. Entries drafted into [Unreleased]:"
if ($entries['Breaking'].Count) { Write-Host "  ### Changed (breaking): $($entries['Breaking'].Count)" }
if ($entries['Added'].Count)    { Write-Host "  ### Added:   $($entries['Added'].Count)" }
if ($entries['Fixed'].Count)    { Write-Host "  ### Fixed:   $($entries['Fixed'].Count)" }
if ($entries['Changed'].Count)  { Write-Host "  ### Changed: $($entries['Changed'].Count)" }
Write-Host ""
Write-Host "Review CHANGELOG.md, edit as needed, then commit before releasing."
