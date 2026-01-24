#!/usr/bin/env python3
"""Update README.md download section with latest release links."""

import os
import re
import sys


def main():
    version = os.environ.get('VERSION', '0.0.0')
    tag = os.environ.get('TAG_NAME', 'v0.0.0')
    repo = os.environ.get('REPO_URL', '')

    section = f"""<!-- DOWNLOAD_SECTION_START -->
## Download Latest Release

**Version:** `{tag}`

| Platform | Architecture | Download |
|----------|-------------|----------|
| **macOS** | Apple Silicon | [Download]({repo}/releases/download/{tag}/FinceptTerminal-v{version}-macOS-arm64.dmg) |
| **macOS** | Intel | [Download]({repo}/releases/download/{tag}/FinceptTerminal-v{version}-macOS-x64.dmg) |
| **Linux** | x64 (AppImage) | [Download]({repo}/releases/download/{tag}/FinceptTerminal-v{version}-Linux-x64.AppImage) |
| **Linux** | x64 (Debian) | [Download]({repo}/releases/download/{tag}/FinceptTerminal-v{version}-Linux-x64.deb) |
| **Windows** | x64 | [Download]({repo}/releases/download/{tag}/FinceptTerminal-v{version}-Windows-x64.msi) |

[View All Releases]({repo}/releases)
<!-- DOWNLOAD_SECTION_END -->"""

    readme_path = 'README.md'
    if not os.path.exists(readme_path):
        print("README.md not found")
        return 1

    with open(readme_path, 'r') as f:
        content = f.read()

    updated = re.sub(
        r'<!-- DOWNLOAD_SECTION_START -->.*?<!-- DOWNLOAD_SECTION_END -->',
        section, content, flags=re.DOTALL
    )

    with open(readme_path, 'w') as f:
        f.write(updated)

    print(f"README.md updated with {tag} download links")
    return 0


if __name__ == "__main__":
    sys.exit(main())
