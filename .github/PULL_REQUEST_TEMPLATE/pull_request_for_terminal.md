## What does this PR do?
<!-- Brief description of the change -->


## Type of Change
- [ ] Bug fix
- [ ] New feature / screen
- [ ] Performance improvement
- [ ] Refactoring
- [ ] Documentation
- [ ] Build / config change

## Related Issues
<!-- Fixes #, Closes #, or Related to # -->


## Changes Made
<!-- List key files/components changed and what was done -->


## How to Test
1.
2.
3.

## Checklist
- [ ] Builds without errors on target platform
- [ ] UI thread is never blocked (no `waitForFinished()` on main thread)
- [ ] Timers start/stop in `showEvent()`/`hideEvent()`
- [ ] No raw `QProcess` for Python — used `PythonRunner::instance().run()`
- [ ] No sensitive data (API keys, credentials) committed
- [ ] Tested manually on: (Windows / macOS / Linux)

## Screenshots
<!-- For UI changes, include before/after screenshots -->
