# Developing Daurade

The working notes behind Daurade: the agreements, how the work flows, why the code is shaped as it is, where it
stands and what comes next. What Daurade is and how to play it is in the [README](../README.md) and the
[manual](MANUAL.md).

Status: 28 October 2026. Version 0.1 built by CI, installed from the site and playing on an FM-1; released on `main`.

## Agreements

| What | Agreed |
| --- | --- |
| Home | GitHub, `github.com/delirioux/daurade`, branch `main`; unfinished work on `dev` until it has run on the FM-1. |
| Upstream | A fork of Flyfish and [Tuna](https://github.com/delirioux/tuna) (themselves forks of [Felucca](https://github.com/hugelton/Felucca)). Never push to Tuna or Felucca. Felucca's five engines are a submodule, `firmware/engines`. |
| Name | Ocean animals, as Tuna, Flyfish and Makohai: the gilt-head bream, silver with a gold band. The logo: four chrome knobs, two by two, and a bladed wordmark. |
| What it is | A groove machine: filtered loops, a pumping mix, a robot voice, very crisp drums. Its sounds and styles are its own; the repository names no artist, record or film as a model, in code, docs or history. |
| Language | English throughout: code, comments, commits, docs. |
| Licence | GPL-3.0-only. The drum one-shots are synthesized by `tools/da_drums.py` and dedicated to the public domain (CC0). |
| Commits | Author `delirioux <340923392+delirioux@users.noreply.github.com>`. Work done with Claude ends with the `Co-Authored-By` and `Claude-Session` lines. |
| Repository contents | The firmware, its tests and tools, its docs and its plan (this file). Chat discussions stay out. Docs and screenshots change with the code. |
| Controls | The keys only ever play music; settings, pages and sets are on knobs and buttons. |
| Colours | NEBULA is the default palette; Felucca's palettes stay on MENU. |

## How changes reach GitHub

Claude pushes to `dev` (and to `main` when told) directly. A push builds the package (Actions > the run > Artifacts
> `daurade-firmware`) and the site: main's release at delirioux.github.io/daurade, dev's test build under `dev/`.
A push to `main` with a new `DA_VERSION` (`firmware/src/da_version.h`) also tags it `vX.Y` and makes a GitHub
release with the package and `docs/RELEASE.md` (the workflow's `release` job: Claude's sandbox cannot push tags).

## Where Daurade lives

Daurade's files are `firmware/src/da_*`, `tests/da_*`, `tools/da_*`, `web/da_*` and `web/emu/da_*`. From Tuna:
`tuna_libm.c` and `tuna_sys.h`. Everything else is Felucca's, touched only where Daurade hooks in (`felucca.c`,
`main.c`, `audio.c`, `panel.c`, `gfx.c`, `eng_sample.c`: `FELUCCA_DAURADE`; `tools/build.py`: `DA_MODULES`;
`tools/gen_samples.py`: the drum set at 44.1 kHz, kept whole; `tools/gen_ui_palettes.py`: NEBULA).

| File | Does |
| --- | --- |
| `da_groove.h/.c` | the engine: ten styles, twelve chord loops, the rhythm families of each part, the hook, the arc, the sweep and the pump, DROP, SOLO, the keys, swing and human timing. Plain C, no Felucca: it runs on the host as it does on the FM-1 |
| `da_sounds.h` | the forty sounds on Felucca's engines and their trims, the two kits, and how a part's cards set its track (level, pan, sends, the matrix's sweep and pump slots, the VOICE page, TONE) |
| `da_fm6.inc` | the FM6 patches (REED EP, SLAP, BRASS, FRETLESS), made by `tools/da_fm6.py` |
| `da_ui.h/.c` | the thirteen pages, the cards, the styles list, the sets, on Felucca's `gfx.c` |
| `da_fw.c` | the glue: the groove on the tracks and MIDI out, loading a part's sound when it changes, the panel, the key LEDs, the settings and the sets in flash, the splash |
| `da_logo.inc` | the splash logo, made by `tools/da_logo.py` from the same drawing as `assets/logo.svg` (`tools/draw_logo.py`) |
| `tools/da_drums.py` | the seven drum one-shots, synthesized, seeded, into `assets/samples-cc0/DRUMS/` |
| `tests/da_groove_test.c` | the engine: every style plays and ends its notes, a song replays exactly, the tempo, style and song on the bar line, DROP tapped and held, SOLO in the scale, the keys, a bad record |
| `tests/da_ui_test.c` | the screen: the controls, every page in every palette, the layout lint, PNGs and a GIF (`docs/screens/`) |
| `tests/da_felucca.c` | a groove through Felucca's real engines to a WAV: voices, steals, peaks, the cost |
| `tests/da_audition.c` | every sound's loudness, for the trims in `da_sounds.h` |
| `web/emu/da_web.c` | the browser build, on Felucca's emulator |

## Decisions and why

- **The sweep and the pump in the matrix, not in the DSP.** Each swept part's track carries MODW to its filter
  (slot 1, -32) and EXPR to its level (slot 2, +64); the groove sets `mw` from the cut and `ex_off` from the duck
  every 32 frames. No new DSP, no cost on the second core, and every engine with a filter or a brightness sweeps the
  same way. At -64 a closed sweep left the CHROME stab almost silent: -32 and a swept part's CUT at 100 or more.
- **The sweep breathes over sixteen bars**: from 45 % of SWEEP to all of it on a smoothstep, up over fourteen bars,
  down over two, so the filter opens into the turnaround. BUILD opens it fully, DROP closes it.
- **The pump**: a 6 ms duck on each kick, back over 70 % of the beat, as deep as PUMP x 0.8; the drums never duck.
  It is a level, not a compressor: predictable, cheap, in time.
- **Eight voices for everything.** Felucca shares eight voices across the four tracks. The chops play three notes,
  not four; every drum has a ring length and is let go; the closed hat chokes the open one (not when HATS is OPEN).
  Measured over 40 s through Felucca's engines: FILTER HOUSE steals none, FUNK 7, ROBOT POP 8, DISCO 16 (its
  string swells), out of 700 to 1,100 notes.
