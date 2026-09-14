from __future__ import annotations
from .structure import Bar, extract_structure_features
from .momentum import extract_momentum_features
from .pressure import extract_pressure_features

def build_feature_vector(bars: list[dict] | list[Bar]) -> dict[str, float]:
    parsed: list[Bar] = []
    for b in bars:
        if isinstance(b, Bar):
            parsed.append(b)
        else:
            parsed.append(Bar(float(b["open"]), float(b["high"]), float(b["low"]), float(b["close"])))
    if not parsed:
        return {}
    s = extract_structure_features(parsed)
    m = extract_momentum_features(parsed)
    p = extract_pressure_features(parsed)
    return {
        "structure_bias": 1.0 if s.structure_bias == "BULLISH" else (-1.0 if s.structure_bias == "BEARISH" else 0.0),
        "break_of_structure": 1.0 if s.break_of_structure else 0.0,
        "momentum_score": float(getattr(m, "momentum_score", 0.0)),
        "pressure_delta": float(getattr(p, "pressure_delta", 0.0)),
    }
