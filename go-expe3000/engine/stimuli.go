package engine

type StimType int

const (
	StimImage StimType = iota
	StimSound
	StimText
	StimEnd
)

type Stimulus struct {
	TimestampMS uint64
	DurationMS  uint64
	Type        StimType
	FilePath    string
}

type Experiment struct {
	Stimuli []Stimulus
}
