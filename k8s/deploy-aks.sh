#!/usr/bin/env bash
set -euo pipefail

# Minimal, cost-aware AKS deploy for URL shortener
# Prereqs: az CLI, docker, kubectl logged in (az login)

# Config (override via env or flags)
: "${RG:=rg-urlshortener-wus3}"
: "${LOC:=westus3}"
: "${AKS:=aks-urlshortener}"
: "${ACR:=acrurlshortener$RANDOM}"   # unique by default
: "${NODE_SIZE:=Standard_B2s}"      # low-cost VM
: "${NODE_COUNT:=1}"
: "${TAG:=v$(date +%Y%m%d%H%M%S)}"

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
  bash k8s/deploy-aks.sh

Defaults:
  RG=${RG} LOC=${LOC} AKS=${AKS} ACR=${ACR} TAG=${TAG}
  NODE_SIZE=${NODE_SIZE} NODE_COUNT=${NODE_COUNT}
Optional PG (if CREATE_PG=true):
  PG_NAME=${PG_NAME} PG_VERSION=${PG_VERSION} PG_ADMIN=${PG_ADMIN} PG_PASSWORD=***
EOF
}

log() { echo "[deploy-aks] $*"; }

# 0) Resource group
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

# 3.5) (Optional) Create Azure Database for PostgreSQL Flexible Server (dev-friendly)
if [ "${CREATE_PG}" = "true" ]; then
  if [ -z "${PG_PASSWORD}" ]; then
    PG_PASSWORD=$(tr -dc A-Za-z0-9 </dev/urandom | head -c 20)
  fi
  if ! az postgres flexible-server show -g "${RG}" -n "${PG_NAME}" >/dev/null 2>&1; then
    log "Creating Azure Postgres Flexible Server ${PG_NAME} (public-access dev mode)"
    az postgres flexible-server create \
      -g "${RG}" -n "${PG_NAME}" -l "${LOC}" \
      --version "${PG_VERSION}" \
      --tier Burstable --sku-name Standard_B1ms --storage-size 32 \
      --admin-user "${PG_ADMIN}" --admin-password "${PG_PASSWORD}" \
      --public-access 0.0.0.0 \
      -o none
  else
    log "Postgres server ${PG_NAME} exists"
  fi
  # Ensure database exists
  az postgres flexible-server db create -g "${RG}" -s "${PG_NAME}" -d urlshortener -o none || true
  # Compose DATABASE_URL using admin (simplified for dev). For prod, create a least-privileged app user.
  PG_FQDN=$(az postgres flexible-server show -g "${RG}" -n "${PG_NAME}" --query fullyQualifiedDomainName -o tsv)
  export DATABASE_URL="postgres://${PG_ADMIN}:${PG_PASSWORD}@${PG_FQDN}:5432/urlshortener?sslmode=require"
  log "DATABASE_URL set for deployment (admin-based, dev only)."
  log "IMPORTANT: Restrict firewall and create a dedicated app user for production."
fi

# 4) Build, tag, push image
log "Building docker image and pushing to ACR: ${ACR_LOGIN_SERVER}/url-shortener:${TAG}"
docker build -t url-shortener:aks .
docker tag url-shortener:aks "${ACR_LOGIN_SERVER}/url-shortener:${TAG}"
az acr login -n "${ACR}" >/dev/null
docker push "${ACR_LOGIN_SERVER}/url-shortener:${TAG}"

# 5) Get kubeconfig
log "Getting cluster credentials"
az aks get-credentials -g "${RG}" -n "${AKS}" --overwrite-existing >/dev/null

# 6) Create/update external PostgreSQL secret
if [ -z "${DATABASE_URL:-}" ]; then
  log "DATABASE_URL not set."
  log "Either set CREATE_PG=true to auto-provision a dev Postgres, or export a connection string:"
  log "  export DATABASE_URL=postgres://app:password@host:5432/urlshortener?sslmode=require"
  exit 3
fi
log "Applying external-postgres secret"
kubectl create secret generic external-postgres \
  --from-literal=DATABASE_URL="${DATABASE_URL}" \
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
