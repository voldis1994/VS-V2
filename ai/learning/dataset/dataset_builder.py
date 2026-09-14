
"""Build training datasets from feature episodes."""
from __future__ import annotations
import json
from pathlib import Path
from typing import Any

def build_dataset(episodes_dir: Path, output: Path) -> dict[str, Any]:
    rows = []
    if episodes_dir.exists():
        for f in sorted(episodes_dir.glob("*.json")):
            rows.append(json.loads(f.read_text()))
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps({"rows": rows, "count": len(rows)}, indent=2))
    return {"count": len(rows), "output": str(output)}
