package main

import (
	"os"
	"runtime"

	"expe3000/engine"
	"github.com/Zyko0/go-sdl3/bin/binimg"
	"github.com/Zyko0/go-sdl3/bin/binsdl"
	"github.com/Zyko0/go-sdl3/bin/binttf"
)

func init() {
	runtime.LockOSThread()
}

func main() {
	defer binsdl.Load().Unload()
	defer binimg.Load().Unload()
	defer binttf.Load().Unload()

	cfg := engine.DefaultConfig()
	cfg.LoadCache()

	// Default stimuli dir if empty
	if cfg.StimuliDir == "" {
		if _, err := os.Stat("assets"); err == nil {
			cfg.StimuliDir = "assets"
		}
	}

	if engine.RunGuiSetup(cfg) {
		engine.Run(cfg)
	}
}
