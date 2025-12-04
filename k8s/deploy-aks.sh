
#!/usr/bin/env bash
set -euo pipefail

# Minimal, cost-aware AKS deploy for URL shortener

# Prereqs: az CLI, docker, kubectl logged in (az login)

# Config (override via env or flags)
: "${RG:=rg-urlshortener-wus3}"
: "${LOC:=westus3}"
: "${AKS:=aks-urlshortener}"
: "${ACR:=acrurlshortener2261}"   # fixed, use the latest and only ACR
: "${NODE_SIZE:=Standard_B2s}"      # low-cost VM
: "${NODE_COUNT:=1}"
: "${TAG:=v$(date +%Y%m%d%H%M%S)}"
: "${KEYVAULT_NAME:=}"
: "${KV_SECRET_NAME:=}"
: "${KV_JWT_SECRET_NAME:=}"
: "${JWT_SECRET:=}"

# By default, skip infra (resource group, ACR, AKS) setup for faster deploys
: "${SKIP_INFRA:=true}"

# Optional: create Azure Database for PostgreSQL Flexible Server automatically
# If CREATE_PG=true, a dev-friendly public-access DB will be created.
# NOTE: For production, restrict firewall and create a dedicated app user.
: "${CREATE_PG:=false}"
: "${PG_NAME:=pg-urlshortener-$RANDOM}"
: "${PG_VERSION:=16}"
: "${PG_ADMIN:=pgadmin}"
# If not provided, a random password will be generated.
: "${PG_PASSWORD:=}"

usage() {
  cat <<EOF
Usage: RG=... LOC=... AKS=... ACR=... TAG=... NODE_SIZE=Standard_B2s NODE_COUNT=1 \
  [CREATE_PG=true PG_NAME=... PG_ADMIN=... PG_PASSWORD=...] \
  [KEYVAULT_NAME=kv-name KV_SECRET_NAME=pg-secret KV_JWT_SECRET_NAME=jwt-secret] \
  bash k8s/deploy-aks.sh

Defaults:
  RG=${RG} LOC=${LOC} AKS=${AKS} ACR=${ACR} TAG=${TAG}
  NODE_SIZE=${NODE_SIZE} NODE_COUNT=${NODE_COUNT}
Optional PG (if CREATE_PG=true):
  PG_NAME=${PG_NAME} PG_VERSION=${PG_VERSION} PG_ADMIN=${PG_ADMIN} PG_PASSWORD=***
Optional Key Vault:
  KEYVAULT_NAME=${KEYVAULT_NAME:-<unset>} KV_SECRET_NAME=${KV_SECRET_NAME:-<unset>} KV_JWT_SECRET_NAME=${KV_JWT_SECRET_NAME:-<unset>}
EOF
}

log() { echo "[deploy-aks] $*"; }

# 3.7) Ensure Redis is deployed as standalone (NO replicas, minimal resources)
log "Installing/Updating Redis (standalone, NO replicas) via Helm"
helm repo add bitnami https://charts.bitnami.com/bitnami >/dev/null 2>&1 || true
helm repo update >/dev/null 2>&1
helm upgrade --install redis bitnami/redis \
  --namespace cache --create-namespace \
  --set auth.enabled=true \
  --set auth.password=UrlShortRedis2025 \
  --set architecture=standalone \
  --set replica.replicaCount=0 \
  --set master.persistence.size=2Gi \
  --set master.resources.requests.memory=128Mi \
  --set master.resources.requests.cpu=50m \
  --set master.resources.limits.memory=256Mi \
  --set master.resources.limits.cpu=200m


# 0-3) Infra setup (resource group, ACR, AKS, attach ACR) -- skip by default
if [ "${SKIP_INFRA}" = "false" ]; then
  log "Ensuring resource group ${RG} in ${LOC}"
  az group create -n "${RG}" -l "${LOC}" >/dev/null

  # 1) ACR (Basic SKU to limit cost ~ $5/mo)
  if ! az acr show -n "${ACR}" >/dev/null 2>&1; then
    log "Creating ACR ${ACR} (Basic)"
    az acr create -g "${RG}" -n "${ACR}" --sku Basic >/dev/null
  else
    log "ACR ${ACR} exists"
  fi
  ACR_LOGIN_SERVER=$(az acr show -n "${ACR}" --query loginServer -o tsv)

  # 2) AKS cluster (control plane free, pay for nodes). Low-cost B2s, 1 node.
  if ! az aks show -g "${RG}" -n "${AKS}" >/dev/null 2>&1; then
    log "Creating AKS ${AKS} (${NODE_SIZE} x ${NODE_COUNT})"
    az aks create -g "${RG}" -n "${AKS}" \
      --node-vm-size "${NODE_SIZE}" --node-count "${NODE_COUNT}" \
      --enable-managed-identity --generate-ssh-keys >/dev/null
  else
    log "AKS ${AKS} exists"
  fi

  # 3) Attach ACR permissions to AKS (so k8s can pull images)
  log "Attaching ACR to AKS"
  az aks update -g "${RG}" -n "${AKS}" --attach-acr "${ACR}" >/dev/null
