# expe3000 (Go Version)

This is a port of the `expe3000` multimedia stimulus delivery system to Go, using the [go-sdl3](https://github.com/Zyko0/go-sdl3) bindings.

## Prerequisites

- Go 1.21 or later.
- SDL3 libraries (automatically handled by `binsdl` on most platforms, but ensure system dependencies for SDL3 are met).

## Building

```bash
cd go-expe3000
go build -o expe3000-go .
```

## Running

```bash
./expe3000-go -csv ../experiment.csv -stimuli-dir ../assets -font ../fonts/Inconsolata.ttf
```

### Options

- `-csv`: Path to the stimulus CSV file (required).
- `-stimuli-dir`: Directory containing image and sound assets.
- `-font`: Path to a TTF font file for text stimuli.
- `-output`: Path for the results CSV file (default: `results.csv`).
- `-width`, `-height`: Screen resolution (default: 1920x1080).
- `-fullscreen`: Run in fullscreen mode.
- `-no-vsync`: Disable VSync.
- `-no-fixation`: Disable the fixation cross.
- `-dlp`: Serial device path for DLP-IO8-G triggers.

## Key Changes from C Version

- **Software Mixer**: Implemented in Go for thread-safety and compatibility with `go-sdl3`.
- **Serial Communication**: Uses `go.bug.st/serial` for cross-platform support without CGo.
- **Resource Caching**: Uses a Go map for more efficient lookups.
- **CSV Parsing**: Uses the standard `encoding/csv` package.
