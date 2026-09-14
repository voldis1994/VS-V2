-- Stage 10: safe V2 client/account/subscription migration markers + model tracking
-- Preserves existing clients/broker/account/settings; adds V2 authority metadata.

ALTER TABLE clients
  ADD COLUMN IF NOT EXISTS v2_migrated BOOLEAN NOT NULL DEFAULT true,
  ADD COLUMN IF NOT EXISTS v2_migrated_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  ADD COLUMN IF NOT EXISTS preferred_model_id VARCHAR(128),
  ADD COLUMN IF NOT EXISTS preferred_model_version VARCHAR(64),
  ADD COLUMN IF NOT EXISTS execution_authority VARCHAR(16) NOT NULL DEFAULT 'cpp';

COMMENT ON COLUMN clients.execution_authority IS 'cpp = C++ market-core authoritative; legacy = TS entry brain (disabled in production LIVE)';

ALTER TABLE account_instrument_settings
  ADD COLUMN IF NOT EXISTS v2_managed BOOLEAN NOT NULL DEFAULT true;

-- Ensure existing rows inherit V2-managed defaults without wiping settings
UPDATE clients
SET v2_migrated = true,
    v2_migrated_at = COALESCE(v2_migrated_at, NOW()),
    execution_authority = COALESCE(NULLIF(execution_authority, ''), 'cpp')
WHERE v2_migrated IS DISTINCT FROM true
   OR execution_authority IS NULL
   OR execution_authority = '';
