#!/usr/bin/env python3
"""
Integration tests for MiniMax LLM provider in Fincept Terminal.

These tests verify the MiniMax API endpoint connectivity and response format
using the real MiniMax API. Requires MINIMAX_API_KEY environment variable.
"""

import json
import os
import sys
import unittest

import urllib.request
import urllib.error


MINIMAX_API_KEY = os.environ.get("MINIMAX_API_KEY", "")
MINIMAX_BASE_URL = "https://api.minimax.io/v1"


def skip_without_api_key(func):
    """Skip test if MINIMAX_API_KEY is not set."""
    return unittest.skipIf(
        not MINIMAX_API_KEY,
        "MINIMAX_API_KEY not set — skipping integration test"
    )(func)


class TestMiniMaxApiConnectivity(unittest.TestCase):
    """Verify MiniMax API endpoints are reachable and respond correctly."""

    @skip_without_api_key
    def test_models_endpoint_returns_404(self):
        """MiniMax does not have a public /v1/models endpoint.
        The Fincept Terminal uses fallback_models() instead."""
        req = urllib.request.Request(
            f"{MINIMAX_BASE_URL}/models",
            headers={
                "Authorization": f"Bearer {MINIMAX_API_KEY}",
                "Content-Type": "application/json",
            },
        )
        with self.assertRaises(urllib.error.HTTPError) as ctx:
            urllib.request.urlopen(req, timeout=15)
        self.assertEqual(ctx.exception.code, 404,
                         "MiniMax /v1/models should return 404 — no public endpoint")

    @skip_without_api_key
    def test_chat_completions_basic(self):
        """POST /v1/chat/completions with MiniMax-M2.7 should return valid response."""
        payload = json.dumps({
            "model": "MiniMax-M2.7",
            "messages": [
                {"role": "user", "content": "Say 'hello' and nothing else."}
            ],
            "temperature": 0.5,
            "max_tokens": 32,
        }).encode()

        req = urllib.request.Request(
            f"{MINIMAX_BASE_URL}/chat/completions",
            data=payload,
            headers={
                "Authorization": f"Bearer {MINIMAX_API_KEY}",
                "Content-Type": "application/json",
            },
            method="POST",
        )
        try:
            with urllib.request.urlopen(req, timeout=30) as resp:
                self.assertEqual(resp.status, 200)
                data = json.loads(resp.read().decode())
                self.assertIn("choices", data,
                              "Response must contain 'choices' (OpenAI format)")
                self.assertGreater(len(data["choices"]), 0)
                content = data["choices"][0]["message"]["content"]
                self.assertIsInstance(content, str)
                self.assertGreater(len(content), 0,
                                   "Response content should not be empty")
        except urllib.error.HTTPError as e:
            self.fail(f"Chat completions returned HTTP {e.code}: {e.read().decode()[:200]}")

    @skip_without_api_key
    def test_temperature_clamping_boundary(self):
        """MiniMax API should accept temperature at the boundary value 1.0."""
        payload = json.dumps({
            "model": "MiniMax-M2.7",
            "messages": [
                {"role": "user", "content": "Hi"}
            ],
            "temperature": 1.0,
            "max_tokens": 8,
        }).encode()

        req = urllib.request.Request(
            f"{MINIMAX_BASE_URL}/chat/completions",
            data=payload,
            headers={
                "Authorization": f"Bearer {MINIMAX_API_KEY}",
                "Content-Type": "application/json",
            },
            method="POST",
        )
        try:
            with urllib.request.urlopen(req, timeout=30) as resp:
                self.assertEqual(resp.status, 200)
        except urllib.error.HTTPError as e:
            self.fail(f"Temperature 1.0 should be accepted, got HTTP {e.code}")


if __name__ == "__main__":
    unittest.main(verbosity=2)
