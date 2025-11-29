#!/usr/bin/env python3
"""End-to-end smoke tests for the Drogon URL shortener API."""
from __future__ import annotations

import argparse
import json
import os
import sys
import textwrap
import time
import traceback
import uuid
from dataclasses import dataclass
from typing import Any, Callable, Dict, Iterable, List, Optional, Sequence

import requests


@dataclass
class TestOutcome:
    name: str
    passed: bool
    details: str = ""


class ApiClient:
    def __init__(self, base_url: str, timeout: float) -> None:
        self.base_url = base_url.rstrip("/")
        self.timeout = timeout

    def request(
        self,
        method: str,
        path: str,
        *,
        token: Optional[str] = None,
        expected: Optional[Iterable[int]] = None,
        json_body: Optional[Dict[str, Any]] = None,
        allow_redirects: bool = False,
        headers: Optional[Dict[str, str]] = None,
    ) -> requests.Response:
        url = f"{self.base_url}{path}"
        hdrs: Dict[str, str] = {"Accept": "application/json"}
        if json_body is not None:
            hdrs["Content-Type"] = "application/json"
        if token:
            hdrs["Authorization"] = f"Bearer {token}"
        if headers:
            hdrs.update(headers)
        response = requests.request(
            method=method,
            url=url,
            json=json_body,
            headers=hdrs,
            timeout=self.timeout,
            allow_redirects=allow_redirects,
        )
        if expected and response.status_code not in expected:
            snippet = response.text[:512]
            raise AssertionError(
                f"{method} {path} expected {list(expected)}, got {response.status_code}: {snippet}"
            )
        return response


class TestSuite:
    def __init__(self) -> None:
        self._outcomes: List[TestOutcome] = []

    def add(self, name: str, func: Callable[[], str]) -> None:
        try:
            details = func()
            self._outcomes.append(TestOutcome(name=name, passed=True, details=details))
        except Exception as exc:  # pylint: disable=broad-except
            tb = traceback.format_exc()
            detail = f"{exc}\n{tb}"
            self._outcomes.append(TestOutcome(name=name, passed=False, details=detail))

    @property
    def failed(self) -> List[TestOutcome]:
        return [o for o in self._outcomes if not o.passed]

    def report(self) -> None:
        width = max((len(o.name) for o in self._outcomes), default=0)
        print("\nResults:\n" + "=" * 50)
        for outcome in self._outcomes:
            status = "PASS" if outcome.passed else "FAIL"
            print(f"{outcome.name.ljust(width)} : {status}")
            if outcome.details:
                detail = textwrap.indent(outcome.details.strip(), prefix="    ")
                print(detail)
        print("=" * 50)
        if self.failed:
            print(f"❌ {len(self.failed)} test(s) failed")
        else:
            print("✅ All tests passed")


@dataclass
class UserInputs:
    name: str
    email: str
    password: str
    target_url: str


def parse_args() -> argparse.Namespace:
    ts = int(time.time())
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-url", default=os.getenv("BASE_URL", "http://localhost:9090"))
    parser.add_argument("--timeout", type=float, default=float(os.getenv("API_TIMEOUT", "10")))
    parser.add_argument("--name", default=os.getenv("TEST_NAME", f"Test User {ts}"))
    parser.add_argument(
        "--email",
        default=os.getenv("TEST_EMAIL", f"test_user_{ts}_{uuid.uuid4().hex[:6]}@example.com"),
    )
    parser.add_argument(
        "--password",
        default=os.getenv("TEST_PASSWORD", f"P@ssw0rd{ts}!"),
    )
    parser.add_argument(
        "--target-url",
        default=os.getenv("TEST_TARGET", "https://www.example.com/"),
    )
    parser.add_argument(
        "--ttl",
        type=int,
        default=int(os.getenv("TEST_TTL", "900")),
        help="TTL in seconds for shortened links",
    )
    return parser.parse_args()


def test_health(client: ApiClient) -> str:
    resp = client.request("GET", "/api/v1/health", expected={200}, allow_redirects=False)
    body = resp.text.strip()
    if body != "ok":
        raise AssertionError(f"Unexpected health body: {body}")
    return "service responded with ok"


def test_register(client: ApiClient, user: UserInputs, ctx: Dict[str, Any]) -> str:
    payload = {"name": user.name, "email": user.email, "password": user.password}
    resp = client.request("POST", "/api/v1/register", json_body=payload, expected={200})
    data = resp.json()
    ctx["register_token"] = data["token"]
    ctx["user_id"] = data["user_id"]
    return f"user_id={ctx['user_id']}"


def test_duplicate_register(client: ApiClient, user: UserInputs) -> str:
    payload = {"name": user.name, "email": user.email, "password": user.password}
    resp = client.request("POST", "/api/v1/register", json_body=payload, expected={409})
    return f"duplicate rejected with {resp.status_code}"


