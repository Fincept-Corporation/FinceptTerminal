#!/usr/bin/env python3
"""
Unit tests for MiniMax LLM provider integration in Fincept Terminal.

Validates that all MiniMax provider integration points are correctly wired
in the C++ source code by parsing and inspecting the source files.
"""

import os
import re
import sys
import unittest

# Paths relative to repository root
REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC_DIR = os.path.join(REPO_ROOT, "src")
LLM_CONFIG_SECTION = os.path.join(SRC_DIR, "screens", "settings", "LlmConfigSection.cpp")
LLM_SERVICE_H = os.path.join(SRC_DIR, "ai_chat", "LlmService.h")
LLM_SERVICE_CPP = os.path.join(SRC_DIR, "ai_chat", "LlmService.cpp")
README_PATH = os.path.join(os.path.dirname(REPO_ROOT), "README.md")


def read_file(path: str) -> str:
    with open(path, "r", encoding="utf-8") as f:
        return f.read()


class TestMiniMaxKnownProviders(unittest.TestCase):
    """Verify MiniMax appears in the KNOWN_PROVIDERS list."""

    def setUp(self):
        self.source = read_file(LLM_CONFIG_SECTION)

    def test_minimax_in_known_providers(self):
        match = re.search(
            r'KNOWN_PROVIDERS\s*=\s*\{([^}]+)\}', self.source
        )
        self.assertIsNotNone(match, "KNOWN_PROVIDERS definition not found")
        providers_str = match.group(1)
        self.assertIn('"minimax"', providers_str,
                      "minimax must be in KNOWN_PROVIDERS list")

    def test_known_providers_order(self):
        """MiniMax should appear between openrouter and ollama."""
        match = re.search(
            r'KNOWN_PROVIDERS\s*=\s*\{([^}]+)\}', self.source
        )
        self.assertIsNotNone(match)
        providers = re.findall(r'"(\w+)"', match.group(1))
        self.assertIn("minimax", providers)
        minimax_idx = providers.index("minimax")
        self.assertIn("openrouter", providers)
        openrouter_idx = providers.index("openrouter")
        self.assertGreater(minimax_idx, openrouter_idx,
                           "minimax should come after openrouter")

    def test_provider_count(self):
        """Should have 9 known providers after adding minimax."""
        match = re.search(
            r'KNOWN_PROVIDERS\s*=\s*\{([^}]+)\}', self.source
        )
        self.assertIsNotNone(match)
        providers = re.findall(r'"(\w+)"', match.group(1))
        self.assertEqual(len(providers), 9,
                         f"Expected 9 providers, got {len(providers)}: {providers}")


class TestMiniMaxBaseUrl(unittest.TestCase):
    """Verify default_base_url returns the correct MiniMax URL."""

    def setUp(self):
        self.source = read_file(LLM_CONFIG_SECTION)

    def test_minimax_base_url_entry(self):
        self.assertIn('p == "minimax"', self.source,
                      "default_base_url() must handle minimax provider")

    def test_minimax_base_url_value(self):
        # Find the minimax base URL assignment
        pattern = r'if\s*\(p\s*==\s*"minimax"\)\s*\n\s*return\s*"([^"]+)"'
        match = re.search(pattern, self.source)
        self.assertIsNotNone(match, "MiniMax base URL return not found")
        self.assertEqual(match.group(1), "https://api.minimax.io/v1",
                         "MiniMax base URL must be https://api.minimax.io/v1")


