# URL Shortener Service

Production-ready Drogon (C++) service that issues short URLs behind authenticated APIs, persists data in PostgreSQL, and ships with a polished HTML dashboard plus automated smoke tests.

- **Live demo**: https://urlshortener-demo.westus3.cloudapp.azure.com
- **Latest deploy**: AKS cluster `aks-urlshortener` in `rg-urlshortener-wus3` (Azure West US 3)
- **Status**: ✅ Stable — backend, UI, and automated tests are all green (see `scripts/api_test_suite.py`).

## Highlights

- **Secure auth** – Registration/login APIs issue bearer tokens; Drogon filters enforce protected routes.
- **PostgreSQL storage** – Uses Drogon ORM with pooled connections, migrations, TTL enforcement, and metadata for each short code.
- **Modern UI** – `public/index.html` now offers a login-first console with gated navigation, instant validation, and link management tools.
- **Automated validation** – Python test suite hits every auth + shorten + redirect endpoint locally or against prod via `BASE_URL`.
- **Cloud deployment** – Single command (`k8s/deploy-aks.sh`) builds the container, pushes to Azure Container Registry, and rolls out to AKS with external Postgres wiring.

## Architecture Overview

```text
                 ┌─────────────────────────────┐
                 │  Browser / Static Console   │
                 │  (public/index.html)        │
                 └──────────────┬──────────────┘
                                │ HTTPS
                                ▼
                 ┌─────────────────────────────┐
                 │ Azure Load Balancer (AKS)   │
                 │ k8s Service: url-shortener  │
                 └──────────────┬──────────────┘
                                │ ClusterIP
                                ▼
                 ┌─────────────────────────────┐
                 │ AKS Node (Standard_B2s)     │
                 │  └─ Pod: Drogon container   │
                 │        - Controllers        │
                 │        - Services           │
                 │        - Static assets      │
                 └──────────────┬──────────────┘
                                │ libpq over TLS
                                ▼
            ┌────────────────────────────────────────┐
            │ Azure Database for PostgreSQL Flexible │
            │  - urlshortener schema                 │
            │  - managed backups + SSL               │
            └────────────────────────────────────────┘

ACR (Azure Container Registry) stores the built images that AKS pulls during rollouts,
and `k8s/deploy-aks.sh` wires the `external-postgres` secret carrying `DATABASE_URL`.
```

## Quick Start

```bash
# Clone and build
cmake -S . -B build
cmake --build build -j

# Provide Postgres connection
export DATABASE_URL='postgres://user:pass@host:5432/urlshortener?sslmode=require'

# Run Drogon server
./build/url_shortener

# Open UI
xdg-open http://localhost:9090/
```

### Configuration

`config.json` + environment variables drive runtime behavior:

| Setting | Description |
| --- | --- |
| `DATABASE_URL` | Required. Standard Postgres URI (set as env var or `database.url`). |
| `app.base_url` | Optional. Overrides host used when echoing `short` links (defaults to detected origin). |
| `database.pool_size` | Optional connection pool override; defaults to 4. |
| `jwt.secret`, `jwt.ttl_seconds` | Token signing secret + lifetime (configurable via Drogon config). |

## Using the APIs

All endpoints return JSON unless noted.

| Method | Path | Notes |
| --- | --- | --- |
| POST | `/api/v1/register` | `{"name","email","password"}` → `{ user_id, name, email, token }` |
| POST | `/api/v1/login` | Issues a fresh token for `{ "email","password" }` |
| POST | `/api/v1/shorten` | Auth required (`Authorization: Bearer <token>`). Body: `{ "url", "ttl"? }` |
| GET | `/api/v1/urls` | Auth required. Lists caller’s links with timestamps + expiry. |
| GET | `/api/v1/info/{code}` | Public metadata for any code. |
| GET | `/{code}` | Redirects to the stored destination. |
| GET | `/api/v1/health` | Health probe (checks DB connectivity). |

