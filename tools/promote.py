#!/usr/bin/env python3
from ai.registry import ModelRegistry
from pathlib import Path
ModelRegistry(Path('models/production')).register('baseline', 'v1', {'status': 'candidate'})
