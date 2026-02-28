package main

import (
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
