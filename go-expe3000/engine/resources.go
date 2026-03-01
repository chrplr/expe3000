package engine

import (
	"fmt"
	"os"
	"path/filepath"
	"runtime"
	"strings"

	"github.com/Zyko0/go-sdl3/img"
	"github.com/Zyko0/go-sdl3/sdl"
	"github.com/Zyko0/go-sdl3/ttf"
)

func GetDefaultFontPath() string {
	// Check local fonts directory
	entries, err := os.ReadDir("fonts")
	if err == nil {
		for _, entry := range entries {
			if !entry.IsDir() {
				ext := strings.ToLower(filepath.Ext(entry.Name()))
				if ext == ".ttf" || ext == ".ttc" {
					return filepath.Join("fonts", entry.Name())
				}
			}
		}
	}

	// System paths
	var paths []string
	switch runtime.GOOS {
	case "windows":
		paths = []string{"C:\\Windows\\Fonts\\arial.ttf"}
	case "darwin":
		paths = []string{"/System/Library/Fonts/Helvetica.ttc"}
	default:
		paths = []string{
			"/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
			"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
		}
	}

	for _, p := range paths {
		if _, err := os.Stat(p); err == nil {
			return p
		}
	}

	return ""
}

type SoundResource struct {
	Data []byte
	Spec sdl.AudioSpec
}

type Resource struct {
	Texture *sdl.Texture
	W, H    float32
	Sound   SoundResource
}

type CacheEntry struct {
	Texture *sdl.Texture
	W, H    float32
	Sound   SoundResource
}

type ResourceCache struct {
	entries map[string]*CacheEntry
}

func NewResourceCache() *ResourceCache {
	return &ResourceCache{
		entries: make(map[string]*CacheEntry),
	}
}

func (c *ResourceCache) Load(renderer *sdl.Renderer, exp *Experiment, font *ttf.Font, textColor sdl.Color, stimuliDir string) ([]Resource, error) {
	resources := make([]Resource, len(exp.Stimuli))
	targetSpec := sdl.AudioSpec{Format: sdl.AUDIO_S16, Channels: 2, Freq: 44100}

	for i, s := range exp.Stimuli {
		key := fmt.Sprintf("%d:%s", s.Type, s.FilePath)
		if entry, ok := c.entries[key]; ok {
			resources[i] = Resource{Texture: entry.Texture, W: entry.W, H: entry.H, Sound: entry.Sound}
			continue
		}

		entry := &CacheEntry{}
		fullPath := filepath.Join(stimuliDir, s.FilePath)

		switch s.Type {
		case StimImage:
			tex, err := img.LoadTexture(renderer, fullPath)
			if err != nil {
				fmt.Printf("Failed to load image: %s (%v)\n", fullPath, err)
			} else {
				entry.Texture = tex
				w, h, _ := tex.Size()
				entry.W, entry.H = w, h
			}
		case StimSound:
			spec := &sdl.AudioSpec{}
			data, err := sdl.LoadWAV(fullPath, spec)
			if err != nil {
				fmt.Printf("Failed to load sound %s: %v\n", fullPath, err)
			} else {
				if spec.Format == targetSpec.Format && spec.Channels == targetSpec.Channels && spec.Freq == targetSpec.Freq {
					entry.Sound.Spec = *spec
					entry.Sound.Data = data
				} else {
					dstData, err := sdl.ConvertAudioSamples(spec, data, &targetSpec)
					if err != nil {
						fmt.Printf("Failed to convert sound %s: %v\n", fullPath, err)
						entry.Sound.Spec = *spec
						entry.Sound.Data = data
					} else {
						entry.Sound.Spec = targetSpec
						entry.Sound.Data = dstData
					}
				}
			}
		case StimText:
			if font != nil {
				surf, err := font.RenderTextBlended(s.FilePath, textColor)
				if err == nil && surf != nil {
					tex, err := renderer.CreateTextureFromSurface(surf)
					if err == nil {
						entry.Texture = tex
						entry.W = float32(surf.W)
						entry.H = float32(surf.H)
					}
					surf.Destroy()
				}
			}
		}

		c.entries[key] = entry
		resources[i] = Resource{Texture: entry.Texture, W: entry.W, H: entry.H, Sound: entry.Sound}
	}

	return resources, nil
}

func (c *ResourceCache) Destroy() {
	for _, entry := range c.entries {
		if entry.Texture != nil {
			entry.Texture.Destroy()
		}
	}
}
