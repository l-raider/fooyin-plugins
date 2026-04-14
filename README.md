# fooyin-plugins

Third-party plugins for the [Fooyin](https://github.com/fooyin/fooyin/) media player.

See [BUILD.md](BUILD.md) for build and installation instructions.

## Plugins
Configurable under **Settings → Plugins → "plugin name"**. 

### Audio Checksum

- MD5 checksum scanner for selected tracks, accessible via **right-click → Utilities → Audio Checksum…**.  
- FLAC files are checked against the embedded STREAMINFO MD5.  
- Configureable tag name for checksum data
- Configureable concurrency for file processing
- Currently only tested on FLAC and MP3 files.  

### Shortcut Extender

- Adds new shorcuts (hotkeys) under **Settings → Shortcuts → Shortcut Extender**.  
  - **Delete Currently Playing**: Deletes the currently playing/paused track's file (trashbin or permanent).
  - **FileOps Preset Shortcuts**: Exposes each File Operation preset as a bindable shortcut.  
