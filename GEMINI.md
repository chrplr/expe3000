# GEMINI.md - expe3000 Project Context

This file provides instructional context for Gemini CLI interactions with the `expe3000` project.

## Project Overview
`expe3000` is a multimedia stimulus delivery system designed for experimental psychology and neuroscience tasks (e.g., fMRI, MEG, EEG). It is built using **C** and **SDL3**, focusing on millisecond-accurate timing and low-latency audio delivery.

### Key Technologies
- **SDL3**: Core framework for video, audio, and input.
- **SDL3_image & SDL3_ttf**: For loading images and rendering text.
- **CMake**: Build system.
- **DLP-IO8-G**: Supported hardware for serial-based triggers.

### Architecture
- **Main Loop (`src/main.c`)**: Manages the experiment lifecycle, including predictive stimulus onset (VSYNC-aware), event logging, and resource management.
- **CSV Parser (`src/csv_parser.c`)**: Reads a timed stimulus schedule from a CSV file (`timestamp_ms,duration_ms,type,content`).
- **Trigger Logic (`src/dlp.c`)**: Handles cross-platform (Linux/Windows) serial communication for external triggers.
- **Resource Caching**: Implements a simple cache in `main.c` to avoid redundant loading of stimuli.

## Building and Running

### Prerequisites
- SDL3, SDL3_image, and SDL3_ttf libraries.
- CMake (>= 3.16) and a C compiler (GCC, Clang, or MSVC).

### Commands
```bash
# Configure and Build
mkdir build && cd build
cmake ..
make

# Run the example experiment
./expe3000 ../experiment.csv --stimuli-dir ../assets
```

### Windows-Specific Build
Refer to `INSTALL_Windows.md` for MSYS2/MinGW-w64 instructions. Note that the project uses a custom `ldd`-based DLL collection script in GitHub Actions for packaging.

## Development Conventions

### Coding Style
- Follow the existing C style: K&R-like bracing, clear documentation headers for major sections, and logical naming.
- **Manual Audio Mixing**: Audio is handled via a callback (`audio_callback` in `main.c`) to maintain low latency. Do not use high-level SDL audio functions without consulting the mixer logic.

### Testing
- No automated test suite exists currently. Verify changes by running the provided `experiment.csv` and checking the output `results.csv`.
- **Timing Accuracy**: When modifying the rendering loop, ensure the predictive onset logic (`look_ahead_ms`) and VSYNC handling are preserved.

### Contribution Guidelines
- Ensure that `README.md` and `INSTALL_*.md` files are updated when changing dependencies or build processes.
- The `.github/workflows/build.yml` file is critical for releases; ensure it includes all required assets and binary artifacts.

## Current State & Issues
- **Windows Console Output**: On Windows, the application may behave as a `WinMain` app depending on the linker flags, potentially suppressing `stdout` if not run from a terminal that captures it or if linked as a GUI app.
- **DLP Integration**: The DLP device is initialized in `main.c` and used for onset/offset triggers.
