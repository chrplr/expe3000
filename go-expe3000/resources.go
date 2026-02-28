package main

import (
	"fmt"
	"path/filepath"

	"github.com/Zyko0/go-sdl3/img"
	"github.com/Zyko0/go-sdl3/sdl"
	"github.com/Zyko0/go-sdl3/ttf"
)

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
