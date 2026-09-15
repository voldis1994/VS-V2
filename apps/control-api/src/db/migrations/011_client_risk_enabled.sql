-- Per-client admin risk lock (client web cannot START when false)
ALTER TABLE clients
  ADD COLUMN IF NOT EXISTS risk_enabled BOOLEAN NOT NULL DEFAULT true;

COMMENT ON COLUMN clients.risk_enabled IS 'Admin risk override. When false, client panel cannot start robot.';
