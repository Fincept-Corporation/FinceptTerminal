// installscript.qs -- Fincept Terminal QtIFW component script
//
// Handles:
//   - Platform shortcuts on install (Start Menu, Desktop, .desktop entry)
//   - Full user-data cleanup on uninstall (with user confirmation)
//
// Data locations cleaned on uninstall:
//   Windows : %LOCALAPPDATA%\com.fincept.terminal\
//             %LOCALAPPDATA%\Fincept\*                  (legacy)
//             %LOCALAPPDATA%\FinceptTerminal\*          (legacy)
//             %APPDATA%\Fincept\*                       (QSettings roaming)
//             HKCU\Software\Fincept                     (registry)
//             Windows Credential Manager: FinceptTerminal/*
//             %TEMP%\fincept_*
//   macOS   : ~/Library/Application Support/com.fincept.terminal/
//             ~/Library/Preferences/com.fincept.FinceptTerminal.plist
//             ~/Library/Preferences/Fincept.plist (if present)
//             Keychain: com.fincept.terminal service entries
//             $TMPDIR/fincept_*, /tmp/fincept_*
//   Linux   : ~/.local/share/com.fincept.terminal/
//             ~/.config/Fincept/
//             /tmp/fincept_*
//             ~/.local/share/applications/fincept-terminal.desktop
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
                "  - Log files\n" +
                "  - Downloaded files and cached data\n" +
                "  - ML models (Whisper, etc.)\n" +
                "  - Python runtime and virtual environments\n" +
                "  - Workspaces and profiles\n" +
                "  - Saved credentials and API keys\n" +
                "  - Application settings\n\n" +
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

// ---------- Windows ----------

