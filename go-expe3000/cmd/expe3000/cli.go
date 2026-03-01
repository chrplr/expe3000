package main

import (
	"flag"
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
	cfg.BGColor = engine.ParseColor(*bgColorStr)
	cfg.TextColor = engine.ParseColor(*textColorStr)
	cfg.FixationColor = engine.ParseColor(*fixColorStr)

	engine.Run(cfg)
}
