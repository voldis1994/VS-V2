from __future__ import annotations

def exit_quality(mfe: float, realized: float) -> float:
    if mfe <= 0:
        return 0.0
    return max(0.0, min(1.0, realized / mfe))
