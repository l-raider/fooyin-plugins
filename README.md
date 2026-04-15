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

### BPM Analyzer

- BPM scanner for selected tracks, accessible via **right-click → Utilities → BPM Analyzer…**.
- Decodes audio directly and feeds samples into SoundTouch's BPMDetect engine.
- Writes the result to the standard `BPM` tag.
- Configurable analysis length, BPM precision, concurrency, and aggregation method.

#### Aggregation methods

SoundTouch detects individual beat positions throughout the track and returns them as a list of timestamps with confidence strengths. The plugin converts each consecutive pair of beats into a BPM candidate:

$$\text{BPM} = \frac{60}{\Delta t \text{ (seconds between beats)}}$$

Because no two beats are perfectly spaced, there will be dozens or hundreds of slightly differing candidates. The aggregation method decides how those candidates are collapsed into a single reported value.

**Weighted Average** *(recommended)*  
Each candidate BPM is weighted by the mean *beat strength* of the two surrounding beats, as reported by SoundTouch. Stronger, more confident beats contribute more to the final result. This produces the most accurate value on most music because it naturally suppresses weak or ambiguous detections.

$$\text{BPM} = \frac{\sum_i w_i \cdot b_i}{\sum_i w_i}$$

**Mean** *(arithmetic average)*  
All candidate BPMs are summed and divided by their count, treating every beat interval equally regardless of detector confidence. Works well on very consistent, metronomic music (electronic, techno) but can be pulled off by outliers on live recordings or tracks with tempo drift.

**Median**  
Candidates are sorted and the middle value is taken. A handful of wildly wrong beat detections won't skew the result, making this a good choice for music with occasional irregular beats — jazz, live performances, or tracks where BPMDetect struggles with a section.

**Mode** *(most frequent integer BPM)*  
Candidates are rounded to the nearest integer BPM and binned by total weight. The bin with the highest accumulated weight wins, always producing a whole-number result. Best for libraries where you want clean round values and the music has a steady, fixed tempo.

---

### Shortcut Extender

- Adds new shorcuts (hotkeys) under **Settings → Shortcuts → Shortcut Extender**.  
  - **Delete Currently Playing**: Deletes the currently playing/paused track's file (trashbin or permanent).
  - **FileOps Preset Shortcuts**: Exposes each File Operation preset as a bindable shortcut.  
