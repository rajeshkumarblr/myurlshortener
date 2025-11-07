#!/usr/bin/env bash
set -euo pipefail

echo "[1/5] Building Docker image (url-shortener:kind)"
docker build -t url-shortener:kind .

echo "[2/5] Loading image into Kind cluster"
kind load docker-image url-shortener:kind --name kind

echo "[3/5] Applying Kubernetes manifests"
kubectl apply -f k8s/deployment.yaml
kubectl apply -f k8s/service.yaml
kubectl apply -f k8s/service-nodeport.yaml
kubectl apply -f k8s/nginx-config.yaml
kubectl apply -f k8s/nginx-deployment.yaml
kubectl apply -f k8s/nginx-service.yaml

echo "[4/5] Waiting for rollout"
kubectl rollout status deployment/url-shortener --timeout=90s
kubectl get pods -l app=url-shortener
kubectl get svc -l app=url-shortener

BACKEND_IP=$(kubectl get svc url-shortener -o jsonpath='{.spec.clusterIP}')
echo "[4b/5] Rewriting API gateway config to use backend IP ${BACKEND_IP}"
cat <<EOF | kubectl apply -f -
apiVersion: v1
kind: ConfigMap
metadata:
	name: nginx-gateway-conf
	labels:
		app: url-shortener
		component: api-gateway
data:
	nginx.conf: |
		events {}
		http {
			server {
				listen 8080;
				server_name _;

				set \$backend ${BACKEND_IP}:9090;

				location = /api/v1/health {
					proxy_pass http://\$backend;
					proxy_set_header Host \$host;
					proxy_set_header X-Real-IP \$remote_addr;
					proxy_set_header X-Forwarded-For \$proxy_add_x_forwarded_for;
					proxy_set_header X-Forwarded-Proto \$scheme;
				}

				location / {
					proxy_pass http://\$backend;
					proxy_set_header Host \$host;
					proxy_set_header X-Real-IP \$remote_addr;
					proxy_set_header X-Forwarded-For \$proxy_add_x_forwarded_for;
					proxy_set_header X-Forwarded-Proto \$scheme;
				}
			}
		}
EOF

kubectl rollout restart deployment/api-gateway
kubectl rollout status deployment/api-gateway --timeout=90s || true

LOCAL_PORT=${LOCAL_PORT:-9090}
TARGET_PORT=9090

echo "[5/5] Checking NodePort access for API gateway (api-gateway-nodeport)"

# Prefer NodePort on Kind if reachable (not always mapped to localhost)
if curl -fsS --max-time 2 http://localhost:30080/api/v1/health >/dev/null 2>&1; then
	echo "✅ API gateway reachable at http://localhost:30080"
	echo "   Try: curl http://localhost:30080/api/v1/health"
	echo "   Create: curl -X POST -H 'Content-Type: application/json' \\
						-d '{"url":"https://example.com"}' http://localhost:30080/api/v1/shorten"
	exit 0
fi

echo "NodePort not reachable on localhost; falling back to port-forward of gateway service."
echo "[5/5] Port-forwarding to localhost and verifying health"

attempts=0
max_attempts=15
PF_PID=""

cleanup() {
	if [ -n "${PF_PID}" ] && kill -0 "${PF_PID}" 2>/dev/null; then
		kill "${PF_PID}" 2>/dev/null || true
		wait "${PF_PID}" 2>/dev/null || true
	fi
}
trap cleanup EXIT

set +e
while [ ${attempts} -lt ${max_attempts} ]; do
	echo "- Trying localhost:${LOCAL_PORT} → svc/api-gateway:80"
	# Start port-forward in background, suppress noisy output
	kubectl port-forward svc/api-gateway ${LOCAL_PORT}:80 >/dev/null 2>&1 &
	PF_PID=$!

	# Wait for health to become available (up to ~15s)
	for i in $(seq 1 15); do
		# If port-forward died, break early to try next port
		if ! kill -0 "${PF_PID}" 2>/dev/null; then
			break
		fi
		if curl -fsS --max-time 1 "http://localhost:${LOCAL_PORT}/api/v1/health" >/dev/null 2>&1; then
			echo "✅ Port-forward ready on http://localhost:${LOCAL_PORT}"
			echo "   Try: curl http://localhost:${LOCAL_PORT}/api/v1/health"
			echo "   Create: curl -X POST -H 'Content-Type: application/json' \
					-d '{"url":"https://example.com"}' http://localhost:${LOCAL_PORT}/api/v1/shorten"
			# Keep process running in background and exit script successfully
			# Remove EXIT trap and detach the background port-forward
			trap - EXIT
			disown "${PF_PID}" 2>/dev/null || true
			set -e
			exit 0
		fi
		sleep 1
	done

	# If we reach here, either health never came up or the forward died
	echo "  Port ${LOCAL_PORT} failed or not healthy yet; trying next port..."
	cleanup
	attempts=$((attempts + 1))
	LOCAL_PORT=$((LOCAL_PORT + 1))
done
set -e

echo "❌ Failed to establish a healthy port-forward after ${max_attempts} attempts." >&2
exit 1
