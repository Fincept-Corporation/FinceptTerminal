#!/usr/bin/env python3
"""Update the download-table block inside README.md.

Called from release.yml with two env vars:

    README       Path to the README file (default: README.md).
    NEW_BLOCK    Full replacement markdown, including the
                 <!-- DOWNLOAD-TABLE-START --> / END sentinels.

Behaviour:
  * If the START sentinel already exists in the README, replace the
    entire START…END block.
  * Otherwise, replace the old "### Option 1 — Download Pre-built
    Binary" section (up to the next "---" or "### Option 2" line).
  * If neither pattern matches, insert the block just after the
    "## Installation" heading.

Exits 0 on success, 1 on failure.
"""
from __future__ import annotations

import os
import pathlib
import re
import sys


def main() -> int:
    readme_path = pathlib.Path(os.environ.get("README", "README.md"))
    new_block = os.environ.get("NEW_BLOCK")
    if new_block is None:
        print("error: NEW_BLOCK env var is required", file=sys.stderr)
        return 1
    if not readme_path.exists():
        print(f"error: README not found: {readme_path}", file=sys.stderr)
        return 1

    content = readme_path.read_text(encoding="utf-8")
    updated = content

    marker_pattern = re.compile(
        r"<!-- DOWNLOAD-TABLE-START -->.*?<!-- DOWNLOAD-TABLE-END -->",
        re.DOTALL,
    )
    if marker_pattern.search(content):
        updated = marker_pattern.sub(new_block, content)
        mode = "existing markers replaced"
    else:
        heading_pattern = re.compile(
            r"### Option 1 — Download Pre-built Binary.*?(?=\n---|\n### Option 2)",
            re.DOTALL,
        )
        updated = heading_pattern.sub(new_block, content)
        if updated == content:
            updated = content.replace(
                "## Installation\n",
                "## Installation\n\n" + new_block + "\n",
            )
            mode = "block prepended before '## Installation'"
        else:
            mode = "sentinels inserted by replacing old Option-1 heading"

    if updated == content:
        print("warning: README not modified — no pattern matched", file=sys.stderr)
    else:
        readme_path.write_text(updated, encoding="utf-8")
    print(f"README updated ({mode})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
