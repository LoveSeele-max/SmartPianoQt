$ErrorActionPreference = "Stop"

$qtPrefix = if ($env:QT_PREFIX) { $env:QT_PREFIX } else { "D:\Qt\6.11.1\mingw_64" }
$qtBin = Join-Path $qtPrefix "bin"
$mingwBin = "D:\mingw64\bin"
$fluidSynthPrefix = if ($env:FLUIDSYNTH_PREFIX) { $env:FLUIDSYNTH_PREFIX } else { "E:\fluidsynth-v2.5.4-win10-x64-cpp11" }
$fluidSynthBin = Join-Path $fluidSynthPrefix "bin"
$soundFontsDir = Join-Path $PSScriptRoot "soundfonts"
$exe = Join-Path $PSScriptRoot "build\SmartPianoQt.exe"

function Get-FirstSoundFontFile {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path) -or -not (Test-Path -LiteralPath $Path)) {
        return $null
    }

    $item = Get-Item -LiteralPath $Path
    if (-not $item.PSIsContainer) {
        if ($item.Extension -in @(".sf2", ".sf3")) {
            return $item.FullName
        }
        return $null
    }

    $file = Get-ChildItem -LiteralPath $item.FullName -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Extension -in @(".sf2", ".sf3") } |
        Sort-Object @{ Expression = { if ($_.Name -match "(?i)(piano|grand|keys)") { 0 } else { 1 } } }, Name |
        Select-Object -First 1

    if ($file) {
        return $file.FullName
    }
    return $null
}

function Resolve-SoundFontSource {
    foreach ($candidate in @($env:SMARTPIANO_SOUNDFONT, $env:SOUNDFONT_PATH, "E:\UprightPianoKW-SF2-20220221")) {
        $resolved = Get-FirstSoundFontFile $candidate
        if ($resolved) {
            return $resolved
        }
    }
    return $null
}

function Connect-SoundFont {
    if (-not (Test-Path -LiteralPath $soundFontsDir)) {
        New-Item -ItemType Directory -Path $soundFontsDir | Out-Null
    }

    $existing = Get-FirstSoundFontFile $soundFontsDir
    if ($existing) {
        $env:SMARTPIANO_SOUNDFONT = $existing
        return
    }

    $source = Resolve-SoundFontSource
    if (-not $source) {
        return
    }

    $target = Join-Path $soundFontsDir (Split-Path $source -Leaf)
    if (-not (Test-Path -LiteralPath $target)) {
        try {
            New-Item -ItemType HardLink -Path $target -Target $source | Out-Null
        } catch {
            try {
                New-Item -ItemType SymbolicLink -Path $target -Target $source | Out-Null
            } catch {
                Copy-Item -LiteralPath $source -Destination $target
            }
        }
    }

    $env:SMARTPIANO_SOUNDFONT = $target
    Write-Host "SoundFont linked: $target"
}

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

Connect-SoundFont

$env:PATH = "$qtBin;$mingwBin;$env:PATH"
& $exe
