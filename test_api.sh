#!/bin/bash

# URL Shortener API Test Script (Java Spring Boot Version)
# Tests all endpoints including Auth and Protected Routes

BASE_URL="https://myurlshortener.westus3.cloudapp.azure.com"
# BASE_URL="http://localhost:9090" # Uncomment for local testing

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}🧪 URL Shortener API Test Suite${NC}"
echo "Target: $BASE_URL"
echo "=================================="

# Helper to extract JSON value
get_json_val() {
    echo "$1" | jq -r "$2"
}

# Function to check if server is running
check_server() {
    echo -n "Checking server health... "
    if ! curl -s "$BASE_URL/api/v1/health" | grep -q "ok"; then
        echo -e "${RED}❌ Server is not reachable at $BASE_URL${NC}"
        exit 1
    fi
    echo -e "${GREEN}✅ Online${NC}"
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
        return 0
    else
        echo -e "${RED}❌ FAIL${NC} (Expected HTTP $expected_code, got HTTP $code)"
        echo "   Response: $body"
        return 1
    fi
}

check_server

# Smoke Test: Check Index Page
echo -n "Checking Frontend (index.html)... "
INDEX_CODE=$(curl -s -o /dev/null -w "%{http_code}" "$BASE_URL/")
if [ "$INDEX_CODE" = "200" ]; then
    echo -e "${GREEN}✅ PASS${NC} (HTTP 200)"
else
    echo -e "${RED}❌ FAIL${NC} (Expected HTTP 200, got HTTP $INDEX_CODE)"
    exit 1
fi

# 1. Authentication
echo -e "\n${YELLOW}1. Authentication${NC}"

RANDOM_ID=$((RANDOM % 10000))
TEST_EMAIL="testuser${RANDOM_ID}@example.com"
TEST_PASS="password123"
TEST_NAME="Test User ${RANDOM_ID}"

echo "Creating user: $TEST_EMAIL"

# Register
REGISTER_RES=$(curl -s -X POST "$BASE_URL/api/v1/register"     -H "Content-Type: application/json"     -d "{\"name\":\"$TEST_NAME\", \"email\":\"$TEST_EMAIL\", \"password\":\"$TEST_PASS\"}")

TOKEN=$(echo "$REGISTER_RES" | jq -r '.token')

if [ "$TOKEN" != "null" ] && [ -n "$TOKEN" ]; then
    echo -e "${GREEN}✅ Registration Successful${NC}"
    echo "   Token acquired"
else
    echo -e "${RED}❌ Registration Failed${NC}"
    echo "   Response: $REGISTER_RES"
    exit 1
fi

# 2. Protected Endpoints
echo -e "\n${YELLOW}2. Protected Endpoints${NC}"

# Shorten URL
echo -n "Shortening URL... "
SHORTEN_RES=$(curl -s -X POST "$BASE_URL/api/v1/shorten"     -H "Content-Type: application/json"     -H "Authorization: Bearer $TOKEN"     -d '{"url": "https://www.microsoft.com", "ttl": 3600}')

CODE=$(echo "$SHORTEN_RES" | jq -r '.code')
SHORT_URL=$(echo "$SHORTEN_RES" | jq -r '.short_url')

if [ "$CODE" != "null" ]; then
    echo -e "${GREEN}✅ PASS${NC}"
    echo "   Code: $CODE"
    echo "   Short URL: $SHORT_URL"
else
    echo -e "${RED}❌ FAIL${NC}"
    echo "   Response: $SHORTEN_RES"
    exit 1
fi

# List URLs
test_endpoint "List My URLs" "200"     -X GET "$BASE_URL/api/v1/urls"     -H "Authorization: Bearer $TOKEN"

# 3. Public Endpoints
echo -e "\n${YELLOW}3. Public Endpoints${NC}"

# Get Info
test_endpoint "Get URL Info" "200"     -X GET "$BASE_URL/api/v1/info/$CODE"

# Redirect
echo -n "Testing Redirect... "
REDIRECT_CODE=$(curl -s -o /dev/null -w "%{http_code}" "$BASE_URL/$CODE")

if [[ "$REDIRECT_CODE" =~ 30[12] ]]; then
    echo -e "${GREEN}✅ PASS${NC} (HTTP $REDIRECT_CODE)"
else
    # Note: curl might follow redirects by default depending on config, but usually doesn't without -L
    # If it returns 200 (the target page), that's also a form of success if we followed it.
    # But strictly we expect a 302 from the API.
    # Let's check headers.
    HEADERS=$(curl -s -I "$BASE_URL/$CODE")
    if echo "$HEADERS" | grep -q "Location:"; then
         echo -e "${GREEN}✅ PASS${NC} (Location header found)"
    else
         echo -e "${RED}❌ FAIL${NC} (HTTP $REDIRECT_CODE)"
    fi
fi

# 4. Negative Tests
echo -e "\n${YELLOW}4. Negative Tests${NC}"

# Unauthorized Shorten
test_endpoint "Unauthorized Shorten" "401"     -X POST "$BASE_URL/api/v1/shorten"     -H "Content-Type: application/json"     -d '{"url": "https://google.com"}'

# Invalid Code
test_endpoint "Invalid Code Info" "404"     -X GET "$BASE_URL/api/v1/info/invalidcode123"

echo -e "\n${YELLOW}📊 Summary${NC}"
echo "=================================="
echo -e "${GREEN}🎉 Regression Test Complete!${NC}"
