package engine

import (
	"fmt"
	"os"
	"strings"

	"github.com/Zyko0/go-sdl3/sdl"
)

type Config struct {
	CSVFile        string
	OutputFile     string
	StimuliDir     string
	StartSplash    string
	EndSplash      string
	FontFile       string
	DLPDevice      string
	FontSize       int
	ScreenWidth    int
	ScreenHeight   int
	DisplayIndex   int
	ScaleFactor    float32
	TotalDuration  uint64
	UseFixation    bool
	Fullscreen     bool
	VSync          bool
	BGColor        sdl.Color
	TextColor      sdl.Color
	FixationColor  sdl.Color
}

func ParseColor(s string) sdl.Color {
	var r, g, b, a uint8
	fmt.Sscanf(s, "%d,%d,%d,%d", &r, &g, &b, &a)
	if a == 0 && s != "" && !strings.Contains(s, ",0") {
		a = 255
	}
	return sdl.Color{R: r, G: g, B: b, A: a}
}

const CacheFile = ".expe3000_cache"

func (cfg *Config) SaveCache() {
	f, err := os.Create(CacheFile)
	if err != nil {
		return
	}
	defer f.Close()

	fmt.Fprintf(f, "csv_file=%s\n", cfg.CSVFile)
	fmt.Fprintf(f, "output_file=%s\n", cfg.OutputFile)
	fmt.Fprintf(f, "stimuli_dir=%s\n", cfg.StimuliDir)
	fmt.Fprintf(f, "screen_w=%d\n", cfg.ScreenWidth)
	fmt.Fprintf(f, "screen_h=%d\n", cfg.ScreenHeight)
	if cfg.UseFixation {
		fmt.Fprintf(f, "use_fixation=1\n")
	} else {
		fmt.Fprintf(f, "use_fixation=0\n")
	}
	if cfg.Fullscreen {
		fmt.Fprintf(f, "fullscreen=1\n")
	} else {
		fmt.Fprintf(f, "fullscreen=0\n")
	}
	fmt.Fprintf(f, "bg_color=%d,%d,%d\n", cfg.BGColor.R, cfg.BGColor.G, cfg.BGColor.B)
	fmt.Fprintf(f, "text_color=%d,%d,%d\n", cfg.TextColor.R, cfg.TextColor.G, cfg.TextColor.B)
	fmt.Fprintf(f, "fixation_color=%d,%d,%d\n", cfg.FixationColor.R, cfg.FixationColor.G, cfg.FixationColor.B)
}

func (cfg *Config) LoadCache() {
	data, err := os.ReadFile(CacheFile)
	if err != nil {
		return
	}

	lines := strings.Split(string(data), "\n")
	for _, line := range lines {
		parts := strings.SplitN(line, "=", 2)
		if len(parts) != 2 {
			continue
		}
		key, val := parts[0], parts[1]
		val = strings.TrimSpace(val)

		switch key {
		case "csv_file":
			cfg.CSVFile = val
		case "output_file":
			cfg.OutputFile = val
		case "stimuli_dir":
			cfg.StimuliDir = val
		case "screen_w":
			fmt.Sscanf(val, "%d", &cfg.ScreenWidth)
		case "screen_h":
			fmt.Sscanf(val, "%d", &cfg.ScreenHeight)
		case "use_fixation":
			cfg.UseFixation = (val != "0")
		case "fullscreen":
			cfg.Fullscreen = (val != "0")
		case "bg_color":
			cfg.BGColor = ParseColor(val)
		case "text_color":
			cfg.TextColor = ParseColor(val)
		case "fixation_color":
			cfg.FixationColor = ParseColor(val)
		}
	}
}

func DefaultConfig() *Config {
	return &Config{
		OutputFile:    "results.csv",
		FontSize:      24,
		ScreenWidth:   1920,
		ScreenHeight:  1080,
		ScaleFactor:   1.0,
		UseFixation:   true,
		VSync:         true,
		BGColor:       sdl.Color{R: 0, G: 0, B: 0, A: 255},
		TextColor:     sdl.Color{R: 255, G: 255, B: 255, A: 255},
		FixationColor: sdl.Color{R: 255, G: 255, B: 255, A: 255},
	}
}
