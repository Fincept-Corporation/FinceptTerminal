## Phase One Startup Inventory

This inventory captures the current startup responsibilities from `fincept-qt/src/app/main.cpp` and assigns each responsibility to exactly one Phase 1 destination owner.

| Startup responsibility | Destination owner |
| --- | --- |
| Raw pre-Qt CLI parse for `--profile`, `--server`, `--server-host`, `--server-port`, and `--server-db` | `main.cpp` |
| Earliest TLS/OpenSSL bootstrap, including `QT_TLS_BACKEND` and macOS OpenSSL preloads before any Qt app object exists | `PhaseOneStartupCoordinator` |
| Earliest crash-handler install before Qt object construction | `PhaseOneStartupCoordinator` |
| Choosing process mode before creating the application object | `main.cpp` |
| Constructing `QCoreApplication` for server mode | `PhaseOneStartupCoordinator` |
| Constructing `QApplication` for client mode | `main.cpp` |
| `QApplication::setAttribute(Qt::AA_ShareOpenGLContexts)` for client UI | `PhaseOneClientUiStartup` |
| Retrying `QSslSocket::setActiveBackend("openssl")` after app construction | `PhaseOneStartupCoordinator` |
| Preserving profile selection for client-mode path resolution and manifest setup | `PhaseOneClientStartup` |
| App root creation needed before profile manifest writes | `PhaseOneClientStartup` |
| InstanceLock acquisition and secondary-instance exit path | `PhaseOneClientLifecycle` |
| Same-profile new-window IPC handoff from relaunch to primary instance | `PhaseOneClientLifecycle` |
| Windows foreground-handoff allowance for a secondary client instance | `PhaseOneClientLifecycle` |
| Wiring `message_received` and `lastWindowClosed` lifecycle hooks | `PhaseOneClientLifecycle` |
| TerminalShell process bootstrap and shutdown hook | `PhaseOneClientStartup` |
| DataHub metatype registration and early producer warmup | `PhaseOneClientStartup` |
| Component catalog load | `PhaseOneClientStartup` |
| Synchronous service registrations needed for first client paint | `PhaseOneClientStartup` |
| Deferred service registration batch after first client paint | `PhaseOneClientStartup` |
| App directory creation for the client profile | `PhaseOneClientStartup` |
| Legacy file migration from old app-data locations | `PhaseOneClientStartup` |
| Legacy WAL/SHM cleanup for obsolete client DB locations | `PhaseOneClientStartup` |
| Logger file selection and Qt message-handler fan-in | `PhaseOneStartupCoordinator` |
| Log-level and JSON logging config load | `PhaseOneClientStartup` |
| Migration registration for the authoritative process database | `PhaseOneStartupCoordinator` |
| Client main database open and cache database open | `PhaseOneClientStartup` |
| Theme and language restore after client DB open | `PhaseOneClientUiStartup` |
| News-prune deferred task after DB open | `PhaseOneClientStartup` |
| Session id creation and `ScreenStateManager` bootstrap | `PhaseOneClientStartup` |
| Legacy settings migration into the client DB | `PhaseOneClientStartup` |
| SessionManager start | `PhaseOneClientStartup` |
| Auth bootstrap via `TerminalShell::bootstrap_auth()` | `PhaseOneClientStartup` |
| Stack-owned `SessionGuard` lifetime | `PhaseOneClientLifecycle` |
| Main-thread pinning for `ReportBuilderService` and `ForumService` | `PhaseOneClientStartup` |
| MCP tool initialization | `PhaseOneClientStartup` |
| Python environment check and package-sync decisions | `PhaseOneClientStartup` |
| Setup-screen creation and completion flow | `PhaseOneClientUiStartup` |
| Crash-recovery dialog and restore decisions | `PhaseOneClientUiStartup` |
| Primary and restored secondary `WindowFrame` creation | `PhaseOneClientUiStartup` |
| Background agent discovery warmup | `PhaseOneClientStartup` |
| Server-mode authoritative DB path resolution, server-only writable path selection, and AppPaths override activation | `PhaseOneServerRuntime` |
| Server-mode migration registration, database open, route registration, bind/start, and abort-on-failure behavior | `PhaseOneServerRuntime` |
| Server-mode exclusion of instance lock, desktop shell startup, window creation, and client MCP/UI startup | `PhaseOneServerRuntime` |

Notes:

- TLS/OpenSSL bootstrap and `QT_TLS_BACKEND` early init are assigned to `PhaseOneStartupCoordinator`, the earliest shared startup owner.
- InstanceLock and same-profile new-window IPC handoff are assigned to `PhaseOneClientLifecycle`.
- Task 7 only moves the dedicated server runtime path. Client-path extraction remains deferred to Task 7b, so `main.cpp` continues to own the existing client startup flow until that follow-up lands.
