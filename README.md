# Sonora

An Audacious-style audio player for Linux, built with Qt 6 and GStreamer (C++17).
Multiple playlists, CUE sheets, LED level meters with true-peak detection, a spectrum analyser, a selectable
sound service, and media-key support.

Homepage: <https://github.com/przemyslawguza/sonora> - License: GPL-3.0-or-later (see `COPYING`).

## Features

**Playback**
* Formats: whatever your GStreamer plugins decode (MP3, FLAC, Ogg Vorbis/Opus, AAC/M4A, WAV, WavPack, APE, ...).
* Transport buttons like Audacious (Previous, Play, Pause, Stop, Next, Add, Repeat, Shuffle), drawn by the program so
  they are visible with any theme. Repeat, Shuffle, Previous and Next work inside the *filtered* view of the playing
  playlist.
* Elapsed / total time; click the time (or View > Show remaining time) for the remaining time.
* View > Dark mode.

**Playlists**
* Any number of playlists as tabs (`+`, Ctrl+T new, Ctrl+W close, F2 or double-click to rename, drag to reorder).
  Everything is restored on start.
* Playlist manager (View > Playlist manager, Ctrl+Y): all playlists with entry count and total length; click to
  show, double-click to play.
* Columns: title, artist, album, year, time, file type. Right-click the header to show/hide columns, click it to
  sort (ascending / descending / original order).
* Filter per playlist (Ctrl+F): all words must match title, artist, album, year or path. Enter plays the first
  match, Esc clears.
* Metadata tooltip on hover: artist, album, year, track/disc, genre, composer, length, codec, bitrate, sample rate,
  channels/bit depth, file size, path, comment. Tags are read in a background thread and cached on disk.
* Add Files / Folder (recursive) / URL, drag & drop, `sonora file-or-folder ...` on the command line.
* Import M3U / M3U8 / PLS (as new playlists or into the current one; relative paths are resolved).
* Export M3U8 (UTF-8), PLS or XSPF: whole playlist, only the entries shown, only the selected ones, or all playlists
  into a folder; absolute or relative paths; optional track information.

**CUE sheets**
* Add a `.cue` (file dialog, drag & drop, folder, or an M3U) and every track becomes a playlist entry with title,
  artist, album, year and genre from the sheet. The playlist may contain just the `.cue`; the audio file does not
  have to be listed. Time, seeking and the length of the last track are per track.
* Works with FLAC, APE, WavPack, WAV, MP3 ... images. The audio file is found even if the sheet names it with a
  different case, extension (`album.wav` vs `album.flac`) or a Windows path; UTF-8 and legacy code pages
  (Windows-1250/1251/1252, chosen by your language) are read.
* Adding a folder does not list an image twice (as one big file *and* as tracks).
* Exporting writes a cue sheet as one `.cue` line that other players understand (optional).

**Level meters and spectrum**
* Narrow LED ladder per channel (green, yellow, orange, red above 0 dB) with peak-hold LED; range, width, dB per LED
  and scale are configurable.
* True-peak (inter-sample) detection: the signal is oversampled to 192 kHz (at least 4x for 44.1/48 kHz material,
  as in ITU-R BS.1770), so the red LED lights when the waveform *between* samples exceeds 0 dB.
* Spectrum analyser: frequency range, linear/log axis, bar count, dB range and FFT size are configurable.
* Both displays are synchronised with the sound automatically: every result carries the stream time of the audio it
  describes and is shown when the audio sink has played up to that moment (the raw analysis runs 1.3 - 2.3 s ahead).
  Preferences > *Synchronisation* has one fine-tuning offset for both (-500 ... +4000 ms) for devices that play
  later than the computer thinks (Bluetooth, external amplifier).
* On pause both drop immediately and resume in step.

**Sound output** (File > Preferences)
* Sound service: Automatic, PipeWire, PulseAudio, ALSA, JACK, OSS ... (only installed ones are listed), output
  device per service, test tone, buffer/latency time, or a custom GStreamer sink (`alsasink device=hw:1,0`).
  Changing the output while playing keeps the position.

**Keyboard**

| Key | Action |
|---|---|
| Space | play / pause (or start playing when stopped) |
| X / C / V | play / pause / stop |
| Z / B | previous / next |
| Ctrl+Left / Ctrl+Right | seek -5 s / +5 s |
| Media keys (Play/Pause, Stop, Next, Previous) | as expected - while the window has focus, and through MPRIS2 over D-Bus also when it is in the background (GNOME, KDE, ...) |
| Ctrl+O / Ctrl+Shift+O / Ctrl+U | add files / folder / URL |
| Ctrl+I | import playlist as new |
| Ctrl+T / Ctrl+W / F2 | new / close / rename playlist |
| Ctrl+F, Esc | filter, clear filter |
| Delete | remove selected entries |
| Ctrl+Y | playlist manager |
| Ctrl+P | preferences |
| Ctrl+Q | quit |

Typing in the filter box is not affected by the single-letter keys.

## Defaults

All defaults live in `src/Config.h`; `load()` falls back to them and the Preferences texts are generated from them.

| Setting | Default |
|---|---|
| Spectrum analyser | 0 - 22 kHz linear, 224 bars, 4096 FFT bands, display -60 ... -20 dB |
| Level meters | -60 ... +1 dB, 0.33 dB per LED (about 186 LEDs), 10 px per channel, true-peak on |
| Meter / spectrum fine-tuning | 0 ms |

Saved settings override defaults: delete `~/.config/sonora/` to see new defaults.

## Build

Requirements: Qt 6 (Widgets, DBus), GStreamer 1.x with `gstreamer1.0-plugins-good` (the `level` and `spectrum`
elements), CMake >= 3.18, a C++17 compiler. The packages are built and targeted for Debian 13 and Ubuntu 26.04 LTS;
older distributions (e.g. Debian 12 with Qt 6.4) are not tested.

    sudo apt install build-essential debhelper cmake pkgconf qt6-base-dev libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev
    cmake -S . -B build && cmake --build build -j && ./build/sonora

## Debian package (Debian 13 "trixie", Ubuntu 26.04 LTS)

    ./build-deb.sh            # -> ../sonora_1.0.0_<arch>.deb

There is no architecture-specific code: run the same command on an amd64 or arm64 machine / chroot / container
(`sbuild`, `pbuilder`, or a Docker `arm64v8/debian:trixie` container with QEMU). Cross-compiling with `dpkg-buildpackage -aarm64`
additionally needs the Qt 6 host tools (`QT_HOST_PATH`); native builds are simpler.

Codecs come from the GStreamer plugin packages (`-good`, `-ugly`, `-bad`, `-libav`); the package recommends them
together with the ALSA, PulseAudio and PipeWire sinks.

## Troubleshooting

* `SONORA_DEBUG=1 sonora` prints player events (open, pre-roll, seek, EOS, errors) - useful when something does not
  play.
* `SONORA_PROFILE=1 sonora` prints start-up timings.

## Known limitations

* A cue sheet embedded inside a FLAC (`CUESHEET` tag) is not read; use an external `.cue`.
* No cover art, tag editing, equaliser or ReplayGain.
* Tracks that are separate files are not played gaplessly (tracks of one cue image switch without re-opening the file).
* The meters and spectrum rely on the sound sink reporting its playback position; if a sink does not, they fall back
  to a plain delay (the fine-tuning offset).
