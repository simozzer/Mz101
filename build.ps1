<#
    Build the plugin, after holding the engine to its checks.

        .\build.ps1                  engine check, then the VST3 and the standalone
        .\build.ps1 -NoPlugin        engine check only, for iterating on the DSP
        .\build.ps1 -Wav <dir>       and write demo phrases to <dir> to listen to

    The check gates the plugin rather than standing in for it: if the engine is out of tune
    or blows up, there is no point wrapping a plugin round it, and if it passes the plugin is
    built from those same sources straight after.

    cl.exe is never on the PATH - vcvars64.bat sets up INCLUDE, LIB and PATH for it, and
    vswhere says where Visual Studio is rather than a versioned path being written down here.
#>
param(
    [switch]$NoPlugin,
    [string]$Wav = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Error "No Visual Studio found. Install VS Community with 'Desktop development with C++'."
}

$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
                     -property installationPath
if (-not $vsPath) {
    Write-Error "Visual Studio is installed but the C++ tools are not. Add 'Desktop development with C++'."
}

$vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"

# ---------------------------------------------------------------- the engine check

$out = Join-Path $env:TEMP "mz101check"
New-Item -ItemType Directory -Force -Path $out | Out-Null

$exe = Join-Path $out "DspCheck.exe"
$sources = @(
    (Join-Path $root "Tests\DspCheck.cpp"),
    (Join-Path $root "Source\SH101\Engine.cpp")
)
$include = Join-Path $root "Source\SH101"
$quoted = ($sources | ForEach-Object { "`"$_`"" }) -join " "

Write-Host "building the engine check"
cmd /c "call `"$vcvars`" >nul 2>&1 && cd /d `"$out`" && cl /nologo /std:c++17 /EHsc /W4 /O2 /fp:fast /I `"$include`" $quoted /Fe:`"$exe`"" |
    Where-Object { $_ -match "error|warning" }
if ($LASTEXITCODE -ne 0) { Write-Error "the engine check did not build" }

if ($Wav -ne "") {
    New-Item -ItemType Directory -Force -Path $Wav | Out-Null
    & $exe --wav (Resolve-Path $Wav)
} else {
    & $exe
}
if ($LASTEXITCODE -ne 0) { Write-Error "the engine failed its checks - not building a plugin around it" }

if ($NoPlugin) { exit 0 }

# ---------------------------------------------------------------- the plugin

$cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
if (-not $cmake) {
    $bundled = Join-Path $vsPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    if (Test-Path $bundled) { $cmake = $bundled }
}
if (-not $cmake) { Write-Error "No cmake on the PATH and none inside Visual Studio." }

$buildDir = Join-Path $root "build"

if (-not (Test-Path (Join-Path $buildDir "CMakeCache.txt"))) {
    Write-Host ""
    Write-Host "configuring $buildDir (first time)"
    & $cmake -S $root -B $buildDir
    if ($LASTEXITCODE -ne 0) { Write-Error "cmake configure failed" }
}

Write-Host ""
Write-Host "building the VST3 and the standalone"

& $cmake --build $buildDir --config Release --target Mz101_VST3 Mz101_Standalone -- /m /v:minimal |
    Where-Object { $_ -match "error|warning C|installed to|could not be" }
if ($LASTEXITCODE -ne 0) { Write-Error "the plugin build failed" }

# Report what is on disk, and when it was written, rather than that the build returned zero.
$artefacts = @(
    (Join-Path $buildDir "Mz101_artefacts\Release\VST3\Mz101.vst3\Contents\x86_64-win\Mz101.vst3"),
    (Join-Path $buildDir "Mz101_artefacts\Release\Standalone\Mz101.exe")
)

$newest = Get-ChildItem (Join-Path $root "Source") -Recurse -Include *.cpp, *.h |
          Sort-Object LastWriteTime -Descending | Select-Object -First 1

Write-Host ""
foreach ($a in $artefacts) {
    if (-not (Test-Path $a)) { Write-Error "expected $a and it is not there" }
    $f = Get-Item $a
    $stale = $f.LastWriteTime -lt $newest.LastWriteTime
    Write-Host ("ok  -  {0,-12} {1,7} KB   {2}{3}" -f $f.Name, [math]::Round($f.Length / 1KB), $f.LastWriteTime,
                $(if ($stale) { "   *** OLDER THAN $($newest.Name) ***" } else { "" })) `
               -ForegroundColor $(if ($stale) { "Red" } else { "Green" })
    if ($stale) { Write-Error "an artefact is older than the sources it was built from" }
}

exit 0
