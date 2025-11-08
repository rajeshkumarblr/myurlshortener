-- Initialize local Postgres for url-shortener app
-- Run this on your host Postgres (localhost:5432)

-- Adjust password as needed
DO $$
BEGIN
   IF NOT EXISTS (SELECT FROM pg_catalog.pg_roles WHERE rolname = 'app') THEN
      CREATE ROLE app LOGIN PASSWORD 'appsecret';
   END IF;
END
$$;

-- Create database if not exists
DO $$
BEGIN
   IF NOT EXISTS (SELECT FROM pg_database WHERE datname = 'urlshortener') THEN
      CREATE DATABASE urlshortener OWNER app;
   END IF;
END
$$;

-- Ensure privileges
ALTER DATABASE urlshortener OWNER TO app;
GRANT ALL PRIVILEGES ON DATABASE urlshortener TO app;
