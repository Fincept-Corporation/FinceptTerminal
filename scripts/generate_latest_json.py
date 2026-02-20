#!/usr/bin/env python3
"""Generate latest.json for Tauri auto-updater from release files."""

import json
import os
import glob
from datetime import datetime, timezone

VERSION = os.environ["VERSION"]
TAG_NAME = os.environ["TAG_NAME"]
REPO_URL = os.environ["REPO_URL"]
RELEASE_DIR = "release-files"
BASE_URL = f"{REPO_URL}/releases/download/{TAG_NAME}"


def read_sig(path):
    with open(path, "r") as f:
        return f.read().strip()


platforms = {}

# macOS arm64
sig_file = os.path.join(RELEASE_DIR, "FinceptTerminal-arm64-darwin.app.tar.gz.sig")
tar_file = os.path.join(RELEASE_DIR, "FinceptTerminal-arm64-darwin.app.tar.gz")
if os.path.exists(sig_file) and os.path.exists(tar_file):
    platforms["darwin-aarch64"] = {
        "signature": read_sig(sig_file),
        "url": f"{BASE_URL}/FinceptTerminal-arm64-darwin.app.tar.gz",
    }

# macOS x64
sig_file = os.path.join(RELEASE_DIR, "FinceptTerminal-x64-darwin.app.tar.gz.sig")
tar_file = os.path.join(RELEASE_DIR, "FinceptTerminal-x64-darwin.app.tar.gz")
if os.path.exists(sig_file) and os.path.exists(tar_file):
    platforms["darwin-x86_64"] = {
        "signature": read_sig(sig_file),
        "url": f"{BASE_URL}/FinceptTerminal-x64-darwin.app.tar.gz",
    }

# Linux x64
sig_file = os.path.join(RELEASE_DIR, "FinceptTerminal-x86_64-linux.AppImage.tar.gz.sig")
tar_file = os.path.join(RELEASE_DIR, "FinceptTerminal-x86_64-linux.AppImage.tar.gz")
if os.path.exists(sig_file) and os.path.exists(tar_file):
    platforms["linux-x86_64"] = {
        "signature": read_sig(sig_file),
        "url": f"{BASE_URL}/FinceptTerminal-x86_64-linux.AppImage.tar.gz",
    }

# Windows x64
sig_file = os.path.join(RELEASE_DIR, "FinceptTerminal-x86_64-windows.nsis.zip.sig")
zip_file = os.path.join(RELEASE_DIR, "FinceptTerminal-x86_64-windows.nsis.zip")
if os.path.exists(sig_file) and os.path.exists(zip_file):
    platforms["windows-x86_64"] = {
        "signature": read_sig(sig_file),
        "url": f"{BASE_URL}/FinceptTerminal-x86_64-windows.nsis.zip",
    }

latest = {
    "version": VERSION,
    "notes": f"See the release notes at {REPO_URL}/releases/tag/{TAG_NAME}",
    "pub_date": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
    "platforms": platforms,
}

out_path = os.path.join(RELEASE_DIR, "latest.json")
with open(out_path, "w") as f:
    json.dump(latest, f, indent=2)

print(f"Generated {out_path} with {len(platforms)} platforms: {list(platforms.keys())}")
