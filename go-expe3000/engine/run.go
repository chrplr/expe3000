package engine

import (
	"fmt"
	"os"
	"strings"
	"time"

	"github.com/Zyko0/go-sdl3/sdl"
	"github.com/Zyko0/go-sdl3/ttf"
)

func Run(cfg *Config) {
	if cfg.CSVFile == "" {
		fmt.Println("Error: CSV file is required.")
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
		font, err = ttf.OpenFont(cfg.FontFile, float32(cfg.FontSize))
		if err != nil {
			fmt.Printf("Failed to load font: %s (%v)\n", cfg.FontFile, err)
		}
	} else {
		fontPath := GetDefaultFontPath()
		if fontPath != "" {
			font, err = ttf.OpenFont(fontPath, float32(cfg.FontSize))
			if err != nil {
				fmt.Printf("Failed to load default font: %s (%v)\n", fontPath, err)
			}
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