else
  log "Skipping infra setup (SKIP_INFRA=true)"
  # Still need ACR_LOGIN_SERVER for image tag
  ACR_LOGIN_SERVER=$(az acr show -n "${ACR}" --query loginServer -o tsv)
fi

# 3.6) Install PostgreSQL via Helm (Self-hosted in AKS)
log "Installing PostgreSQL via Helm"
helm repo add bitnami https://charts.bitnami.com/bitnami >/dev/null 2>&1 || true
helm repo update >/dev/null 2>&1
helm upgrade --install postgres bitnami/postgresql \
  --set auth.postgresPassword=UrlShortPass2025 \
  --set auth.database=urlshortener \
  --set primary.persistence.size=2Gi \
  --set primary.resources.requests.memory=256Mi \
  --set primary.resources.requests.cpu=100m \
  --set primary.resources.limits.memory=512Mi \
  --set primary.resources.limits.cpu=500m

# Internal Cluster DNS for Postgres
DB_HOST="postgres-postgresql.default.svc.cluster.local"
DB_PORT="5432"
DB_USER="postgres"
DB_PASS="UrlShortPass2025"
DB_NAME="urlshortener"
JDBC_URL="jdbc:postgresql://${DB_HOST}:${DB_PORT}/${DB_NAME}"

# 4) Build, tag, push image (optional)
: "${BUILD_IMAGE:=true}"
if [ "${BUILD_IMAGE}" = "true" ]; then
  log "Building docker image and pushing to ACR: ${ACR_LOGIN_SERVER}/url-shortener:${TAG}"
  docker build -t url-shortener:aks .
  docker tag url-shortener:aks "${ACR_LOGIN_SERVER}/url-shortener:${TAG}"
  az acr login -n "${ACR}" >/dev/null
  docker push "${ACR_LOGIN_SERVER}/url-shortener:${TAG}"
else
  log "Skipping Docker build/push (BUILD_IMAGE=false)"
fi

# 5) Get kubeconfig
log "Getting cluster credentials"
az aks get-credentials -g "${RG}" -n "${AKS}" --overwrite-existing >/dev/null

# 6) Create/update external PostgreSQL secret
# Use a default JWT secret if not provided (for demo purposes)
# Must be at least 32 chars (256 bits) for HS256
FINAL_JWT_SECRET="${JWT_SECRET:-MySuperSecretKeyForUrlShortener2025!}"

log "Applying external-postgres secret"
kubectl create secret generic external-postgres \
  --from-literal=SPRING_DATASOURCE_URL="${JDBC_URL}" \
  --from-literal=SPRING_DATASOURCE_USERNAME="${DB_USER}" \
  --from-literal=SPRING_DATASOURCE_PASSWORD="${DB_PASS}" \
  --from-literal=JWT_SECRET="${FINAL_JWT_SECRET}" \
  -o yaml --dry-run=client | kubectl apply -f -

# 8) Deploy app manifests (ClusterIP service, Deployment, Ingress)
log "Applying app manifests"
kubectl apply -f k8s/service.yaml
kubectl apply -f k8s/deployment.yaml

log "Setting image to ${ACR_LOGIN_SERVER}/url-shortener:${TAG}"
kubectl set image deployment/url-shortener url-shortener="${ACR_LOGIN_SERVER}/url-shortener:${TAG}"

# 8) Wait for rollout and LoadBalancer IP
log "Waiting for rollout"
kubectl rollout status deployment/url-shortener --timeout=120s

log "Waiting for service external IP/hostname..."
for i in $(seq 1 60); do
  SERVICE_IP=$(kubectl get svc url-shortener \
    -o jsonpath='{.status.loadBalancer.ingress[0].ip}' 2>/dev/null || true)
  SERVICE_HOST=$(kubectl get svc url-shortener \
    -o jsonpath='{.status.loadBalancer.ingress[0].hostname}' 2>/dev/null || true)
  if [ -n "${SERVICE_IP}" ] || [ -n "${SERVICE_HOST}" ]; then
    break
  fi
  sleep 5
  printf "."
done

echo
if [ -z "${SERVICE_IP:-}" ] && [ -z "${SERVICE_HOST:-}" ]; then
  log "Could not get service address. Check: kubectl get svc url-shortener"
  exit 1
fi

SERVICE_ADDR=${SERVICE_IP:-$SERVICE_HOST}
log "Service ready at: http://${SERVICE_ADDR}"
echo "Try: curl http://${SERVICE_ADDR}/api/v1/health"
echo "Create: curl -X POST -H 'Content-Type: application/json' \
  -d '{\"url\":\"https://example.com\"}' http://${SERVICE_ADDR}/api/v1/shorten"
