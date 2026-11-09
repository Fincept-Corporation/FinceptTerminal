// installscript.qs -- Fincept Terminal QtIFW component script
//
// Handles:
//   - Platform shortcuts on install (Start Menu, Desktop, .desktop entry)
//   - Full user-data cleanup on uninstall (with user confirmation)
//
// Data locations cleaned on uninstall (every profile lives inside the data root):
//   All     : the data root, plus $FINCEPT_DATA_DIR when it holds Fincept data
//             ~/.fincept  ~/.oecd_cache  ~/.cache/fincept-terminal   (bundled Python scripts)
//             ~/.claude/projects/*com-fincept-terminal-agent-workspace*  (coding-agent runs)
//   Windows : %LOCALAPPDATA%\com.fincept.terminal\
//             %LOCALAPPDATA%\Fincept\FinceptTerminal, %LOCALAPPDATA%\FinceptTerminal (legacy, Qt cache)
//             %APPDATA%\Fincept\FinceptTerminal (legacy), %APPDATA%\fincept-terminal (agents.db)
//             %LOCALAPPDATA%\fincept-dev
//             HKCU\Software\Fincept\FinceptTerminal (QSettings; the Fincept parent only when empty)
//             Credential Manager: FinceptTerminal/* (versions <= 4.0.2)
//             %TEMP%\fincept_* (files and folders), %TEMP%\fincept-boot.log
//   macOS   : ~/Library/Application Support/{com.fincept.terminal, Fincept/FinceptTerminal, FinceptTerminal, fincept-dev}
//             ~/Library/Preferences/com.fincept.FinceptTerminal.plist (+ bundle-id domain)
//             ~/Library/Caches, Saved Application State, HTTPStorages entries
//             Keychain: service com.fincept.terminal (versions <= 4.0.2)
//             $TMPDIR and /tmp: fincept_*, fincept-boot.log, fincept-<hash> instance socket
//   Linux   : ~/.local/share/{com.fincept.terminal, Fincept/FinceptTerminal, FinceptTerminal, fincept-dev}
//             ~/.config/Fincept/FinceptTerminal(-Secure).conf, ~/.cache/Fincept/FinceptTerminal
//             libsecret: schema com.fincept.terminal (versions <= 4.0.2)
//             /tmp and $TMPDIR: fincept_*, fincept-boot.log, fincept-<hash> instance socket
//             ~/.local/share/applications/fincept-terminal.desktop
//
// The standalone uninstallers in packaging/uninstall/ (uninstall.ps1 / .cmd /
// .sh) remove the same set for every install type (DMG, .deb/.rpm, AppImage,
// Flatpak) — keep the lists in step.
//
// Debug: run the maintenance tool with `-v` (or `--verbose`) to see console.log output.

// ---------------------------------------------------------------------------
// Component constructor: wire up lifecycle hooks
// ---------------------------------------------------------------------------

function Component()
{
    try {
        console.log("[Fincept] Component() constructor — isInstaller=" +
                    installer.isInstaller() +
                    " isUninstaller=" + installer.isUninstaller() +
                    " isUpdater=" + installer.isUpdater() +
                    " isPackageManager=" + installer.isPackageManager());

        // Pre-register the auto-answer for the data-cleanup confirmation
        // dialog. Without this, headless invocations (`purge --default-answer`)
        // deadlock on QMessageBox.question() in onUninstallationStarted —
        // --default-answer only handles IFW's *own* dialogs, not script-
        // spawned ones. This is what was causing the bug reported in #240
        // where `purge` exited 1 with no cleanup.
        // ID must match the first arg of QMessageBox.question() below.
        if (typeof QMessageBox !== "undefined" && installer.setMessageBoxAutomaticAnswer) {
            installer.setMessageBoxAutomaticAnswer(
                "fincept.uninstall.data", QMessageBox.No);
        }

        // Connect signals using the 1-arg form. The 2-arg form (thisObj, fn) is
        // not reliably supported by the QJSEngine that backs QtIFW component
        // scripts — connections silently no-op on some versions.
        if (installer.isInstaller()) {
            installer.installationFinished.connect(onInstallationFinished);
        }

        if (installer.isUninstaller()) {
            installer.uninstallationStarted.connect(onUninstallationStarted);
            installer.uninstallationFinished.connect(onUninstallationFinished);
        }
    } catch (e) {
        // Never throw out of Component() — IFW treats that as a fatal load
        // error and aborts before any UI shows (the "GUI flashes and closes"
        // symptom in #240). Log and continue with defaults.
        console.log("[Fincept] Component() constructor error: " + e);
    }
}

