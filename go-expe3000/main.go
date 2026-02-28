package main

import (
	"flag"
	"fmt"
	"os"
	"runtime"
	"strings"
	"time"

	"github.com/Zyko0/go-sdl3/bin/binimg"
	"github.com/Zyko0/go-sdl3/bin/binsdl"
	"github.com/Zyko0/go-sdl3/bin/binttf"
	"github.com/Zyko0/go-sdl3/sdl"
	"github.com/Zyko0/go-sdl3/ttf"
)

func init() {
	// SDL3 requires the main thread for some operations.
	runtime.LockOSThread()
}

func parseColor(s string) sdl.Color {
	var r, g, b, a uint8
	fmt.Sscanf(s, "%d,%d,%d,%d", &r, &g, &b, &a)
	return sdl.Color{R: r, G: g, B: b, A: a}
}

func main() {
	// Load binaries
	defer binsdl.Load().Unload()
	defer binimg.Load().Unload()
	defer binttf.Load().Unload()

	cfg := DefaultConfig()

	csvFile := flag.String("csv", "", "Stimulus CSV file")
	outputFile := flag.String("output", "results.csv", "Output CSV file")
	stimuliDir := flag.String("stimuli-dir", "", "Directory containing stimuli")
	startSplash := flag.String("start-splash", "", "Start splash image")
	endSplash := flag.String("end-splash", "", "End splash image")
	fontFile := flag.String("font", "", "TTF font file")
	fontSize := flag.Int("font-size", 24, "Font size")
	dlpDevice := flag.String("dlp", "", "DLP-IO8-G device")
	screenW := flag.Int("width", 1920, "Screen width")
	screenH := flag.Int("height", 1080, "Screen height")
	displayIdx := flag.Int("display", 0, "Display index")
	scaleFactor := flag.Float64("scale", 1.0, "Scale factor for stimuli")
	noVSync := flag.Bool("no-vsync", false, "Disable VSync")
	noFixation := flag.Bool("no-fixation", false, "Disable fixation cross")
	fullscreen := flag.Bool("fullscreen", false, "Enable fullscreen")
	bgColorStr := flag.String("bg-color", "0,0,0,255", "Background color (R,G,B,A)")
	textColorStr := flag.String("text-color", "255,255,255,255", "Text color (R,G,B,A)")
	fixColorStr := flag.String("fixation-color", "255,255,255,255", "Fixation color (R,G,B,A)")

	flag.Parse()

	cfg.CSVFile = *csvFile
	cfg.OutputFile = *outputFile
	cfg.StimuliDir = *stimuliDir
	cfg.StartSplash = *startSplash
	cfg.EndSplash = *endSplash
	cfg.FontFile = *fontFile
	cfg.FontSize = *fontSize
	cfg.DLPDevice = *dlpDevice
	cfg.ScreenWidth = *screenW
	cfg.ScreenHeight = *screenH
	cfg.DisplayIndex = *displayIdx
	cfg.ScaleFactor = float32(*scaleFactor)
	cfg.VSync = !*noVSync
	cfg.UseFixation = !*noFixation
	cfg.Fullscreen = *fullscreen
	cfg.BGColor = parseColor(*bgColorStr)
	cfg.TextColor = parseColor(*textColorStr)
	cfg.FixationColor = parseColor(*fixColorStr)

	if cfg.CSVFile == "" {
		fmt.Println("Error: CSV file is required.")
		flag.Usage()
		os.Exit(1)
	}

	if err := sdl.Init(sdl.INIT_VIDEO | sdl.INIT_AUDIO | sdl.INIT_EVENTS); err != nil {
		fmt.Printf("SDL_Init Error: %v\n", err)
		os.Exit(1)
	}
	defer sdl.Quit()

	if err := ttf.Init(); err != nil {
		fmt.Printf("TTF_Init Error: %v\n", err)
		os.Exit(1)
	}
	defer ttf.Quit()

	windowFlags := sdl.WINDOW_RESIZABLE
	if cfg.Fullscreen {
		windowFlags |= sdl.WINDOW_FULLSCREEN
	}

	window, renderer, err := sdl.CreateWindowAndRenderer("expe3000 (Go)", cfg.ScreenWidth, cfg.ScreenHeight, windowFlags)
	if err != nil {
		fmt.Printf("CreateWindowAndRenderer Error: %v\n", err)
		os.Exit(1)
	}
	defer window.Destroy()
	defer renderer.Destroy()

	if cfg.VSync {
		renderer.SetVSync(1)
	} else {
		renderer.SetVSync(0)
	}

	var font *ttf.Font
	if cfg.FontFile != "" {
		font, err = ttf.OpenFont(*fontFile, float32(*fontSize))
		if err != nil {
			fmt.Printf("Failed to load font: %s (%v)\n", *fontFile, err)
		}
	}
	defer func() {
		if font != nil {
			font.Close()
		}
	}()

	exp, err := LoadExperiment(cfg.CSVFile)
	if err != nil {
		fmt.Printf("Failed to load experiment: %v\n", err)
		os.Exit(1)
	}

	if len(exp.Stimuli) > 0 {
		lastStim := exp.Stimuli[len(exp.Stimuli)-1]
		cfg.TotalDuration = lastStim.TimestampMS + lastStim.DurationMS + 500
	}

	cache := NewResourceCache()
	defer cache.Destroy()

	resources, err := cache.Load(renderer, exp, font, cfg.TextColor, cfg.StimuliDir)
	if err != nil {
		fmt.Printf("Failed to load resources: %v\n", err)
		os.Exit(1)
	}

	mixer := NewAudioMixer()
	spec := &sdl.AudioSpec{Format: sdl.AUDIO_S16, Channels: 2, Freq: 44100}
	cb := sdl.NewAudioStreamCallback(mixer.Callback)
	stream := sdl.AUDIO_DEVICE_DEFAULT_PLAYBACK.OpenAudioDeviceStream(spec, cb)
	if stream == nil {
		fmt.Printf("Failed to open audio stream\n")
		os.Exit(1)
	}
	defer stream.Destroy()
	stream.ResumeDevice()

	var dlp *DLPIO8G
	if cfg.DLPDevice != "" {
		dlp, err = NewDLPIO8G(cfg.DLPDevice, 9600)
		if err != nil {
			fmt.Printf("Failed to initialize DLP device: %v\n", err)
		} else {
			defer dlp.Close()
		}
	}

	log := &EventLog{}

	if !DisplaySplash(renderer, cfg.StartSplash, cfg.ScreenWidth, cfg.ScreenHeight, cfg.ScaleFactor, cfg.BGColor) {
		return
	}

	success := RunExperiment(cfg, exp, resources, renderer, mixer, log, dlp, font)

	if success {
		DisplaySplash(renderer, cfg.EndSplash, cfg.ScreenWidth, cfg.ScreenHeight, cfg.ScaleFactor, cfg.BGColor)
	}

	timestamp := time.Now().Format("20060102-150405")
	outputName := strings.Replace(cfg.OutputFile, ".csv", "_"+timestamp+".csv", 1)
	if err := log.Save(outputName); err != nil {
		fmt.Printf("Failed to save event log: %v\n", err)
	} else {
		fmt.Printf("\nResults saved to %s\n", outputName)
	}
}
