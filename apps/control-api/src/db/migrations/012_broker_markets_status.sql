-- Persist last Capital markets pull outcome so Control Panel can show why catalog is empty.
ALTER TABLE broker_connections
  ADD COLUMN IF NOT EXISTS last_markets_error TEXT,
  ADD COLUMN IF NOT EXISTS last_markets_pulled_at TIMESTAMPTZ,
  ADD COLUMN IF NOT EXISTS last_markets_count INTEGER NOT NULL DEFAULT 0;