// ---------------------------------------------------------------------------
// Install: create platform shortcuts
// ---------------------------------------------------------------------------

Component.prototype.createOperations = function()
{
    // Always call base first so file extraction happens.
    component.createOperations();

    var targetDir = installer.value("TargetDir");

    if (systemInfo.kernelType === "winnt") {
        // Start Menu shortcut
        component.addOperation("CreateShortcut",
            targetDir + "/FinceptTerminal.exe",
            "@StartMenuDir@/Fincept Terminal.lnk",
            "workingDirectory=" + targetDir,
            "iconPath=" + targetDir + "/FinceptTerminal.exe",
            "iconId=0",
            "description=Professional Financial Intelligence Terminal");

        // Desktop shortcut
        component.addOperation("CreateShortcut",
            targetDir + "/FinceptTerminal.exe",
            "@DesktopDir@/Fincept Terminal.lnk",
            "workingDirectory=" + targetDir,
            "iconPath=" + targetDir + "/FinceptTerminal.exe",
            "iconId=0",
            "description=Professional Financial Intelligence Terminal");
    }

    if (systemInfo.kernelType === "linux") {
        component.addOperation("CreateDesktopEntry",
            "@HomeDir@/.local/share/applications/fincept-terminal.desktop",
            "Version=1.0\n" +
            "Type=Application\n" +
            "Name=Fincept Terminal\n" +
            "GenericName=Financial Intelligence Terminal\n" +
            "Comment=Professional financial data terminal with AI analytics\n" +
            "Exec=" + targetDir + "/bin/FinceptTerminal %U\n" +
            "Icon=" + targetDir + "/share/icons/hicolor/256x256/apps/fincept-terminal.png\n" +
            "Terminal=false\n" +
            "StartupWMClass=FinceptTerminal\n" +
            "StartupNotify=true\n" +
            "Categories=Finance;Office;Science;\n" +
            "Keywords=finance;trading;stocks;crypto;portfolio;AI;analytics;markets;\n"
        );
    }
    // macOS: .app bundle is self-contained, no shortcuts needed
};

function onInstallationFinished()
{
    console.log("[Fincept] Installation finished.");
}

// ---------------------------------------------------------------------------
// Uninstall: ask the user, then clean everything
// ---------------------------------------------------------------------------

function onUninstallationStarted()
{
    try {
        console.log("[Fincept] uninstallationStarted.");

        // Per-machine install at C:\Program Files\... requires elevation to
        // delete files. Without this, the cmd.exe / reg.exe calls below
        // silently fail and IFW's own RemoveTargetDir step hits ACCESS_DENIED.
        // gainAdminRights() is a no-op if we're already elevated, and on
        // non-Windows it just returns. Failure here is non-fatal — log and
        // continue; some Windows configurations let `cmd /c rmdir` work
        // unelevated for user-owned trees.
        if (systemInfo.kernelType === "winnt") {
            try {
                installer.gainAdminRights();
            } catch (e) {
                console.log("[Fincept] gainAdminRights failed (continuing): " + e);
            }
        }

        // Headless invocations (`purge --default-answer`, `--accept-messages`)
        // skip script-spawned QMessageBox confirmation entirely. The auto-
        // answer registered in Component() handles IFW's accounting; we just
        // need to not block here.
        var headless =
            (typeof installer.isCommandLineInstance === "function" &&
                installer.isCommandLineInstance()) ||
            (typeof gui === "undefined" || gui === null);

        var clean = false;
        if (headless) {
            console.log("[Fincept] Headless uninstall — skipping data-cleanup prompt.");
        } else {
            var answer = QMessageBox.question(
                "fincept.uninstall.data",
                "Remove Fincept Terminal User Data?",
                "Do you want to remove all Fincept Terminal user data?\n\n" +
                "This includes:\n" +
                "  - Databases (chat history, portfolio, watchlists)\n" +
                "  - Log files and crash dumps\n" +
                "  - Downloaded files and cached data\n" +
                "  - Python runtime and virtual environments\n" +
                "  - Workspaces, layouts and every profile\n" +
                "  - Saved credentials and API keys\n" +
                "  - Application settings\n" +
                "  - AI Quant Lab models and caches in your home folder\n\n" +
                "Choose 'No' to keep your data for a future reinstall.",
                QMessageBox.Yes | QMessageBox.No,
                QMessageBox.No
            );
            clean = (answer === QMessageBox.Yes);
        }

        if (clean) {
            console.log("[Fincept] Cleaning user data.");
            try {
                cleanUserData();
            } catch (e) {
                console.log("[Fincept] cleanUserData threw: " + e);
            }
        } else {
            console.log("[Fincept] Keeping user data.");
        }
    } catch (e) {
        // Never propagate — IFW treats a thrown signal handler as a fatal
        // uninstall error and exits 1 with no cleanup, which is exactly the
        // symptom from #240.
        console.log("[Fincept] onUninstallationStarted error: " + e);
    }
}

