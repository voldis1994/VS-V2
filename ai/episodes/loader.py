"""Load Stage-7 EpisodeStore JSON (*.episode.json) for learning."""
from __future__ import annotations

import json
from pathlib import Path
from typing import Any


EPISODE_SUFFIX = ".episode.json"
EPISODE_SCHEMA = "vs-v2-episode-1"


def list_episode_ids(root: Path) -> list[str]:
    if not root.exists():
        return []
    ids: list[str] = []
    for path in sorted(root.iterdir()):
        name = path.name
        if path.is_file() and name.endswith(EPISODE_SUFFIX):
            ids.append(name[: -len(EPISODE_SUFFIX)])
    return ids


def load_episode(path: Path) -> dict[str, Any]:
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, dict):
        raise ValueError(f"episode must be object: {path}")
    if "episode_id" not in data:
        raise ValueError(f"episode_id required: {path}")
    return data


def load_episodes(root: Path, *, sealed_only: bool = True) -> list[dict[str, Any]]:
    """Load episodes sorted by outcome.entry_ts then episode_id (stable, time-ordered)."""
    episodes: list[dict[str, Any]] = []
    for eid in list_episode_ids(root):
        ep = load_episode(root / f"{eid}{EPISODE_SUFFIX}")
        if sealed_only and not (ep.get("sealed") and ep.get("outcome", {}).get("sealed")):
            continue
        episodes.append(ep)
    episodes.sort(key=_episode_sort_key)
    return episodes


def _episode_sort_key(ep: dict[str, Any]) -> tuple[int, str]:
    outcome = ep.get("outcome") or {}
    entry_ts = int(outcome.get("entry_ts") or 0)
    return (entry_ts, str(ep.get("episode_id", "")))
