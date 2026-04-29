# Crypto Center — Wallet Connect (Phase 1)

**Status**: draft
**Owner**: tilakpatel22
**Last updated**: 2026-04-27

---

## 1. Goal

Build a new **Crypto Center** screen in the Fincept Terminal where users can:
- Connect any major Solana wallet (Phantom, Solflare, Backpack, Glow, Coinbase Solana) and any WalletConnect-v2 mobile wallet.
- See live $FNCPT + SOL balances.
- Disconnect cleanly. Re-connect on next launch without re-prompting (session persistence).

This is **Phase 1** of a longer $FNCPT integration roadmap (gating, burn, staking, prediction markets, buyback). Phase 1 ships **wallet identity + balance display only**. No transactions. No swaps. That comes in Phase 2.

---

## 2. Decisions locked

| # | Decision | Choice |
|---|---|---|
| D1 | Browser-extension transport | **QWebEngineView** hosting Solana Wallet Adapter + WalletConnect v2 SDK |
| D2 | Initial chain scope | **Solana-only** (EVM/MetaMask deferred) |
| D3 | Hardware wallets | **Via Phantom/Solflare host** (no direct Ledger USB in v1) |
| D4 | Sign-in proof | **Signed-message challenge** (random nonce, 5 min TTL, ed25519 verify) |
| D5 | Persisted state | pubkey + label + connected_at in `SecureStorage`; **never** keys/signatures |
| D6 | RPC provider | **Helius** (free tier) — fall back to public `api.mainnet-beta.solana.com` if key missing |
| D7 | $FNCPT mint | `9LUqJ5aQTjQiUCL93gi33LZcscUoSBJNhVCYpPzEpump` (pump.fun, devnet/mainnet TBD) |
| D8 | Screen registration | **Lazy** (`router_->register_factory(...)`) per P2 |
| D9 | Data path | New DataHub producer; `wallet:balance:<pubkey>` topic family per D1/D2 |

---

## 3. Threat model (mandatory before any code)

**Assets to protect:**
1. User's private keys → **never touched, never seen**. Phantom/etc. is the sole signer.
2. User's pubkey → low-sensitivity but tied to identity; stored in `SecureStorage`.
3. Connection session → if hijacked, an attacker can phish the user with fake "sign this" prompts.
4. Local web bridge → if exposed beyond loopback, remote sites could trigger connect/sign UI.

**Attacks considered + mitigations:**

| Attack | Mitigation |
|---|---|
| Pubkey spoofing (user pastes any address) | Signed-message challenge: server generates `nonce`, web page calls `wallet.signMessage(nonce)`, server verifies sig matches pubkey |
| Bridge hijack from other browser tabs | Bind to `127.0.0.1` only, randomly chosen port, single-use connect token in URL, kill server on success/timeout/30s |
| Replay of signed nonce | Nonce includes timestamp + random; expires in 5 min; one-time use, persisted in memory only |
| Long-lived session abuse | Session = pubkey only; every signing op (Phase 2+) re-prompts wallet UI; no auto-sign anywhere ever |
| `SecureStorage` tampering | Live balance always re-fetched from RPC; pubkey treated as untrusted until next sig challenge on critical ops |
| QWebEngine sandbox escape | All web content loaded from local resource (`qrc://`); no remote JS; CSP `default-src 'self'`; navigation locked |
| Malicious WC pairing relay | Use official relay `wss://relay.walletconnect.org`; pin project ID; verify session metadata before showing prompts |
| Clipboard / address swap malware | Show pubkey prominently after connect; user must visually verify before any future tx |

**Out of scope for Phase 1:** transaction signing, fund movement, swap, burn. These have their own threat model and ship in Phase 2 with separate review.

---

## 4. Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│ CryptoCenterScreen (UI only — P6)                                   │
│  ┌────────────────────┐  ┌────────────────────┐                     │
│  │ Wallet status card │  │ Balances card      │                     │
│  │ (pubkey, label)    │  │ ($FNCPT, SOL)      │                     │
│  └────────────────────┘  └────────────────────┘                     │
│  ┌────────────────────┐                                             │
│  │ Connect / disconnect button → ConnectWalletDialog                │
│  └────────────────────┘                                             │
└─────────────────────────────────────────────────────────────────────┘
        │ subscribes to DataHub topics                                 ▲
        ▼                                                              │
