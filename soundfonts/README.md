# SoundFonts

Put a piano `.sf2` or `.sf3` file in this folder to enable sampled piano playback.

SmartPianoQt looks here at startup and prefers files whose names contain `piano`, `grand`, or `keys`. The actual SoundFont files are ignored by git because they are usually large and license-specific.

You also need a FluidSynth runtime DLL available to the app. On Windows, either install FluidSynth so `libfluidsynth-3.dll` is on `PATH`, copy the DLL next to `SmartPianoQt.exe`, or set:

```powershell
$env:FLUIDSYNTH_DLL = "C:\path\to\libfluidsynth-3.dll"
```

Useful starting points:

- FluidSynth downloads: https://www.fluidsynth.org/download/
- MuseScore SoundFont notes: https://musescore.org/en/handbook/4/soundfonts