def test_login(client: ApiClient, user: UserInputs, ctx: Dict[str, Any]) -> str:
    payload = {"email": user.email, "password": user.password}
    resp = client.request("POST", "/api/v1/login", json_body=payload, expected={200})
    data = resp.json()
    ctx["login_token"] = data["token"]
    return f"token_len={len(ctx['login_token'])}"


def test_login_invalid_password(client: ApiClient, user: UserInputs) -> str:
    payload = {"email": user.email, "password": "wrong-password"}
    resp = client.request("POST", "/api/v1/login", json_body=payload, expected={401})
    return f"invalid login returned {resp.status_code}"


def test_shorten_requires_auth(client: ApiClient, user: UserInputs) -> str:
    payload = {"url": user.target_url}
    resp = client.request("POST", "/api/v1/shorten", json_body=payload, expected={401})
    return f"unauthenticated request rejected ({resp.status_code})"


def test_shorten_success(client: ApiClient, user: UserInputs, ctx: Dict[str, Any], ttl: int) -> str:
    payload = {"url": user.target_url, "ttl": ttl}
    resp = client.request(
        "POST",
        "/api/v1/shorten",
        json_body=payload,
        expected={200},
        token=ctx["login_token"],
    )
    data = resp.json()
    ctx["short_code"] = data["code"]
    ctx["short_url"] = data["short"]
    return f"code={ctx['short_code']} short={ctx['short_url']}"


def test_list_urls(client: ApiClient, ctx: Dict[str, Any]) -> str:
    resp = client.request("GET", "/api/v1/urls", expected={200}, token=ctx["login_token"])
    data = resp.json()
    if not isinstance(data, list):
        raise AssertionError("Expected list response for /urls")
    if not data:
        raise AssertionError("No URLs returned for authenticated user")
    return f"returned {len(data)} urls"


def test_info_valid(client: ApiClient, ctx: Dict[str, Any]) -> str:
    code = ctx["short_code"]
    resp = client.request("GET", f"/api/v1/info/{code}", expected={200})
    data = resp.json()
    if data.get("code") != code:
        raise AssertionError(f"Mismatched code in info response: {data}")
    return "info endpoint returned metadata"


def test_info_invalid(client: ApiClient) -> str:
    resp = client.request("GET", "/api/v1/info/does-not-exist", expected={404})
    return f"invalid code returned {resp.status_code}"


def test_redirect(client: ApiClient, ctx: Dict[str, Any]) -> str:
    code = ctx["short_code"]
    resp = client.request("GET", f"/{code}", expected={302}, allow_redirects=False)
    location = resp.headers.get("Location")
    if not location:
        raise AssertionError("Missing Location header on redirect")
    # Follow redirect to ensure destination is reachable.
    final = requests.get(location, timeout=client.timeout)
    return f"redirect -> {location} (final HTTP {final.status_code})"


def test_malformed_payload(client: ApiClient) -> str:
    raw = "{"  # intentionally broken JSON
    url = f"{client.base_url}/api/v1/shorten"
    resp = requests.post(url, data=raw, headers={"Content-Type": "application/json"}, timeout=client.timeout)
    if resp.status_code != 400:
        raise AssertionError(f"Expected 400 for malformed JSON, got {resp.status_code}")
    return "malformed JSON rejected"


def run_suite(args: argparse.Namespace) -> int:
    client = ApiClient(base_url=args.base_url, timeout=args.timeout)
    user = UserInputs(name=args.name, email=args.email, password=args.password, target_url=args.target_url)
    ctx: Dict[str, Any] = {}
    suite = TestSuite()

    suite.add("Health", lambda: test_health(client))
    suite.add("Register", lambda: test_register(client, user, ctx))
    suite.add("Duplicate Register", lambda: test_duplicate_register(client, user))
    suite.add("Login", lambda: test_login(client, user, ctx))
    suite.add("Login Invalid", lambda: test_login_invalid_password(client, user))
    suite.add("Shorten Requires Auth", lambda: test_shorten_requires_auth(client, user))
    suite.add("Shorten", lambda: test_shorten_success(client, user, ctx, args.ttl))
    suite.add("List URLs", lambda: test_list_urls(client, ctx))
    suite.add("Info Valid", lambda: test_info_valid(client, ctx))
    suite.add("Info Invalid", lambda: test_info_invalid(client))
    suite.add("Redirect", lambda: test_redirect(client, ctx))
    suite.add("Malformed JSON", lambda: test_malformed_payload(client))

    suite.report()
    return 0 if not suite.failed else 1


def main() -> None:
    args = parse_args()
    exit_code = run_suite(args)
    sys.exit(exit_code)


if __name__ == "__main__":
    main()