┌─────────────────────────────────────────────────────────────────────┐
│ DataHub (existing)                                                  │
│   topic: wallet:state          (push-only, owned by WalletService)  │
│   topic: wallet:balance:<pk>   (TTL 30s, min_interval 10s)          │
│   topic: market:price:fncpt    (TTL 60s — Jupiter price API)        │
└─────────────────────────────────────────────────────────────────────┘
        ▲ publishes
        │
┌─────────────────────────────────────────────────────────────────────┐
│ WalletService (singleton, QObject)                                  │
│   - connect_extension() → spawns ConnectWalletDialog                │
│   - connect_walletconnect()                                         │
│   - disconnect()                                                    │
│   - current_pubkey() / is_connected()                               │
│   - signals: wallet_connected, wallet_disconnected, balance_updated │
│ WalletBalanceProducer  → wallet:balance:<pk>                        │
│ FncptPriceProducer     → market:price:fncpt                         │
└─────────────────────────────────────────────────────────────────────┘
        ▲ uses
        │
┌─────────────────────────────────────────────────────────────────────┐
│ ConnectWalletDialog (modal, QWebEngineView host)                    │
│   - loads qrc:/wallet/index.html                                    │
│   - QWebChannel exposes WalletBridge to JS                          │
│   - JS uses Solana Wallet Adapter (extensions) +                    │
│         @walletconnect/sign-client (QR for mobile)                  │
│   - on connect: bridge.delivered(pubkey, signed_nonce)              │
└─────────────────────────────────────────────────────────────────────┘
        ▲ verifies sig
        │
┌─────────────────────────────────────────────────────────────────────┐
│ Ed25519Verifier (libsodium or Botan — pick one in Task 0)           │
│   - verify(pubkey_b58, message, signature_b58) → bool               │
└─────────────────────────────────────────────────────────────────────┘
```

**Key files (planned):**
- `src/screens/crypto_center/CryptoCenterScreen.{h,cpp}`
- `src/screens/crypto_center/ConnectWalletDialog.{h,cpp}`
- `src/screens/crypto_center/WalletStatusCard.{h,cpp}`
- `src/screens/crypto_center/BalanceCard.{h,cpp}`
- `src/services/wallet/WalletService.{h,cpp}`
- `src/services/wallet/WalletBalanceProducer.{h,cpp}`
- `src/services/wallet/FncptPriceProducer.{h,cpp}`
- `src/services/wallet/Ed25519Verifier.{h,cpp}`
- `src/services/wallet/SolanaRpcClient.{h,cpp}` (thin wrapper over `HttpClient`)
- `resources/wallet/index.html` + `wallet.js` (Wallet Adapter + WC v2 SDK bundle)
- `docs/DATAHUB_TOPICS.md` — add `wallet:state`, `wallet:balance:*`, `market:price:fncpt`
- `fincept-qt/CMakeLists.txt` — add `Qt6::WebEngineWidgets`, `Qt6::WebChannel`, ed25519 lib

**Existing files touched:**
- `src/app/main.cpp` — register `CryptoCenterScreen` factory; bootstrap `WalletService` singleton; register producers with hub.
- `src/screens/(navigation registry)` — add nav entry.

---

## 5. Phases & tasks

### Phase 0 — Plumbing (no UI yet)

**Task 0.1 — Pick & integrate ed25519 lib**
- Add **libsodium** via FetchContent (already used by some crypto deps; small, BSD-licensed). Confirm no conflict with existing deps.
- Wrap in `Ed25519Verifier::verify(QString pubkey_b58, QByteArray msg, QString sig_b58)`.
- Verification: unit test with known-good vector (Solana's `nacl_test` vectors).

**Task 0.2 — Add Qt WebEngine + WebChannel deps**
- Update `CMakeLists.txt`: `find_package(Qt6 REQUIRED COMPONENTS WebEngineWidgets WebChannel)`.
- Update Windows deploy to bundle QtWebEngineProcess.exe + resources.
- Verification: minimal QWebEngineView test compiles, links, runs.

**Task 0.3 — Build the wallet bridge HTML+JS**
- `resources/wallet/index.html` — minimal page with Wallet Adapter UI + WC v2 SDK.
- Bundled offline (no remote CDN). Use `pnpm` or `npm` to build `wallet.js` and check the bundle into `resources/wallet/dist/wallet.bundle.js`.
- Loaded from `qrc:/wallet/index.html` — **never from `https://`**.
- CSP meta tag locks origins.
- Exposes one `WalletBridge` over QWebChannel with: `request_connect()`, `request_disconnect()`, `delivered(pubkey, signature)`, `error(code, msg)`, `qr_uri(uri)` (for WC).
- Verification: load page in standalone `QWebEngineView` test app, click "Connect", verify Phantom prompt fires.

