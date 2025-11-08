#!/usr/bin/env bash
set -euo pipefail

# Minimal, cost-aware AKS deploy for URL shortener
# Prereqs: az CLI, docker, helm, kubectl logged in (az login)

# Config (override via env or flags)
: "${RG:=rg-urlshortener}"
: "${LOC:=eastus}"
: "${AKS:=aks-urlshortener}"
: "${ACR:=acrurlshortener$RANDOM}"   # unique by default
: "${NODE_SIZE:=Standard_B2s}"      # low-cost VM
: "${NODE_COUNT:=1}"
: "${TAG:=v$(date +%Y%m%d%H%M%S)}"

usage() {
  cat <<EOF
Usage: RG=... LOC=... AKS=... ACR=... TAG=... NODE_SIZE=Standard_B2s NODE_COUNT=1 \
  bash k8s/deploy-aks.sh

Defaults:
  RG=${RG} LOC=${LOC} AKS=${AKS} ACR=${ACR} TAG=${TAG}
  NODE_SIZE=${NODE_SIZE} NODE_COUNT=${NODE_COUNT}
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

# 4) Build, tag, push image
log "Building docker image and pushing to ACR: ${ACR_LOGIN_SERVER}/url-shortener:${TAG}"
docker build -t url-shortener:aks .
docker tag url-shortener:aks "${ACR_LOGIN_SERVER}/url-shortener:${TAG}"
az acr login -n "${ACR}" >/dev/null
docker push "${ACR_LOGIN_SERVER}/url-shortener:${TAG}"

# 5) Get kubeconfig
log "Getting cluster credentials"
az aks get-credentials -g "${RG}" -n "${AKS}" --overwrite-existing >/dev/null

# 6) Install NGINX Ingress Controller (one shared LB)
log "Installing ingress-nginx via Helm"
helm repo add ingress-nginx https://kubernetes.github.io/ingress-nginx >/dev/null || true
helm repo update >/dev/null
helm upgrade --install ingress-nginx ingress-nginx/ingress-nginx \
  --namespace ingress-nginx --create-namespace \
  --set controller.resources.requests.cpu=50m \
  --set controller.resources.requests.memory=64Mi \
  --set controller.resources.limits.cpu=200m \
  --set controller.resources.limits.memory=256Mi \
  --wait

# 7) Create/update external PostgreSQL secret (expects DATABASE_URL defined externally or passed as ENV)
if [ -z "${DATABASE_URL:-}" ]; then
  log "DATABASE_URL not set; please export a production connection string (sslmode=require)."
  log "Example: export DATABASE_URL=postgres://app:password@host:5432/urlshortener?sslmode=require"
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

# 9) Apply Ingress that routes / to the service (through NGINX IC)
log "Applying Ingress"
kubectl apply -f k8s/ingress.yaml

# 10) Wait for rollout and ingress IP
log "Waiting for rollout"
kubectl rollout status deployment/url-shortener --timeout=120s

log "Waiting for ingress external IP/hostname..."
for i in $(seq 1 60); do
  INGRESS_IP=$(kubectl get svc -n ingress-nginx ingress-nginx-controller \
    -o jsonpath='{.status.loadBalancer.ingress[0].ip}' 2>/dev/null || true)
  INGRESS_HOST=$(kubectl get svc -n ingress-nginx ingress-nginx-controller \
    -o jsonpath='{.status.loadBalancer.ingress[0].hostname}' 2>/dev/null || true)
  if [ -n "${INGRESS_IP}" ] || [ -n "${INGRESS_HOST}" ]; then
    break
  fi
  sleep 5
  printf "."
done

echo
if [ -z "${INGRESS_IP:-}" ] && [ -z "${INGRESS_HOST:-}" ]; then
  log "Could not get Ingress address. Check: kubectl get svc -n ingress-nginx"
  exit 1
fi

INGRESS_ADDR=${INGRESS_IP:-$INGRESS_HOST}
log "Ingress ready at: http://${INGRESS_ADDR}"
echo "Try: curl http://${INGRESS_ADDR}/api/v1/health"
echo "Create: curl -X POST -H 'Content-Type: application/json' \
  -d '{\"url\":\"https://example.com\"}' http://${INGRESS_ADDR}/api/v1/shorten"
