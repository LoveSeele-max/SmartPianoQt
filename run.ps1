$ErrorActionPreference = "Stop"

$qtPrefix = if ($env:QT_PREFIX) { $env:QT_PREFIX } else { "D:\Qt\6.11.1\mingw_64" }
$qtBin = Join-Path $qtPrefix "bin"
$mingwBin = "D:\mingw64\bin"
$fluidSynthPrefix = if ($env:FLUIDSYNTH_PREFIX) { $env:FLUIDSYNTH_PREFIX } else { "E:\fluidsynth-v2.5.4-win10-x64-cpp11" }
$fluidSynthBin = Join-Path $fluidSynthPrefix "bin"
$exe = Join-Path $PSScriptRoot "build\SmartPianoQt.exe"

if (-not (Test-Path $exe)) {
    throw "SmartPianoQt.exe not found. Build the project first: cmake --build build"
}

if (-not (Test-Path $qtBin)) {
    throw "Qt bin directory not found: $qtBin. Set QT_PREFIX to your Qt installation prefix."
}

if (Test-Path $fluidSynthBin) {
    $env:PATH = "$fluidSynthBin;$env:PATH"
    if (-not $env:FLUIDSYNTH_DLL) {
        $candidate = Join-Path $fluidSynthBin "libfluidsynth-3.dll"
        if (Test-Path $candidate) {
            $env:FLUIDSYNTH_DLL = $candidate
        }
    }
}

$env:PATH = "$qtBin;$mingwBin;$env:PATH"
& $exe
