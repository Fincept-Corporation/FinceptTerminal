# Wallet HTML — vendored libraries

This directory holds third-party JavaScript bundles that the wallet HTML pages
embed at load time. Everything here is **vendored** — committed verbatim into
the repository, served from `qrc://`, never fetched at runtime.

## Why vendored

The wallet pages run inside a `QWebEngineView` with a strict CSP
(`connect-src 'self'`). A network-loaded library would either require
relaxing the CSP or running our own loader proxy. Both options enlarge the
attack surface; vendoring the exact bytes we ship is simpler.

## Required files

### `web3.js` — `@solana/web3.js` UMD bundle

- **Origin:** https://github.com/solana-labs/solana-web3.js (releases page)
- **Version expected:** 1.95.x (latest 1.x stable as of Phase 2 plan)
- **Filename in this directory:** `web3.js`
- **Approximate size:** ~150 KB minified
- **Module exposure:** the UMD build defines `window.solanaWeb3` globally; that's
  the entry point our HTML uses.

### How to refresh the bundle

1. Download the official UMD release from the GitHub releases page (e.g.
   `https://github.com/solana-labs/solana-web3.js/releases/download/v1.95.X/solana-web3.iife.js`).
2. Copy it into this directory as `web3.js`.
3. Verify with `sha256sum web3.js`; record the hash + version + date in the
   table below before committing.
4. Update the wallet HTML's CSP if any new origins are required (none should
   be — the bundle is self-contained).

## Version log

| Date | web3.js version | sha256 |
|---|---|---|
| 2026-04-28 | 1.95.8 (IIFE minified, via unpkg) | `a759deca1b65df140e8dda5ad8645c19579536bf822e5c0c7e4adb7793a5bd08` |

## What this directory is NOT

- Not a place to fetch from a CDN at runtime.
- Not a place to run `npm install`.
- Not a place for compiled TypeScript output (we host pre-built bundles only).

If you need a library that doesn't ship a hand-buildable UMD bundle, talk to
the wallet-bridge owner first — adding a Node-build dependency to the Qt
project is a serious decision.
