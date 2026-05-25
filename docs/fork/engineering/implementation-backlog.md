# Implementation Backlog

Status: Active

## Active And Planned Work

### Item: Client/server multi-user phase one

- Rationale: convert the desktop terminal into a server-authoritative multi-user client while keeping all screens available

- Status: in progress

- Related code: `fincept-qt/src/app/main.cpp`, `fincept-qt/src/app/PhaseOneStartupCoordinator.cpp`, `fincept-qt/src/auth/AuthManager.cpp`, `fincept-qt/src/auth/PhaseOneAuthFlowBridge.cpp`, `fincept-qt/src/auth/PhaseOneSessionAuthBridge.cpp`, `fincept-qt/src/multiuser/client/`, `fincept-qt/src/multiuser/server/`, `fincept-qt/src/storage/sqlite/migrations/v032_multiuser_phase_one.cpp`, `fincept-qt/src/services/portfolio/PortfolioService.cpp`, `fincept-qt/tests/auth/`, `docs/fork/engineering/phase-one-sessiondata-compat.md`, `docs/fork/engineering/phase-one-startup-inventory.md`, `docs/fork/engineering/phase-one-startup-verification.md`, `docs/fork/engineering/phase-one-api-key-caller-audit.md`

- Current status notes:
  - focused phase-one migration, transport, auth/session, bootstrap/admin, and portfolio contract tests are passing in the feature worktree
  - final broad verification is still limited by environment/build-target issues outside the core phase-one behavior checks
  - next implementation steps remain final verification cleanup and documentation closure
