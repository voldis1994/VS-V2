#!/usr/bin/env python3
from ai.validation.promotion_gate import promotion_gate
print(promotion_gate({'out_of_sample': True, 'walk_forward': True, 'monte_carlo': True, 'probability_calibration': True}))
