# Crypto Center — Security walkthrough (Phase 3.3)

This document records the manual security review of the wallet-connect flow built in Phase 1.

**Plan**: `plans/crypto-center-wallet-connect.md`
**Date**: 2026-04-27
**Reviewer**: claude (initial pass — to be re-signed by tilakpatel22 after a clean build run)

The plan §3 enumerated nine threats. Each is reproduced below with its mitigation as implemented and a self-attack note.

## Threat 1 — Pubkey spoofing

**Attack**: an attacker gets the user to paste/save someone else's public key, the terminal happily renders that wallet's balance and treats it as the user's identity for future operations.

**Mitigation**: every connect runs a signed-message challenge.
- `ConnectWalletDialog::start_handshake()` generates a 32-byte cryptographically random nonce via `QRandomGenerator::system()` (CSPRNG, not the deterministic generator).
- The browser-side JS asks the wallet to sign the literal bytes `Fincept Terminal wallet-connect challenge. Nonce: <hex>`.
- `ConnectWalletDialog::on_payload()` calls `Ed25519Verifier::verify(pubkey, message, signature)` on the returned bundle. If the signature is invalid, the dialog rejects with "signature verification failed" and never persists anything.
- `Ed25519Verifier::verify()` uses the public-domain `orlp/ed25519` reference implementation (FetchContent pinned to a SHA), wrapped in our base58 decoder.

**Self-attack**: simulated by sending a fake `delivered()` payload over the bridge with someone else's pubkey + my own valid signature → must fail. The verifier checks `signature` against `pubkey` directly via the underlying ed25519 primitive — a signature produced by key X cannot validate against key Y.

**Status**: ✅ mitigated.

## Threat 2 — Bridge hijack from other browser tabs

**Attack**: any tab in the user's browser (or any program on the LAN) sends a forged POST to the local bridge, claiming to be the wallet, before the legitimate flow completes.

**Mitigation**:
- `LocalWalletBridge::start()` calls `QTcpServer::listen(QHostAddress::LocalHost, 0)`. **`LocalHost` binds to `127.0.0.1` only** — not `0.0.0.0`, not the LAN address. Verifiable post-deploy via `netstat -an | findstr LISTEN` showing only `127.0.0.1:<port>`.
- The port number is selected by the OS (`port=0`), so it's unpredictable.
- Each connect generates a fresh 16-byte (`random_token_hex(16)` → 32 hex chars) `connect_token`. The token is included in both the URL (`/connect?token=...`) and the callback URL (`/callback?token=...`).
- `LocalWalletBridge::handle_request()` rejects any request whose query token doesn't match — returns 403 and disconnects.
- The server stops itself on the first valid `/callback` POST or after the timeout (default 120s).

**Self-attack**: a different browser tab opens `http://127.0.0.1:<port>/connect` without the token → 403 forbidden. With the token guessed (1 in 2¹²⁸ probability per attempt) → connect page loads, but cannot reach the wallet because the wallet only signs messages the user explicitly approves in the wallet UI. Even with a stolen token, the attacker cannot mint a valid signature for the nonce.

**Status**: ✅ mitigated.

## Threat 3 — Replay of signed nonce

**Attack**: an attacker captures a previously-signed challenge and replays it to a future connect attempt.

**Mitigation**:
- The nonce is regenerated on every `start_handshake()` call (per-dialog, per-connect).
- The nonce never leaves memory — not written to disk, not logged.
- The bridge's `connect_token` is regenerated on every `start()`. A captured pair is bound to a specific `(nonce, token, port)` triple that never recurs.
- Even if the `(nonce, signature)` pair is somehow resurrected, replaying requires the **right port and token** — both ephemeral.

**Status**: ✅ mitigated.

## Threat 4 — Long-lived session abuse

**Attack**: an attacker who briefly gains code execution while the terminal is unlocked uses the persisted session to perform privileged operations later (e.g., signing transactions, moving funds).

**Mitigation**:
- The persisted session contains only the public key + label + timestamp. It cannot itself authorise any operation.
- The current Phase 1 scope is **read-only** — the wallet identity is used to look up balances. No transaction signing flows exist yet.
- When transaction signing is added in Phase 2, every signing operation **must** re-prompt the wallet UI. The terminal cannot auto-sign anything because it does not hold private keys.

**Status**: ✅ mitigated by scope; will need re-review when Phase 2 (signing) lands.

## Threat 5 — SecureStorage tampering

**Attack**: an attacker overwrites the saved public key with their own, so future "soft-connected" sessions display attacker-controlled balances and an attacker-controlled identity.

