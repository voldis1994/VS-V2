
from __future__ import annotations
import json
from pathlib import Path
from typing import Any

class ModelRegistry:
    def __init__(self, root: Path):
        self.root = root
        self.root.mkdir(parents=True, exist_ok=True)

    def register(self, name: str, version: str, metadata: dict[str, Any]) -> Path:
        dest = self.root / name / version / "metadata.json"
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_text(json.dumps(metadata, indent=2))
        return dest

    def list_versions(self, name: str) -> list[str]:
        p = self.root / name
        if not p.exists():
            return []
        return sorted(x.name for x in p.iterdir() if x.is_dir())