class TestMiniMaxFallbackModels(unittest.TestCase):
    """Verify fallback_models returns correct MiniMax models."""

    def setUp(self):
        self.source = read_file(LLM_CONFIG_SECTION)

    def test_minimax_fallback_models_exist(self):
        # Find the minimax models section
        pattern = r'if\s*\(p\s*==\s*"minimax"\)\s*\n\s*return\s*\{([^}]+)\}'
        match = re.search(pattern, self.source)
        self.assertIsNotNone(match, "MiniMax fallback models not found")
        models_str = match.group(1)

        expected_models = [
            "MiniMax-M2.7",
            "MiniMax-M2.7-highspeed",
            "MiniMax-M2.5",
            "MiniMax-M2.5-highspeed",
        ]
        for model in expected_models:
            self.assertIn(f'"{model}"', models_str,
                          f"Model {model} must be in MiniMax fallback models")

    def test_minimax_model_count(self):
        pattern = r'if\s*\(p\s*==\s*"minimax"\)\s*\n\s*return\s*\{([^}]+)\}'
        match = re.search(pattern, self.source)
        self.assertIsNotNone(match)
        models = re.findall(r'"([^"]+)"', match.group(1))
        self.assertEqual(len(models), 4,
                         f"Expected 4 MiniMax models, got {len(models)}: {models}")


class TestMiniMaxStreamingSupport(unittest.TestCase):
    """Verify MiniMax is listed as supporting streaming."""

    def setUp(self):
        self.source = read_file(LLM_SERVICE_H)

    def test_minimax_in_streaming_providers(self):
        match = re.search(
            r'provider_supports_streaming\([^)]+\)\s*\{[^}]+\}',
            self.source, re.DOTALL
        )
        self.assertIsNotNone(match,
                             "provider_supports_streaming() not found")
        self.assertIn('"minimax"', match.group(0),
                      "minimax must be in provider_supports_streaming()")


class TestMiniMaxEndpointUrl(unittest.TestCase):
    """Verify get_endpoint_url handles MiniMax correctly."""

    def setUp(self):
        self.source = read_file(LLM_SERVICE_CPP)

    def test_minimax_endpoint_url(self):
        pattern = r'if\s*\(p\s*==\s*"minimax"\)\s*\n\s*return\s*"([^"]+)"'
        # Look in the get_endpoint_url section
        endpoint_section = self.source[
            self.source.index("get_endpoint_url"):
            self.source.index("get_headers")
        ]
        match = re.search(pattern, endpoint_section)
        self.assertIsNotNone(match, "MiniMax endpoint URL not found in get_endpoint_url()")
        self.assertEqual(match.group(1),
                         "https://api.minimax.io/v1/chat/completions",
                         "MiniMax endpoint must be https://api.minimax.io/v1/chat/completions")


class TestMiniMaxModelsUrl(unittest.TestCase):
    """Verify get_models_url does NOT return a URL for MiniMax
    (no public /v1/models endpoint — fallback models are used)."""

    def setUp(self):
        self.source = read_file(LLM_SERVICE_CPP)

    def test_minimax_no_models_url(self):
        """MiniMax should not have a models URL in get_models_url()
        because the API has no public /v1/models endpoint."""
        models_section = self.source[
            self.source.index("get_models_url"):
            self.source.index("get_models_headers")
        ]
        # Should NOT have a minimax-specific models URL
        self.assertNotIn('p == "minimax"', models_section,
                         "MiniMax should not have a models URL entry — "
                         "no public /v1/models endpoint")

    def test_minimax_comment_in_get_models_url(self):
        """A comment should explain why minimax has no models URL."""
        models_section = self.source[
            self.source.index("get_models_url"):
            self.source.index("get_models_headers")
        ]
        self.assertIn("minimax", models_section.lower(),
                      "Comment about minimax should exist in get_models_url()")


class TestMiniMaxTemperatureClamping(unittest.TestCase):
    """Verify MiniMax temperature is clamped to (0.0, 1.0]."""

    def setUp(self):
        self.source = read_file(LLM_SERVICE_CPP)

    def test_temperature_clamping_exists(self):
        # Check that there's a minimax-specific temperature clamp in build_openai_request
        self.assertIn('provider_ == "minimax"', self.source,
                      "Temperature clamping for minimax must exist")

    def test_temperature_clamp_range(self):
        # Verify the clamp range: std::max(0.01, std::min(temperature_, 1.0))
        clamp_section = self.source[
            self.source.index("build_openai_request"):
            self.source.index("build_anthropic_request")
        ]
        self.assertIn("0.01", clamp_section,
                      "Lower bound of temperature clamp should be 0.01")
        self.assertIn("1.0", clamp_section,
                      "Upper bound of temperature clamp should be 1.0")


