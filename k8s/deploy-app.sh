#!/usr/bin/env bash
set -euo pipefail

# Application deployment for URL shortener (Build -> Push -> Deploy)

: "${RG:=rg-urlshortener-wus3}"
: "${AKS:=aks-urlshortener}"
: "${ACR:=acrurlshortener2261}"
: "${TAG:=v$(date +%Y%m%d%H%M%S)}"

log() { echo "[deploy-app] $*"; }

# 1. Get Credentials (needed for kubectl)
log "Getting cluster credentials"
az aks get-credentials -g "${RG}" -n "${AKS}" --overwrite-existing >/dev/null

# 2. Build & Push
ACR_LOGIN_SERVER=$(az acr show -n "${ACR}" --query loginServer -o tsv)
log "Building docker image and pushing to ACR: ${ACR_LOGIN_SERVER}/url-shortener:${TAG}"
docker build -t url-shortener:aks .
docker tag url-shortener:aks "${ACR_LOGIN_SERVER}/url-shortener:${TAG}"
az acr login -n "${ACR}" >/dev/null
docker push "${ACR_LOGIN_SERVER}/url-shortener:${TAG}"

# 3. Deploy
log "Applying app manifests"
kubectl apply -f k8s/service.yaml
kubectl apply -f k8s/deployment.yaml

log "Setting image to ${ACR_LOGIN_SERVER}/url-shortener:${TAG}"
kubectl set image deployment/url-shortener url-shortener="${ACR_LOGIN_SERVER}/url-shortener:${TAG}"

# 4. Wait
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
