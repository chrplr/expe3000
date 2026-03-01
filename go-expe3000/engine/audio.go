package engine

import (
	"sync"
	"unsafe"

	"github.com/Zyko0/go-sdl3/sdl"
)

const (
	MaxActiveSounds   = 16
	AudioScratchBytes = 4096
)

type ActiveSound struct {
	Resource *SoundResource
	PlayPos  uint32
	Active   bool
}

type AudioMixer struct {
	Slots   [MaxActiveSounds]ActiveSound
	Mutex   sync.Mutex
	Scratch []byte
}

func NewAudioMixer() *AudioMixer {
	return &AudioMixer{
		Scratch: make([]byte, AudioScratchBytes),
	}
}

func (m *AudioMixer) Callback(stream *sdl.AudioStream, additionalAmount, totalAmount int32) {
	remaining := int(additionalAmount)
	for remaining > 0 {
		chunk := remaining
		if chunk > AudioScratchBytes {
			chunk = AudioScratchBytes
		}

		// Clear scratch
		for i := 0; i < chunk; i++ {
			m.Scratch[i] = 0
		}

		m.Mutex.Lock()
		dst := unsafe.Slice((*int16)(unsafe.Pointer(&m.Scratch[0])), chunk/2)
		for i := 0; i < MaxActiveSounds; i++ {
			s := &m.Slots[i]
			if !s.Active {
				continue
			}

			soundRemaining := uint32(len(s.Resource.Data)) - s.PlayPos
			toMix := uint32(chunk)
			if toMix > soundRemaining {
				toMix = soundRemaining
			}

			src := unsafe.Slice((*int16)(unsafe.Pointer(&s.Resource.Data[s.PlayPos])), toMix/2)
			for j := range src {
				val := int32(dst[j]) + int32(src[j])
				if val > 32767 {
					val = 32767
				} else if val < -32768 {
					val = -32768
				}
				dst[j] = int16(val)
			}

			s.PlayPos += toMix
			if s.PlayPos >= uint32(len(s.Resource.Data)) {
				s.Active = false
			}
		}
		m.Mutex.Unlock()

		stream.PutData(m.Scratch[:chunk])
		remaining -= chunk
	}
}

func (m *AudioMixer) Play(res *SoundResource) bool {
	m.Mutex.Lock()
	defer m.Mutex.Unlock()

	for i := 0; i < MaxActiveSounds; i++ {
		if !m.Slots[i].Active {
			m.Slots[i].Resource = res
			m.Slots[i].PlayPos = 0
			m.Slots[i].Active = true
			return true
		}
	}
	return false
}
