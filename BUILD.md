# Building fooyin-plugins

## Requirements

- CMake 3.14+
- C++20 compiler (GCC 11+, Clang 13+)
- Ninja (`ninja-build`)
- Qt 6.4+ development files
- fooyin headers and cmake config (see below)

### Debian/Ubuntu dependencies

```bash
sudo apt install cmake ninja-build g++ qt6-base-dev libqt6svg6-dev
```

---

## Quick start (recommended — uses fooyin submodule)

This repository includes fooyin as a git submodule. The build script will
build and install fooyin's headers automatically on first run.

```bash
# Clone including the fooyin submodule
git clone --recurse-submodules <repo-url>
cd fooyin-plugins/audiochecksum

# Build (compiles fooyin from submodule on first run, then the plugin)
./build.sh
```

The compiled plugin `.so` will be in `audiochecksum/build/`.

If you already cloned without `--recurse-submodules`:

```bash
git submodule update --init
```

---

## Installing the plugin

### User install (no root required)

```bash
mkdir -p ~/.local/lib/fooyin/plugins
cp audiochecksum/build/libaudiochecksum.so ~/.local/lib/fooyin/plugins/
```

### System install via build script

```bash
cd audiochecksum
./build.sh --install   # writes to the fooyin plugin dir under CMAKE_INSTALL_PREFIX
```

---

## Build script options

Run from inside `audiochecksum/`:

| Option | Description |
|---|---|
| *(none)* | Auto-detects fooyin: submodule → `/usr/local` → `/usr` |
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
> Use the submodule approach or build fooyin from source.

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
though it strips the right number of bytes. Other formats (`S16`, `S32`)
are already at their native width and need no adjustment.

---

## Plugin output location

After a successful build the plugin library is at:

```
audiochecksum/build/libaudiochecksum.so
```

fooyin loads plugins from (checked in order):

1. `~/.local/lib/fooyin/plugins/`  — per-user
2. `/usr/local/lib/fooyin/plugins/` — system-wide
