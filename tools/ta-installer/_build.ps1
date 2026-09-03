# Build ta-installer.exe (single-exe TypeAnything installer).
#
# Steps:
#   1. _stage_embed.ps1 copies fresh artifacts into embed/.
#   2. xmake compiles main.cpp + installer.rc (embeds all of embed/).
#   3. Output: build\windows\x64\release\ta-installer.exe.

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

& "$PSScriptRoot\_stage_embed.ps1"

# Path scrub
$pathParts = $env:PATH -split ';' | Where-Object {
    $_ -notmatch '\\Git\\(mingw|usr)' `
    -and $_ -notmatch '\\msys' `
    -and $_ -notmatch '\\Git\\bin' `
    -and $_ -notmatch '\\anaconda3'
}
$vsInstaller = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer"
if ((Test-Path $vsInstaller) -and ($pathParts -notcontains $vsInstaller)) {
    $pathParts = ,$vsInstaller + $pathParts
}
$env:PATH = ($pathParts -join ';')
$env:VSINSTALLERDIR = "$vsInstaller\"

# VS dev env
$tmp = Join-Path $env:TEMP "vcvars_env_inst.txt"
$vcvars = @(
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat",
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat",
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvarsall.bat",
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat"
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $vcvars) { throw "Visual Studio C++ build tools not found" }
cmd.exe /c "`"$vcvars`" x64 >nul 2>nul && set > `"$tmp`""
foreach ($line in (Get-Content $tmp -Encoding Default)) {
    if ($line -match '^([^=]+)=(.*)$') {
        Set-Item -Path "env:$($Matches[1])" -Value $Matches[2]
    }
}
$env:INCLUDE = ($env:INCLUDE -split ';' | Where-Object { $_ -notmatch '\\anaconda3' }) -join ';'
$env:LIB     = ($env:LIB     -split ';' | Where-Object { $_ -notmatch '\\anaconda3' }) -join ';'

$xmake = (Get-Command xmake.exe -ErrorAction SilentlyContinue).Source
if (-not $xmake) {
    $xmake = Get-ChildItem "$env:LOCALAPPDATA\Microsoft\WinGet\Packages\Xmake-io.Xmake_*\xmake.exe" `
        -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName
}
if (-not $xmake) { throw "xmake.exe not found" }

& $xmake f -p windows -a x64 -m release -y
if ($LASTEXITCODE -ne 0) { Write-Host "xmake config failed"; exit $LASTEXITCODE }

& $xmake build -j4 -y ta-installer
if ($LASTEXITCODE -ne 0) { Write-Host "xmake build failed"; exit $LASTEXITCODE }

Write-Host ""
Write-Host "=== ta-installer build done ==="
$out = ".\build\windows\x64\release"
Get-ChildItem -Recurse $out -Include *.exe,*.dll,*.html,*.css,*.js | ForEach-Object {
    Write-Host ("  {0,10}B  {1}" -f $_.Length, $_.FullName)
}
