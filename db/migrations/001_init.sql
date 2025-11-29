-- Apply on an empty PostgreSQL database to recreate the existing schema.

BEGIN;

SET LOCAL search_path = public;

CREATE TABLE IF NOT EXISTS url_mapping (
    code        VARCHAR(16) PRIMARY KEY,
    url         TEXT        NOT NULL,
    expires_at  TIMESTAMPTZ
);

CREATE INDEX IF NOT EXISTS idx_url_mapping_expires_at
    ON url_mapping(expires_at);

COMMIT;
