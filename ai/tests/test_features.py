from ai.learning.features.structure import Bar, extract_structure_features
from ai.learning.features.momentum import extract_momentum_features
from ai.learning.features.pressure import extract_pressure_features


def test_structure_bias_bullish():
    bars = [Bar(1, 1.1, 0.9, 1.0), Bar(1.0, 1.2, 0.95, 1.15)]
    feat = extract_structure_features(bars)
    assert feat.structure_bias == "BULLISH"


def test_momentum_positive():
    bars = [Bar(1, 1.1, 0.9, 1.0), Bar(1.0, 1.2, 0.95, 1.1)]
    feat = extract_momentum_features(bars)
    assert feat.momentum_score > 0


def test_pressure_delta():
    bars = [Bar(1, 1.1, 0.9, 1.05), Bar(1.0, 1.2, 0.95, 1.1)]
    feat = extract_pressure_features(bars)
    assert -1.0 <= feat.pressure_delta <= 1.0
