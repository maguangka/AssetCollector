# Generates the plugin.pipl registration resource for Illustrator native plugins.
# Replicates the behavior of the SDK's tools/pipl/create_pipl.py without requiring Python.
#
# Usage:
#   powershell -NoProfile -ExecutionPolicy Bypass -File create_pipl.ps1 -OutputDir "C:\path\to\project"
#
# The generated file is written to <OutputDir>\plugin.pipl (must sit next to the .rc file).

param(
    [Parameter(Mandatory = $true)][string]$OutputDir,
    [string]$PluginName = "FontCollector",
    [string]$EntryPoint = "PluginMain",
    [string]$OutputName = "plugin.pipl"
)

$ErrorActionPreference = "Stop"

$OutputDir = $OutputDir.TrimEnd('"', '\')

if (-not (Test-Path -LiteralPath $OutputDir)) {
    throw "Output directory does not exist: $OutputDir"
}

$bytes = New-Object System.Collections.Generic.List[byte]

function Write-BE32([int]$value) {
    for ($i = 3; $i -ge 0; $i--) {
        $bytes.Add((($value -shr ($i * 8)) -band 0xFF))
    }
}

# Writes a string padded to a multiple of 4 bytes; returns the padded byte count.
function Write-PaddedString([string]$value) {
    $raw = [System.Text.Encoding]::ASCII.GetBytes($value)
    $paddedLen = (($raw.Length + 3) -band -bnot 3)
    $bytes.AddRange($raw)
    for ($i = $raw.Length; $i -lt $paddedLen; $i++) {
        $bytes.Add(0)
    }
    return $paddedLen
}

# Writes one pipl property: vendor(4) + key(4) + id(4) + length(4) + value(padded to 4).
# Field order replicates tools/pipl/pipl_gen.py: length field precedes the value.
function Write-Property([string]$key, [string]$value) {
    $bytes.AddRange([System.Text.Encoding]::ASCII.GetBytes("ADBE"))
    $bytes.AddRange([System.Text.Encoding]::ASCII.GetBytes($key))
    Write-BE32 0
    Write-BE32 (($value.Length + 3) -band -bnot 3)
    $null = Write-PaddedString $value
}
Write-BE32 1

# --- plugin block: version(4) + property count(4) + properties ---------------
Write-BE32 0          # pipl version
Write-BE32 4          # property count (kind, ivrs, wx86, pinm)

Write-Property "kind" "SPEA"
Write-Property "ivrs" ("`0`0`0" + [char]0x02)   # integer value 2 as 4-byte big-endian
Write-Property "wx86" $EntryPoint
Write-Property "pinm" $PluginName

$outPath = Join-Path $OutputDir $OutputName
[System.IO.File]::WriteAllBytes($outPath, $bytes.ToArray())
Write-Output "Generated $outPath ($($bytes.Count) bytes)"