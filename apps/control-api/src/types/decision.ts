export interface Decision { instrument_id: number; action: string; buy_score: number; sell_score: number; confidence: number; reason: string; reason_codes: string[]; }
