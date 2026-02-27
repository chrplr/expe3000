# expe3000

A high-precision multimedia stimulus delivery system built with C and SDL3. Designed for experimental psychology and neuroscience tasks requiring millisecond-accurate timing and low-latency audio.

## Features
- **Precise Timing:** High-resolution timing loop with VSYNC synchronization and predictive onset look-ahead (pre-rendering).
- **Low-Latency Audio:** Uses a manual mixing callback to keep the audio hardware "warm" and minimize startup delay.
- **Text Stimuli:** Support for rendering text via TTF fonts.
- **Unified Event Log:** Records stimulus onsets, offsets, and user responses in a single CSV file with a comprehensive metadata header.
- **Splashscreens:** Optional start and end screens that wait for user input.
- **Advanced Display Options:** Supports multiple monitors, custom resolutions, logical scaling, and magnification factors.
- **Auto-exit:** Automatically concludes the experiment after the last stimulus or a specified total duration.

## Installation

Pre-compiled binaries for Linux, macOS, and Windows are available in the [Releases](https://github.com/chrplr/expe3000/releases) section of the GitHub project.

For detailed installation and compilation instructions, please refer to the guide for your operating system:

- [Linux](INSTALL_Linux.md)
- [macOS](INSTALL_MACOSX.md)
- [Windows](INSTALL_Windows.md)

## Usage

```bash
./expe3000 <experiment_csv> [options]
```

### Options
- `-h, --help`: Show help message.
- `--output [file]`: Specify the output log file (default: `results.csv`).
- `--fixation`: Display a white center fixation cross between visual stimuli.
- `--fullscreen`: Run in fullscreen mode on the selected display.
- `--display [index]`: Select monitor index (default: 0).
- `--res [WxH]`: Set resolution (default: 1920x1080).
- `--scale [factor]`: Apply a magnifying factor to images (default: 1.0).
- `--start-splash [file]`: Display a PNG splashscreen at the start and wait for a keypress.
- `--end-splash [file]`: Display a PNG splashscreen at the end and wait for a keypress.
- `--total-duration [ms]`: Minimum duration for the experiment loop to run.
- `--font [file]`: Specify the TTF font file for text stimuli (optional, defaults to system Arial/Liberation).
- `--font-size [pt]`: Set the font size in points (default: 24).
- `--no-vsync`: Disable VSYNC synchronization (not recommended for precise timing).

### CSV Format
The experiment file should be a CSV (comma-separated) with the following columns:
`timestamp_ms,duration_ms,type,content`

**Types:** `IMAGE`, `SOUND`, `TEXT`

**Example (`experiment.csv`):**
```csv
# timestamp, duration, type, content
1000,500,IMAGE,assets/target.png
2000,0,SOUND,assets/beep.wav
3000,1500,TEXT,Please press the space bar
```
*Note: Use `0` duration for sounds.*

### Example Command
```bash
./expe3000 experiment.csv --fullscreen --fixation --font assets/Roboto.ttf --font-size 32 --start-splash assets/welcome.png
```

## Output
Upon exiting, the program generates a log file (default: `results.csv`) containing:
- **Metadata Header:** Detailed session info (date, user, host, command, OS, driver, renderer, resolution).
- **Event Log:**
  - `timestamp_ms`: The time of the event relative to the start of the experiment.
  - `event_type`: `IMAGE_ONSET`, `IMAGE_OFFSET`, `SOUND_ONSET`, `TEXT_ONSET`, `TEXT_OFFSET`, or `RESPONSE`.
  - `label`: The stimulus content/file path or the name of the key pressed.

## Control
- **Escape:** Interrupt the experiment and exit (results are still saved).
- **Any Key:** Advances splashscreens.



## License

This code was developped by Christophe Pallier <christophe@pallier.org> using Gemini3. OF course, all remaining bugs are the responsability of Gemini 3 ;-)

The code is distributed under the GNU LICENSE GPLv3.

The files in the `assets` folder are NOT public domain. The images were created by Minye Zhan (https://www.linkedin.com/in/minye-zhan-3b414626/) who retains all rights to them. Do not reuse without permission from her.
