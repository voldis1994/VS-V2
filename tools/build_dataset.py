#!/usr/bin/env python3
from pathlib import Path
from ai.learning.dataset import build_dataset
import sys
build_dataset(Path('data/episodes'), Path('data/datasets/train/dataset.json'))
