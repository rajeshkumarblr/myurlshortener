#!/usr/bin/env bash
set -euo pipefail

# Smoke-test Auth + URL APIs end-to-end.
# Usage: BASE_URL=http://localhost:9090 ./scripts/auth_api_tests.sh

BASE_URL=${BASE_URL:-http://localhost:9090}
MAX_TIME=${MAX_TIME:-10}
JQ_BIN=${JQ_BIN:-jq}
CURL_BIN=${CURL_BIN:-curl}

for tool in "$CURL_BIN" "$JQ_BIN"; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Required tool '$tool' not found in PATH" >&2
        exit 1
    fi
done

EPOCH=$(date +%s)
TEST_NAME=${TEST_NAME:-"Test User $EPOCH"}
TEST_EMAIL=${TEST_EMAIL:-"testuser_${EPOCH}@example.com"}
TEST_PASSWORD=${TEST_PASSWORD:-"P@ssw0rd${EPOCH}!"}
URL_TO_SHORTEN=${URL_TO_SHORTEN:-"https://example.org/demo/$EPOCH"}

log() {
    printf '\n[%s] %s\n' "$(date -Iseconds)" "$1"
}

call_api() {
    local method=$1
    local path=$2
    local body=${3-}
    shift 3 || true
    local headers=() extra
    while (($#)); do
        headers+=("-H" "$1")
        shift
    done
    local response
    if [[ -n $body ]]; then
        response=$($CURL_BIN -sS -w '\n%{http_code}' --max-time "$MAX_TIME" -X "$method" "${BASE_URL}${path}" \
            -H 'Content-Type: application/json' "${headers[@]}" -d "$body")
    else
        response=$($CURL_BIN -sS -w '\n%{http_code}' --max-time "$MAX_TIME" -X "$method" "${BASE_URL}${path}" "${headers[@]}")
    fi
    local body_out code_out
    body_out=$(printf '%s' "$response" | head -n -1)
    code_out=$(printf '%s' "$response" | tail -n 1)
    printf '%s\n%s\n' "$body_out" "$code_out"
}

require_code() {
    local actual=$1 expected=$2 message=$3
    if [[ "$actual" != "$expected" ]]; then
        printf 'Expected HTTP %s, got %s (%s)\n' "$expected" "$actual" "$message" >&2
        exit 1
    fi
}

log "Health check"
read -r health_body health_code < <(call_api GET '/api/v1/health')
require_code "$health_code" 200 "health"
printf 'Health OK: %s\n' "$health_body"

log "Registering $TEST_EMAIL"
register_payload=$($JQ_BIN -n --arg name "$TEST_NAME" --arg email "$TEST_EMAIL" --arg password "$TEST_PASSWORD" '{name:$name,email:$email,password:$password}')
read -r register_body register_code < <(call_api POST '/api/v1/register' "$register_payload")
require_code "$register_code" 200 "register"
register_token=$(printf '%s' "$register_body" | $JQ_BIN -r '.token')
if [[ -z "$register_token" || "$register_token" == "null" ]]; then
    echo "Registration response missing token: $register_body" >&2
    exit 1
fi
printf 'Registered user id=%s email=%s\n' "$(printf '%s' "$register_body" | $JQ_BIN -r '.user_id')" "$TEST_EMAIL"

log "Logging in"
login_payload=$($JQ_BIN -n --arg email "$TEST_EMAIL" --arg password "$TEST_PASSWORD" '{email:$email,password:$password}')
read -r login_body login_code < <(call_api POST '/api/v1/login' "$login_payload")
require_code "$login_code" 200 "login"
login_token=$(printf '%s' "$login_body" | $JQ_BIN -r '.token')
if [[ -z "$login_token" || "$login_token" == "null" ]]; then
    echo "Login response missing token: $login_body" >&2
    exit 1
fi
printf 'Login token acquired (len=%s)\n' "${#login_token}"

bearer_header="Authorization: Bearer $login_token"

log "Shortening URL"
shorten_payload=$($JQ_BIN -n --arg url "$URL_TO_SHORTEN" --argjson ttl 900 '{url:$url, ttl:$ttl}')
read -r shorten_body shorten_code < <(call_api POST '/api/v1/shorten' "$shorten_payload" "$bearer_header")
require_code "$shorten_code" 200 "shorten"
short_code=$(printf '%s' "$shorten_body" | $JQ_BIN -r '.code')
short_url=$(printf '%s' "$shorten_body" | $JQ_BIN -r '.short')
printf 'Short code=%s url=%s\n' "$short_code" "$short_url"

log "Listing URLs for user"
read -r list_body list_code < <(call_api GET '/api/v1/urls' '' "$bearer_header")
require_code "$list_code" 200 "list"
printf 'List response: %s\n' "$list_body"

log "Fetching info for short code"
read -r info_body info_code < <(call_api GET "/api/v1/info/${short_code}")
require_code "$info_code" 200 "info"
printf 'Info response: %s\n' "$info_body"

log "Following redirect"
redirect_code=$($CURL_BIN -s -o /dev/null -w '%{http_code}' --max-time "$MAX_TIME" "$short_url")
require_code "$redirect_code" 200 "redirect target"
printf 'Redirect ultimately returned %s (expected 200 from destination)\n' "$redirect_code"

log "All auth smoke tests passed"
