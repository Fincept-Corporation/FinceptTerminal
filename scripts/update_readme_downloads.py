#!/usr/bin/env python3
"""Update README.md download section with latest release links."""

import os
import re

VERSION = os.environ["VERSION"]
TAG_NAME = os.environ["TAG_NAME"]
REPO_URL = os.environ["REPO_URL"]
BASE_URL = f"{REPO_URL}/releases/download/{TAG_NAME}"

new_section = f"""<!-- DOWNLOAD_SECTION_START -->
## Download Latest Release

**Version:** `{TAG_NAME}`

| Platform | Architecture | Download |
|----------|-------------|----------|
| **macOS** | Apple Silicon | [Download]({BASE_URL}/FinceptTerminal-v{VERSION}-macOS-arm64.dmg) |
| **macOS** | Intel | [Download]({BASE_URL}/FinceptTerminal-v{VERSION}-macOS-x64.dmg) |
| **Linux** | x64 (AppImage) | [Download]({BASE_URL}/FinceptTerminal-v{VERSION}-Linux-x64.AppImage) |
| **Linux** | x64 (Debian) | [Download]({BASE_URL}/FinceptTerminal-v{VERSION}-Linux-x64.deb) |
| **Windows** | x64 | [Download]({BASE_URL}/FinceptTerminal-v{VERSION}-Windows-x64.msi) |

[View All Releases]({REPO_URL}/releases)
<!-- DOWNLOAD_SECTION_END -->"""

readme_path = "README.md"
with open(readme_path, "r", encoding="utf-8") as f:
    content = f.read()

updated = re.sub(
    r"<!-- DOWNLOAD_SECTION_START -->.*?<!-- DOWNLOAD_SECTION_END -->",
    new_section,
    content,
    flags=re.DOTALL,
)

with open(readme_path, "w", encoding="utf-8") as f:
    f.write(updated)

print(f"Updated README.md download section to {TAG_NAME}")
