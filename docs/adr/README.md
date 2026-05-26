# Architecture Decision Records

This directory captures load-bearing architectural decisions that future
readers — humans and AI assistants — need to understand to avoid re-litigating
or accidentally reversing them.

## When to write one

A decision needs an ADR when:
- It would be re-suggested by anyone reading the code cold (e.g. "why don't we use platform keychains?").
- Reversing it would require coordinated work across the codebase.
- The reasoning is not derivable from the code itself.

Refactors and bug fixes do **not** need ADRs. Use commit messages.

## Format

Each ADR is a short markdown file. Number sequentially. Status is one of
`Accepted`, `Superseded`, `Deprecated`. Use the template in
[`TEMPLATE.md`](TEMPLATE.md).

## Index

- [0001 — Modular monolith over microservices](0001-modular-monolith.md)
- [0002 — DataHub as the canonical data plane](0002-datahub-as-data-plane.md)
- [0003 — SecureStorage uses SQLite + AES-256-GCM (no platform keychains)](0003-securestorage-sqlite-only.md)
- [0004 — Broker enum mapping via `BrokerEnumMap<T>` (Phase 4)](0004-broker-enum-map.md)
- [0005 — Wallet/Update are intentional UI-coordinator services](0005-ui-coordinator-services.md)
- [0006 — Defer QCoro until CI gate green on all three platforms](0006-defer-qcoro.md)
- [0007 — Screens do not own caches (Phase 6 policy)](0007-screens-do-not-own-caches.md)
