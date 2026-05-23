# SoundFonts

Put a piano `.sf2` or `.sf3` file in this folder to enable sampled piano playback.

SmartPianoQt looks here at startup and prefers files whose names contain `piano`, `grand`, or `keys`. The actual SoundFont files are ignored by git because they are usually large and license-specific.

`run.ps1` can also link a SoundFont automatically on startup. It first uses an existing file in this folder, then checks `SMARTPIANO_SOUNDFONT`, `SOUNDFONT_PATH`, and the local Upright Piano folder:

```text
E:\UprightPianoKW-SF2-20220221
```

For a custom file or folder, set:

```powershell
$env:SMARTPIANO_SOUNDFONT = "D:\path\to\piano.sf2"
```

You also need a FluidSynth runtime DLL available to the app. On Windows, either install FluidSynth so `libfluidsynth-3.dll` is on `PATH`, copy the DLL next to `SmartPianoQt.exe`, or set:

```powershell
$env:FLUIDSYNTH_DLL = "C:\path\to\libfluidsynth-3.dll"
```

This local path is detected automatically by the app and by `run.ps1`:

```text
E:\fluidsynth-v2.5.4-win10-x64-cpp11
```

Useful starting points:

- FluidSynth downloads: https://www.fluidsynth.org/download/
- MuseScore SoundFont notes: https://musescore.org/en/handbook/4/soundfonts
