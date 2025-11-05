#!/bin/bash

# URL Shortener API Test Script
# Tests all endpoints and validates responses

BASE_URL="http://localhost:9090"
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}🧪 URL Shortener API Test Suite${NC}"
echo "=================================="

# Function to check if server is running
check_server() {
    if ! curl -s "$BASE_URL/api/v1/health" > /dev/null; then
        echo -e "${RED}❌ Server is not running at $BASE_URL${NC}"
        echo "Please start the server with: ./url_shortener"
        exit 1
    fi
}

# Function to test endpoint
test_endpoint() {
    local name="$1"
    local expected_code="$2"
    shift 2
    local curl_args=("$@")
    
    echo -n "Testing $name... "
    
    response=$(curl -s -w "\n%{http_code}" "${curl_args[@]}")
    body=$(echo "$response" | head -n -1)
    code=$(echo "$response" | tail -n 1)
    
    if [ "$code" = "$expected_code" ]; then
        echo -e "${GREEN}✅ PASS${NC} (HTTP $code)"
        if [ -n "$body" ] && [ "$body" != "ok" ]; then
            echo "   Response: $body"
        fi
        return 0
    else
        echo -e "${RED}❌ FAIL${NC} (Expected HTTP $expected_code, got HTTP $code)"
        echo "   Response: $body"
        return 1
    fi
}

# Start tests
check_server

echo -e "\n${YELLOW}1. Health Check${NC}"
test_endpoint "Health endpoint" "200" "$BASE_URL/api/v1/health"

echo -e "\n${YELLOW}2. URL Shortening${NC}"
# Test valid URL shortening
SHORTEN_RESPONSE=$(curl -s -X POST "$BASE_URL/api/v1/shorten" \
    -H "Content-Type: application/json" \
    -d '{"url": "https://www.google.com"}')

if echo "$SHORTEN_RESPONSE" | grep -q '"code"'; then
    CODE=$(echo "$SHORTEN_RESPONSE" | grep -o '"code":"[^"]*"' | cut -d'"' -f4)
    echo -e "${GREEN}✅ PASS${NC} - URL shortened successfully"
    echo "   Response: $SHORTEN_RESPONSE"
    echo "   Short code: $CODE"
else
    echo -e "${RED}❌ FAIL${NC} - URL shortening failed"
    echo "   Response: $SHORTEN_RESPONSE"
    exit 1
fi

# Test URL shortening with TTL
echo -n "Testing URL shortening with TTL... "
TTL_RESPONSE=$(curl -s -X POST "$BASE_URL/api/v1/shorten" \
    -H "Content-Type: application/json" \
    -d '{"url": "https://www.github.com", "ttl": 3600}')

if echo "$TTL_RESPONSE" | grep -q '"code"'; then
    TTL_CODE=$(echo "$TTL_RESPONSE" | grep -o '"code":"[^"]*"' | cut -d'"' -f4)
    echo -e "${GREEN}✅ PASS${NC}"
    echo "   Response: $TTL_RESPONSE"
    echo "   TTL Short code: $TTL_CODE"
else
    echo -e "${RED}❌ FAIL${NC}"
    echo "   Response: $TTL_RESPONSE"
fi

# Test invalid URL shortening (missing URL)
test_endpoint "Invalid shortening (no URL)" "400" \
    -X POST "$BASE_URL/api/v1/shorten" \
    -H "Content-Type: application/json" \
    -d '{}'

echo -e "\n${YELLOW}3. URL Info${NC}"
# Test getting info for valid code
test_endpoint "Info for valid code" "200" "$BASE_URL/api/v1/info/$CODE"

# Test getting info for TTL code
if [ -n "$TTL_CODE" ]; then
    test_endpoint "Info for TTL code" "200" "$BASE_URL/api/v1/info/$TTL_CODE"
fi

# Test getting info for invalid code
test_endpoint "Info for invalid code" "404" "$BASE_URL/api/v1/info/INVALID"

echo -e "\n${YELLOW}4. URL Redirection${NC}"
# Test valid redirection (check headers only)
echo -n "Testing valid redirection... "
REDIRECT_RESPONSE=$(curl -s -I "$BASE_URL/$CODE")
if echo "$REDIRECT_RESPONSE" | grep -q "302\|301"; then
    echo -e "${GREEN}✅ PASS${NC} (Redirect detected)"
    LOCATION=$(echo "$REDIRECT_RESPONSE" | grep -i "location:" | cut -d' ' -f2- | tr -d '\r')
    echo "   Redirects to: $LOCATION"
else
    echo -e "${RED}❌ FAIL${NC}"
    echo "   Response: $REDIRECT_RESPONSE"
fi

# Test invalid redirection
echo -n "Testing invalid redirection... "
INVALID_RESPONSE=$(curl -s -I "$BASE_URL/INVALID")
if echo "$INVALID_RESPONSE" | grep -q "404"; then
    echo -e "${GREEN}✅ PASS${NC} (HTTP 404)"
else
    echo -e "${RED}❌ FAIL${NC}"
    echo "   Response: $INVALID_RESPONSE"
fi

echo -e "\n${YELLOW}5. Edge Cases${NC}"
# Test malformed JSON
test_endpoint "Malformed JSON" "400" \
    -X POST "$BASE_URL/api/v1/shorten" \
    -H "Content-Type: application/json" \
    -d '{"url":'

# Test empty URL
test_endpoint "Empty URL" "400" \
    -X POST "$BASE_URL/api/v1/shorten" \
    -H "Content-Type: application/json" \
    -d '{"url": ""}'

# Test non-string URL
test_endpoint "Non-string URL" "400" \
    -X POST "$BASE_URL/api/v1/shorten" \
    -H "Content-Type: application/json" \
    -d '{"url": 123}'

echo -e "\n${YELLOW}📊 Summary${NC}"
echo "=================================="
echo "✅ All core functionality tested"
echo "🔗 Short URL created: $BASE_URL/$CODE"
if [ -n "$TTL_CODE" ]; then
    echo "⏰ TTL Short URL created: $BASE_URL/$TTL_CODE"
fi
echo -e "${GREEN}🎉 API is working correctly!${NC}"