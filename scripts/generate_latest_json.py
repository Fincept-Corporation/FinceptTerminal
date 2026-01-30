#!/usr/bin/env python3
"""Generate unified latest.json for Tauri auto-updater from platform-specific build artifacts."""

import json
import os
import glob
import sys
from datetime import datetime, timezone


def main():
    version = os.environ.get('VERSION', '0.0.0')
    tag_name = os.environ.get('TAG_NAME', 'v0.0.0')
    repo_url = os.environ.get('REPO_URL', '')

    merged = {
        "version": version,
        "notes": f"FinceptTerminal {tag_name} - See release notes at {repo_url}/releases/tag/{tag_name}",
        "pub_date": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "platforms": {}
    }

    # Try to merge from platform-specific latest.json files first
    platform_files = glob.glob("all-artifacts/**/latest-*.json", recursive=True)
    print(f"Found {len(platform_files)} platform-specific latest.json files")

    for pf in platform_files:
        print(f"Processing: {pf}")
        try:
            with open(pf, 'r') as f:
                data = json.load(f)
                if 'platforms' in data:
                    for platform_key, platform_data in data['platforms'].items():
                        if 'url' in platform_data:
                            filename = os.path.basename(platform_data['url'])
                            platform_data['url'] = f"{repo_url}/releases/download/{tag_name}/{filename}"
                    merged['platforms'].update(data['platforms'])
                    print(f"  Added platforms: {list(data['platforms'].keys())}")
        except Exception as e:
            print(f"  Error processing {pf}: {e}")

    # If no platforms found from JSON files, generate from artifacts
    if not merged['platforms']:
        print("No platforms found from JSON files, generating from artifacts...")

        # Windows NSIS (x86_64)
        nsis_zips = glob.glob("release-files/*-x86_64-windows.nsis.zip")
        nsis_sigs = glob.glob("release-files/*-x86_64-windows.nsis.zip.sig")
        if nsis_zips and nsis_sigs:
            nsis_filename = os.path.basename(nsis_zips[0])
            with open(nsis_sigs[0], 'r') as f:
                signature = f.read().strip()
            merged['platforms']['windows-x86_64'] = {
                "signature": signature,
                "url": f"{repo_url}/releases/download/{tag_name}/{nsis_filename}"
            }
            print(f"Added windows-x86_64 from artifacts: {nsis_filename}")

        # macOS ARM64 (aarch64)
        macos_arm_tars = glob.glob("release-files/*-arm64-darwin.app.tar.gz")
        macos_arm_sigs = glob.glob("release-files/*-arm64-darwin.app.tar.gz.sig")
        if macos_arm_tars and macos_arm_sigs:
            tar_filename = os.path.basename(macos_arm_tars[0])
            with open(macos_arm_sigs[0], 'r') as f:
                signature = f.read().strip()
            merged['platforms']['darwin-aarch64'] = {
                "signature": signature,
                "url": f"{repo_url}/releases/download/{tag_name}/{tar_filename}"
            }
            print(f"Added darwin-aarch64 from artifacts: {tar_filename}")

        # macOS x64 (x86_64)
        macos_x64_tars = glob.glob("release-files/*-x64-darwin.app.tar.gz")
        macos_x64_sigs = glob.glob("release-files/*-x64-darwin.app.tar.gz.sig")
        if macos_x64_tars and macos_x64_sigs:
            tar_filename = os.path.basename(macos_x64_tars[0])
            with open(macos_x64_sigs[0], 'r') as f:
                signature = f.read().strip()
            merged['platforms']['darwin-x86_64'] = {
                "signature": signature,
                "url": f"{repo_url}/releases/download/{tag_name}/{tar_filename}"
            }
            print(f"Added darwin-x86_64 from artifacts: {tar_filename}")

        # Linux x86_64
        linux_tars = glob.glob("release-files/*-x86_64-linux.AppImage.tar.gz")
        linux_sigs = glob.glob("release-files/*-x86_64-linux.AppImage.tar.gz.sig")
        if linux_tars and linux_sigs:
            tar_filename = os.path.basename(linux_tars[0])
            with open(linux_sigs[0], 'r') as f:
                signature = f.read().strip()
            merged['platforms']['linux-x86_64'] = {
                "signature": signature,
                "url": f"{repo_url}/releases/download/{tag_name}/{tar_filename}"
            }
            print(f"Added linux-x86_64 from artifacts: {tar_filename}")

    # Write the merged JSON
    with open('release-files/latest.json', 'w') as f:
        json.dump(merged, f, indent=2)

    print(f"\n=== Generated latest.json ===")
    print(json.dumps(merged, indent=2))

    if not merged['platforms']:
        print("ERROR: No platforms found for latest.json!")
        return 1

    print(f"\nSuccessfully generated latest.json with {len(merged['platforms'])} platform(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
