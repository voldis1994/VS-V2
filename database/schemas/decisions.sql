CREATE TABLE IF NOT EXISTS decisions (id BIGSERIAL PRIMARY KEY, instrument_id INT, action TEXT, confidence DOUBLE PRECISION, reason TEXT, created_at TIMESTAMPTZ DEFAULT NOW());
