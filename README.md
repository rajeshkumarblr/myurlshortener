# URL Shortener Service

Production-ready **Java Spring Boot** service that issues short URLs behind authenticated APIs, persists data in PostgreSQL, and ships with a polished HTML dashboard plus automated smoke tests.

- **Live demo**: https://myurlshortener.westus3.cloudapp.azure.com
- **Latest deploy**: AKS cluster `aks-urlshortener` in `rg-urlshortener-wus3` (Azure West US 3)
- **Status**: ✅ Stable — backend, UI, and automated tests are all green (see `test_api.sh`).

## Highlights

- **Secure auth** – Registration/login APIs issue JWT bearer tokens; Spring Security filters enforce protected routes.
- **PostgreSQL storage** – Self-hosted PostgreSQL (Helm) running in-cluster for a fully self-contained deployment.
- **Redis Caching** – High-performance URL resolution using the Cache-Aside pattern to minimize database load.
- **Modern UI** – Frontend built with **ReactJS** and **Vite**, offering a responsive dashboard, secure authentication, and real-time link management.
- **Automated validation** – Bash test suite (`test_api.sh`) hits every auth + shorten + redirect endpoint locally or against prod via `BASE_URL`.
- **Cloud deployment** – Dockerized application deployed to Azure Kubernetes Service (AKS) with Nginx Ingress.

## Architecture Overview

```mermaid
graph TD
    User[User / Browser] -->|HTTPS| Ingress[Nginx Ingress Controller]
    Ingress -->|Routing| Service[K8s Service: url-shortener]
    Service -->|Load Balance| Pod[Spring Boot Pods]
    
    subgraph Kubernetes Cluster
        Pod -->|Read/Write| DB[(PostgreSQL 16)]
        Pod -->|Cache Aside| Redis[(Redis Cache)]
    end
    
    style Pod fill:#6db33f,stroke:#333,stroke-width:2px,color:white
    style DB fill:#336791,stroke:#333,stroke-width:2px,color:white
    style Redis fill:#d82c20,stroke:#333,stroke-width:2px,color:white
    style Ingress fill:#4053d6,stroke:#333,stroke-width:2px,color:white
```

- **Language:** Java 21
- **Framework:** Spring Boot 3.2.3
- **Frontend:** ReactJS + Vite
- **Database:** PostgreSQL 16
- **Cache:** Redis
- **Containerization:** Docker
- **Orchestration:** Kubernetes (AKS)

## Quick Start

### Prerequisites
- Java 21
- Maven
- Docker (optional)

### Local Development

1.  **Build with Maven**
    ```bash
    ./mvnw clean package
    ```

2.  **Run Dependencies (Docker Compose or Local)**
    Ensure you have PostgreSQL running on port 5432 and Redis on port 6379.

3.  **Run Application**
    ```bash
    export SPRING_DATASOURCE_URL='jdbc:postgresql://localhost:5432/urlshortener'
    export SPRING_DATASOURCE_USERNAME='postgres'
    export SPRING_DATASOURCE_PASSWORD='password'
    
    java -jar target/urlshortener-0.0.1-SNAPSHOT.jar
    ```

4.  **Access UI**
    Open http://localhost:9090/

## Configuration

`application.properties` + environment variables drive runtime behavior:

| Setting | Description |
| --- | --- |
| `SPRING_DATASOURCE_URL` | Required. JDBC URL for Postgres. |
| `SPRING_DATASOURCE_USERNAME` | Database username. |
| `SPRING_DATASOURCE_PASSWORD` | Database password. |
| `REDIS_HOST` | Optional. Redis hostname (defaults to `localhost`). |
| `APP_BASE_URL` | Optional. Overrides host used when echoing `short` links. |
| `JWT_SECRET` | JWT signing secret. |

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

## Testing

We use a comprehensive Bash script to verify the API functionality.

```bash
# Run against Production (Default)
./test_api.sh

# Run against Localhost
# Uncomment BASE_URL="http://localhost:9090" in test_api.sh
./test_api.sh
```

The suite performs:
1.  **Health Check**: Verifies server is online.
2.  **Smoke Test**: Checks if `index.html` loads (HTTP 200).
3.  **Auth Flow**: Registers a new random user and retrieves a JWT token.
4.  **Protected Routes**: Shortens a URL and lists user URLs using the token.
5.  **Public Routes**: Verifies redirection and info endpoints.
6.  **Negative Tests**: Checks 401 Unauthorized and 404 Not Found scenarios.

## Deployment

The application is deployed to Azure Kubernetes Service.

### Key Manifests (in `k8s/`)
- `deployment.yaml`: Defines the application pods, environment variables, and resource limits.
- `service.yaml`: Exposes the application internally.
- `ingress.yaml`: Configures Nginx Ingress to route traffic from the public IP.

### Database Access (Secure)

To access the production PostgreSQL database securely from your local machine (e.g., using VS Code or pgAdmin), use **Kubernetes Port Forwarding**:

1.  **Start the Tunnel**
    ```bash
    # Forward remote port 5432 to local port 5433 (to avoid conflict with local Postgres)
    kubectl port-forward svc/postgres-postgresql 5433:5432
    ```

2.  **Connect via CLI**
    ```bash
    # Connect using the 'postgres' user and the production password
    psql 'postgres://postgres:UrlShortPass2025@localhost:5433/urlshortener'
    ```

3.  **Connect via GUI**
    - **Host:** `localhost`
    - **Port:** `5433`
    - **User:** `postgres`
    - **Password:** `UrlShortPass2025`
    - **Database:** `urlshortener`

## Monitoring & Observability

The cluster includes a full monitoring stack:

- **Prometheus**: Scrapes metrics from the application (`/actuator/prometheus`) and PostgreSQL.
- **Grafana**: Visualizes metrics with pre-configured dashboards.

### Accessing Grafana
```bash
# Forward Grafana port
kubectl port-forward svc/grafana 3000:80
```
Open http://localhost:3000 (Default login: `admin` / `admin`).

## Repository Layout

| Path | Purpose |
| --- | --- |
| `src/main/java` | Spring Boot application source code. |
| `src/main/resources` | Configuration and static assets (HTML/JS). |
| `k8s/` | Kubernetes manifests for AKS deployment. |
| `test_api.sh` | End-to-end regression test script. |
| `legacy_cpp/` | Archived C++ implementation (deprecated). |

## License

MIT – see `LICENSE`.
