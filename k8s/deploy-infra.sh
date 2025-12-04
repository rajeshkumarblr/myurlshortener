#!/usr/bin/env bash
set -euo pipefail

# Infrastructure deployment for URL shortener (Azure Infra + Redis + Postgres)

: "${RG:=rg-urlshortener-wus3}"
: "${LOC:=westus3}"
: "${AKS:=aks-urlshortener}"
: "${ACR:=acrurlshortener2261}"
: "${NODE_SIZE:=Standard_B2s}"
: "${NODE_COUNT:=1}"
: "${JWT_SECRET:=}"

log() { echo "[deploy-infra] $*"; }

# 1. Azure Infrastructure
log "Ensuring resource group ${RG} in ${LOC}"
az group create -n "${RG}" -l "${LOC}" >/dev/null

if ! az acr show -n "${ACR}" >/dev/null 2>&1; then
  log "Creating ACR ${ACR} (Basic)"
  az acr create -g "${RG}" -n "${ACR}" --sku Basic >/dev/null
else
  log "ACR ${ACR} exists"
fi

if ! az aks show -g "${RG}" -n "${AKS}" >/dev/null 2>&1; then
  log "Creating AKS ${AKS} (${NODE_SIZE} x ${NODE_COUNT})"
  az aks create -g "${RG}" -n "${AKS}" \
    --node-vm-size "${NODE_SIZE}" --node-count "${NODE_COUNT}" \
    --enable-managed-identity --generate-ssh-keys >/dev/null
else
  log "AKS ${AKS} exists"
fi

log "Attaching ACR to AKS"
az aks update -g "${RG}" -n "${AKS}" --attach-acr "${ACR}" >/dev/null

log "Getting cluster credentials"
az aks get-credentials -g "${RG}" -n "${AKS}" --overwrite-existing >/dev/null

# 2. Redis
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

# 3. PostgreSQL
log "Installing/Updating PostgreSQL via Helm"
helm upgrade --install postgres bitnami/postgresql \
  --set auth.postgresPassword=UrlShortPass2025 \
  --set auth.database=urlshortener \
  --set primary.persistence.size=2Gi \
  --set primary.resources.requests.memory=256Mi \
  --set primary.resources.requests.cpu=100m \
  --set primary.resources.limits.memory=512Mi \
  --set primary.resources.limits.cpu=500m

# 4. Secrets
DB_HOST="postgres-postgresql.default.svc.cluster.local"
DB_PORT="5432"
DB_USER="postgres"
DB_PASS="UrlShortPass2025"
DB_NAME="urlshortener"
JDBC_URL="jdbc:postgresql://${DB_HOST}:${DB_PORT}/${DB_NAME}"
FINAL_JWT_SECRET="${JWT_SECRET:-MySuperSecretKeyForUrlShortener2025!}"

log "Applying external-postgres secret"
kubectl create secret generic external-postgres \
  --from-literal=SPRING_DATASOURCE_URL="${JDBC_URL}" \
  --from-literal=SPRING_DATASOURCE_USERNAME="${DB_USER}" \
  --from-literal=SPRING_DATASOURCE_PASSWORD="${DB_PASS}" \
  --from-literal=JWT_SECRET="${FINAL_JWT_SECRET}" \
  -o yaml --dry-run=client | kubectl apply -f -

log "Infrastructure deployment complete."
