CREATE TABLE IF NOT EXISTS model_registry (id BIGSERIAL PRIMARY KEY, name TEXT, version TEXT, status TEXT, metadata JSONB, created_at TIMESTAMPTZ DEFAULT NOW());
