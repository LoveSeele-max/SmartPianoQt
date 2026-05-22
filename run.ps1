$ErrorActionPreference = "Stop"

$qtPrefix = if ($env:QT_PREFIX) { $env:QT_PREFIX } else { "D:\Qt\6.11.1\mingw_64" }
$qtBin = Join-Path $qtPrefix "bin"
$mingwBin = "D:\mingw64\bin"
$exe = Join-Path $PSScriptRoot "build\SmartPianoQt.exe"

if (-not (Test-Path $exe)) {
    throw "SmartPianoQt.exe not found. Build the project first: cmake --build build"
}

if (-not (Test-Path $qtBin)) {
    throw "Qt bin directory not found: $qtBin. Set QT_PREFIX to your Qt installation prefix."
}

$env:PATH = "$qtBin;$mingwBin;$env:PATH"
& $exe
