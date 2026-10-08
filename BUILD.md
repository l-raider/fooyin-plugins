# Building fooyin-plugins

## Requirements

- CMake 3.14+
- C++23 compiler (GCC 13+, Clang 16+)
- Ninja (`ninja-build`)
- Qt 6.4+ development files
- fooyin **0.13.0 or newer** headers and CMake config (see below)
- taglib-devel ffmpeg-devel (needed to build fooyin itself)

### Debian/Ubuntu dependencies

```bash
sudo apt install cmake ninja-build g++ qt6-base-dev libqt6svg6-dev
```

---

## Quick start (recommended — sibling fooyin checkout)

The build scripts look for a fooyin checkout next to this repository
(`../fooyin`). If found, they build fooyin with `-DINSTALL_HEADERS=ON` into
`../fooyin/build/install` and build the plugin against it:

```bash
git clone --recurse-submodules <repo-url>
git clone <fooyin-url> ../fooyin        # sibling checkout

cd fooyin-plugins/audiochecksum
./build.sh                              # builds fooyin if needed, then the plugin
```

The compiled plugin module is placed in `audiochecksum/build/`
(`fyplugin_audiochecksum.so`).

`bpmanalyzer` also needs its SoundTouch submodule; `build.sh` initialises it
automatically when missing:

```bash
cd fooyin-plugins/bpmanalyzer
./build.sh
```

---

## Installing the plugin

### User install (no root required)

```bash
mkdir -p ~/.local/lib/fooyin/plugins
cp audiochecksum/build/fyplugin_audiochecksum.so ~/.local/lib/fooyin/plugins/
```

### System install via build script

```bash
cd audiochecksum
./build.sh --install   # writes to the fooyin plugin dir under CMAKE_INSTALL_PREFIX
```

---

## Build script options

Run from inside `audiochecksum/` or `bpmanalyzer/`:

| Option | Description |
|---|---|
| *(none)* | Auto-detects fooyin: `../fooyin` checkout → `/usr/local` → `/usr` |
| `--prefix /path` | Use a specific fooyin install prefix |
| `--install` | Install the plugin after building |
| `--debug` | Build in Debug mode (default: Release) |
| `--clean` | Delete the `build/` directory before configuring |
| `--help` | Show usage summary |

---

## Using a system-installed or custom fooyin

If fooyin is already installed with development headers
(`-DINSTALL_HEADERS=ON`), point `--prefix` at its install location:

```bash
# Example: fooyin installed to /usr/local
./build.sh --prefix /usr/local
```

To install fooyin from source with headers:

```bash
cmake -S fooyin -G Ninja -B /tmp/fooyin-build \
      -DINSTALL_HEADERS=ON \
      -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build /tmp/fooyin-build
sudo cmake --install /tmp/fooyin-build

cd audiochecksum
./build.sh --prefix /usr/local
```

> **Note:** The Flathub fooyin package does not include development headers.
> Use a fooyin checkout/install with headers, or build fooyin from source.

### Minimum fooyin version

The plugins require fooyin **0.13.0** or newer. They use
`AudioDecoder::readAudio()` and `requestAbort()` (available earlier), plus
`AudioDecoder::VerifyIntegrity` and the CUE segment properties
(`_CUE_INDEX01_SECTOR`/`_CUE_END_SECTOR`), which were added in 0.13.0.

---

## Developer notes

### Audio checksum and sample format packing

The FLAC STREAMINFO MD5 is computed over raw PCM samples packed at their
**native byte width** in little-endian order (see `format_input_()` in
[libFLAC/md5.c](https://github.com/xiph/flac/blob/master/src/libFLAC/md5.c)).
For 24-bit FLAC this means **3 bytes per sample**, tightly packed as
`[LSB, MID, MSB]` of the 24-bit value.

FFmpeg (used by fooyin's decoder) decodes 24-bit FLAC as `S24In32` — each
24-bit sample is stored **left-aligned** in a 32-bit word (`value << 8`).
On a little-endian machine the 4-byte layout is `[0x00, LSB, MID, MSB]`.

To match the FLAC reference, `S24In32` buffers must be repacked to 3
bytes/sample by **skipping byte 0** (the zero pad) and copying bytes 1–3
of each 32-bit word. Copying bytes 0–2 instead produces a wrong MD5 even
though it strips the right number of bytes.

The FLAC-canonical path is only used when the decoded sample format matches
the stream's native bit depth (`S16`↔16, `S24In32`↔24, `S32`↔32, `U8`↔8).
Other depths (e.g. 20-bit FLAC, decoded as `S32`) fall back to the computed
`MD5 (S16)` value and are not compared against STREAMINFO.

---

## Plugin output location

After a successful build the plugin modules are at:

```
audiochecksum/build/fyplugin_audiochecksum.so
bpmanalyzer/build/fyplugin_bpmanalyzer.so
```

fooyin loads plugins from (checked in order):

1. `~/.local/lib/fooyin/plugins/`  — per-user
2. `/usr/local/lib/fooyin/plugins/` — system-wide
