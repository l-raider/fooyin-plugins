# fooyin-plugins

Third-party plugins for the [Fooyin](https://github.com/fooyin/fooyin/) media player.

See [BUILD.md](BUILD.md) for build and installation instructions.

---

## Plugins

### AudioChecksum

Calculates and verifies audio checksums for selected tracks via a right-click context menu entry ("Audio Checksum…" under Utilities).

- Computes a canonical MD5 fingerprint of the decoded audio stream, independent of tags or container metadata
- FLAC files are verified against the embedded STREAMINFO MD5 so results match the values written by flac/metaflac
- Other formats are normalised to pcm_s16le before hashing, matching FFmpeg's `md5` muxer output
- Scanning runs in parallel using a configurable thread pool
- Results are shown in a sortable table with per-track pass/fail status
- Configurable via Settings → Plugins → AudioChecksum (thread count)

---

### ShortcutExtender

Adds keyboard-shortcut-driven actions that go beyond fooyin's built-in shortcut system.

**Delete Currently Playing**

- Registers a bindable "Delete currently playing file" action (Settings → Shortcuts → Shortcut Extender)
- Supports move-to-trash or permanent delete, configurable in Settings → Plugins → Shortcut Extender
- Advances playback to the next track (or stops) before deleting so the audio engine releases its file handle
- Optional confirmation dialog; "Do not ask again" checkbox suppresses it permanently
- Safe inside Flatpak: trash operations write to the host `~/.local/share/Trash` rather than the app-private container

**FileOps Preset Shortcuts**

- Reads presets defined in the [FileOps](https://github.com/fooyin/fooyin/) plugin and registers a bindable shortcut for each one
- Each preset action can be configured independently (Settings → Plugins → Shortcut Extender):
  - *Track source* — operate on the currently selected tracks or the currently playing track
  - *Execution mode* — show a preview/confirm dialog before running, or execute immediately
- The confirm dialog lists every file operation (source → destination) before committing
- Supports all FileOps operation types: Copy, Move, Rename
