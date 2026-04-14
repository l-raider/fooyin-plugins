# fooyin-plugins

Third-party plugins for the [Fooyin](https://github.com/fooyin/fooyin/) media player.

See [BUILD.md](BUILD.md) for build and installation instructions.

---

## Plugins

### AudioChecksum

MD5 checksum scanner for selected tracks, accessible via **right-click → Utilities → Audio Checksum…**. FLAC files are checked against the embedded STREAMINFO MD5. Results are shown in a sortable pass/fail table. Thread count is configurable under **Settings → Plugins → AudioChecksum**.

---

### ShortcutExtender

Adds bindable actions under **Settings → Shortcuts → Shortcut Extender**.

**Delete Currently Playing** — deletes the current track's file (trash or permanent), advancing playback first so the file handle is released. Optional confirmation dialog with a "don't ask again" option. Configure trash vs. permanent under **Settings → Plugins → Shortcut Extender**.

**FileOps Preset Shortcuts** — exposes each FileOps preset as a bindable shortcut. Per-preset options: track source (selection or now playing) and execution mode (confirm dialog or immediate). Configure under **Settings → Plugins → Shortcut Extender**.


