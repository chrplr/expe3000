package main

import (
	"encoding/csv"
	"fmt"
	"os"
	"strconv"
	"strings"
)

func LoadExperiment(path string) (*Experiment, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer f.Close()

	reader := csv.NewReader(f)
	records, err := reader.ReadAll()
	if err != nil {
		return nil, err
	}

	var stimuli []Stimulus
	for i, record := range records {
		if len(record) < 4 {
			continue // or return error
		}

		timestamp, err := strconv.ParseUint(record[0], 10, 64)
		if err != nil {
			return nil, fmt.Errorf("line %d: invalid timestamp: %v", i+1, err)
		}

		duration, err := strconv.ParseUint(record[1], 10, 64)
		if err != nil {
			return nil, fmt.Errorf("line %d: invalid duration: %v", i+1, err)
		}

		var stype StimType
		switch strings.ToLower(record[2]) {
		case "image":
			stype = StimImage
		case "sound":
			stype = StimSound
		case "text":
			stype = StimText
		default:
			return nil, fmt.Errorf("line %d: unknown stimulus type: %s", i+1, record[2])
		}

		stimuli = append(stimuli, Stimulus{
			TimestampMS: timestamp,
			DurationMS:  duration,
			Type:        stype,
			FilePath:    record[3],
		})
	}

	return &Experiment{Stimuli: stimuli}, nil
}
