#!/bin/bash
# Script to run the expe3000 example

# Get the directory where the script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

echo "Running expe3000 with default experiment.csv..."
./expe3000 experiment.csv --stimuli-dir assets

echo "Experiment finished. Press any key to close..."
read -n 1