function onUninstallationFinished()
{
    console.log("[Fincept] Uninstallation finished.");
}

// ---------------------------------------------------------------------------
// Data cleanup implementation — dispatches to per-platform routines
// ---------------------------------------------------------------------------

function cleanUserData()
{
    if (systemInfo.kernelType === "winnt") {
        cleanUserDataWindows();
    } else if (systemInfo.kernelType === "darwin") {
        cleanUserDataMac();
    } else {
        cleanUserDataLinux();
    }
}

// A data root moved with FINCEPT_DATA_DIR — removed only when it really holds
// Fincept Terminal data, never on the variable's word alone.
function customDataRoot()
{
    var dir = installer.environmentVariable("FINCEPT_DATA_DIR");
    if (!dir)
        return "";
    var markers = ["/profiles.json", "/data/fincept.db", "/workspace.db", "/venv-numpy2", "/.setup_complete"];
    for (var i = 0; i < markers.length; ++i) {
        if (installer.fileExists(dir + markers[i]))
            return dir;
    }
    console.log("[Fincept] FINCEPT_DATA_DIR does not look like Fincept data, kept: " + dir);
    return "";
}

// ---------- Windows ----------

function cleanUserDataWindows()
{
    var localAppData = installer.environmentVariable("LOCALAPPDATA");
    var appData      = installer.environmentVariable("APPDATA");
    var tempDir      = installer.environmentVariable("TEMP");

    var userProfile  = installer.environmentVariable("USERPROFILE");

    // 1. Main data root (all profiles) and a FINCEPT_DATA_DIR override
    removeDirWindows(localAppData + "/com.fincept.terminal");
    removeDirWindows(customDataRoot());

    // 2. Legacy data roots (also Qt's shader cache under Fincept\FinceptTerminal)
    removeDirWindows(localAppData + "/Fincept/FinceptTerminal");
    removeDirWindows(localAppData + "/FinceptTerminal");
    // Remove the Fincept/ parent if it's now empty
    removeDirIfEmptyWindows(localAppData + "/Fincept");
    removeDirWindows(localAppData + "/fincept-dev");

    // 3. Roaming: legacy data root, and the agent repository (agents.db)
    removeDirWindows(appData + "/Fincept/FinceptTerminal");
    removeDirIfEmptyWindows(appData + "/Fincept");
    removeDirWindows(appData + "/fincept-terminal");

    // 4. Registry — QSettings default format on Windows
    runAndLog("reg.exe", ["delete", "HKCU\\Software\\Fincept\\FinceptTerminal", "/f"]);
    // The vendor key only when nothing is left in it: `reg delete /f` is
    // recursive and would take any other Fincept product's settings with it.
    // `reg query` prints no HKEY_ line for an empty key.
    runAndLog("cmd.exe", ["/c",
        "for /f %c in ('reg query \"HKCU\\Software\\Fincept\" 2^>nul ^| find /c \"HKEY_\"') do " +
        "(if %c==0 reg delete \"HKCU\\Software\\Fincept\" /f >nul 2>&1) & exit /b 0"]);

    // 4b. Folders the bundled Python scripts and coding agents create in the profile
    if (userProfile) {
        removeDirWindows(userProfile + "/.fincept");
        removeDirWindows(userProfile + "/.oecd_cache");
        removeDirWindows(userProfile + "/.cache/fincept-terminal");
        runAndLog("cmd.exe", ["/c",
            "for /d %d in (\"" + toWin(userProfile) + "\\.claude\\projects\\*com-fincept-terminal-agent-workspace*\") do " +
            "rmdir /s /q \"%d\" 2>nul & exit /b 0"]);
    }

    // 5. Windows Credential Manager entries: FinceptTerminal/*
    //    cmdkey has no wildcard delete. Enumerate via a cmd.exe FOR loop —
    //    we used PowerShell originally but #240 confirmed the maintenance
    //    tool fails on locked-down Win11 boxes where AppLocker/Defender
    //    blocks installer-spawned powershell.exe. cmd.exe has no such
    //    restrictions. The FOR /F parses `cmdkey /list` lines that match
    //    "Target: FinceptTerminal/..." and deletes each.
    runAndLog("cmd.exe", ["/c",
        "for /f \"tokens=1,* delims=:\" %a in ('cmdkey /list 2^>nul ^| findstr /i \"FinceptTerminal/\"') do " +
        "(for /f \"tokens=*\" %c in (\"%b\") do cmdkey /delete:\"%c\" >nul 2>&1) & exit /b 0"
    ]);

    // 6. Temp files — single shell string so wildcards expand inside cmd.
    //    Covers: fincept_cell_*, fincept_arg_*, fincept_report_autosave.*,
    //            fincept_paste_*, fincept_chart_*, fincept_spark_*, cancel flags,
    //            replay files, UpdateService temp, and folders such as
    //            fincept_databento_cache (`del` alone skips folders).
    runAndLog("cmd.exe", ["/c",
        "del /q /f \"" + toWin(tempDir) + "\\fincept_*\" 2>nul & " +
        "del /q /f \"" + toWin(tempDir) + "\\fincept-boot.log\" 2>nul & " +
        "(for /d %d in (\"" + toWin(tempDir) + "\\fincept_*\") do rmdir /s /q \"%d\" 2>nul) & " +
        "exit /b 0"]);

    // Screenshots the user saved (fincept_YYYYMMDD_HHMMSS.png in the profile)
    // are the user's own files and are deliberately kept.
}

