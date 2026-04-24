<!--
Before opening this PR, confirm you have read .github/CONTRIBUTING.md.
PRs that do not link a scope-approved issue, or that contain auto-formatter churn,
or that are single-line wording edits without maintainer request, WILL BE CLOSED.
-->

## Scope gate (required)

- [ ] This PR closes an issue that carries the `good-first-issue`, `help-wanted`, or `scope:approved` label.
- [ ] I confirmed the scope with a maintainer on the issue before writing code.
- [ ] This PR is from a **topic branch** (not `main` on my fork).
- [ ] This PR makes **one logical change**. It does not bundle unrelated fixes.
- [ ] I did **not** run Black / autopep8 / isort / clang-format / Prettier on files I did not otherwise modify.
- [ ] Diff is minimal — no reformatting of surrounding lines that are unrelated to the change.

Linked issue (required — use `Closes #NNN` or `Fixes #NNN`):

Closes #

## What does this PR do?
<!-- Plain-English description of the user-visible effect. "Small improvement" is not acceptable. -->


## Type of change

- [ ] Bug fix
- [ ] New feature / screen
- [ ] Performance improvement
- [ ] Refactoring (with linked issue)
- [ ] Documentation (see CONTRIBUTING — docs-only PRs allowed only for genuine errors)
- [ ] Build / config change

## Changes made
<!-- List key files and what was done to each. Do not paste the diff. -->


## How to test

1.
2.
3.

## Architecture / code-quality checklist

- [ ] Builds without errors on my target platform (Windows / macOS / Linux) — state which below
- [ ] UI thread is never blocked (no `waitForFinished()` on main thread) — see CLAUDE.md P1
- [ ] Timers start/stop in `showEvent()` / `hideEvent()` — see P3
- [ ] No raw `QProcess` for Python — used `PythonRunner::instance().run()` — see P4
- [ ] No `print()` in Python scripts — used `logger.info` / `logger.warning` — see P14
- [ ] No sensitive data (API keys, credentials) committed
- [ ] DataHub rules (D1–D5) respected if touching data flow
- [ ] Tested manually on: (Windows / macOS / Linux — pick one)

## Screenshots / logs
<!-- For UI changes include before/after. For bug fixes include the log line that proves the fix. -->
