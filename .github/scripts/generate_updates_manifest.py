#!/usr/bin/env python3
"""Generate updates.json (manifest schema-version 3) for the in-app updater.

The terminal reads updates.json and its detached Ed25519 signature
updates.json.sig as assets of the latest GitHub release
(releases/latest/download/updates.json), falling back to the copy of the pair
committed on main. Schema and pinned public key:
fincept-qt/src/services/updater/UpdateManifest.h. This script only writes the
manifest; .github/scripts/sign_updates_manifest.sh signs it.

Environment:
    WORK     Directory holding the release files. Searched recursively, so the
             release job's dist/<platform>/ layout works as it is.
    VERSION  Version the installers were built as: the CMake project version,
             strictly "X.Y.Z", no "v" (the client rejects anything else).
    TAG      Release tag: "v<VERSION>", or "v<VERSION>-<suffix>" (prerelease).
    REPO     "owner/name", e.g. "Fincept-Corporation/FinceptTerminal".
    NOTES    Optional short plain text. Default: "Fincept Terminal v<VERSION>".

Usage:
    generate_updates_manifest.py --output <path> [--allow-missing]
    generate_updates_manifest.py --self-test

Assets are matched by EXACT file name (table below), so the "-wizard"
installers, files of another version and leftovers of discontinued kinds are
never picked up. Linux ships only the .deb and the Flatpak: the AppImage
(linux-*-appimage) and .rpm (linux-*-rpm) keys are gone, so installed copies
of those kinds find no package and the client points them to the release page.
The output is deterministic -- fixed key order, indent=2, ASCII only, LF line
endings, one trailing newline -- and written as bytes by this script, because
the signature covers the file byte for byte.

Exit status: 0 on success; 1 when a required asset is missing (--allow-missing
downgrades that to a warning, for local use), an input is malformed or the
output cannot be written; 2 on bad command-line usage.
"""
from __future__ import annotations

import argparse
import contextlib
import hashlib
import io
import json
import os
import pathlib
import re
import sys
from typing import Callable, Mapping

SCHEMA_VERSION = 3

# (manifest asset key, release file name -- "{v}" is VERSION, required?)
# The key is "<platform>-<install kind>" as the client derives it
# (InstallContext::asset_key). Required = produced by every release: the
# release job only runs after all build jobs succeeded. The others are
# listed when such a file exists and skipped otherwise.
ASSETS: tuple[tuple[str, str, bool], ...] = (
    ("windows-x64-ifw",      "FinceptTerminal-{v}-windows-x64-setup.exe",   True),
    ("windows-arm64-ifw",    "FinceptTerminal-{v}-windows-arm64-setup.exe", False),
    ("macos-arm64-dmg",      "FinceptTerminal-{v}-macos-arm64-setup.dmg",   True),
    ("macos-x64-dmg",        "FinceptTerminal-{v}-macos-x64-setup.dmg",     False),
    ("linux-x64-deb",        "FinceptTerminal-{v}-linux-x64.deb",           True),
    ("linux-arm64-deb",      "FinceptTerminal-{v}-linux-arm64.deb",         False),
    ("linux-x64-flatpak",    "FinceptTerminal-{v}-linux-x86_64.flatpak",    True),
    ("linux-arm64-flatpak",  "FinceptTerminal-{v}-linux-aarch64.flatpak",   False),
)

CHUNK_SIZE = 1 << 20  # installers are hundreds of MB: never read one whole

