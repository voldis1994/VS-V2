from __future__ import annotations
from .metrics import classification_metrics

def evaluate_predictions(y_true, y_pred, y_prob=None):
    return classification_metrics(y_true, y_pred, y_prob)
