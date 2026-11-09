# Contribution Policy — Fincept Terminal

This file is the **contribution policy** — the rules every PR must follow. GitHub surfaces it automatically on issues and PRs, so we keep it short and strict.

Looking for **how to build, where code lives, code conventions?** → [`docs/CONTRIBUTING.md`](../docs/CONTRIBUTING.md).

Maintainers' operational runbook → [`.github/MAINTAINERS.md`](./MAINTAINERS.md).

---

## TL;DR

1. **Open an issue first.** Wait for a maintainer to add `good-first-issue`, `help-wanted`, or `scope:approved`.
2. **One logical change per PR.** Minimum diff. Link the issue with `Closes #NNN`.
3. **No auto-formatter churn.** No `print()` / `std::cout`. No PR from your fork's `main` branch.

PRs that ignore these rules are closed — not personal, it's automated.

---

## Full policy

### 1. PRs must link a labeled, scope-approved issue

Open an issue first, or claim an existing one that carries one of:

- `good-first-issue` — newcomer-friendly, pre-approved.
- `help-wanted` — open for external contribution.
- `scope:approved` — maintainer has agreed the approach with the author.

PRs against issues **without** one of these labels are auto-flagged by the `PR Scope Gate` workflow and closed after 7 days. A maintainer can bypass the gate on a per-PR basis by adding `scope:approved` directly to the PR.

You may not claim more than one issue at a time until your previous PR merges or closes.

### 2. No unsolicited wording / formatting / comment-only PRs

Rewording a single error message, renaming a local variable, rewrapping a comment, fixing a typo in a log string, or running `black` / `clang-format` / `isort` / `autopep8` / `prettier` on a file you otherwise didn't touch are **not** accepted unless a maintainer explicitly requested them on an issue.

**Exception:** typos in user-facing docs (`README.md`, `docs/*.md`) may be submitted directly — batch multiple typos into one PR.

### 3. No auto-formatter churn

Do not run any code formatter over existing files. Match the surrounding style exactly. A diff showing hundreds of reformatted lines for ~15 lines of real change will be closed without review.

### 4. PRs must build and run

The change must compile on your target platform before submission. `build-cpp.yml` CI must pass. If you couldn't build locally, say so in the PR body — don't submit untested code and claim it "should work".

### 5. Minimum scope, one logical change per PR

One feature or one bug fix per PR. Do not bundle unrelated changes. If your fix reveals a second bug, open a separate issue and PR.

### 6. Use a topic branch, never your fork's `main`

Examples: `fix/execute-multi-user-config`, `feat/crypto-depth-chart`, `perf/market-data-coalesce`. PRs opened from `your-fork:main` will be asked to rebase onto a topic branch.

### 7. Describe the change like you mean it

The PR description must explain the user-visible effect and the reasoning. "Small improvements" / "fix bug" / "update code" / "minor refactor" are not acceptable descriptions — the PR will be closed.

---

## PRs closed on sight

- Single-line string edits with no linked approved issue.
- Black / autopep8 / isort / clang-format / prettier-only PRs.
- `README.md` / `CONTRIBUTING.md` rewrites that don't fix an actual error.
- PRs that add the author to a contributors / credits file, or add a CODEOWNERS entry for themselves.
- PRs that delete "dead code" without an analysis proving it's unreachable.
- PRs that touch `CLAUDE.md`, this file, `MAINTAINERS.md`, or any `docs/*` file without an explicit maintainer ask.
- PRs whose body is only "fix bug" / "small improvements" / "update code".

---

## Hacktoberfest

This repository carries the `hacktoberfest-excluded` topic. PRs opened during October that don't follow this policy are labeled `invalid` / `spam` and closed — they will not count toward Hacktoberfest. The `hacktoberfest-accepted` label is applied **only** to substantive, reviewed contributions.

---

## Contributor list / credits

The contributor list is **manually curated** — we don't auto-include every merged author. Inclusion requires:

- **3+ merged non-trivial PRs** (non-trivial = >50 lines of real code change, not formatting or comments), OR
- **1 substantive feature** contribution reviewed and accepted (new screen, new broker integration, new data source, major refactor).

If you're contributing for the list and not for the product, this isn't the right project for you — and that's fine.

---

## Contributor terms

Fincept Terminal is licensed under **AGPL-3.0-or-later** — the full text is in [`LICENSE`](../LICENSE), and it is the only licence that applies to this repository. The terms below apply only to people who contribute code or content to the repository; they do not apply to ordinary use of the software.

By contributing to Fincept Terminal (via pull request, patch, comment, issue, or any other means), each contributor agrees to the following terms:

1. **Assignment and licence-in.** To the maximum extent permitted by law, each contribution is a work made for hire owned by Fincept Corporation. To the extent any contribution is not a work made for hire, the contributor hereby presently and unconditionally assigns to Fincept Corporation all right, title, and interest in and to the contribution, including all copyright, moral rights (waived to the maximum extent permitted), and related rights. Without limiting the foregoing, the contributor grants Fincept Corporation a perpetual, worldwide, non-exclusive, royalty-free, irrevocable, sublicensable licence — with the right to sublicense through multiple tiers — to use, reproduce, modify, distribute, sublicense, and relicense the contribution under AGPL-3.0, under the proprietary licence of Fincept Terminal Enterprise, and under any future licence selected by Fincept Corporation.
2. **No joint authorship.** Each contributor expressly disclaims any claim of joint authorship in the Software as a whole or in any other contributor's contributions. Contributing does not grant the contributor any licence to the Software as a whole or to any other contributor's contributions beyond the AGPL-3.0 licence under which the Software is published.
3. **Warranty and indemnity.** Each contributor warrants that (a) the contribution is the contributor's original work or is properly attributed and licensed for the contribution; (b) the contributor has the legal right to make the contribution and grant the rights herein; (c) the contribution is free of any obligations to any current or former employer, client, university, or third party that would conflict with this assignment; and (d) the contribution does not infringe any third-party rights. The contributor shall indemnify, defend, and hold harmless Fincept Corporation from any breach of these warranties.
4. **As-is.** Contributions are provided "as is" without warranties of any kind beyond the warranties in item 3 above.
5. **No Enterprise rights back.** Contributing does not grant the contributor a seat in, or any licence to, Fincept Terminal Enterprise, nor access to Fincept's private datasets or paid API tiers.

---

## Questions before you code?

- Ask on the issue before writing code. Maintainers are happy to scope-approve early.
- [support@fincept.in](mailto:support@fincept.in) · [Discord](https://discord.gg/ae87a8ygbN)