# [0-9], not \d: \d also matches non-ASCII digits, which the client rejects.
VERSION_RE = re.compile(r"[0-9]+\.[0-9]+\.[0-9]+")
REPO_RE = re.compile(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+")
INSTALLER_VERSION_RE = re.compile(r"FinceptTerminal-([0-9]+\.[0-9]+\.[0-9]+)-")


class ManifestError(Exception):
    """Input that must not produce a manifest."""


def sha256_and_size(path: pathlib.Path, chunk_size: int = CHUNK_SIZE) -> tuple[str, int]:
    """SHA-256 (lower-case hex) and byte count of a file, read in chunks."""
    digest = hashlib.sha256()
    size = 0
    with open(path, "rb") as f:
        while True:
            block = f.read(chunk_size)
            if not block:
                break
            digest.update(block)
            size += len(block)
    return digest.hexdigest(), size


def build_manifest(
    work: pathlib.Path,
    version: str,
    tag: str,
    repo: str,
    notes: str | None = None,
    allow_missing: bool = False,
    log: Callable[[str], None] = print,
    chunk_size: int = CHUNK_SIZE,
) -> dict:
    if not VERSION_RE.fullmatch(version):
        raise ManifestError(
            f"VERSION '{version}' is not X.Y.Z -- the updater only accepts a plain "
            f"X.Y.Z version (pass the CMake project version, without 'v' or a suffix)")
    if not re.fullmatch(rf"v{re.escape(version)}(-[0-9A-Za-z][0-9A-Za-z.-]*)?", tag):
        raise ManifestError(
            f"TAG '{tag}' does not name VERSION {version}. The installers are built, "
            f"named and report themselves as {version} (fincept-qt/CMakeLists.txt), so "
            f"the release tag must be v{version} (or v{version}-<suffix> for a "
            f"prerelease). Tag the commit whose CMakeLists.txt has the version you release.")
    if not REPO_RE.fullmatch(repo):
        raise ManifestError(f"REPO '{repo}' is not 'owner/name'")
    if not work.is_dir():
        raise ManifestError(f"WORK directory not found: {work}")

    by_name: dict[str, list[pathlib.Path]] = {}
    for p in sorted(work.rglob("*")):
        if p.is_file():
            by_name.setdefault(p.name, []).append(p)
    log(f"Found {sum(len(v) for v in by_name.values())} file(s) under {work}")

    assets: dict[str, dict] = {}
    missing: list[str] = []
    asset_names = set()
    for key, pattern, required in ASSETS:
        name = pattern.format(v=version)
        asset_names.add(name)
        paths = by_name.get(name, [])
        if len(paths) > 1:
            raise ManifestError(
                f"{name} exists more than once ({', '.join(str(p) for p in paths)}) "
                f"-- cannot tell which one the release will carry")
        if not paths:
            if required:
                missing.append(f"{key} ({name})")
            else:
                log(f"  {key:<21} not present (optional) -- skipped")
            continue
        sha, size = sha256_and_size(paths[0], chunk_size)
        if size == 0:
            raise ManifestError(f"{paths[0]} is empty -- the build produced a broken {key} asset")
        assets[key] = {
            "url": f"https://github.com/{repo}/releases/download/{tag}/{name}",
            "sha256": sha,
            "size": size,
        }
        log(f"  {key:<21} {name}  {size} bytes  sha256 {sha}")

    for name in sorted(by_name):
        if name.startswith("FinceptTerminal-") and name not in asset_names:
            log(f"  (not an update asset, ignored: {name})")

    if missing:
        msg = "required update asset(s) missing: " + ", ".join(missing)
        found = sorted({m.group(1) for n in by_name if (m := INSTALLER_VERSION_RE.match(n))})
        if found and version not in found:
            msg += (f". Found installers for version(s) {', '.join(found)} only -- "
                    f"VERSION must be the version the installers were built as")
        if not allow_missing:
            raise ManifestError(msg)
        log(f"::warning::{msg} (--allow-missing: continuing without them)")
    if not assets:
        raise ManifestError(f"no update asset found under {work} for version {version}")

    notes = (notes or "").strip() or f"Fincept Terminal v{version}"
    return {
        "schema-version": SCHEMA_VERSION,
        "version": version,
        "release-url": f"https://github.com/{repo}/releases/tag/{tag}",
        "notes": notes,
        "assets": assets,
    }


def render(manifest: dict) -> bytes:
    """The exact bytes that get signed and served."""
    return (json.dumps(manifest, indent=2, ensure_ascii=True) + "\n").encode("ascii")


def write_bytes(path: pathlib.Path, data: bytes) -> None:
    """Write via a temporary file + rename, so a failed run leaves no half file."""
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_name(path.name + ".tmp")
    with open(tmp, "wb") as f:
        f.write(data)
    os.replace(tmp, path)


def main(argv: list[str] | None = None, env: Mapping[str, str] | None = None) -> int:
    env = os.environ if env is None else env
    ap = argparse.ArgumentParser(description="Generate updates.json (schema-version 3).")
    ap.add_argument("--output", metavar="PATH", help="where to write the manifest")
    ap.add_argument("--allow-missing", action="store_true",
                    help="warn instead of failing when a required asset is missing (local use)")
    ap.add_argument("--self-test", action="store_true", help="run the built-in tests and exit")
    args = ap.parse_args(argv)
    if args.self_test:
        return self_test()
    if not args.output:
        ap.error("--output is required")

    unset = [k for k in ("WORK", "VERSION", "TAG", "REPO") if not env.get(k)]
    if unset:
        print(f"::error::missing environment variable(s): {', '.join(unset)}")
        return 1
    try:
        manifest = build_manifest(pathlib.Path(env["WORK"]), env["VERSION"], env["TAG"],
                                  env["REPO"], env.get("NOTES"), args.allow_missing)
    except ManifestError as e:
        print(f"::error::{e}")
        return 1
    out = pathlib.Path(args.output)
    try:
        write_bytes(out, render(manifest))
    except OSError as e:
        print(f"::error::cannot write {out}: {e}")
        return 1
    print(f"Wrote {out}: schema-version {SCHEMA_VERSION}, version {manifest['version']}, "
          f"{len(manifest['assets'])} asset(s): {', '.join(manifest['assets'])}")
    return 0


# ── Self-test ────────────────────────────────────────────────────────────────
# Dummy release files in a temporary directory (removed afterwards).
# Run: python3 .github/scripts/generate_updates_manifest.py --self-test

def self_test() -> int:
    import tempfile

    results: list[tuple[bool, str]] = []

    def check(ok: bool, label: str) -> None:
        results.append((bool(ok), label))

    def run_main(argv: list[str], env: dict) -> tuple[int, str]:
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf), contextlib.redirect_stderr(buf):
            try:
                rc = main(argv, env)
            except SystemExit as e:  # argparse usage errors
                rc = e.code if isinstance(e.code, int) else 2
        return rc, buf.getvalue()

    v, tag, repo = "4.5.3", "v4.5.3", "Example-Org/ExampleRepo"
    files = {
        # every required asset, in the release job's dist/<platform>/ layout
        "windows/FinceptTerminal-4.5.3-windows-x64-setup.exe": b"W" * 5000,
        "macos/FinceptTerminal-4.5.3-macos-arm64-setup.dmg": bytes(range(256)) * 9,
        "linux/FinceptTerminal-4.5.3-linux-x64.deb": b"!<arch>\n" + b"d" * 1234,
        "flatpak/FinceptTerminal-4.5.3-linux-x86_64.flatpak": b"F" * 4097,
        # optional variant that exists -> listed
        "flatpak/FinceptTerminal-4.5.3-linux-aarch64.flatpak": b"A" * 10,
        # never update assets (discontinued Linux kinds included)
        "linux/FinceptTerminal-4.5.3-linux-x64-setup.run": b"appimage\n" * 333,
        "linux/FinceptTerminal-4.5.3-linux-x64.rpm": b"\xed\xab\xee\xdb" + b"r" * 777,
        "linux/FinceptTerminal-4.5.3-linux-x64-setup.run.zsync": b"zsync",
        "linux/FinceptTerminal-4.5.3-linux-x64-wizard.bin": b"wizard",
        "macos/FinceptTerminal-4.5.3-macos-arm64-wizard.zip": b"wizard",
        "windows/FinceptTerminal-4.5.2-windows-x64-setup.exe": b"old",
        "updates.json": b"{}",
    }
    expected_keys = ["windows-x64-ifw", "macos-arm64-dmg", "linux-x64-deb",
                     "linux-x64-flatpak", "linux-arm64-flatpak"]
    quiet = lambda _msg: None  # noqa: E731

    def cases() -> None:
        with tempfile.TemporaryDirectory(prefix="updates-manifest-selftest-") as td:
            root = pathlib.Path(td)
            work = root / "dist"
            for rel, data in files.items():
                p = work / rel
                p.parent.mkdir(parents=True, exist_ok=True)
                p.write_bytes(data)
            env = {"WORK": str(work), "VERSION": v, "TAG": tag, "REPO": repo}

            # 1. Content. chunk_size=7 makes every file span many reads.
            m = build_manifest(work, v, tag, repo, log=quiet, chunk_size=7)
            check(list(m) == ["schema-version", "version", "release-url", "notes", "assets"],
                  "top-level key order")
            check(m["schema-version"] == 3 and m["version"] == v, "schema-version 3 + version")
            check(m["release-url"] == f"https://github.com/{repo}/releases/tag/{tag}", "release-url")
            check(m["notes"] == "Fincept Terminal v4.5.3", "default notes")
            check(list(m["assets"]) == expected_keys,
                  "asset keys/order (AppImage, rpm, zsync, wizard, other version, updates.json ignored)")
            for rel, data in files.items():
                name = rel.rsplit("/", 1)[-1]
                entry = next((a for a in m["assets"].values() if a["url"].endswith("/" + name)), None)
                if entry is None:
                    continue
                check(entry["url"] == f"https://github.com/{repo}/releases/download/{tag}/{name}",
                      f"url of {name}")
                check(entry["sha256"] == hashlib.sha256(data).hexdigest(), f"chunked sha256 of {name}")
                check(entry["size"] == len(data), f"size of {name}")
                check(list(entry) == ["url", "sha256", "size"], f"asset field order of {name}")

            # 2. Bytes.
            b1 = render(m)
            b2 = render(build_manifest(work, v, tag, repo, log=quiet))
            check(b1 == b2, "deterministic output (two runs, different chunk sizes)")
            check(b1.endswith(b"}\n") and not b1.endswith(b"\n\n"), "exactly one trailing newline")
            check(b"\r" not in b1, "LF line endings only")
            check(json.loads(b1) == m and b1.startswith(b'{\n  "schema-version": 3,'), "indent=2 JSON")

            # 3. CLI writes the file itself, byte for byte.
            out = root / "out" / "updates.json"
            rc, log = run_main(["--output", str(out)], env)
            check(rc == 0 and out.read_bytes() == b1, f"--output writes the rendered bytes (rc={rc})")
            check(not (root / "out" / "updates.json.tmp").exists(), "no temp file left behind")
            rc, _ = run_main([], env)
            check(rc == 2, "--output is required")
            rc, log = run_main(["--output", str(root / "x.json")], {k: env[k] for k in ("WORK", "TAG")})
            check(rc == 1 and "VERSION" in log and "REPO" in log, "missing env vars reported")

            # 4. Non-ASCII notes stay ASCII on disk.
            rc, _ = run_main(["--output", str(out)], dict(env, NOTES="  Thèmes — faster  \n"))
            n = json.loads(out.read_bytes())
            check(rc == 0 and n["notes"] == "Thèmes — faster" and out.read_bytes().isascii(),
                  "custom NOTES stripped and ASCII-escaped")

            # 5. Required asset missing.
            deb = work / "linux/FinceptTerminal-4.5.3-linux-x64.deb"
            deb_bytes = deb.read_bytes()
            deb.unlink()
            missing_out = root / "missing.json"
            rc, log = run_main(["--output", str(missing_out)], env)
            check(rc == 1 and "linux-x64-deb" in log and not missing_out.exists()
                  and "Found installers" not in log,
                  "missing required asset fails and writes nothing")
            rc, log = run_main(["--output", str(missing_out), "--allow-missing"], env)
            check(rc == 0 and "linux-x64-deb" not in json.loads(missing_out.read_bytes())["assets"]
                  and "::warning::" in log, "--allow-missing warns and omits the asset")
            deb.write_bytes(deb_bytes)

            # 6. Wrong VERSION: points at the version the files do have.
            rc, log = run_main(["--output", str(root / "y.json")], dict(env, VERSION="4.5.9", TAG="v4.5.9"))
            check(rc == 1 and "4.5.2, 4.5.3" in log, "version mismatch hint lists the versions found")

            # 7. Malformed inputs.
            for bad, label in ((dict(env, VERSION="4.5.3-rc1", TAG="v4.5.3-rc1"), "suffixed VERSION"),
                               (dict(env, TAG="v4.5.4"), "TAG for another version"),
                               (dict(env, TAG="4.5.3"), "TAG without v"),
                               (dict(env, REPO="no-slash"), "REPO without owner"),
                               (dict(env, WORK=str(root / "nope")), "WORK missing")):
                rc, _ = run_main(["--output", str(root / "z.json"), "--allow-missing"], bad)
                check(rc == 1, f"rejects {label}")
            rc, _ = run_main(["--output", str(root / "z.json"), "--allow-missing"],
                             dict(env, TAG="v4.5.3-rc.1"))
            check(rc == 0, "accepts prerelease TAG v<VERSION>-<suffix>")

            # 8. Duplicate and empty assets.
            dup = work / "extra" / "FinceptTerminal-4.5.3-linux-x64.deb"
            dup.parent.mkdir()
            dup.write_bytes(b"dup")
            rc, log = run_main(["--output", str(root / "d.json"), "--allow-missing"], env)
            check(rc == 1 and "more than once" in log, "duplicate asset name rejected")
            dup.unlink()
            (work / "linux/FinceptTerminal-4.5.3-linux-x64.deb").write_bytes(b"")
            rc, log = run_main(["--output", str(root / "e.json"), "--allow-missing"], env)
            check(rc == 1 and "is empty" in log, "empty asset rejected")

    try:
        cases()
    except Exception as e:  # a crash is a failed check, not a traceback
        check(False, f"unexpected {type(e).__name__}: {e}")

    failed = [label for ok, label in results if not ok]
    for label in failed:
        print(f"FAIL: {label}")
    print(f"self-test: {len(results) - len(failed)}/{len(results)} checks passed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
