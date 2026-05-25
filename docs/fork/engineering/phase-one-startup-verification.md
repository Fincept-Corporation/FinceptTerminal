# Phase One Startup Verification

## Scope

- Task: 7b (`extract client startup and preserve desktop startup behavior`)
- Worktree: `/home/leo/dev/projects/FinceptTerminal/.worktrees/client-server-phase1`
- Date: 2026-05-24

## Verification Commands Run

```bash
cmake --preset linux-debug -DFINCEPT_BUILD_TESTS=ON && cmake --build --preset linux-debug --parallel 4 --target FinceptTerminal
```

Outcome:

- Configure and build succeeded.
- `build/linux-debug/FinceptTerminal` is now produced in this worktree.

```bash
ninja -C build/linux-debug CMakeFiles/FinceptTerminal.dir/Unity/unity_25_cxx.cxx.o
```

Outcome:

- Succeeded after the extraction changes.
- This unity object contains the Task 7b sources:
  - `src/app/main.cpp`
  - `src/app/PhaseOneClientLifecycle.cpp`
  - `src/app/PhaseOneClientStartup.cpp`
  - `src/app/PhaseOneClientUiStartup.cpp`
  - `src/app/PhaseOneStartupCoordinator.cpp`

## Client-Startup Log/Output Sample

- Offscreen client launch sample from `QT_QPA_PLATFORM=offscreen ./build/linux-debug/FinceptTerminal --profile phase1-smoke-a`:

```text
[PhaseOneStartup] QT_TLS_BACKEND=openssl
[PhaseOneStartup] client profile=phase1-smoke-a root=/home/leo/.local/share/com.fincept.terminal
[PhaseOneStartup] crash handler installed for client mode
[PhaseOneStartup] enabled Qt::AA_ShareOpenGLContexts
```

## Same-Profile Second-Instance Observable Result

- Offscreen smoke result:
  - first `phase1-smoke-a` process stayed running for at least 5 seconds
  - second `phase1-smoke-a` launch exited immediately with code `0`
  - measured second-launch duration was `0` seconds at shell granularity
- This matches the expected same-profile handoff/secondary-instance path.

## Different-Profile Concurrency Result

- Offscreen smoke result:
  - `phase1-smoke-a` and `phase1-smoke-b` were both running concurrently after 5 seconds
  - this preserves separate-primary behavior for different profiles

## Profile-Specific Path/Lock Proof

- Path proof: `PhaseOneClientStartup::initialize_pre_app()` still calls `ProfileManager::instance().set_active(options.profile)` before the client app object exists, then creates `AppPaths::root()` immediately so profile-manifest/path setup remains profile-scoped before later startup work.
- Lock proof: `PhaseOneClientLifecycle::acquire_primary()` still uses `FinceptTerminal-<active-profile>` as the lock key, so lock/IPC scope remains aligned with the selected profile.

## Same-Machine Server Smoke

Commands used:

```bash
./build/linux-debug/FinceptTerminal --server --server-host 127.0.0.1 --server-port 45450 --server-db build/linux-debug/phase1-smoke.db
```

Verified over raw TCP HTTP requests against the running server:

- bootstrap status initially returns `{"bootstrap_open": true}`
- first-admin bootstrap succeeds and bootstrap status flips to `false`
- username/password login succeeds and returns `session_id`, `expires_at`, `role`, `user_id`, and `username`
- a second login for the same user replaces the first session; the old session then returns `401 session_revoked`
- admin user creation succeeds and `GET /phase1/admin/users` returns both admin and standard user entries
- `GET /phase1/admin/audit-events` works, including filters by `user_identity` and `action_type`
- shared portfolio creation succeeds
- shared holding creation/listing succeeds
- logout succeeds and follow-up `GET /phase1/auth/session` returns `401 session_revoked`

## Overall Task 7b Verification Status

- Extraction compile coverage: yes, for the Task 7b client-startup sources via the built unity object.
- Full desktop smoke launch: partially verified offscreen in this environment.
- Same-profile relaunch observation: verified offscreen.
- Different-profile concurrent observation: verified offscreen.
- GUI-level login-screen interaction and all-screens visual confirmation were not exercised interactively in this headless environment.
