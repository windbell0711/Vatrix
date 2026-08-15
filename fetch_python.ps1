# vx: fetch the official NuGet python 3.12 package (DLL + headers + full stdlib + lib-dynload
# + vcruntime runtime DLLs) into third_party/python/, then build a MinGW import lib.
# Re-run is idempotent; bump $Version to update (single point of change).
param(
    [string]$Version = '3.12.10',
    [string]$OutDir = (Join-Path $PSScriptRoot '..\third_party\python'),
    [string]$MsysBin = 'D:\msys64\ucrt64\bin'
)
$ErrorActionPreference = 'Stop'
if (![System.IO.Path]::IsPathRooted($OutDir)) {
    $OutDir = Join-Path $PSScriptRoot $OutDir
}
$OutDir = [System.IO.Path]::GetFullPath($OutDir)
$tools = Join-Path $OutDir 'tools'
$dll = Join-Path $tools 'python312.dll'
$implib = Join-Path $tools 'libpython312.a'
$stamp = Join-Path $tools '.fetched'

if (Test-Path $stamp) {
    Write-Output "python $Version already fetched -> $OutDir"
    exit 0
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$objdump = Join-Path $MsysBin 'objdump.exe'
$dlltool = Join-Path $MsysBin 'dlltool.exe'
if (!(Test-Path $objdump) -or !(Test-Path $dlltool)) {
    Write-Error "objdump/dlltool not found in $MsysBin; cannot build the MinGW import lib"
}

$nugetUrl = "https://www.nuget.org/api/v2/package/python/$Version"
$nupkg = Join-Path $env:TEMP "python-$Version.nupkg"
$nupkgZip = Join-Path $env:TEMP "python-$Version.zip"

Write-Output "Downloading $nugetUrl"
Invoke-WebRequest -Uri $nugetUrl -OutFile $nupkg -UseBasicParsing
# vx: Expand-Archive requires a .zip extension, so copy the nupkg to a .zip name first
Copy-Item -LiteralPath $nupkg -Destination $nupkgZip -Force
Expand-Archive -Path $nupkgZip -DestinationPath $OutDir -Force

# vx: generate the MinGW import lib from the official (MSVC-built) python312.dll
if (!(Test-Path $implib)) {
    Write-Output "Generating $implib"
    $exports = (& $objdump -p $dll) | Select-String -Pattern '^\s+\[ *\d+\]' |
        ForEach-Object { $_.Line.Trim() -split '\s+' | Select-Object -Last 1 } |
        Where-Object { $_ -match '^[A-Za-z_][A-Za-z0-9_]*$' }
    $defPath = Join-Path $tools 'python312.def'
    @('LIBRARY python312.dll', 'EXPORTS') + ($exports | ForEach-Object { "    $_" }) |
        Set-Content -LiteralPath $defPath -Encoding ASCII
    & $dlltool -d $defPath -l $implib
    if ($LASTEXITCODE -ne 0) { Write-Error "dlltool failed" }
}

Set-Content -LiteralPath $stamp -Value $Version
Write-Output "python $Version fetched -> $OutDir"