**Task 0.4 — SolanaRpcClient**
- Wrap `HttpClient` for: `getBalance`, `getTokenAccountsByOwner`, `getRecentBlockhash` (future).
- Helius URL from `KeyConfigManager` (`solana/helius_api_key`); fallback to public RPC.
- Verification: unit test against devnet known-balance pubkey.

### Phase 1 — WalletService + producers

**Task 1.1 — WalletService skeleton**
- Singleton, lives in `main.cpp` bootstrap.
- State: `current_pubkey_`, `connected_at_`, `label_`. Restored from `SecureStorage` on startup (read-only — no signature trust until next challenge).
- Signals: `wallet_connected(QString pubkey)`, `wallet_disconnected()`, `balance_updated(QString pubkey, double sol, double fncpt)`.
- Methods: `connect_with_dialog(QWidget* parent)`, `disconnect()`, `is_connected()`, `current_pubkey()`.
- Verification: write/read pubkey roundtrips through SecureStorage in a unit test.

**Task 1.2 — ConnectWalletDialog**
- `QDialog` modal, hosts `QWebEngineView`, sets up `QWebChannel`, exposes `WalletBridge`.
- Generates 32-byte nonce, sends to JS via bridge property.
- Receives `delivered(pubkey, sig_b58)` → calls `Ed25519Verifier::verify(...)` → on success, emits `connected(pubkey)`.
- 60s overall timeout, cancel button, clear error states.
- Verification: end-to-end manual test — open dialog, connect Phantom (devnet), confirm pubkey lands.

**Task 1.3 — WalletBalanceProducer**
- Subscribes hub topic `wallet:balance:<pubkey>`.
- Policy: TTL 30s, min_interval 10s, push_only false.
- On request: `SolanaRpcClient::getBalance(pubkey)` → SOL; `getTokenAccountsByOwner(pubkey, FNCPT_MINT)` → FNCPT.
- Publishes `WalletBalance{ sol_lamports, fncpt_raw, fncpt_decimals }`.
- Verification: subscribe in a test harness, observe publish, change pubkey, observe new topic.

**Task 1.4 — FncptPriceProducer**
- Subscribes hub topic `market:price:fncpt`.
- Policy: TTL 60s, min_interval 30s.
- Calls Jupiter Price API: `https://api.jup.ag/price/v2?ids=<FNCPT_MINT>`.
- Publishes `{ usd_price, sol_price, ts }`.
- Verification: hit the API live, confirm parse.

**Task 1.5 — Register everything in main.cpp**
- After `DataHub` is up: `WalletService::instance().ensure_registered_with_hub()`.
- After router is built: `router_->register_factory("crypto_center", []{ return new CryptoCenterScreen(...); });`.
- Verification: app boots, screen reachable from nav, no producers spawn until subscribed.

**Task 1.6 — Update DataHub docs**
- Append rows to `docs/DATAHUB_TOPICS.md` for `wallet:state`, `wallet:balance:*`, `market:price:fncpt`.
- Per D5 in CLAUDE.md (DataHub rules): **same commit as producers**.
- Verification: doc renders, every topic the producers publish is documented.

### Phase 2 — UI (CryptoCenterScreen)

**Task 2.1 — Screen shell**
- Header: "Crypto Center" + connection chip (green/red).
- Empty state: big "Connect Wallet" button + 1-paragraph explainer + security note ("we never see your keys").
- Connected state: WalletStatusCard + BalanceCard side-by-side, then activity placeholder ("coming Phase 2").
- Theme: follow Obsidian DESIGN_SYSTEM.md, no inline styles per P7.
- Verification: visual review on dark + light themes.

**Task 2.2 — WalletStatusCard**
- Shows pubkey (truncated `9LUq…Epump`, full on hover/copy click), connected_at (relative), wallet label ("Phantom", "Solflare" — from Wallet Adapter).
- "Disconnect" button → `WalletService::disconnect()`.
- "Copy address" button → `QClipboard`.
- Verification: copy works, disconnect clears state, UI swaps to empty state.

