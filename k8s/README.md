# Kubernetes (Kind) Deployment

This app deploys to a local Kind cluster with a single Deployment that connects to an external PostgreSQL running on your host.

- App image: `url-shortener:kind`
- Service: `ClusterIP` on port `9090`
- Optional `NodePort` service on `30090` (best-effort; port-forward fallback)
- Database: `DATABASE_URL` provided via secret `external-postgres`

## Quickstart

1) Build, load, deploy (auto-creates secret with your host IP):

```bash
bash k8s/deploy-kind.sh
```

2) Access the app

- Try NodePort first:
```bash
curl -s http://localhost:30090/api/v1/health
```
- If unavailable, port-forward the service:
```bash
kubectl port-forward svc/url-shortener 9090:9090
curl -s http://localhost:9090/api/v1/health
```

3) Create a short URL
```bash
curl -s -X POST -H 'Content-Type: application/json' \
  -d '{"url":"https://example.com","ttl":120}' \
  http://localhost:9090/api/v1/shorten
```

## Database Secret

The script creates/updates the secret `external-postgres` with a `DATABASE_URL` like:

```
postgres://app:appsecret@<HOST_IP>:5432/urlshortener?sslmode=disable
```

It auto-detects `<HOST_IP>` so pods can reach the host Postgres from Kind’s network.

Update it manually if needed:
```bash
kubectl create secret generic external-postgres \
  --from-literal=DATABASE_URL="postgres://app:appsecret@172.17.0.1:5432/urlshortener?sslmode=disable" \
  -o yaml --dry-run=client | kubectl apply -f -
```

## Clean up
```bash
kubectl delete all -l app=url-shortener --ignore-not-found
```

## Notes
- Liveness probe is TCP to avoid DB outages killing the pod.
- Readiness checks `/api/v1/health`, which includes a DB ping.
- For cloud DBs, set `sslmode=require` in `DATABASE_URL`.
