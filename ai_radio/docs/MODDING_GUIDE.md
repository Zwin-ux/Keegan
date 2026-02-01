# Keegan Modding Guide

## Goal
Make the radio engine moddable by packaging moods, stems, and metadata into a "frequency pack".

## Quick start (today)
The engine currently loads `config/moods.json` only. To test a pack right now:
1) Copy the pack's `moods.json` into `config/moods.json`.
2) Place any new stems under `assets/stems/<mood>/`.
3) Run the EXE.

A proper mod loader is planned. This guide defines the pack format so mods are stable once the loader lands.

## Pack layout
```
mods/
  <mod_id>/
    manifest.json
    moods.json
    stems/
      focus/
      rain/
      arcade/
      sleep/
    voice/
    presets/
    visuals/
    scripts/
    license.txt
```

## manifest.json
Minimal required fields:
- id: unique pack id
- name: display name
- version: semver
- engineVersion: supported engine range
- frequencySeed: default frequency (float)
- region: region tag (string)

Example:
```
{
  "id": "neon_arcade",
  "name": "Neon Arcade",
  "version": "1.0.0",
  "engineVersion": ">=0.1.0",
  "frequencySeed": 101.8,
  "region": "us-midwest",
  "author": "username",
  "permissions": ["vibe.basic", "vibe.apps"],
  "entry": "scripts/vibe.lua"
}
```

## moods.json
The mood pack format matches `config/moods.json` today. Each mood includes:
- id, display_name
- energy, tension, warmth, color
- density_curve, narrative_frequency
- allowed_transitions
- stems (file, role, gain_db, optional probability)
- synth (preset, seed, pattern_density)

Use the core mood IDs for now:
- focus_room
- rain_cave
- arcade_night
- sleep_ship

This keeps compatibility with the tray UI and heuristics.

## Audio guidance
- WAV files, 48kHz preferred.
- Keep stems loop-safe (clean loop points).
- Normalize to avoid clipping. Target -12 to -6 dBFS peaks.
- Keep ambience wide but avoid extreme phase issues.

## Permissions (planned)
- vibe.basic: time of day + intensity
- vibe.apps: active process class
- vibe.activity: input activity
- vibe.audio: RMS of output only

## Validation (planned)
A future `tools/pack_validate.py` will check:
- Missing stems
- Bad JSON
- Unsupported fields
- Loudness and clipping issues

## Example pack
See `mods/example_neon` for a complete example.