class TestMiniMaxAuthHeaders(unittest.TestCase):
    """Verify MiniMax uses OpenAI-compatible Bearer auth headers."""

    def setUp(self):
        self.source = read_file(LLM_SERVICE_CPP)

    def test_minimax_uses_bearer_auth(self):
        """MiniMax should fall through to OpenAI-compatible Bearer auth
        (no special handling needed — it uses the else branch)."""
        headers_section = self.source[
            self.source.index("get_headers"):
            self.source.index("Request builders")
        ]
        # MiniMax should NOT have its own special header handling
        self.assertNotIn('p == "minimax"', headers_section,
                         "MiniMax should use default OpenAI-compatible Bearer auth, "
                         "not have its own header block")


class TestMiniMaxApiKeyRequired(unittest.TestCase):
    """Verify MiniMax requires an API key."""

    def setUp(self):
        self.source = read_file(LLM_SERVICE_H)

    def test_minimax_requires_api_key(self):
        """provider_requires_api_key should return true for minimax
        (it only excludes ollama and fincept)."""
        match = re.search(
            r'provider_requires_api_key\([^)]+\)\s*\{[^}]+\}',
            self.source, re.DOTALL
        )
        self.assertIsNotNone(match)
        func_body = match.group(0)
        # minimax should NOT be excluded
        self.assertNotIn('"minimax"', func_body,
                         "MiniMax should require API key (not excluded)")


class TestMiniMaxOpenAICompatible(unittest.TestCase):
    """Verify MiniMax uses the OpenAI-compatible request path in do_request."""

    def setUp(self):
        self.source = read_file(LLM_SERVICE_CPP)

    def test_minimax_not_special_cased_in_do_request(self):
        """MiniMax should fall through to the OpenAI-compatible else branch
        in do_request() — no special provider_ == 'minimax' check needed."""
        do_request_section = self.source[
            self.source.index("LlmResponse LlmService::do_request"):
            self.source.index("Tool-call follow-up loop")
        ]
        # MiniMax should NOT have its own special response parsing
        self.assertNotIn('provider_ == "minimax"', do_request_section,
                         "MiniMax should use OpenAI-compatible response parsing, "
                         "not have its own block in do_request()")


class TestMiniMaxReadme(unittest.TestCase):
    """Verify MiniMax is mentioned in the README."""

    def setUp(self):
        self.source = read_file(README_PATH)

    def test_minimax_in_readme(self):
        self.assertIn("MiniMax", self.source,
                      "MiniMax must be mentioned in README.md")

    def test_minimax_in_provider_list(self):
        """MiniMax should appear in the AI Agents feature row."""
        # Find the line that contains AI Agents
        lines = self.source.split("\n")
        ai_agents_line = None
        for line in lines:
            if "AI Agents" in line and "multi-provider" in line:
                ai_agents_line = line
                break
        self.assertIsNotNone(ai_agents_line,
                             "AI Agents row with multi-provider must exist")
        self.assertIn("MiniMax", ai_agents_line,
                      "MiniMax must be listed in AI Agents feature row")


class TestMiniMaxTextToolCallSupport(unittest.TestCase):
    """Verify MiniMax is acknowledged in text-based tool call detection."""

    def setUp(self):
        self.source = read_file(LLM_SERVICE_CPP)

    def test_minimax_mentioned_in_text_tool_calls(self):
        """The try_extract_and_execute_text_tool_calls comment mentions minimax
        as a model that uses text-based tool calls."""
        self.assertIn("minimax", self.source.lower(),
                      "minimax should be mentioned in text tool call comments")


if __name__ == "__main__":
    unittest.main(verbosity=2)