- **Very crisp drums of our own.** No complete CC0 kit could be fetched, so `tools/da_drums.py` synthesizes seven:
  the kicks deep (PUNCH falls from 170 to 41 Hz, ROUND from 110 to 36 Hz, ringing 0.8 and 1 s; 0.1's second
  build, 3 dB more below 50 Hz in the mix) with the body starting at phase 0 and its own beater click, the snare and clap with shaped noise, the hats
  from high-passed metallic partials. Stored at 44.1 kHz (the rest of Felucca's sets are 22.05) and kept whole, with
  no fade, so the attack is the first sample. VEL reaches AMP through the matrix's slot 4. DRUM swaps in Felucca's
  DRUM engine kits (TIGHT, 80s).
- **The robot is the VOICE engine**: full buzz, a high Q, two vowels a note in time with the tempo (VOWELS, SHIFT,
  TALK, GLIDE on the VOICE page). PHRASE sings AH EH OH EH AH EH EH EH, a vowel per note: the groove counts the
  lead's notes from each hook's start, and the firmware sets the VOICE engine's two vowels just before each note-on
  (this note's, gliding into the next's).
- **Even levels.** `tests/da_audition.c` measures each sound's loudness alone; the trims bring them within a few dB,
  so turning a SOUND card does not jump. The kits sat 20 dB above the rest before the trims.
- **On the bar line.** A style or a song turned in comes in on the next bar, so a groove never changes mid-bar;
  the PRESETS list shows while turning, NEXT on the footer until it lands. `sound_sig` bumps only on a style or a set,
  so the firmware reloads every part then and only the changed part otherwise.
- **The keys snap to the scale.** Whatever is pressed is in key; the hook makes way while a key is held. The LEDs
  show the chord's notes.
- **DROP on REC, SOLO on SEQ.** Tapped, DROP lasts two bars and comes back on the bar line; held, it stays until let
  go. SOLO plays SOLO's bars from the next beat, on the style's solo sound, in the scale.
- **A song is a seed.** ALGORITHM's number seeds the chord loop, the bass line and the hook, so a song always comes
  back the same (the groove test checks it note for note).
- **No libm, no doubles**: floats on the FPU (`-mcpu=r3 -mfprev1`), `sinf` and friends from `tuna_libm.c`.
- **Settings save themselves while stopped**, in Felucca's first project object ("DAS1"); the eight sets in the
  second. The record only grows at its end, so older settings still load; a bad record is fixed field by field.
- **NEBULA**: deep indigo, electric blue, hot magenta, white with a hint of lilac: chrome under neon.

## Measured on the host

40 s of each style with the arc, through Felucca's engines (`tests/da_felucca.sh`): peaks between -4.6 and
-3.3 dBFS, nothing clipped, the eight voices full at the busiest moments. The mix costs 85 to 160 ns a sample on
this host (FILTER HOUSE the least, DISCO the most), in the range of Tuna's pieces; the real number comes from the
FM-1. The browser build boots, plays and draws the same screens.

## Plan

1. **On the FM-1.** It installs, boots and plays (the deep kicks, the robot). Still to measure: the CPU at the
   busiest style over a long session, the settings and sets surviving a power cycle, MIDI out.
2. **The sound.** Tune the trims and the sweep's range by ear on the FM-1's speaker and on headphones; a second
   kick for SESSION if the 80s kit is not enough.
3. **The browser.** The Try page on the site, as Tuna's.
4. **More styles?** Only if Daurade earns it: the record has room for them.