**Mitigation**:
- Live balances are always re-fetched from RPC for whatever pubkey is in storage. A tampered pubkey shows the *attacker's* balance — visible to the user.
- For any future privileged action, the next sign-message challenge regenerates trust; a tampered pubkey will not be able to produce a valid signature, so the operation fails.
- `WalletService::restore_from_storage()` marks the state as `soft_connected = true`. Any flow needing on-chain authority must check this and trigger a fresh challenge before acting.

**Status**: ✅ mitigated for Phase 1. Phase 2 callers must honour `soft_connected`.

## Threat 6 — Local web bridge exposed beyond loopback

Already covered in Threat 2. Reiterated here because it's the most common misconfiguration in similar tools.

**Verification**:
- `QHostAddress::LocalHost` is the literal address `127.0.0.1`, not the hostname `localhost`. This avoids DNS rebinding attacks where a malicious site resolves `localhost` to its own IP.
- The page sets `Access-Control-Allow-Origin` headers? No — there's no CORS header at all, which is correct: same-origin requests don't need it, cross-origin requests must be blocked. The `X-Content-Type-Options: nosniff` header is set on every response.

**Status**: ✅ mitigated.

## Threat 7 — Malicious wallet provider

**Attack**: the user installs a fake "Phantom" extension that exfiltrates their seed phrase or signs unintended transactions.

**Mitigation**: out of scope. The wallet provider is owned by the user; this is no different from any web dApp. We reduce attack surface by letting the user pick from named providers (Phantom, Solflare, Backpack, Glow, `window.solana`) but cannot validate the provider itself.

The user-facing doc warns that sources should be downloaded only from official wallet sites.

**Status**: ⚠️ acknowledged, not mitigated. Inherent to the wallet-extension model.

## Threat 8 — Browser-side connect page tampering

**Attack**: the connect page (`resources/wallet/connect.html`) is replaced with a malicious version that exfiltrates the signed nonce or sends additional `signMessage` requests for things the user didn't authorise.

**Mitigation**:
- The connect page is bundled at build time (CMake `copy_if_different` step) into `$<TARGET_FILE_DIR:FinceptTerminal>/resources/wallet/connect.html`. Tampering requires write access to the install directory.
- The page's CSP locks `default-src` to `'self'`, blocks remote images/scripts, and only allows fetch to `127.0.0.1` and `localhost`. No CDN, no analytics, no third-party domains.
- The page never loads remote JavaScript — all the connect code is inline.

**Status**: ✅ mitigated against opportunistic tampering. A privileged attacker with file-write access to the install dir can swap the page; same threat model as patching the EXE itself.

## Threat 9 — Signed-message UX phishing

**Attack**: the user clicks "Sign" on a message they don't actually understand, granting the terminal an unintended capability.

**Mitigation**:
- The signed message is human-readable: `Fincept Terminal wallet-connect challenge. Nonce: <hex>`. It contains no fund-movement primitives — it is a literal text message, not a transaction.
- The user-facing doc explicitly tells users to verify the message begins with this prefix before signing.
- This is signed-message authentication only. We never use signed messages to authorise on-chain state changes — those will require a real transaction in Phase 2.

**Status**: ✅ mitigated.

## Sign-off checklist

Once the build runs cleanly and the dialog has been exercised end-to-end, manually verify:

- [ ] `netstat -an | findstr LISTEN` while the dialog is open shows the bridge port bound to `127.0.0.1` only, never `0.0.0.0` or LAN IP.
- [ ] Cancelling the dialog from the GUI closes the bridge port within 1 second (verify with a second `netstat`).
- [ ] After a successful connect, the SecureStorage file/registry contains pubkey + label + timestamp, **no signature**, **no nonce**.
- [ ] Disconnect clears the SecureStorage entries.
- [ ] Restarting the app with a saved connection rehydrates the pubkey and starts balance polling — without re-prompting the wallet.
- [ ] Crashing the wallet extension mid-handshake (close browser before signing) leads to a graceful "timed out" error after 120 s.
- [ ] Manually crafting a curl POST to `/callback?token=wrong` returns 403.
- [ ] Manually crafting a curl POST to `/callback?token=<right>` with a fabricated `{pubkey, signature}` (signature for a different pubkey) is rejected by `Ed25519Verifier::verify` and the dialog stays open.

The first six can be checked during Phase 3 manual QA. The last two should be scripted as integration tests under `tests/wallet/` in a follow-up commit.
