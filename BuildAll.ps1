# Builds AssetCollector for multiple Illustrator SDK eras.
# Produces:
#   AssetCollector.aip       (compiled against the 2026 SDK headers)
#   AssetCollector2023.aip   (compiled against the 2023 SDK headers)
#
# Usage:
#   powershell -NoProfile -ExecutionPolicy Bypass -File BuildAll.ps1 [-Debug] [-Clean]

param(
    [switch]$Debug,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

$msbuild = "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe"
$projDir = "E:\Adobe Illustrator 2026 SDK\samplecode\AssetCollector"
$config = if ($Debug) { "Debug" } else { "Release" }

$projects = @(
    "$projDir\AssetCollector.vcxproj",
    "$projDir\AssetCollector2023.vcxproj"
)

foreach ($proj in $projects) {
    if ($Clean) {
        Write-Host "=== Cleaning $([IO.Path]::GetFileName($proj)) ($config|x64) ==="
        & $msbuild $proj /p:Configuration=$config /p:Platform=x64 /t:Clean /v:minimal /nologo
        if ($LASTEXITCODE -ne 0) { throw "Clean failed: $proj" }
    }
    Write-Host "=== Building $([IO.Path]::GetFileName($proj)) ($config|x64) ==="
    & $msbuild $proj /p:Configuration=$config /p:Platform=x64 /m /v:minimal /nologo
    if ($LASTEXITCODE -ne 0) { throw "Build failed: $proj" }
}

$outDir = "E:\Adobe Illustrator 2026 SDK\samplecode\output\win\x64\$config"
Write-Host ""
Write-Host "=== Outputs ==="
Get-ChildItem $outDir -Filter "*.aip" | ForEach-Object { Write-Host ("{0,-30} {1} bytes" -f $_.Name, $_.Length) }
Write-Host ""
Write-Host "Done."