Register/login responses include a bearer token; send it via `Authorization: Bearer <token>` (or the legacy `x-api-key`) on authenticated routes.

### API Sequence Example (register → shorten → redirect)

```text
User Browser              Drogon API Pod               PostgreSQL
  |  POST /register        |                              |
  |----------------------->| create user + hash password |  INSERT INTO users
  |                        |---------------------------->|  (returns id)
  |                        |<----------------------------|  OK
  |<-----------------------| 201 {token}
  |
  |  POST /shorten (token) |                              |
  |----------------------->| validate + create code       |
  |                        |---------------------------->| INSERT INTO links
  |                        |<----------------------------| OK
  |<-----------------------| 201 {code, short URL}
  |
  |  GET /{code}           |                              |
  |----------------------->| lookup code                  |
  |                        |---------------------------->| SELECT ... WHERE code
  |                        |<----------------------------| destination URL
  |<-----------------------| 302 Location: original URL
```

```bash
TOKEN=$(curl -s -X POST http://localhost:9090/api/v1/register \
  -H 'Content-Type: application/json' \
  -d '{"name":"Demo","email":"demo@example.com","password":"SecretPwd1"}' | jq -r '.token')

curl -s -X POST http://localhost:9090/api/v1/shorten \
  -H "Authorization: Bearer ${TOKEN}" \
  -H 'Content-Type: application/json' \
  -d '{"url":"https://example.com","ttl":600}'

curl -s -H "Authorization: Bearer ${TOKEN}" http://localhost:9090/api/v1/urls
```

## Testing

```
python3 -m venv .venv && source .venv/bin/activate
pip install -r scripts/requirements.txt

# Local server
BASE_URL=http://localhost:9090 python scripts/api_test_suite.py

# Against prod (AKS)
BASE_URL=http://4.249.87.222 python scripts/api_test_suite.py
```

The suite performs health → register/login → shorten/list/info/redirect checks and fails fast on regressions.

## Deployment

```
DATABASE_URL='postgres://.../urlshortener?sslmode=require' \
  bash k8s/deploy-aks.sh
```

What the script does:

1. Ensures Azure resource group, AKS cluster, and ACR (Basic SKU) exist.
2. Builds the Docker image using the repo’s multi-stage Dockerfile.
3. Pushes to ACR and wires AKS with `external-postgres` secret containing `DATABASE_URL`.
4. Applies `k8s/deployment.yaml` + `k8s/service.yaml`, waits for rollout, and prints the LoadBalancer endpoint.

### Pulling secrets from Azure Key Vault

Keep the PostgreSQL URI out of local shells by storing it as a Key Vault secret and letting the script retrieve it automatically:

```bash
az keyvault create -n kv-urlshortener -g rg-urlshortener-wus3 -l westus3
az keyvault secret set -n pg-urlshortener -o tsv \
  --vault-name kv-urlshortener \
  --value 'postgres://user:pass@pg-urlshortener.postgres.database.azure.com:5432/urlshortener?sslmode=require'

KEYVAULT_NAME=kv-urlshortener KV_SECRET_NAME=pg-urlshortener \
  bash k8s/deploy-aks.sh
```

`DATABASE_URL` is still honored if exported, and `CREATE_PG=true` can provision a dev Postgres automatically. If neither applies, providing `KEYVAULT_NAME` + `KV_SECRET_NAME` is the preferred secure path.

Re-run the Python smoke suite pointing at the returned hostname or the friendly DNS (`urlshortener-demo.westus3.cloudapp.azure.com`).

## Repository Layout

| Path | Purpose |
| --- | --- |
| `src/` | Drogon services, controllers, helper utilities. |
| `public/index.html` | Auth-aware dashboard served by Drogon. |
| `k8s/` | Deployment + service manifests and AKS helper script. |
| `scripts/api_test_suite.py` | End-to-end verification suite. |

## License

MIT – see `LICENSE`.
