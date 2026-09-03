# Stage all artifacts for installer.rc to embed.
# Run before xmake build.

$ErrorActionPreference = "Stop"
$here = $PSScriptRoot
$repoRoot = (Resolve-Path (Join-Path $here "..\..")).Path
$embed = Join-Path $here "embed"
Set-Location $here

if (Test-Path $embed) { Remove-Item -Recurse -Force $embed }
New-Item -ItemType Directory -Force -Path $embed,"$embed\target-ui","$embed\install-ui" | Out-Null

# 1. Weasel binaries. Prefer fresh builds; fall back to the currently
# installed TypeAnything shell when only librime was rebuilt.
$buildBin = Join-Path $repoRoot "third_party\weasel\build\windows\x64\release"
$installedRoot = $env:TYPEANYTHING_INSTALL_DIR
if (-not $installedRoot) {
    $runValue = (Get-ItemProperty "HKLM:\Software\Microsoft\Windows\CurrentVersion\Run" `
        -Name WeaselServer -ErrorAction SilentlyContinue).WeaselServer
    if ($runValue -match '^"([^"]+)"') { $serverPath = $Matches[1] }
    elseif ($runValue)                  { $serverPath = $runValue }
    if ($serverPath -and (Test-Path $serverPath)) { $installedRoot = Split-Path $serverPath }
}
if (-not $installedRoot) { $installedRoot = "${env:ProgramFiles}\Rime\weasel-0.17.4" }

$rimeDll = Join-Path $repoRoot "third_party\weasel\librime\dist\lib\rime.dll"
if (-not (Test-Path $rimeDll)) { throw "Fresh rime.dll not found: $rimeDll" }
Copy-Item $rimeDll "$embed\rime.dll"

function StageWeaselBinary($buildRelative, $installedName) {
    $fresh = Join-Path $buildBin $buildRelative
    $installed = Join-Path $installedRoot $installedName
    if (Test-Path $fresh)          { Copy-Item $fresh (Join-Path $embed $installedName) }
    elseif (Test-Path $installed)  { Copy-Item $installed (Join-Path $embed $installedName) }
    else                           { throw "Weasel binary not found: $installedName" }
}
StageWeaselBinary "WeaselTSF\weaselx64.dll" "weaselx64.dll"
StageWeaselBinary "WeaselServer\WeaselServer.exe" "WeaselServer.exe"
StageWeaselBinary "WeaselDeployer\WeaselDeployer.exe" "WeaselDeployer.exe"

# 2. ta-settings.exe + loader
$taBin = Join-Path $repoRoot "tools\ta-settings\build\windows\x64\release"
Copy-Item "$taBin\ta-settings.exe"     "$embed\ta-settings.exe"
$loader = Join-Path $taBin "WebView2Loader.dll"
if (-not (Test-Path $loader)) {
    $loader = Join-Path $repoRoot "tools\ta-settings\vendor\webview2\x64\WebView2Loader.dll"
}
if (-not (Test-Path $loader)) { throw "WebView2Loader.dll not found" }
Copy-Item $loader "$embed\WebView2Loader.dll"

# 3. target ui (for ta-settings to load post-install)
foreach ($f in "index.html","style.css","app.js","fish.png") {
    Copy-Item "$taBin\ui\$f" "$embed\target-ui\$f"
}

# 4. installer ui (this exe's own webview pages)
foreach ($f in "index.html","style.css","app.js","fish.png") {
    Copy-Item "$here\ui\$f" "$embed\install-ui\$f"
}

# 5. schema yaml template + supplement dict (modern AI/IT/slang terms)
$schemaDir = Join-Path $repoRoot "third_party\weasel\librime\plugins\typeanything\schema"
Copy-Item "$schemaDir\typeanything.schema.yaml" "$embed\typeanything.schema.yaml"
$dictSource = Join-Path $schemaDir "typeanything.dict.yaml"
if (-not (Test-Path $dictSource)) { $dictSource = Join-Path $env:APPDATA "Rime\typeanything.dict.yaml" }
if (-not (Test-Path $dictSource)) { throw "typeanything.dict.yaml not found" }
Copy-Item $dictSource "$embed\typeanything.dict.yaml"

# 6. Rime base data — required on cold machines (no prior Weasel install).
#    Prefer the installed Weasel data dir (has key_bindings/punctuation that
#    librime/data/minimal/ lacks). Fall back to minimal/ for files missing.
$pfData = Join-Path $installedRoot "data"
$minData = Join-Path $repoRoot "third_party\weasel\librime\data\minimal"
function StageDataFile($name) {
    $pf  = Join-Path $pfData $name
    $min = Join-Path $minData $name
    $dst = Join-Path $embed $name
    if (Test-Path $pf)        { Copy-Item $pf  $dst }
    elseif (Test-Path $min)   { Copy-Item $min $dst }
    else                      { throw "data file not found: $name" }
}
StageDataFile "default.yaml"
StageDataFile "luna_pinyin.dict.yaml"
StageDataFile "luna_pinyin.schema.yaml"
StageDataFile "essay.txt"
StageDataFile "symbols.yaml"
StageDataFile "punctuation.yaml"
StageDataFile "key_bindings.yaml"
# weasel.yaml — base panel/style template. Without this on a cold machine,
# WeaselDeployer cannot produce build\weasel.yaml and the candidate window
# renders at ~0px on high-DPI screens (issue #14).
StageDataFile "weasel.yaml"

# 6b. OpenCC data — required by the schema's simplifier@simplification_filter
#     (opencc_config: t2s.json). Without these the simplifier fails to load,
#     traditional luna_pinyin output passes through unconverted AND the
#     candidate generation chain errors out → empty candidate window.
#     Only the t2s (繁→简) chain is needed: t2s.json + TSPhrases + TSCharacters.
$opencc = Join-Path $pfData "opencc"
$openccDst = Join-Path $embed "opencc"
New-Item -ItemType Directory -Force -Path $openccDst | Out-Null
foreach ($f in "t2s.json","TSPhrases.ocd2","TSCharacters.ocd2") {
    $srcF = Join-Path $opencc $f
    if (-not (Test-Path $srcF)) { throw "OpenCC data missing: $srcF (install official 小狼毫 once to get opencc/)" }
    Copy-Item $srcF (Join-Path $openccDst $f)
}

# 7. Translate / classify prompts (UTF-8). Source of truth = embed_prompts.txt
#    (generated from tools/eval/run_eval.py validated prompts).
Copy-Item "$here\embed_prompts.txt" "$embed\typeanything_prompts.txt"

# Compile installer.rc → .res → .obj (real COFF) so xmake link picks it up
# as a normal object file. xmake's built-in RC handling outputs RES format
# but under a .obj name, which link.exe silently drops.
$rcExe = Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\bin" -Directory -ErrorAction SilentlyContinue |
         Sort-Object Name -Descending | ForEach-Object {
             $candidate = Join-Path $_.FullName "x64\rc.exe"
             if (Test-Path $candidate) { return $candidate }
         } | Select-Object -First 1
$cvtres = Get-ChildItem "${env:ProgramFiles(x86)}\Microsoft Visual Studio" -Recurse `
          -Filter cvtres.exe -File -ErrorAction SilentlyContinue |
          Where-Object { $_.FullName -match '\\bin\\Host[xX]64\\x64\\cvtres\.exe$' } |
          Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName

if (-not $rcExe)   { throw "rc.exe not found (need Windows SDK)" }
if (-not $cvtres)  { throw "cvtres.exe not found (need Visual Studio C++ build tools)" }

Write-Host "==> rc.exe : $rcExe"
Write-Host "==> cvtres : $cvtres"

$rc  = Join-Path $here "installer.rc"
$res = Join-Path $here "embed\installer.res"
$obj = Join-Path $here "embed\installer.res.obj"

& $rcExe -nologo -fo $res $rc
if ($LASTEXITCODE -ne 0) { throw "rc.exe failed: $LASTEXITCODE" }
& $cvtres /MACHINE:X64 /NOLOGO /OUT:$obj $res
if ($LASTEXITCODE -ne 0) { throw "cvtres failed: $LASTEXITCODE" }

Write-Host "==> embed staged:"
Get-ChildItem -Recurse $embed | ForEach-Object { Write-Host ("  {0,10}B  {1}" -f $_.Length, $_.FullName.Substring($embed.Length + 1)) }