function cleanUserDataWindows()
{
    var localAppData = installer.environmentVariable("LOCALAPPDATA");
    var appData      = installer.environmentVariable("APPDATA");
    var tempDir      = installer.environmentVariable("TEMP");

    // 1. Main data root
    removeDirWindows(localAppData + "/com.fincept.terminal");

    // 2. Legacy data roots
    removeDirWindows(localAppData + "/Fincept/FinceptTerminal");
    removeDirWindows(localAppData + "/FinceptTerminal");
    // Remove the Fincept/ parent if it's now empty
    removeDirIfEmptyWindows(localAppData + "/Fincept");

    // 3. Roaming QSettings (INI fallback, rare but possible)
    removeDirWindows(appData + "/Fincept/FinceptTerminal");
    removeDirIfEmptyWindows(appData + "/Fincept");

    // 4. Registry — QSettings default format on Windows
    runAndLog("reg.exe", ["delete", "HKCU\\Software\\Fincept\\FinceptTerminal", "/f"]);
    runAndLog("reg.exe", ["delete", "HKCU\\Software\\Fincept\\FinceptTerminal-Secure", "/f"]);
    // Remove parent key last — only succeeds if no other Fincept apps remain.
    runAndLog("reg.exe", ["delete", "HKCU\\Software\\Fincept", "/f"]);

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
    //            fincept_paste_*, fincept_chart_*, fincept_spark_*, UpdateService temp.
    runAndLog("cmd.exe", ["/c",
        "del /q /f \"" + toWin(tempDir) + "\\fincept_*\" 2>nul & " +
        "del /q /f \"" + toWin(tempDir) + "\\fincept-boot.log\" 2>nul & " +
        "exit /b 0"]);

    // 7. Screenshots saved under %USERPROFILE% by MainWindow "save screenshot"
    //    with the strict pattern fincept_YYYYMMDD_HHMMSS.png. cmd.exe's `del`
    //    wildcards aren't strict enough on their own ("fincept_*.png" would
    //    nuke any user file matching), so we narrow with a FOR loop that
    //    checks the digit-shape via findstr.
    var userProfile = installer.environmentVariable("USERPROFILE");
    if (userProfile) {
        var up = toWin(userProfile);
        runAndLog("cmd.exe", ["/c",
            "for /f \"delims=\" %f in ('dir /b /a-d \"" + up + "\\fincept_*.png\" 2^>nul ^| " +
            "findstr /r \"^fincept_[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]_[0-9][0-9][0-9][0-9][0-9][0-9]\\.png$\"') do " +
            "del /q /f \"" + up + "\\%f\" 2>nul & exit /b 0"
        ]);
    }
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

    // 1. Main data root
    removeDirPosix(home + "/Library/Application Support/com.fincept.terminal");

    // 2. Preferences / plist
    removeFilePosix(home + "/Library/Preferences/com.fincept.FinceptTerminal.plist");
    removeFilePosix(home + "/Library/Preferences/Fincept.FinceptTerminal.plist");
    removeFilePosix(home + "/Library/Preferences/Fincept.FinceptTerminal-Secure.plist");

    // 3. Caches (Qt/QSettings/logs occasionally land here)
    removeDirPosix(home + "/Library/Caches/com.fincept.terminal");
    removeDirPosix(home + "/Library/Caches/Fincept");

    // 4. Saved application state
    removeDirPosix(home + "/Library/Saved Application State/com.fincept.terminal.savedState");

    // 5. Keychain: delete all entries under service "com.fincept.terminal".
    //    security(1) removes one entry per call — loop until it fails (no more).
    runAndLog("/bin/bash", ["-c",
        "while /usr/bin/security delete-generic-password -s 'com.fincept.terminal' >/dev/null 2>&1; do :; done; exit 0"
    ]);

    // 6. Temp files
    runAndLog("/bin/bash", ["-c",
        "rm -f /tmp/fincept_* /tmp/fincept-boot.log 2>/dev/null; " +
        "rm -f \"${TMPDIR:-/tmp}\"/fincept_* \"${TMPDIR:-/tmp}\"/fincept-boot.log 2>/dev/null; " +
        "exit 0"
    ]);

    // 7. Timestamped screenshots saved to $HOME by MainWindow save-screenshot
    //    Pattern: fincept_YYYYMMDD_HHMMSS.png — strict match to avoid collateral.
    runAndLog("/bin/bash", ["-c",
        "find \"" + shellEscape(home) + "\" -maxdepth 1 -type f " +
        "-regex '.*/fincept_[0-9]\\{8\\}_[0-9]\\{6\\}\\.png$' " +
        "-delete 2>/dev/null; exit 0"
    ]);
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

    // 1. Main data root (respect XDG)
    removeDirPosix(xdgDat + "/com.fincept.terminal");
    removeDirPosix(home   + "/.local/share/com.fincept.terminal");

    // 2. QSettings .conf files
    removeFilePosix(xdgCfg + "/Fincept/FinceptTerminal.conf");
    removeFilePosix(xdgCfg + "/Fincept/FinceptTerminal-Secure.conf");
    removeDirIfEmptyPosix(xdgCfg + "/Fincept");

    // 3. Cache dir (if the app used one)
    removeDirPosix(xdgCch + "/com.fincept.terminal");
    removeDirPosix(xdgCch + "/Fincept");

    // 4. Desktop entry (installed via CreateDesktopEntry at install time — IFW's
    //    own UNDO step removes it, but clean up any stale copies just in case).
    removeFilePosix(home + "/.local/share/applications/fincept-terminal.desktop");

    // 5. Temp files
    runAndLog("/bin/bash", ["-c",
        "rm -f /tmp/fincept_* /tmp/fincept-boot.log 2>/dev/null; " +
        "rm -f \"${TMPDIR:-/tmp}\"/fincept_* \"${TMPDIR:-/tmp}\"/fincept-boot.log 2>/dev/null; " +
        "exit 0"
    ]);

    // 6. Timestamped screenshots saved to $HOME by MainWindow save-screenshot
    runAndLog("/bin/bash", ["-c",
        "find \"" + shellEscape(home) + "\" -maxdepth 1 -type f " +
        "-regextype posix-extended " +
        "-regex '.*/fincept_[0-9]{8}_[0-9]{6}\\.png' " +
        "-delete 2>/dev/null; exit 0"
    ]);
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
