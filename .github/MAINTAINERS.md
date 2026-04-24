# Maintainer Runbook

Internal notes for Fincept Terminal maintainers. Contributors read [`CONTRIBUTING.md`](./CONTRIBUTING.md); this file is for people with repo write access.

## What the automation handles for you

You do **not** need to click around in GitHub Settings. These workflows keep the repo configured:

| Workflow                                        | What it does                                                                                           |
|-------------------------------------------------|--------------------------------------------------------------------------------------------------------|
| [`sync-labels.yml`](./workflows/sync-labels.yml)           | Creates / updates every label listed in [`.github/labels.json`](./labels.json). Runs on push to `main` when that file changes, or on manual dispatch. |
| [`sync-repo-topics.yml`](./workflows/sync-repo-topics.yml) | Ensures `hacktoberfest-excluded` is present in repo topics. Runs weekly + on manual dispatch.          |
| [`pr-gate.yml`](./workflows/pr-gate.yml)                   | On every PR event, flags PRs that don't close a labeled / scope-approved issue with `needs-scope-approval` and a templated comment. |
| [`pr-stale-close.yml`](./workflows/pr-stale-close.yml)     | Closes PRs that have carried `needs-scope-approval` for 7+ days with the `invalid` label.              |

To change labels: edit `.github/labels.json` and merge — the sync workflow applies the change. Extra labels that exist on the repo but aren't in the file are left alone (not deleted).

To add a repo topic: edit the `REQUIRED` array in `sync-repo-topics.yml` and merge.

## Scope-approval flow

1. A contributor opens an issue using one of the templates.
2. Triage it. If the scope is clear and you want external help, add one of:
   - `good-first-issue` — small, self-contained, documented enough for a newcomer.
   - `help-wanted` — you want external help but it is non-trivial.
   - `scope:approved` — you have discussed the approach with the author and agreed on the change.
3. Only after one of these labels is applied should anyone open a PR against the issue.
4. A PR that does not close a labeled issue is auto-flagged by `pr-gate.yml` with `needs-scope-approval` and a templated comment. After 7 days, `pr-stale-close.yml` closes it with the `invalid` label.
5. **Bypass:** apply `scope:approved` directly on the PR itself to bypass the gate — use this for maintainer PRs and for external PRs where the scope is obvious and the change is already reviewable.

## Hacktoberfest

- The repo-topics sync keeps `hacktoberfest-excluded` applied year-round, so spam PRs can't be counted.
- Do **not** add the `hacktoberfest-accepted` label to any PR unless it is a real, substantive contribution.
- Use the `spam` label for obvious farming attempts (single-line typo PRs, auto-formatter PRs, README-bloat PRs). Two `spam` / `invalid` labels across Hacktoberfest-participating repos disqualifies the author from the event.
- Most of these PRs are already filtered by `pr-gate.yml` before you see them.

## Contributor list curation

The contributor list is **not** auto-populated from git history. Inclusion rules:

- **3+ merged non-trivial PRs**, OR
- **1 substantive feature** reviewed and accepted (new screen, new broker integration, new data source, major refactor).

Non-trivial = real code change of >50 lines that is not formatting, comments, or generated output.

Review the list quarterly. Remove entries that were added by mistake or that correspond to reverted work.

## PR review triage order

1. PRs with `scope:approved` label → review next.
2. PRs closing a `good-first-issue` / `help-wanted` → review within a week.
3. PRs flagged `needs-scope-approval` → ignore; automation will close them.
4. PRs labeled `spam` → close immediately with one-line reason, do not engage.

## When to engage vs when to close

Engage (with review comments):
- Contributor shows understanding of the codebase.
- Change is in a labeled, approved area.
- Diff is minimal and focused.

Close with a templated comment:
- Auto-formatter churn (Black, clang-format, autopep8, isort).
- Wording-only string changes with no linked approved issue.
- PR adds the author to any contributors / credits file.
- PR from fork's `main` branch after they were asked once to use a topic branch.
- PR body says only "fix bug" / "small improvements" / "update code".

Templated close comment:

> Thanks for the interest in Fincept Terminal. Closing per our [contribution policy](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/.github/CONTRIBUTING.md) — this PR does not meet the scope-approval / minimum-diff / topic-branch requirements. You are welcome to open a labeled issue first and then a follow-up PR once the scope is agreed.
