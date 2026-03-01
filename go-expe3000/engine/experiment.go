package engine

import (
	"encoding/csv"
	"fmt"
	"os"
	"strconv"

	"github.com/Zyko0/go-sdl3/img"
	"github.com/Zyko0/go-sdl3/sdl"
	"github.com/Zyko0/go-sdl3/ttf"
)

type EventLogEntry struct {
	IntendedMS  uint64
	TimestampMS uint64
	Type        string
	Label       string
}

type EventLog struct {
	Entries []EventLogEntry
}

func (l *EventLog) Log(intended, actual uint64, stype, label string) {
	l.Entries = append(l.Entries, EventLogEntry{
		IntendedMS:  intended,
		TimestampMS: actual,
		Type:        stype,
		Label:       label,
	})
}

func (l *EventLog) Save(path string) error {
	f, err := os.Create(path)
	if err != nil {
		return err
	}
	defer f.Close()

	w := csv.NewWriter(f)
	defer w.Flush()

	w.Write([]string{"intended_ms", "actual_ms", "type", "label"})
	for _, e := range l.Entries {
		w.Write([]string{
			strconv.FormatUint(e.IntendedMS, 10),
			strconv.FormatUint(e.TimestampMS, 10),
			e.Type,
			e.Label,
		})
	}
	return nil
}

func DisplaySplash(renderer *sdl.Renderer, filePath string, screenW, screenH int, scaleFactor float32, bgColor sdl.Color) bool {
	if filePath == "" {
		return true
	}
	tex, err := img.LoadTexture(renderer, filePath)
	if err != nil {
		return true
	}
	defer tex.Destroy()

	tw, th, _ := tex.Size()
	dst := sdl.FRect{
		X: (float32(screenW) - tw*scaleFactor) / 2.0,
		Y: (float32(screenH) - th*scaleFactor) / 2.0,
		W: tw * scaleFactor,
		H: th * scaleFactor,
	}

	renderer.SetDrawColor(bgColor.R, bgColor.G, bgColor.B, bgColor.A)
	renderer.Clear()
	renderer.RenderTexture(tex, nil, &dst)
	renderer.Present()

	for {
		var event sdl.Event
		if err := sdl.WaitEvent(&event); err != nil {
			break
		}
		if event.Type == sdl.EVENT_QUIT {
			return false
		}
		if event.Type == sdl.EVENT_KEY_DOWN {
			break
		}
	}
	return true
}

const CrossSize = 20

func drawFixationCross(renderer *sdl.Renderer, w, h int, color sdl.Color) {
	renderer.SetDrawColor(color.R, color.G, color.B, color.A)
	mx, my := float32(w)/2, float32(h)/2
	renderer.RenderLine(mx-CrossSize, my, mx+CrossSize, my)
	renderer.RenderLine(mx, my-CrossSize, mx, my+CrossSize)
}

func RunExperiment(cfg *Config, exp *Experiment, resources []Resource, renderer *sdl.Renderer, mixer *AudioMixer, log *EventLog, dlp *DLPIO8G, font *ttf.Font) bool {
	rr := float32(60.0)
	win, _ := renderer.Window()
	display := sdl.GetDisplayForWindow(win)
	mode, err := display.CurrentDisplayMode()
	if err == nil && mode.RefreshRate > 0 {
		rr = mode.RefreshRate
	}
	fdMS := uint64(1000.0 / rr)
	laMS := fdMS / 2

	stTicks := sdl.Ticks()
	cs := 0
	avi := -1
	var vet uint64
	run := true
	aborted := false

	for run {
		ct := sdl.Ticks() - stTicks

		for {
			var ev sdl.Event
			if !sdl.PollEvent(&ev) {
				break
			}
			switch ev.Type {
			case sdl.EVENT_QUIT:
				run = false
				aborted = true
			case sdl.EVENT_KEY_DOWN:
				if ev.KeyboardEvent().Key == sdl.K_ESCAPE {
					run = false
					aborted = true
				} else {
					log.Log(ct, ct, "RESPONSE", ev.KeyboardEvent().Key.KeyName())
				}
			}
		}

		trig := false
		tidx := -1
		if cs < len(exp.Stimuli) && (ct+laMS) >= exp.Stimuli[cs].TimestampMS {
			s := &exp.Stimuli[cs]
			if (s.Type == StimImage || s.Type == StimText) && resources[cs].Texture != nil {
				avi = cs
				trig = true
				tidx = cs
				vet = ct + s.DurationMS
				if dlp != nil {
					if s.Type == StimImage {
						dlp.Set("1")
					} else {
						dlp.Set("3")
					}
				}
			} else if s.Type == StimSound && resources[cs].Sound.Data != nil {
				if mixer.Play(&resources[cs].Sound) {
					log.Log(s.TimestampMS, ct, "SOUND_ONSET", s.FilePath)
					if dlp != nil {
						dlp.Set("2")
						dlp.Delay(5)
						dlp.Unset("2")
					}
				}
			}
			cs++
			fmt.Printf("\rStimulus: %d/%d ", cs, len(exp.Stimuli))
			os.Stdout.Sync()
		}

		if avi != -1 && ct >= vet {
			intendedOff := exp.Stimuli[avi].TimestampMS + exp.Stimuli[avi].DurationMS
			label := exp.Stimuli[avi].FilePath
			stype := "IMAGE_OFFSET"
			if exp.Stimuli[avi].Type == StimText {
				stype = "TEXT_OFFSET"
			}
			log.Log(intendedOff, ct, stype, label)
			if dlp != nil {
				if exp.Stimuli[avi].Type == StimImage {
					dlp.Unset("1")
				} else {
					dlp.Unset("3")
				}
			}
			avi = -1
		}

		if cs >= len(exp.Stimuli) && avi == -1 && ct >= cfg.TotalDuration {
			run = false
		}

		renderer.SetDrawColor(cfg.BGColor.R, cfg.BGColor.G, cfg.BGColor.B, cfg.BGColor.A)
		renderer.Clear()
		if avi != -1 {
			r := &resources[avi]
			dr := sdl.FRect{
				X: (float32(cfg.ScreenWidth) - (r.W * cfg.ScaleFactor)) / 2.0,
				Y: (float32(cfg.ScreenHeight) - (r.H * cfg.ScaleFactor)) / 2.0,
				W: r.W * cfg.ScaleFactor,
				H: r.H * cfg.ScaleFactor,
			}
			renderer.RenderTexture(r.Texture, nil, &dr)
		} else if cfg.UseFixation {
			drawFixationCross(renderer, cfg.ScreenWidth, cfg.ScreenHeight, cfg.FixationColor)
		}
		renderer.Present()

		if trig {
			ot := sdl.Ticks() - stTicks
			label := exp.Stimuli[tidx].FilePath
			stype := "IMAGE_ONSET"
			if exp.Stimuli[tidx].Type == StimText {
				stype = "TEXT_ONSET"
			}
			log.Log(exp.Stimuli[tidx].TimestampMS, ot, stype, label)
			vet = ot + exp.Stimuli[tidx].DurationMS
		}

		if !cfg.VSync {
			sdl.Delay(1)
		}
	}

	return !aborted
}
