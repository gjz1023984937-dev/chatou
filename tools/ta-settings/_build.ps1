# Build ta-settings.exe (WebView2 UI for Chatou tray menu items).

$ErrorActionPreference = "Stop"
$here = $PSScriptRoot
Set-Location $here

# Strip MSYS / mingw from PATH so xmake picks MSVC cl.exe
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

# Activate VS env
$tmpEnvFile = Join-Path $env:TEMP "vcvars_env_ta.txt"
$vcvars = @(
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat",
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat",
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat",
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat",
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat",
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat",
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat",
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat",
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvarsall.bat",
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat"
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $vcvars) { throw "Visual Studio C++ build tools not found" }
cmd.exe /c "`"$vcvars`" x64 >nul 2>nul && set > `"$tmpEnvFile`""
foreach ($line in (Get-Content $tmpEnvFile -Encoding Default)) {
    if ($line -match '^([^=]+)=(.*)$') {
        Set-Item -Path "env:$($Matches[1])" -Value $Matches[2]
    }
}
$env:INCLUDE = ($env:INCLUDE -split ';' | Where-Object { $_ -notmatch '\\anaconda3' }) -join ';'
$env:LIB     = ($env:LIB     -split ';' | Where-Object { $_ -notmatch '\\anaconda3' }) -join ';'

# Configure xmake for x64 release
$xmake = (Get-Command xmake.exe -ErrorAction SilentlyContinue).Source
if (-not $xmake) {
    $xmake = Get-ChildItem "$env:LOCALAPPDATA\Microsoft\WinGet\Packages\Xmake-io.Xmake_*\xmake.exe" `
        -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName
}
if (-not $xmake) { throw "xmake.exe not found" }

& $xmake f -p windows -a x64 -m release -y
if ($LASTEXITCODE -ne 0) { Write-Host "xmake config failed"; exit $LASTEXITCODE }

# Build
& $xmake build -j4 -y ta-settings
if ($LASTEXITCODE -ne 0) { Write-Host "xmake build failed"; exit $LASTEXITCODE }

Write-Host ""
Write-Host "=== ta-settings build done ==="
$out = ".\build\windows\x64\release"
Get-ChildItem -Recurse $out -Include *.exe,*.dll,*.html,*.css,*.js | ForEach-Object {
    Write-Host "  $($_.FullName)"
}
