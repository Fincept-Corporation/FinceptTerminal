"""
fincept_paths.py - where the bundled scripts keep files.

Every file a script writes belongs to the terminal edition that started it, under
that edition's data root. The terminal sets FINCEPT_DATA_DIR (AppPaths::root())
for every script it starts (PythonRunner::build_python_env), so a script never
needs to guess.

Never write to:
  * the scripts folder: read-only in installed builds (Program Files, /opt, an
    AppImage, a Flatpak, the macOS bundle);
  * a fixed folder in the home directory (~/.fincept, ~/.oecd_cache,
    ~/.cache/fincept-terminal, ...): shared by every edition and every build of
    Fincept Terminal on the machine, removed by the other edition's uninstaller,
    and thrown away at exit inside the Flatpak sandbox (only ~/.local/share
    persists there);
  * a fixed name in the temp folder (tempfile.gettempdir() / "fincept_x"): shared
    by every edition and every OS user, and swept by other uninstallers.

Layout under the data root:
  script-data/<name>   what a user creates through a script (saved models,
                       pipelines, schedules, experiment runs). Kept until the user
                       deletes it or uninstalls with data removal.
  cache/scripts/<name> downloads and caches that can be fetched again.
  logs/                log files (the default profile's log folder).

Data that releases kept in ~/.fincept/<name> is COPIED (never moved) into
script-data/<name> the first time a script asks for it: that folder may hold
files of another build of Fincept Terminal, so the original stays where it is.

Run by hand (no FINCEPT_DATA_DIR), the paths resolve to the edition's default
data root, the same folder the terminal uses.
"""

import os
import shutil
import sys
import tempfile
import uuid
from pathlib import Path

# The edition's data root folder name. Must equal FINCEPT_EDITION_DATA_DIR_NAME
# (fincept-qt/CMakeLists.txt "Edition identity"): the configure step fails when
# this line does not carry it. Never "com.fincept.terminal" - the folder a
# separate private build of Fincept Terminal uses.
EDITION_DATA_DIR_NAME = "com.fincept.terminal.community"

DATA_DIR_ENV = "FINCEPT_DATA_DIR"

# Where releases before the edition split kept script data. Copied from, never
# deleted. Written only as a last resort: when copying one of its folders into
# the data root fails (disk full, permissions), that call works in the legacy
# folder as releases did, so the user's data stays reachable, and the next call
# tries the copy again.
_LEGACY_HOME_DIR_NAME = ".fincept"


def _platform_data_base() -> Path:
    """The platform folder that holds the data root, as AppPaths::platform_data_base()."""
    if os.name == "nt":
        local = os.environ.get("LOCALAPPDATA", "").strip()
        return Path(local) if local else Path.home() / "AppData" / "Local"
    if sys.platform == "darwin":
        return Path.home() / "Library" / "Application Support"
    # AppPaths uses ~/.local/share (not $XDG_DATA_HOME): the Flatpak persists
    # exactly that folder.
    return Path.home() / ".local" / "share"


def data_root() -> Path:
    """The edition's data root: FINCEPT_DATA_DIR, else the platform default."""
    raw = os.environ.get(DATA_DIR_ENV, "").strip()
    if raw:
        return Path(raw).expanduser()
    return _platform_data_base() / EDITION_DATA_DIR_NAME


def _ensure_dir(path: Path) -> Path:
    path.mkdir(parents=True, exist_ok=True)
    return path


def legacy_home_dir() -> Path:
    """~/.fincept - where releases kept script data. A copy source only."""
    return Path.home() / _LEGACY_HOME_DIR_NAME


def _warn(message: str) -> None:
    # stdout carries the script's JSON result; diagnostics go to stderr.
    try:
        sys.stderr.write("[fincept_paths] " + message + "\n")
    except Exception:
        pass


def _copy_legacy(source: Path, target: Path) -> bool:
    """Copy `source` (file or folder) to `target`, which does not exist yet.

    Copied into a uniquely named sibling first and renamed into place, so a
    concurrent script never sees half a copy and an interrupted copy leaves no
    `target` behind (the next call tries again). The source is never modified.
    True when `target` exists afterwards.
    """
    staging = target.with_name(target.name + ".copying-" + uuid.uuid4().hex[:8])
    try:
        target.parent.mkdir(parents=True, exist_ok=True)
        if source.is_dir():
            shutil.copytree(source, staging, symlinks=True)
        else:
            shutil.copy2(source, staging)
        try:
            os.rename(staging, target)
        except OSError:
            # Another script finished the same copy first.
            pass
    except Exception as exc:  # disk full, permissions: the original is untouched
        _warn("could not copy %s from the legacy folder: %s" % (source.name, exc))
    finally:
        if staging.exists():
            if staging.is_dir():
                shutil.rmtree(staging, ignore_errors=True)
            else:
                try:
                    staging.unlink()
                except OSError:
                    pass
    return target.exists()


def _resolve(target: Path, legacy) -> Path:
    """`target`, after copying its legacy counterpart in when there is one.

    When that copy fails the legacy path itself is returned for this call (the
    data stays reachable, as it was in releases) and the next call tries again.
    """
    if not legacy or target.exists():
        return target
    source = legacy_home_dir() / legacy
    # Never follow a link out of the legacy folder.
    if not source.exists() or source.is_symlink():
        return target
    return target if _copy_legacy(source, target) else source


def user_data_dir(*parts: str, legacy=None) -> Path:
    """Folder for what a user creates through a script; created on demand.

    `legacy`: the path under ~/.fincept that held it in releases (for example
    "rl_models"); copied once when this folder does not exist yet.
    """
    return _ensure_dir(_resolve(data_root().joinpath("script-data", *parts), legacy))


def user_data_file(*parts: str, legacy=None) -> Path:
    """A file a user creates through a script; its folder is created on demand.

    `legacy`: the path under ~/.fincept that held it in releases (for example
    "rolling_schedules.json"); copied once when this file does not exist yet.
    """
    path = _resolve(data_root().joinpath("script-data", *parts), legacy)
    _ensure_dir(path.parent)
    return path


def cache_dir(*parts: str) -> Path:
    """Folder for downloads and caches that can be fetched again; created on demand."""
    return _ensure_dir(data_root().joinpath("cache", "scripts", *parts))


def cache_file(*parts: str) -> Path:
    """A cache file under cache_dir(); its folder is created on demand."""
    target = data_root().joinpath("cache", "scripts", *parts)
    _ensure_dir(target.parent)
    return target


def log_dir() -> Path:
    """The edition's log folder (the default profile's), created on demand."""
    return _ensure_dir(data_root() / "logs")


def atomic_write_text(path, text: str, encoding: str = "utf-8") -> None:
    """Replace `path` with `text` in one step: a reader sees the old or the new
    content, never half a file, and a crash mid-write leaves the old file."""
    target = Path(path)
    target.parent.mkdir(parents=True, exist_ok=True)
    fd, tmp = tempfile.mkstemp(prefix="." + target.name + ".", suffix=".tmp", dir=str(target.parent))
    try:
        with os.fdopen(fd, "w", encoding=encoding, newline="") as fh:
            fh.write(text)
            fh.flush()
            os.fsync(fh.fileno())
        os.replace(tmp, target)
    except BaseException:
        try:
            os.unlink(tmp)
        except OSError:
            pass
        raise
