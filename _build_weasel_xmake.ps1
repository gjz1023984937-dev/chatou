# Build the Chatou-flavoured Weasel components with xmake.
#
# Requirements:
#   - librime has already been built under third_party/weasel/librime/dist
#   - Boost 1.84 (set BOOST_ROOT, or place it in ../dependencies/boost_1_84_0)
#   - WTL (set WTL_ROOT, or place it in ../dependencies/WTL)
#   - Visual Studio C++ Desktop workload with ATL

$ErrorActionPreference = "Stop"
$taskRepoRoot = $PSScriptRoot
$taskWeaselRoot = Join-Path $taskRepoRoot "third_party\weasel"
$taskDependencyRoot = Join-Path (Split-Path $taskRepoRoot -Parent) "dependencies"

$taskBoostRoot = $env:BOOST_ROOT
if (-not $taskBoostRoot) {
    $taskBoostRoot = Join-Path $taskDependencyRoot "boost_1_84_0"
}
if (-not (Test-Path (Join-Path $taskBoostRoot "boost"))) {
    throw "Boost not found. Set BOOST_ROOT or place Boost 1.84 at $taskBoostRoot"
}

$taskWtlRoot = $env:WTL_ROOT
if (-not $taskWtlRoot) {
    $taskWtlRoot = Join-Path $taskDependencyRoot "WTL"
}
$taskWtlInclude = Join-Path $taskWtlRoot "Include"
if (-not (Test-Path (Join-Path $taskWtlInclude "atlapp.h"))) {
    throw "WTL not found. Set WTL_ROOT or place WTL at $taskWtlRoot"
}

$taskVcvars = @(
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat",
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat",
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvarsall.bat",
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat"
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $taskVcvars) {
    throw "Visual Studio C++ build tools with ATL were not found"
}

$taskEnvFile = Join-Path $env:TEMP "chatou_vcvars_env.txt"
cmd.exe /c "`"$taskVcvars`" x64 >nul 2>nul && set > `"$taskEnvFile`""
foreach ($taskLine in (Get-Content $taskEnvFile -Encoding Default)) {
    if ($taskLine -match '^([^=]+)=(.*)$') {
        Set-Item -Path "env:$($Matches[1])" -Value $Matches[2]
    }
}

$taskRimeDist = Join-Path $taskWeaselRoot "librime\dist"
$taskRimeLib = Join-Path $taskRimeDist "lib\rime.lib"
if (-not (Test-Path $taskRimeLib)) {
    throw "librime output not found: $taskRimeLib"
}

New-Item -ItemType Directory -Path (Join-Path $taskWeaselRoot "lib64") -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $taskWeaselRoot "include\wtl") -Force | Out-Null
Copy-Item $taskRimeLib (Join-Path $taskWeaselRoot "lib64\rime.lib") -Force
Copy-Item (Join-Path $taskRimeDist "include\*") (Join-Path $taskWeaselRoot "include") -Recurse -Force
Copy-Item (Join-Path $taskWtlInclude "*") (Join-Path $taskWeaselRoot "include\wtl") -Recurse -Force

$env:BOOST_ROOT = $taskBoostRoot
$env:VERSION_MAJOR = "0"
$env:VERSION_MINOR = "8"
$env:VERSION_PATCH = "4"
$env:FILE_VERSION = '"0.8.4.0"'
$env:PRODUCT_VERSION = '"0.8.4.0"'

Set-Location $taskWeaselRoot
xmake f -p windows -a x64 -m release -y
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
xmake build -j4 -y WeaselTSF WeaselServer WeaselDeployer
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "=== Chatou Weasel build complete ==="
Get-ChildItem (Join-Path $taskWeaselRoot "output") -File |
    Where-Object { $_.Name -in @("weaselx64.dll", "WeaselServer.exe", "WeaselDeployer.exe") } |
    ForEach-Object { Write-Host ("  {0,10}B  {1}" -f $_.Length, $_.FullName) }
Set-Location $taskRepoRoot
