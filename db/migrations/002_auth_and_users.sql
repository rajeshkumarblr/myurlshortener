-- Migration: v1 -> v2
-- Adds user accounts, API tokens, and ownership metadata for url mappings.

BEGIN;

-- Ensure helper extensions exist. Safe to re-run.
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";
CREATE EXTENSION IF NOT EXISTS "citext";

-- 1. Core user table storing profile + password hash (BCrypt/Argon2, etc.).
CREATE TABLE IF NOT EXISTS app_user (
    id            BIGSERIAL PRIMARY KEY,
    name          TEXT        NOT NULL,
    email         CITEXT      NOT NULL UNIQUE,
    password_hash TEXT        NOT NULL,
    created_at    TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at    TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- Trigger to keep updated_at in sync (optional but handy).
CREATE OR REPLACE FUNCTION trg_app_user_set_updated()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at := NOW();
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

DROP TRIGGER IF EXISTS trg_app_user_updated ON app_user;
CREATE TRIGGER trg_app_user_updated
BEFORE UPDATE ON app_user
FOR EACH ROW EXECUTE FUNCTION trg_app_user_set_updated();

-- 2. API tokens for programmatic access + rate limiting metadata.
CREATE TABLE IF NOT EXISTS api_token (
    id            BIGSERIAL PRIMARY KEY,
    user_id       BIGINT      NOT NULL REFERENCES app_user(id) ON DELETE CASCADE,
    token         UUID        NOT NULL DEFAULT uuid_generate_v4(),
    label         TEXT,
    created_at    TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    last_used_at  TIMESTAMPTZ,
    UNIQUE(token)
);

-- 3. Extend url_mapping so every short link ties back to a user and captures created_at.
ALTER TABLE url_mapping
    ADD COLUMN IF NOT EXISTS user_id BIGINT REFERENCES app_user(id) ON DELETE SET NULL,
    ADD COLUMN IF NOT EXISTS created_at TIMESTAMPTZ NOT NULL DEFAULT NOW();

-- Helpful composite index for listing a user's links.
CREATE INDEX IF NOT EXISTS idx_url_mapping_user_created
    ON url_mapping(user_id, created_at DESC);

COMMIT;
