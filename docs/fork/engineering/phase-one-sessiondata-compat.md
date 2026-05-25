# Phase-One SessionData Compatibility Audit

Date: 2026-05-24
Scope: Task 3b only from the client/server phase-one plan.

## Audit rules

- Searched for `session().api_key`, `session().session_token`, `fincept_api_key`, `fincept_session`, and app-auth `SecureStorage` reads/writes.
- Kept hosted `UserApi` and other hosted-only service calls on their existing transport.
- Added only compatibility shims, hosted-only fallbacks, or graceful degradation needed to keep screens available under phase-one auth where `SessionData::api_key` is empty.

## SessionData compatibility shim

- `fincept-qt/src/auth/AuthTypes.h`: added `has_hosted_api_key()`, `is_phase_one_session()`, `compatibility_api_key()`, and `compatibility_session_token()`.
- `fincept-qt/src/auth/AuthManager.h` + `fincept-qt/src/auth/AuthManager.cpp`: added `has_hosted_api_key()`, `is_phase_one_session()`, and `fincept_provider_api_key()` so callers stop open-coding hosted credential fallbacks.

## Caller audit

| File | Caller / dependency audited | Disposition | Task 3b result |
| --- | --- | --- | --- |
| `fincept-qt/src/services/quantlib/QuantLibClient.cpp` | `session().api_key` | preserved compatibility shim | Uses `session().compatibility_api_key()` for hosted/phase-one header compatibility. |
| `fincept-qt/src/services/forum/ForumService.cpp` | `session().api_key` | hosted-only fallback | Early-return/log when no hosted API key instead of issuing broken hosted requests. |
| `fincept-qt/src/screens/chat_mode/ChatModeService.cpp` | `session().api_key`, `session().session_token` | preserved compatibility shim | Uses `compatibility_api_key()` and `compatibility_session_token()`. |
| `fincept-qt/src/screens/profile/ProfileScreen.cpp` | `session().api_key` plus hosted `UserApi` sections | graceful degradation | Security/usage/billing/login-history panes now show non-hosted fallback state instead of hanging or offering broken API-key actions. |
| `fincept-qt/src/screens/auth/PricingScreen.cpp` | hosted refresh + checkout flow | hosted-only fallback | Phase-one auth skips hosted refresh wait path; checkout now reports hosted-only unavailability instead of hanging. |
| `fincept-qt/src/app/WindowFrame_Auth.cpp` | auth shell routing depends on hosted plan semantics | graceful degradation for non-phase-one hosted-only flows | Phase-one/no-hosted-key sessions bypass pricing gate and keep shell access available. |
| `fincept-qt/src/services/llm/LlmService.cpp` | `fincept_api_key` | provider-credential compatibility shim | Uses `AuthManager::fincept_provider_api_key()` instead of open-coded settings fallback. |
| `fincept-qt/src/services/agents/AgentService.cpp` | active LLM payload / provider key usage | graceful degradation | Audited only; no direct app-auth storage caller here. Existing `LlmService::is_configured()` gate already omits unusable Fincept payloads when no provider key resolves. |
| `fincept-qt/src/ui/navigation/ToolBar.cpp` | auth session display | graceful degradation | Username falls back to `session.username`; empty hosted-plan sessions render `PHASE 1` and neutral zero-credit styling. |
| `fincept-qt/src/ui/navigation/NavigationBar.cpp` | auth session display | graceful degradation | Same fallback treatment as toolbar. |
| `fincept-qt/src/mcp/tools/ProfileTools.cpp` | profile/session tools + hosted key exposure | hosted-only fallback | Hosted `UserApi` MCP tools now fail explicitly when no hosted key exists; session tool still works. |
| `fincept-qt/src/services/llm/LlmModelsApi.cpp` | `fincept_api_key` | provider-credential compatibility shim | Uses `AuthManager::fincept_provider_api_key()` for Fincept model fetch headers. |
| `fincept-qt/src/screens/settings/LlmConfigSection_Providers.cpp` | `fincept_api_key` | provider-credential compatibility shim | Fincept provider placeholder/test-connection path now resolves through `AuthManager::fincept_provider_api_key()`. |
| `fincept-qt/src/auth/AuthManager.cpp` | `fincept_session`, `fincept_api_key`, `SecureStorage(api_key)` | auth-state compatibility shim, cleanup deferred to Task 4 | Centralized provider-key fallback helper; persistence/storage behavior otherwise left intact for Task 4 cleanup. |
| `fincept-qt/src/app/main.cpp` | `fincept_session` migration check | audit and note only | No change in Task 3b; this remains a legacy migration sentinel. |

## Verification

### Commands run

```bash
cmake --build build/linux-debug --target fincept_auth_session_tests
ctest --test-dir build/linux-debug --output-on-failure -R "^fincept_auth_session_tests$"
ninja -C build/linux-debug \
  "CMakeFiles/FinceptTerminal.dir/Unity/unity_11_cxx.cxx.o" \
  "CMakeFiles/FinceptTerminal.dir/Unity/unity_21_cxx.cxx.o" \
  "CMakeFiles/FinceptTerminal.dir/Unity/unity_20_cxx.cxx.o" \
  "CMakeFiles/FinceptTerminal.dir/Unity/unity_19_cxx.cxx.o" \
  "CMakeFiles/FinceptTerminal.dir/Unity/unity_18_cxx.cxx.o" \
  "CMakeFiles/FinceptTerminal.dir/Unity/unity_6_cxx.cxx.o" \
  "CMakeFiles/FinceptTerminal.dir/Unity/unity_3_cxx.cxx.o" \
  "CMakeFiles/FinceptTerminal.dir/src/mcp/tools/ProfileTools.cpp.o" \
  "CMakeFiles/FinceptTerminal.dir/src/screens/auth/PricingScreen.cpp.o" \
  "CMakeFiles/FinceptTerminal.dir/src/screens/profile/ProfileScreen.cpp.o" \
  "CMakeFiles/FinceptTerminal.dir/src/app/WindowFrame_Auth.cpp.o" \
  "CMakeFiles/FinceptTerminal.dir/src/screens/settings/LlmConfigSection_Providers.cpp.o"
```

### Results by family

- Market / analytics: `unity_11_cxx.cxx.o` rebuilt successfully; covers `QuantLibClient.cpp` and the audited `AgentService.cpp` unity bucket.
- Discussion / profile: `unity_21_cxx.cxx.o`, `ProfileScreen.cpp.o`, and `ProfileTools.cpp.o` rebuilt successfully.
- Chat / LLM: `unity_20_cxx.cxx.o`, `unity_6_cxx.cxx.o`, and `fincept_auth_session_tests` passed after adding compatibility-credential assertions.
- Navigation / auth-shell: `unity_19_cxx.cxx.o`, `unity_18_cxx.cxx.o`, `WindowFrame_Auth.cpp.o`, `PricingScreen.cpp.o`, and `LlmConfigSection_Providers.cpp.o` rebuilt successfully.

### Manual verification notes

- Full `FinceptTerminal` link was not used for verification because this environment is already known to be blocked for broad app builds by missing `QWebSocket` support in Hyperliquid-related paths.
- No runtime GUI walkthrough was executed in this environment; focused object rebuilds were used to verify the edited screen/service families compile after the compatibility changes.

## Follow-up boundary for Task 4

- `fincept_session` and `fincept_api_key` persistence/cleanup remains intentionally in place.
- Hosted `UserApi` methods still use hosted transport only.
- Any future removal of legacy hosted-key persistence should happen in Task 4 after startup/session-recovery behavior is updated.