**Task 2.3 — BalanceCard**
- Subscribes `wallet:balance:<pubkey>` and `market:price:fncpt`.
- Shows: SOL (lamports / 1e9), $FNCPT (raw / 10^decimals), USD value.
- Loading skeleton while data arrives. Stale indicator if last update > 60s ago.
- Refresh button → `hub.request(topic, /*force=*/true)`.
- Verification: kill network → cached value stays + stale indicator; restore → updates.

**Task 2.4 — showEvent / hideEvent lifecycle (P3)**
- subscribe in `showEvent`, `unsubscribe(this)` in `hideEvent`.
- No timers in constructors.
- Verification: navigate away → producer goes idle (verify in DataHub introspection log).

**Task 2.5 — Nav registration**
- Add Crypto Center entry under a new nav group "Crypto" (or under Tools, decide in PR review).
- Icon: wallet glyph (use existing icon set or add SVG).
- Verification: nav shows item, click routes to screen, lazy factory triggers first time only.

### Phase 3 — Polish & verification

**Task 3.1 — Reconnect-on-startup flow**
- On app launch, if `SecureStorage` has a pubkey: `WalletService` enters "soft-connected" state (UI shows pubkey + balances), but **no signing trust** — the next operation that needs a signature will trigger a fresh challenge.
- Verification: restart app, see pubkey + balances without re-prompt.

**Task 3.2 — Disconnect hardens**
- Clears `SecureStorage` keys (pubkey, label, connected_at).
- Calls JS `wallet.disconnect()` if bridge available.
- Unsubscribes balance producer for that pubkey (hub auto-cleans on no subscribers).
- Verification: post-disconnect, no producer activity for that pubkey; restart confirms blank state.

**Task 3.3 — Manual security walkthrough**
- Run through every threat in §3, confirm mitigation works.
- Try to spoof a pubkey by sending a fake `delivered()` over the bridge with someone else's pubkey + your signature → must reject.
- Confirm bridge port is loopback-only (`netstat -an | grep <port>` shows `127.0.0.1` only).
- Verification: signed report attached to plan or PR.

**Task 3.4 — Docs**
- Add a short user-facing doc: "How to connect your wallet" (`docs/CRYPTO_WALLET_CONNECT.md`).
- 1-page max, screenshots for Phantom + Solflare.
- Verification: doc renders, links from screen "Help" icon.

---

## 6. Out of scope (do NOT do in this plan)

- Sending or signing transactions
- Buying/swapping $FNCPT (Jupiter integration)
- Burning $FNCPT
- Staking/lock UI
- Prediction markets
- Multi-chain (EVM)
- Direct Ledger USB
- Backup/restore wallet flows (we don't store keys, so n/a)

These have separate plans.

---

## 7. Risks & open questions

1. **QWebEngine binary size**: ~50 MB extra. Acceptable trade for the cleanest path. Confirm with installer pipeline owner.
2. **Bundled `wallet.js`**: needs a Node build step the first time. Check it in pre-built so end-developers don't need Node.
3. **WalletConnect v2 project ID**: requires a free signup at WalletConnect Cloud. Need to register one for "Fincept Terminal" before Task 0.3.
4. **Helius API key**: need to provision and wire through `KeyConfigManager`. Free tier OK initially.
5. **$FNCPT mint cluster**: pump.fun lives on mainnet. Devnet won't have the token — for dev testing, mock the balance producer or fork the mint to devnet.
6. **Coinbase Wallet (Solana mode)**: support via Wallet Adapter is recent; verify it works on the bundled version we ship.

---

## 8. Definition of Done (Phase 1)

- [ ] User opens Crypto Center, sees empty state.
- [ ] Clicks "Connect Wallet" → browser-extension prompt fires (Phantom/Solflare/etc.).
- [ ] After approving, screen shows their pubkey + SOL + $FNCPT balance + USD value.
- [ ] Disconnect works; restart with disconnect persists; restart with connection persists.
- [ ] No private keys, signatures, or seed phrases ever leave Phantom.
- [ ] No `PythonRunner` calls in any new file (D1 enforced).
- [ ] All data flows via DataHub topics (D4 enforced).
- [ ] All topics added to `docs/DATAHUB_TOPICS.md` (D5 enforced).
- [ ] Screen registered with `register_factory()` (P2 enforced).
- [ ] Producers subscribe-only via `showEvent`/`hideEvent` (P3, D3 enforced).
- [ ] CI green, manual security walkthrough signed off.