function removeDirWindows(pathFwd)
{
    if (!pathFwd) return;
    if (!installer.fileExists(pathFwd)) {
        console.log("[Fincept] skip (not present): " + pathFwd);
        return;
    }
    var win = toWin(pathFwd);
    // Quote the path inside a single shell string so spaces in the path
    // survive argv splitting. `exit /b 0` ensures we never surface an error.
    runAndLog("cmd.exe", ["/c",
        "rmdir /s /q \"" + win + "\" 2>nul & exit /b 0"]);
}

function removeDirIfEmptyWindows(pathFwd)
{
    if (!pathFwd || !installer.fileExists(pathFwd)) return;
    var win = toWin(pathFwd);
    // `rmdir` without /s fails if the directory isn't empty — which is what we want.
    runAndLog("cmd.exe", ["/c",
        "rmdir \"" + win + "\" 2>nul & exit /b 0"]);
}

function toWin(pathFwd)
{
    return pathFwd.replace(/\//g, "\\");
}

// ---------- macOS ----------

function cleanUserDataMac()
{
    var home = installer.environmentVariable("HOME");

    // 1. Main data root (all profiles), a FINCEPT_DATA_DIR override, legacy roots
    removeDirPosix(home + "/Library/Application Support/com.fincept.terminal");
    removeDirPosix(customDataRoot());
    removeDirPosix(home + "/Library/Application Support/Fincept/FinceptTerminal");
    removeDirIfEmptyPosix(home + "/Library/Application Support/Fincept");
    removeDirPosix(home + "/Library/Application Support/FinceptTerminal");
    removeDirPosix(home + "/Library/Application Support/fincept-dev");

    // 2. Preferences / plist. cfprefsd caches preferences: drop the domains
    //    first so the deleted files are not written back.
    runAndLog("/bin/bash", ["-c",
        "/usr/bin/defaults delete com.fincept.FinceptTerminal >/dev/null 2>&1; " +
        "/usr/bin/defaults delete com.fincept.terminal >/dev/null 2>&1; exit 0"]);
    removeFilePosix(home + "/Library/Preferences/com.fincept.FinceptTerminal.plist");
    removeFilePosix(home + "/Library/Preferences/com.fincept.terminal.plist");
    removeFilePosix(home + "/Library/Preferences/Fincept.FinceptTerminal.plist");
    removeFilePosix(home + "/Library/Preferences/Fincept.FinceptTerminal-Secure.plist");

    // 3. Caches (Qt shader cache lands in Caches/Fincept/FinceptTerminal)
    removeDirPosix(home + "/Library/Caches/com.fincept.terminal");
    removeDirPosix(home + "/Library/Caches/Fincept/FinceptTerminal");
    removeDirIfEmptyPosix(home + "/Library/Caches/Fincept");

    // 4. Saved application state, URL-session storage
    removeDirPosix(home + "/Library/Saved Application State/com.fincept.terminal.savedState");
    removeDirPosix(home + "/Library/HTTPStorages/com.fincept.terminal");

    // 4b. Folders the bundled Python scripts and coding agents create in $HOME
    removeHomeExtrasPosix(home);

    // 5. Keychain: delete all entries under service "com.fincept.terminal".
    //    security(1) removes one entry per call — loop until it fails (no more).
    runAndLog("/bin/bash", ["-c",
        "while /usr/bin/security delete-generic-password -s 'com.fincept.terminal' >/dev/null 2>&1; do :; done; exit 0"
    ]);

    // 6. Temp files and the single-instance socket
    removeTempPosix();

    // Screenshots the user saved (fincept_YYYYMMDD_HHMMSS.png in $HOME) are
    // the user's own files and are deliberately kept.
}

// ---------- Linux ----------

function cleanUserDataLinux()
{
    var home   = installer.environmentVariable("HOME");
    var xdgCfg = installer.environmentVariable("XDG_CONFIG_HOME");
    var xdgDat = installer.environmentVariable("XDG_DATA_HOME");
    var xdgCch = installer.environmentVariable("XDG_CACHE_HOME");

    if (!xdgCfg) xdgCfg = home + "/.config";
    if (!xdgDat) xdgDat = home + "/.local/share";
    if (!xdgCch) xdgCch = home + "/.cache";

    // 1. Main data root (the app hard-codes ~/.local/share; XDG too, to be safe),
    //    a FINCEPT_DATA_DIR override, and legacy roots
    removeDirPosix(xdgDat + "/com.fincept.terminal");
    removeDirPosix(home   + "/.local/share/com.fincept.terminal");
    removeDirPosix(customDataRoot());
    removeDirPosix(xdgDat + "/Fincept/FinceptTerminal");
    removeDirIfEmptyPosix(xdgDat + "/Fincept");
    removeDirPosix(xdgDat + "/FinceptTerminal");
    removeDirPosix(xdgDat + "/fincept-dev");

    // 2. QSettings .conf files (FinceptTerminal-Secure.conf: the <= 4.0.2
    //    secure-storage fallback when libsecret was missing)
    removeFilePosix(xdgCfg + "/Fincept/FinceptTerminal.conf");
    removeFilePosix(xdgCfg + "/Fincept/FinceptTerminal-Secure.conf");
    removeDirIfEmptyPosix(xdgCfg + "/Fincept");

    // 3. Cache dirs (Qt shader cache lands in Fincept/FinceptTerminal)
    removeDirPosix(xdgCch + "/com.fincept.terminal");
    removeDirPosix(xdgCch + "/Fincept/FinceptTerminal");
    removeDirIfEmptyPosix(xdgCch + "/Fincept");

    // 3b. libsecret items written by versions <= 4.0.2
    runAndLog("/bin/bash", ["-c",
        "command -v secret-tool >/dev/null 2>&1 && " +
        "secret-tool clear xdg:schema com.fincept.terminal >/dev/null 2>&1; exit 0"]);

    // 3c. Folders the bundled Python scripts and coding agents create in $HOME
    removeHomeExtrasPosix(home);

    // 4. Desktop entry (installed via CreateDesktopEntry at install time — IFW's
    //    own UNDO step removes it, but clean up any stale copies just in case).
    removeFilePosix(home + "/.local/share/applications/fincept-terminal.desktop");

    // 5. Temp files and the single-instance socket
    removeTempPosix();

    // Screenshots the user saved (fincept_YYYYMMDD_HHMMSS.png in $HOME) are
    // the user's own files and are deliberately kept.
}

// ---------- Shared by mac + linux ----------

function removeHomeExtrasPosix(home)
{
    if (!home) return;
    removeDirPosix(home + "/.fincept");                  // AI Quant Lab: mlruns, models
    removeDirPosix(home + "/.oecd_cache");               // OECD connector cache
    removeDirPosix(home + "/.cache/fincept-terminal");   // exchange market cache
    removeFilePosix(home + "/fincept-terminal/agents.db");
    removeDirIfEmptyPosix(home + "/fincept-terminal");
    runAndLog("/bin/bash", ["-c",
        "rm -rf \"" + shellEscape(home) + "\"/.claude/projects/*com-fincept-terminal-agent-workspace* 2>/dev/null; exit 0"]);
}

function removeTempPosix()
{
    // fincept_* files and folders (databento cache), the boot log, and the
    // QLocalServer socket fincept-<16 hex> left behind after a crash.
    runAndLog("/bin/bash", ["-c",
        "for t in /tmp \"${TMPDIR:-/tmp}\"; do " +
        "  rm -rf \"$t\"/fincept_* \"$t\"/fincept-boot.log 2>/dev/null; " +
        "  for s in \"$t\"/fincept-*; do " +
        "    case \"${s##*/}\" in fincept-[0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]" +
        "[0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]) rm -f \"$s\" ;; esac; " +
        "  done; " +
        "done; exit 0"]);
}

// ---------- Unix helpers (mac + linux) ----------

function removeDirPosix(path)
{
    if (!path) return;
    if (!installer.fileExists(path)) {
        console.log("[Fincept] skip (not present): " + path);
        return;
    }
    // Use /bin/rm with -rf so missing paths never error. Shell-wrap so the
    // path string survives any unusual characters (spaces in $HOME, etc).
    runAndLog("/bin/bash", ["-c", "rm -rf \"" + shellEscape(path) + "\"; exit 0"]);
}

function removeFilePosix(path)
{
    if (!path) return;
    if (!installer.fileExists(path)) {
        console.log("[Fincept] skip (not present): " + path);
        return;
    }
    runAndLog("/bin/bash", ["-c", "rm -f \"" + shellEscape(path) + "\"; exit 0"]);
}

function removeDirIfEmptyPosix(path)
{
    if (!path || !installer.fileExists(path)) return;
    // rmdir fails if non-empty; ignore.
    runAndLog("/bin/bash", ["-c", "rmdir \"" + shellEscape(path) + "\" 2>/dev/null; exit 0"]);
}

function shellEscape(s)
{
    // Escape backslashes and double-quotes for embedding in a double-quoted shell string.
    return s.replace(/\\/g, "\\\\").replace(/"/g, "\\\"");
}

// ---------- Generic runner with logging ----------

function runAndLog(program, args)
{
    var result = installer.execute(program, args);
    // installer.execute returns [stdout, exitCode] on success,
    // or [] (empty) if the program failed to launch.
    if (!result || result.length === 0) {
        console.log("[Fincept] FAILED to launch: " + program + " " + args.join(" "));
        return;
    }
    var exitCode = result.length >= 2 ? result[1] : "?";
    console.log("[Fincept] ran " + program + " (exit=" + exitCode + ")");
}
