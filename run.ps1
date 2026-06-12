param(
    [string]$BuildDir = $(if ($env:SMARTPIANO_BUILD_DIR) { $env:SMARTPIANO_BUILD_DIR } else { "build" })
)

$ErrorActionPreference = "Stop"

function Test-Directory {
    param([string]$Path)
    return -not [string]::IsNullOrWhiteSpace($Path) -and
        (Test-Path -LiteralPath $Path -PathType Container)
}

function Test-File {
    param([string]$Path)
    return -not [string]::IsNullOrWhiteSpace($Path) -and
        (Test-Path -LiteralPath $Path -PathType Leaf)
}

function Resolve-FirstDirectory {
    param([string[]]$Candidates)

    foreach ($candidate in $Candidates) {
        if (Test-Directory $candidate) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    return $null
}

function Resolve-FirstFile {
    param([string[]]$Candidates)

    foreach ($candidate in $Candidates) {
        if (Test-File $candidate) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    return $null
}

function Get-CMakePrefixCandidates {
    $items = @()
    foreach ($value in @($env:QT_PREFIX, $env:QT_ROOT_DIR, $env:CMAKE_PREFIX_PATH)) {
        if ([string]::IsNullOrWhiteSpace($value)) {
            continue
        }
        $items += ($value -split ';' | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
    }
    return $items
}

function Get-QtPrefixCandidates {
    $candidates = @()
    $candidates += Get-CMakePrefixCandidates

    foreach ($drive in @("C:", "D:", "E:")) {
        $qtRoot = Join-Path $drive "Qt"
        if (-not (Test-Directory $qtRoot)) {
            continue
        }

        $versions = Get-ChildItem -LiteralPath $qtRoot -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match '^6\.' } |
            Sort-Object Name -Descending

        foreach ($version in $versions) {
            foreach ($kit in @("mingw_64", "msvc2022_64", "msvc2019_64")) {
                $candidates += Join-Path $version.FullName $kit
            }
        }
    }

    return $candidates
}

function Resolve-QtBin {
    $qtPrefix = Resolve-FirstDirectory (Get-QtPrefixCandidates)
    if (-not $qtPrefix) {
        throw "Qt was not found. Set QT_PREFIX or QT_ROOT_DIR to a Qt kit prefix, for example C:\Qt\6.6.3\msvc2019_64."
    }

    $qtBin = Join-Path $qtPrefix "bin"
    if (-not (Test-Directory $qtBin)) {
        throw "Qt bin directory was not found under '$qtPrefix'. Set QT_PREFIX to the Qt kit prefix, not the Qt root folder."
    }

    return $qtBin
}

function Resolve-MinGwBin {
    $candidates = @()
    if ($env:MINGW_BIN) {
        $candidates += $env:MINGW_BIN
    }
    if ($env:MINGW_PREFIX) {
        $candidates += Join-Path $env:MINGW_PREFIX "bin"
    }

    foreach ($drive in @("C:", "D:", "E:")) {
        $candidates += Join-Path $drive "mingw64\bin"
        $candidates += Join-Path $drive "msys64\mingw64\bin"

        $toolsRoot = Join-Path $drive "Qt\Tools"
        if (Test-Directory $toolsRoot) {
            $candidates += Get-ChildItem -LiteralPath $toolsRoot -Directory -Filter "mingw*" -ErrorAction SilentlyContinue |
                ForEach-Object { Join-Path $_.FullName "bin" }
        }
    }

    return Resolve-FirstDirectory $candidates
}

function Resolve-AppExecutable {
    $rootBuildDir = Join-Path $PSScriptRoot $BuildDir
    $candidates = @(
        $env:SMARTPIANO_EXE,
        (Join-Path $rootBuildDir "SmartPianoQt.exe"),
        (Join-Path $rootBuildDir "Release\SmartPianoQt.exe"),
        (Join-Path $PSScriptRoot "build\SmartPianoQt.exe"),
        (Join-Path $PSScriptRoot "build\Release\SmartPianoQt.exe"),
        (Join-Path $PSScriptRoot "build\mingw-debug\SmartPianoQt.exe"),
        (Join-Path $PSScriptRoot "build\mingw-release\SmartPianoQt.exe"),
        (Join-Path $PSScriptRoot "build\msvc-release\Release\SmartPianoQt.exe")
    )

    $exe = Resolve-FirstFile $candidates
    if (-not $exe) {
        throw "SmartPianoQt.exe was not found. Build first with 'cmake --build build' or set SMARTPIANO_EXE."
    }
    return $exe
}

function Resolve-FluidSynthBin {
    $candidates = @()
    if ($env:FLUIDSYNTH_DLL -and (Test-File $env:FLUIDSYNTH_DLL)) {
        $candidates += Split-Path -Parent $env:FLUIDSYNTH_DLL
    }
    if ($env:FLUIDSYNTH_PREFIX) {
        $candidates += Join-Path $env:FLUIDSYNTH_PREFIX "bin"
        $candidates += $env:FLUIDSYNTH_PREFIX
    }
    if ($env:ProgramFiles) {
        $candidates += Join-Path $env:ProgramFiles "FluidSynth\bin"
    }
    if (${env:ProgramFiles(x86)}) {
        $candidates += Join-Path ${env:ProgramFiles(x86)} "FluidSynth\bin"
    }

    foreach ($drive in @("C:", "D:", "E:")) {
        $root = "$drive\"
        if (Test-Directory $root) {
            $candidates += Get-ChildItem -LiteralPath $root -Directory -Filter "fluidsynth*" -ErrorAction SilentlyContinue |
                ForEach-Object { Join-Path $_.FullName "bin" }
        }
    }

    return Resolve-FirstDirectory $candidates
}

function Get-FirstSoundFontFile {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path -ErrorAction SilentlyContinue)) {
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
    $candidates = @(
        $env:SMARTPIANO_SOUNDFONT,
        $env:SOUNDFONT_PATH
    )
    if ($HOME) {
        $candidates += Join-Path $HOME "Documents\SoundFonts"
        $candidates += Join-Path $HOME "Downloads"
    }

    foreach ($candidate in $candidates) {
        $resolved = Get-FirstSoundFontFile $candidate
        if ($resolved) {
            return $resolved
        }
    }
    return $null
}

function Connect-SoundFont {
    $soundFontsDir = Join-Path $PSScriptRoot "soundfonts"
    if (-not (Test-Directory $soundFontsDir)) {
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
    if (-not (Test-File $target)) {
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

$exe = Resolve-AppExecutable
$qtBin = Resolve-QtBin
$mingwBin = Resolve-MinGwBin
$fluidSynthBin = Resolve-FluidSynthBin

$pathParts = @()
if ($fluidSynthBin) {
    $pathParts += $fluidSynthBin
    if (-not $env:FLUIDSYNTH_DLL) {
        $candidate = Join-Path $fluidSynthBin "libfluidsynth-3.dll"
        if (Test-File $candidate) {
            $env:FLUIDSYNTH_DLL = $candidate
        }
    }
}
$pathParts += $qtBin
if ($mingwBin) {
    $pathParts += $mingwBin
}
$pathParts += $env:PATH
$env:PATH = ($pathParts | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }) -join ";"

Connect-SoundFont

Write-Host "Running: $exe"
& $exe
