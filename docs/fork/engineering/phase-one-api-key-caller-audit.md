# Phase-One API Key Caller Audit

Date: 2026-05-24
Worktree: `/home/leo/dev/projects/FinceptTerminal/.worktrees/client-server-phase1`

## Scope

Reviewed current `session().api_key`, `fincept_api_key`, `fincept_provider_api_key()`, and compatibility credential callers touched by phase-one session policy work.

## Classification

| Caller | File | Current credential use | Class | Expected outcome |
| --- | --- | --- | --- | --- |
| Profile API-key reveal/copy UI | `fincept-qt/src/screens/profile/ProfileScreen.cpp` | Reads `session().api_key` only after `has_hosted_api_key()` guard | provider-credential | unchanged provider credential |
| MCP profile tool `profile_get_api_key` | `fincept-qt/src/mcp/tools/ProfileTools.cpp` | Returns `sess.api_key` only after `sess.has_hosted_api_key()` guard | provider-credential | unchanged provider credential |
| Forum REST client | `fincept-qt/src/services/forum/ForumService.cpp` | Sends `session().api_key` only after `has_hosted_api_key()` guard | provider-credential | unchanged provider credential |
| Chat mode service | `fincept-qt/src/screens/chat_mode/ChatModeService.cpp` | Sends `compatibility_api_key()` and `compatibility_session_token()` | legacy-compatibility | fenced; do not treat `fincept_api_key` storage as app-auth |
| QuantLib REST client | `fincept-qt/src/services/quantlib/QuantLibClient.cpp` | Sends `compatibility_api_key()` when authenticated | legacy-compatibility | fenced; phase-one startup restore remains disabled |
| LLM runtime config | `fincept-qt/src/services/llm/LlmService.cpp` | Resolves `fincept_provider_api_key()` for provider `fincept` | provider-credential | unchanged provider credential |
| LLM model discovery | `fincept-qt/src/services/llm/LlmModelsApi.cpp` | Falls back to `fincept_provider_api_key()` for provider `fincept` | provider-credential | unchanged provider credential |
| LLM provider settings UI | `fincept-qt/src/screens/settings/LlmConfigSection_Providers.cpp` | Reads `fincept_provider_api_key()` to show linked-key placeholder and connection state | provider-credential | unchanged provider credential |
| Auth provider-key resolver | `fincept-qt/src/auth/AuthManager.cpp` | Reads persisted `fincept_api_key` / secure `api_key` via `fincept_provider_api_key()` | provider-credential | migrated behind `PhaseOneSessionAuthBridge::resolve_fincept_provider_api_key()` |
| Phase-one compatibility shim | `fincept-qt/src/auth/AuthTypes.h` | `compatibility_api_key()` may surface `session_id` for legacy callers | app-auth | fenced; startup/session restoration no longer treats persisted provider key as authenticated app state |

## Notes

- Hosted/provider callers already gate on `has_hosted_api_key()` or use `fincept_provider_api_key()`. Those stay valid for hosted auth and remain intentionally separate from phase-one session auth.
- Legacy compatibility callers still reuse session identifiers on the wire for older Fincept endpoints. Task 9 fences persisted provider-key restoration rather than migrating those service callers in-place.
- No additional `session().api_key` direct consumers were found beyond `ProfileScreen.cpp` and `ForumService.cpp` in this worktree.

## Task-9 Outcome

- `PhaseOneSessionAuthBridge` now owns the provider-key resolution seam so a phase-one session does not silently become a hosted/provider credential.
- Startup auth recovery remains disabled for persisted `fincept_session`, `fincept_api_key`, and secure-storage `api_key` values in phase-one mode.
- `PhaseOneSessionStateGuard` centralizes invalid-session clearing for `AuthApi`, `PhaseOneUserAdminApi`, and `PhaseOneAuditApi`.

## Representative Regression Proof

Command:

```bash
QT_QPA_PLATFORM=offscreen ./build/linux-debug/tests/fincept_auth_session_ui_tests
```

Relevant result:

```text
PASS   : PhaseOneAuthSessionUiSmokeTest::provider_credentials_stay_separate_from_phase_one_app_sessions()
```

Interpretation:

- A phase-one authenticated session only reuses a persisted provider credential when the stored owner matches the authenticated username.
- `PhaseOneSessionAuthBridge::resolve_fincept_provider_api_key()` now blocks cross-user leakage on a shared desktop profile while preserving same-user provider compatibility for the callers listed above.
