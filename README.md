# expe3000

A multimedia stimulus delivery system built with C and SDL3. Designed for experimental psychology and neuroscience tasks requiring millisecond-accurate timing and low-latency audio. 

**Important Note**: stimuli are presented using a fixed, predifined, schedule. Alhtought keypress events are saved, the behavior of the prgroam can be modified on-line (,e.g. no feedback). This program is suitable for fMR/MEG/EEG experiment with rigid stimulus presentation schemes.


## Features
- **Precise Timing:** High-resolution timing loop with VSYNC synchronization and predictive onset look-ahead (pre-rendering).
- **Low-Latency Audio:** Uses a manual mixing callback to keep the audio hardware "warm" and minimize startup delay.
- **Text Stimuli:** Support for rendering text via TTF fonts.
- **Unified Event Log:** Records stimulus onsets, offsets, and user responses in a single CSV file with a comprehensive metadata header.
- **Splashscreens:** Optional start and end screens that wait for user input.
- **Advanced Display Options:** Supports multiple monitors, custom resolutions, logical scaling, and magnification factors.
- **Auto-exit:** Automatically concludes the experiment after the last stimulus or a specified total duration.

## Usage

```bash
./expe3000 <experiment_csv> [options]
```

The csv file must have three columns:
`timestamp_ms,duration_ms,type,content`

**Types:** `IMAGE`, `SOUND`, `TEXT`

**Example (`experiment.csv`):**
```csv
# timestamp, duration, type, content
1000,500,IMAGE,assets/target.png
2000,0,SOUND,assets/beep.wav
3000,1500,TEXT,Bye
```
*Note: Use `0` duration for sounds.*


### Options
- `-h, --help`: Show help message.
- `--output [file]`: Specify the output log file (default: `results.csv`).
- `--no-fixation`: remove the white center fixation cross.
- `--fullscreen`: Run in fullscreen mode on the selected display.
- `--display [index]`: Select monitor index (default: 0).
- `--res [WxH]`: Set resolution (default: 1920x1080).
- `--scale [factor]`: Apply a magnifying factor to images (default: 1.0).
- `--start-splash [file]`: Display a PNG splashscreen at the start and wait for a keypress.
- `--end-splash [file]`: Display a PNG splashscreen at the end and wait for a keypress.
- `--total-duration [ms]`: Minimum duration for the experiment loop to run.
- `--dlp [path]`: Path to the DLP-IO8-G device for triggers (e.g., `/dev/ttyUSB0` or `COM3`).
- `--font [file]`: Specify the TTF font file for text stimuli (optional, defaults to searching `fonts/` folder then system Arial/Liberation).
- `--font-size [pt]`: Set the font size in points (default: 24).
- `--no-vsync`: Disable VSYNC synchronization (not recommended for precise timing).


### Example Command
```bash
./expe3000 experiment.csv --stimuli-dir assets --fullscreen
```

### Quick Start with Examples
If you downloaded a pre-compiled release, you can quickly test the program using the provided example scripts:
- **Windows**: Double-click `run_example.bat`
- **Linux/macOS**: Run `./run_example.sh` in your terminal.

These scripts run the `expe3000` executable with the included `experiment.csv` and `assets` folder.

### Control
- **Escape:** Interrupt the experiment and exit (results are still saved).


### Output
Upon exiting, the program generates a log file (default: `results.csv`) containing:
- **Metadata Header:** Detailed session info (date, user, host, command, OS, driver, renderer, resolution).
- **Event Log:**
  - `timestamp_ms`: The time of the event relative to the start of the experiment.
  - `event_type`: `IMAGE_ONSET`, `IMAGE_OFFSET`, `SOUND_ONSET`, `TEXT_ONSET`, `TEXT_OFFSET`, or `RESPONSE`.
  - `label`: The stimulus content/file path or the name of the key pressed.



## Installing Binaries

Pre-compiled binaries for Linux, macOS, and Windows are available in the [Releases](https://github.com/chrplr/expe3000/releases) section of the GitHub project.

### Dependencies

The pre-compiled binaries for **Windows** are self-contained and include all necessary DLLs. 

For **Linux** and **macOS**, you need to have the SDL3 libraries installed on your system to run the program:

* *MacOS*. Install via [Homebrew](https://brew.sh/):
   ```bash
   brew install sdl3 sdl3_image sdl3_ttf
   ```
* *Linux*. Install via your package manager (if available):
      - **Ubuntu 24.10+ / Debian Trixie+**: `sudo apt install libsdl3-0 libsdl3-image-0 libsdl3-ttf-0`
      - **Fedora 41+**: `sudo dnf install sdl3 sdl3_image sdl3_ttf`
      - **Arch Linux**: `sudo pacman -S sdl3 sdl3_image sdl3_ttf`

   *Note: If your distribution does not yet provide SDL3 packages, you may need to compile them from source(INSTALL_Linux.md#3-option-b-compiling-sdl3-from-source-universal).*

## Compilation

For detailed installation and compilation instructions, please refer to the guide for your operating system:

- [Linux](INSTALL_Linux.md)
- [macOS](INSTALL_MACOSX.md)
- [Windows](INSTALL_Windows.md)





## License

This code was developed by Christophe Pallier <christophe@pallier.org> using Gemini3. Of course, all remaining bugs are entirely the responsibility of Gemini 3 ;-)

The code is distributed under the GNU LICENSE GPLv3.

The files in the `assets` folder are NOT public domain. The images were created by Minye Zhan (https://www.linkedin.com/in/minye-zhan-3b414626/) who retains all rights to them. Do not reuse without permission from her.
