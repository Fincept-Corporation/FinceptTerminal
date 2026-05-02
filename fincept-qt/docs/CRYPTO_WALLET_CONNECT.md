# Connecting your Solana wallet

The **Crypto Center** screen lets you connect a Solana wallet to Fincept Terminal so you can see your $FNCPT and SOL balances, swap, and earn the fee discount on premium features. The terminal **never sees your private keys** — your wallet (Phantom, Solflare, Backpack, etc.) signs every operation locally.

---

## Supported wallets

Any Solana wallet that handles a `signMessage` request in your default browser will work, including:

- **Phantom** — recommended (most active development, broad hardware-wallet support)
- **Solflare** — desktop + mobile, native Ledger
- **Backpack** — newer, desktop + mobile
- **Glow**, **Coinbase Wallet (Solana mode)** — also work

The wallet does **not** need a browser extension to be installed in the same browser as Fincept Terminal — the terminal opens your default browser, the wallet's extension or webapp picks up the request there.

---

## Connecting

1. Open the **Crypto Center** screen from the main nav.
2. Click **CONNECT WALLET**.
3. Your default browser opens a one-page Fincept handshake. Click **Connect** there. Your wallet pops a "Sign this message" prompt — approve it.
4. The browser tab closes automatically and the terminal shows your public key, SOL balance, and $FNCPT balance.

The "Sign this message" prompt is a **one-time identity challenge** — Fincept asks the wallet to sign a random nonce so we know the public key really belongs to you. Nothing is sent on-chain; no SOL leaves your wallet for this step.

---

## What gets stored locally

Fincept Terminal saves only:

- Your **public key** (base58 string)
- The wallet **label** ("Phantom", "Solflare", etc.)
- The **timestamp** of the connection

These live in the encrypted **SecureStorage** under the keys `wallet.pubkey`, `wallet.label`, and `wallet.connected_at`. **Private keys, signatures, and seed phrases are never seen by the terminal** — they stay in your wallet's vault.

---

## Reconnecting on next launch

When you restart Fincept Terminal with a previously-connected wallet, the screen restores the public key from SecureStorage and shows balances immediately, **without** prompting your wallet again. This is a "soft connect" — any operation that needs a signature (a swap, for example) re-prompts the wallet at that moment.

---

## Disconnecting

Click **Disconnect** on the wallet status card. The terminal:

1. Wipes the public key, label, and timestamp from SecureStorage.
2. Stops all subscriptions tied to that wallet (balance, activity, fee discount).
3. Returns the screen to its empty state.

If you connect a different wallet next time, the new public key replaces the old — the previous one is fully removed.

---

## Security model in one paragraph

The terminal opens a loopback HTTP server on `127.0.0.1` with a random port and a single-use token, then opens your default browser to that URL. The browser-side page handles the wallet adapter handshake (extension or WalletConnect), then POSTs the signed nonce + public key back to the loopback server. The terminal verifies the ed25519 signature against the public key before persisting anything. The bridge port is bound to **localhost only** — other machines on your network cannot see it. The signed nonce is single-use and expires after 60 seconds. See `plans/crypto-center-security-walkthrough.md` for the full threat model and mitigations.

---

## Troubleshooting

**"Connection cancelled"** — you closed the browser tab or clicked Cancel in the wallet. Click **CONNECT WALLET** again to retry.

**"Signature verification failed"** — extremely rare; almost always indicates a wallet bug or a deliberately-malformed signature. Try a different wallet or upgrade your wallet extension.

**Browser doesn't open** — your default browser may not be set, or the OS is blocking handler launches. Set a default browser in OS settings and try again.

**Balances show `—` after connecting** — the RPC may be slow. The Settings tab lets you provide a Helius API key for a faster RPC; without one, the terminal falls back to the public mainnet RPC (rate-limited).

---

## Where to find this in code

- `src/services/wallet/WalletService.{h,cpp}` — connection state machine
- `src/services/wallet/ConnectWalletDialog.{h,cpp}` — challenge + verification
- `src/services/wallet/LocalWalletBridge.{h,cpp}` — loopback HTTP server
- `src/services/wallet/Ed25519Verifier.{h,cpp}` — signature verification (orlp/ed25519)
- `resources/wallet/connect.html` — browser-side handshake page
- `plans/crypto-center-wallet-connect.md` — original plan
- `plans/crypto-center-security-walkthrough.md` — security review

---

**Last updated:** 2026-05-01
