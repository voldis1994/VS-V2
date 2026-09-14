#!/usr/bin/env python3
from pathlib import Path
from ai.learning.training import train_baseline
train_baseline(Path('data/datasets/train/dataset.json'), Path('models/candidate/baseline.json'))
