#!/usr/bin/env python3
"""Lightweight validation entrypoint used by CI."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from ai.validation.promotion_gate import promotion_gate


def main() -> int:
    result = promotion_gate(
        {
            "out_of_sample": True,
            "walk_forward": True,
            "monte_carlo": True,
            "probability_calibration": True,
        }
    )
    print(result)
    return 0 if result else 1


if __name__ == "__main__":
    raise SystemExit(main())
