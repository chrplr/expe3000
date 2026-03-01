package engine

import (
	"fmt"

	"github.com/Zyko0/go-sdl3/sdl"
	"github.com/Zyko0/go-sdl3/ttf"
)

func RunGuiSetup(cfg *Config) bool {
	if err := sdl.Init(sdl.INIT_VIDEO | sdl.INIT_EVENTS); err != nil {
		fmt.Printf("SDL_Init Error: %v\n", err)
		return false
	}
	defer sdl.Quit()

	if err := ttf.Init(); err != nil {
		fmt.Printf("TTF_Init Error: %v\n", err)
		return false
	}
	defer ttf.Quit()

	window, renderer, err := sdl.CreateWindowAndRenderer("expe3000 Setup (Go)", 800, 750, 0)
	if err != nil {
		fmt.Printf("CreateWindowAndRenderer Error: %v\n", err)
		return false
	}
	defer window.Destroy()
	defer renderer.Destroy()

	fontPath := GetDefaultFontPath()
	if fontPath == "" {
		fmt.Println("Error: No default font found for GUI setup")
		return false
	}
	guiFont, err := ttf.OpenFont(fontPath, 18)
	if err != nil {
		fmt.Printf("Failed to load GUI font: %v\n", err)
		return false
	}
	defer guiFont.Close()

	setupDone := false
	focusBox := -1 // 0: csv, 1: stimuliDir, 2: output
	
	type ResOption struct {
		W, H  int
		Label string
	}
	resOptions := []ResOption{
		{800, 600, "800x600 (SVGA)"},
		{1024, 768, "1024x768 (XGA)"},
		{1366, 1024, "1366x1024 (SXGA-)"},
		{1920, 1080, "1920x1080 (FHD)"},
		{2560, 1440, "2560x1440 (QHD)"},
		{3840, 2160, "3840x2160 (4K UHD)"},
	}
	selectedRes := 3 // Default to 1080p
	for i, res := range resOptions {
		if cfg.ScreenWidth == res.W && cfg.ScreenHeight == res.H {
			selectedRes = i
			break
		}
	}

	window.StartTextInput()
	defer window.StopTextInput()

	for !setupDone {
		var e sdl.Event
		for sdl.PollEvent(&e) {
			switch e.Type {
			case sdl.EVENT_QUIT:
				return false
			case sdl.EVENT_MOUSE_BUTTON_DOWN:
				me := e.MouseButtonEvent()
				mx, my := me.X, me.Y
				if mx >= 50 && mx <= 700 && my >= 50 && my <= 80 {
					focusBox = 0
				} else if mx >= 50 && mx <= 700 && my >= 120 && my <= 150 {
					focusBox = 1
				} else if mx >= 50 && mx <= 700 && my >= 190 && my <= 220 {
					focusBox = 2
				} else {
					focusBox = -1
				}

				if mx >= 710 && mx <= 780 {
					if my >= 50 && my <= 80 {
						filters := []sdl.DialogFileFilter{{Name: "CSV Files", Pattern: "csv"}}
						cb := sdl.NewDialogFileCallback(func(fileList []string, filter int32) {
							if len(fileList) > 0 {
								cfg.CSVFile = fileList[0]
							}
						})
						sdl.ShowOpenFileDialog(cb, window, filters, "", false)
					} else if my >= 120 && my <= 150 {
						cb := sdl.NewDialogFileCallback(func(fileList []string, filter int32) {
							if len(fileList) > 0 {
								cfg.StimuliDir = fileList[0]
							}
						})
						sdl.ShowOpenFolderDialog(cb, window, "", false)
					} else if my >= 190 && my <= 220 {
						cb := sdl.NewDialogFileCallback(func(fileList []string, filter int32) {
							if len(fileList) > 0 {
								cfg.OutputFile = fileList[0]
							}
						})
						sdl.ShowSaveFileDialog(cb, window, nil, "results.csv")
					}
				}

				for i := range resOptions {
					if mx >= 50 && mx <= 300 && my >= float32(260+i*40) && my <= float32(290+i*40) {
						selectedRes = i
					}
				}

				if mx >= 50 && mx <= 300 && my >= 520 && my <= 550 {
					cfg.UseFixation = !cfg.UseFixation
				}
				if mx >= 50 && mx <= 300 && my >= 570 && my <= 600 {
					cfg.Fullscreen = !cfg.Fullscreen
				}

				if mx >= 350 && mx <= 450 && my >= 650 && my <= 690 {
					if cfg.CSVFile != "" {
						cfg.ScreenWidth = resOptions[selectedRes].W
						cfg.ScreenHeight = resOptions[selectedRes].H
						cfg.SaveCache()
						setupDone = true
					}
				}
			case sdl.EVENT_TEXT_INPUT:
				ti := e.TextInputEvent()
				if focusBox != -1 {
					var target *string
					switch focusBox {
					case 0: target = &cfg.CSVFile
					case 1: target = &cfg.StimuliDir
					case 2: target = &cfg.OutputFile
					}
					*target += ti.Text
				}
			case sdl.EVENT_KEY_DOWN:
				ke := e.KeyboardEvent()
				if focusBox != -1 {
					if ke.Key == sdl.K_BACKSPACE {
						var target *string
						switch focusBox {
						case 0: target = &cfg.CSVFile
						case 1: target = &cfg.StimuliDir
						case 2: target = &cfg.OutputFile
						}
						if len(*target) > 0 {
							*target = (*target)[:len(*target)-1]
						}
					}
				}
			}
		}

		renderer.SetDrawColor(240, 240, 240, 255)
		renderer.Clear()
		black := sdl.Color{R: 0, G: 0, B: 0, A: 255}

		labels := []string{"Experiment CSV:", "Stimuli Directory:", "Output Results CSV:"}
		labelY := []float32{20, 90, 160}
		for i, label := range labels {
			surf, err := guiFont.RenderTextBlended(label, black)
			if err == nil && surf != nil {
				tex, err := renderer.CreateTextureFromSurface(surf)
				if err == nil {
					r := sdl.FRect{X: 50, Y: labelY[i], W: float32(surf.W), H: float32(surf.H)}
					renderer.RenderTexture(tex, nil, &r)
					tex.Destroy()
				}
				surf.Destroy()
			}
		}

		for i := 0; i < 3; i++ {
			renderer.SetDrawColor(255, 255, 255, 255)
			box := sdl.FRect{X: 50, Y: float32(50 + i*70), W: 650, H: 30}
			renderer.RenderFillRect(&box)
			if focusBox == i {
				renderer.SetDrawColor(0, 120, 255, 255)
			} else {
				renderer.SetDrawColor(180, 180, 180, 255)
			}
			renderer.RenderRect(&box)

			var text string
			switch i {
			case 0: text = cfg.CSVFile
			case 1: text = cfg.StimuliDir
			case 2: text = cfg.OutputFile
			}
			if text != "" {
				surf, err := guiFont.RenderTextBlended(text, black)
				if err == nil && surf != nil {
					tex, err := renderer.CreateTextureFromSurface(surf)
					if err == nil {
						r := sdl.FRect{X: 55, Y: float32(55 + i*70), W: float32(surf.W), H: float32(surf.H)}
						renderer.RenderTexture(tex, nil, &r)
						tex.Destroy()
					}
					surf.Destroy()
				}
			}

			renderer.SetDrawColor(200, 200, 200, 255)
			btn := sdl.FRect{X: 710, Y: float32(50 + i*70), W: 70, H: 30}
			renderer.RenderFillRect(&btn)
			renderer.SetDrawColor(0, 0, 0, 255)
			renderer.RenderRect(&btn)
			surf, err := guiFont.RenderTextBlended("...", black)
			if err == nil && surf != nil {
				tex, err := renderer.CreateTextureFromSurface(surf)
				if err == nil {
					r := sdl.FRect{X: 735, Y: float32(55 + i*70), W: float32(surf.W), H: float32(surf.H)}
					renderer.RenderTexture(tex, nil, &r)
					tex.Destroy()
				}
				surf.Destroy()
			}
		}

		for i, opt := range resOptions {
			renderer.SetDrawColor(255, 255, 255, 255)
			check := sdl.FRect{X: 50, Y: float32(260 + i*40), W: 20, H: 20}
			renderer.RenderFillRect(&check)
			renderer.SetDrawColor(0, 0, 0, 255)
			renderer.RenderRect(&check)
			if selectedRes == i {
				mark := sdl.FRect{X: 54, Y: float32(264 + i*40), W: 12, H: 12}
				renderer.SetDrawColor(0, 150, 0, 255)
				renderer.RenderFillRect(&mark)
			}
			surf, err := guiFont.RenderTextBlended(opt.Label, black)
			if err == nil && surf != nil {
				tex, err := renderer.CreateTextureFromSurface(surf)
				if err == nil {
					r := sdl.FRect{X: 80, Y: float32(260 + i*40), W: float32(surf.W), H: float32(surf.H)}
					renderer.RenderTexture(tex, nil, &r)
					tex.Destroy()
				}
				surf.Destroy()
			}
		}

		// Fixation checkbox
		renderer.SetDrawColor(255, 255, 255, 255)
		fixCheck := sdl.FRect{X: 50, Y: 520, W: 20, H: 20}
		renderer.RenderFillRect(&fixCheck)
		renderer.SetDrawColor(0, 0, 0, 255)
		renderer.RenderRect(&fixCheck)
		if cfg.UseFixation {
			mark := sdl.FRect{X: 54, Y: 524, W: 12, H: 12}
			renderer.SetDrawColor(0, 150, 0, 255)
			renderer.RenderFillRect(&mark)
		}
		surfFix, err := guiFont.RenderTextBlended("Show fixation cross", black)
		if err == nil && surfFix != nil {
			tex, err := renderer.CreateTextureFromSurface(surfFix)
			if err == nil {
				r := sdl.FRect{X: 80, Y: 520, W: float32(surfFix.W), H: float32(surfFix.H)}
				renderer.RenderTexture(tex, nil, &r)
				tex.Destroy()
			}
			surfFix.Destroy()
		}

		// Fullscreen checkbox
		renderer.SetDrawColor(255, 255, 255, 255)
		fullCheck := sdl.FRect{X: 50, Y: 570, W: 20, H: 20}
		renderer.RenderFillRect(&fullCheck)
		renderer.SetDrawColor(0, 0, 0, 255)
		renderer.RenderRect(&fullCheck)
		if cfg.Fullscreen {
			mark := sdl.FRect{X: 54, Y: 574, W: 12, H: 12}
			renderer.SetDrawColor(0, 150, 0, 255)
			renderer.RenderFillRect(&mark)
		}
		surfFull, err := guiFont.RenderTextBlended("Fullscreen mode", black)
		if err == nil && surfFull != nil {
			tex, err := renderer.CreateTextureFromSurface(surfFull)
			if err == nil {
				r := sdl.FRect{X: 80, Y: 570, W: float32(surfFull.W), H: float32(surfFull.H)}
				renderer.RenderTexture(tex, nil, &r)
				tex.Destroy()
			}
			surfFull.Destroy()
		}

		// Start button
		renderer.SetDrawColor(0, 150, 0, 255)
		startBtn := sdl.FRect{X: 350, Y: 650, W: 100, H: 40}
		renderer.RenderFillRect(&startBtn)
		white := sdl.Color{R: 255, G: 255, B: 255, A: 255}
		surfSt, err := guiFont.RenderTextBlended("START", white)
		if err == nil && surfSt != nil {
			tex, err := renderer.CreateTextureFromSurface(surfSt)
			if err == nil {
				r := sdl.FRect{X: 375, Y: 660, W: float32(surfSt.W), H: float32(surfSt.H)}
				renderer.RenderTexture(tex, nil, &r)
				tex.Destroy()
			}
			surfSt.Destroy()
		}

		renderer.Present()
		sdl.Delay(10)
	}

	return true
}
