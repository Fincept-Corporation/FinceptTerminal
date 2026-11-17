// installscript.qs -- Fincept Terminal QtIFW component script
//
// Handles:
//   - Platform shortcuts on install (Start Menu, Desktop, .desktop entry)
//   - Full user-data cleanup on uninstall (with user confirmation)
//
// Data locations cleaned on uninstall (every profile lives inside the data root).
// Every name below is this edition's own (CMakeLists.txt "Edition identity",
// src/core/config/EditionIdentity.h):
//   All     : the data root (with the bundled scripts' script-data/ and cache/scripts/),
//             plus $FINCEPT_DATA_DIR when it holds Fincept data
//             ~/.claude/projects/*com-fincept-terminal-community-agent-workspace*  (coding-agent runs)
//   Windows : %LOCALAPPDATA%\com.fincept.terminal.community\
//             %LOCALAPPDATA%\FinceptCommunity\FinceptTerminalCommunity, %APPDATA%\FinceptCommunity\FinceptTerminalCommunity
//             %LOCALAPPDATA%\fincept-dev
//             HKCU\Software\FinceptCommunity\FinceptTerminalCommunity (QSettings; the parent only when empty)
//             %TEMP%\fincept-community-boot.log
//   macOS   : ~/Library/Application Support/{com.fincept.terminal.community, FinceptCommunity/FinceptTerminalCommunity, fincept-dev}
//             ~/Library/Preferences/com.finceptcommunity.FinceptTerminalCommunity.plist (+ bundle-id domain)
//             ~/Library/Caches, Saved Application State, HTTPStorages entries of com.fincept.terminal.community
//             $TMPDIR and /tmp: fincept-community-boot.log
//   Linux   : ~/.local/share/{com.fincept.terminal.community, FinceptCommunity/FinceptTerminalCommunity, fincept-dev}
//             ~/.config/FinceptCommunity/FinceptTerminalCommunity.conf, ~/.cache/FinceptCommunity/FinceptTerminalCommunity
//             /tmp and $TMPDIR: fincept-community-boot.log
//             ~/.local/share/applications/fincept-terminal-community.desktop
//
// NEVER removed: the names released versions shared with the separate private
// build of Fincept Terminal — %LOCALAPPDATA%\com.fincept.terminal (and the macOS /
// Linux equivalents), HKCU\Software\Fincept\FinceptTerminal (~/.config/Fincept,
// com.fincept.FinceptTerminal.plist), %APPDATA%\fincept-terminal\agents.db, the
// FinceptTerminal/* Credential Manager / com.fincept.terminal Keychain and
// libsecret items, %TEMP%\fincept_* and the fincept-<hash> instance sockets, the
// script folders ~/.fincept, ~/.oecd_cache and ~/.cache/fincept-terminal. They
// may hold the private build's data; the first start of this edition moved (or
// the scripts copied) its own data out of them (src/app/LegacyRootAdoption.h,
// scripts/fincept_paths.py).
//
// The standalone uninstallers in packaging/uninstall/ (uninstall.ps1 / .cmd /
// .sh) remove the same set for every install type (DMG, .deb/.rpm, AppImage,
// Flatpak) — keep the lists in step.
//
// In-app update (Windows, FinceptUpdate=1 — see controlscript.qs): this script
// moves the old install folder aside right before IFW unpacks the new files
// (beginInstallation), deletes it as the last install operation, and moves it
// back when the installation fails (onInstallationFinished). None of that runs
// for a normal install, the CLI flows or the maintenance tool.
//
// Debug: run the installer / maintenance tool with `-d` (or `--verbose`) to see console.log output.

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

// Shortcut names carry the edition: the private build's installer creates
// "Fincept Terminal.lnk" in the Start Menu and on the desktop, and either
// installer would overwrite (and its uninstaller delete) the other's link.
// @StartMenuDir@ is "Fincept Terminal Community"
// (CPACK_IFW_PACKAGE_START_MENU_DIRECTORY).
var finceptShortcutLinks = ["@StartMenuDir@/Fincept Terminal Community.lnk",
                            "@DesktopDir@/Fincept Terminal Community.lnk"];

function finceptShortcutArgs(targetDir, link)
{
    return [targetDir + "/FinceptTerminal.exe",
            link,
            "workingDirectory=" + targetDir,
            "iconPath=" + targetDir + "/FinceptTerminal.exe",
            "iconId=0",
            "description=Professional Financial Intelligence Terminal"];
}

Component.prototype.createOperations = function()
{
    // Always call base first so file extraction happens.
    component.createOperations();

    var targetDir = installer.value("TargetDir");

    if (systemInfo.kernelType === "winnt") {
        // Start Menu and desktop shortcuts
        for (var i = 0; i < finceptShortcutLinks.length; ++i) {
            var a = finceptShortcutArgs(targetDir, finceptShortcutLinks[i]);
            component.addOperation("CreateShortcut", a[0], a[1], a[2], a[3], a[4], a[5]);
        }

        // In-app update: the last operations remove the previous version
        // that beginInstallation moved aside (plus any left by an interrupted
        // earlier update) and the other Add/Remove Programs entries for this
        // folder. They run with the rights IFW gained for this folder and
        // never fail the installation. Kept last on purpose: nothing that can
        // fail runs after them except writing the maintenance tool.
        if (installer.value("FinceptBackupDir"))
            finceptAddCleanupOperations();
    }

    if (systemInfo.kernelType === "linux") {
        // Desktop id and WM class of this edition (the private build's installer
        // writes fincept-terminal.desktop with StartupWMClass=FinceptTerminal).
        component.addOperation("CreateDesktopEntry",
            "@HomeDir@/.local/share/applications/fincept-terminal-community.desktop",
            "Version=1.0\n" +
            "Type=Application\n" +
            "Name=Fincept Terminal\n" +
            "GenericName=Financial Intelligence Terminal\n" +
            "Comment=Professional financial data terminal with AI analytics\n" +
            "Exec=" + targetDir + "/bin/FinceptTerminal %U\n" +
            "Icon=" + targetDir + "/share/icons/hicolor/256x256/apps/fincept-terminal-community.png\n" +
            "Terminal=false\n" +
            "StartupWMClass=FinceptTerminalCommunity\n" +
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
    try {
        finceptRestoreAfterFailedUpdate();
    } catch (e) {
        console.log("[Fincept] update: restore error: " + e);
    }
}

// ---------------------------------------------------------------------------
// In-app update (Windows): move the old folder aside / back
// ---------------------------------------------------------------------------
// FinceptReplaceDir / FinceptBackupDir / FinceptOldUninstallKeys are set by
// controlscript.qs once the running terminal has exited. Folder and registry
// work uses cmd.exe / reg.exe when the paths hold no character cmd.exe would
// interpret (QProcess quotes arguments with spaces, so "C:\Program Files\..."
// qualifies) — they start in milliseconds. Any other path goes through
// Windows PowerShell with -EncodedCommand, which no quoting layer can mangle
// (only cmdlets, so it also works in PowerShell's constrained language mode).
// Every process started while IFW holds elevation runs elevated (IFW routes
// it through its admin server).

// IFW calls this for each component after it has created the target folder
// (asking for elevation when the folder is not writable) and before it
// unpacks anything — the one moment the old folder can be swapped out.
Component.prototype.beginInstallation = function()
{
    try {
        finceptMoveOldInstallationAside();
    } catch (e) {
        console.log("[Fincept] update: beginInstallation error: " + e);
    }
};

function finceptMoveOldInstallationAside()
{
    var target = installer.value("FinceptReplaceDir");
    var backup = installer.value("FinceptBackupDir");
    if (!target || !backup || installer.value("FinceptMovedAside") === "1")
        return;
    for (;;) {
        console.log("[Fincept] update: moving " + target + " -> " + backup);
        // A just-closed terminal (or a helper it started) can hold a file
        // for a moment: retry for about 20 seconds before asking.
        var reason = "";
        for (var attempt = 0; attempt < 20; ++attempt) {
            reason = finceptMoveDir(target, backup);
            if (reason === "") {
                installer.setValue("FinceptMovedAside", "1");
                return;
            }
            if (installer.status === QInstaller.Canceled)
                return;
            finceptSleep(1000);
        }
        console.log("[Fincept] update: move failed: " + reason);
        var answer = QMessageBox.question("fincept.update.folderInUse", "Fincept Terminal cannot be updated",
            "The installed version in\n" + target + "\ncannot be replaced" + (reason ? ":\n" + reason : ".") +
            "\n\nClose every Fincept Terminal window and any other program that uses files in that folder " +
            "(a command prompt open in it, for example), then click Retry.\n" +
            "Click Cancel to stop the update. Nothing will be changed.",
            QMessageBox.Retry | QMessageBox.Cancel, QMessageBox.Retry);
        if (answer !== QMessageBox.Retry)
            break;
    }
    installer.setValue("FinceptUpdateResult", "notStarted");
    installer.interrupt();   // IFW stops before unpacking: the old folder is untouched
}

function finceptRestoreAfterFailedUpdate()
{
    if (installer.value("FinceptMovedAside") !== "1" || installer.status === QInstaller.Success)
        return;
    var target = installer.value("FinceptReplaceDir");
    var backup = installer.value("FinceptBackupDir");
    console.log("[Fincept] update: installation did not finish (status " + installer.status +
                "), putting " + backup + " back");
    // Whatever is left in the target after IFW's rollback belongs to the
    // failed attempt; the previous version is complete in the backup folder.
    var script = finceptRestoreScript(target, backup);
    var ok = false;
    if (finceptCmdSafe(target) && finceptCmdSafe(backup)) {
        if (installer.fileExists(target))
            finceptRemoveDir(target);
        ok = finceptMoveDir(backup, target) === "" && finceptRestored(target, backup);
    } else {
        ok = finceptRunPowerShell(script) && finceptRestored(target, backup);
    }
    if (ok) {
        // When this installer exits it writes its own, empty component list
        // to <folder>\components.xml — over the restored installation's list
        // (its maintenance tool would no longer know what is installed, and
        // its uninstaller would skip this script's questions). Read-only
        // makes that write fail; controlscript.qs clears the flag again once
        // the installer has exited (Finish). Under Program Files the write
        // fails anyway: IFW has given up its elevation by then.
        var list = target + "\\components.xml";
        var r = installer.execute(finceptSystem32() + "\\attrib.exe", ["+R", list]);
        if (r && r.length >= 2 && Number(r[1]) === 0)
            installer.setValue("FinceptProtectedList", list);
    }
    if (!ok && installer.fileExists(backup)) {
        // IFW gives up its elevation before this signal: a folder under
        // Program Files needs one more administrator approval.
        QMessageBox.information("fincept.update.restoreNeedsAdmin", "Restoring the previous version",
            "The update could not be installed.\n\nWindows will now ask for permission to put your previous " +
            "version of Fincept Terminal back in\n" + target, QMessageBox.Ok);
        finceptRunPowerShell("try {\n" +
            "  $p = Start-Process -FilePath (Join-Path $PSHOME 'powershell.exe') -Verb RunAs -WindowStyle Hidden " +
            "-Wait -PassThru -ArgumentList ('-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass " +
            "-EncodedCommand ' + " + finceptPsQuote(finceptEncode(script)) + ")\n" +
            "  exit $p.ExitCode\n" +
            "} catch { exit 1 }\n");
        ok = finceptRestored(target, backup);
    }
    if (ok) {
        installer.setValue("FinceptMovedAside", "");
        finceptRecreateShortcuts(target);
    }
    installer.setValue("FinceptUpdateResult", ok ? "restored" : "restoreFailed");
    console.log("[Fincept] update: previous version " + (ok ? "restored" : "NOT restored, kept in " + backup));
}

function finceptRestored(target, backup)
{
    return installer.fileExists(target + "/FinceptMaintenanceTool.exe") && !installer.fileExists(backup);
}

// IFW's rollback deletes the shortcuts it created, which are the old
// installation's shortcuts too (same names).
function finceptRecreateShortcuts(target)
{
    var dir = target.replace(/\\/g, "/");
    for (var i = 0; i < finceptShortcutLinks.length; ++i) {
        var link = installer.value(finceptShortcutLinks[i].indexOf("@DesktopDir@") === 0 ? "DesktopDir" : "StartMenuDir") +
                   "/Fincept Terminal Community.lnk";
        if (installer.fileExists(link))
            continue;
        var ok = installer.performOperation("CreateShortcut", finceptShortcutArgs(dir, finceptShortcutLinks[i]));
        console.log("[Fincept] update: shortcut " + link + " recreated: " + ok);
    }
}

// True when `path` can be handed to cmd.exe as a QProcess argument without
// cmd interpreting any of it.
function finceptCmdSafe(path)
{
    var p = String(path || "");
    return p.length > 3 && /^[A-Za-z]:\\/.test(p) && !/[&|<>^%!(),;="]/.test(p);
}

function finceptSystem32()
{
    var root = installer.environmentVariable("SystemRoot");
    return (root ? root : "C:\\Windows") + "\\System32";
}

// Waits about `ms` milliseconds without blocking the window (performOperation
// runs the process on a worker thread and keeps the event loop turning).
function finceptSleep(ms)
{
    var started = Date.now();
    installer.performOperation("Execute", [finceptSystem32() + "\\PING.EXE", "-n",
                                           String(Math.max(2, Math.round(ms / 1000) + 1)), "127.0.0.1"]);
    if (Date.now() - started < ms / 4)   // ping did not wait: never spin
        installer.execute(finceptSystem32() + "\\cmd.exe", ["/d", "/c", "ping -n 2 127.0.0.1 >nul"]);
}

// Renames directory `from` to `to` (same volume: instant). Returns "" on
// success, else the reason Windows gave. Refuses an existing `to`: both
// `move` and Move-Item would put `from` INSIDE it.
function finceptMoveDir(from, to)
{
    if (installer.fileExists(to))
        return to + " already exists";
    if (finceptCmdSafe(from) && finceptCmdSafe(to)) {
        var r = installer.execute(finceptSystem32() + "\\cmd.exe", ["/d", "/c", "move", from, to, "2>&1"]);
        if (r && r.length >= 2 && Number(r[1]) === 0 && !installer.fileExists(from) && installer.fileExists(to))
            return "";
        return r && r.length ? String(r[0]).replace(/^\s+|\s+$/g, "") || ("exit code " + r[1]) : "cmd.exe did not start";
    }
    var errFile = finceptTempFile("move");
    var ok = finceptRunPowerShell("$e = " + finceptPsQuote(errFile) + "\n" +
        "try { Move-Item -LiteralPath " + finceptPsQuote(from) + " -Destination " + finceptPsQuote(to) +
        " -ErrorAction Stop; exit 0 }\n" +
        "catch { try { Set-Content -LiteralPath $e -Value $_.Exception.Message -Encoding UTF8 } catch {}; exit 1 }\n");
    var reason = finceptReadAndDelete(errFile);
    return ok ? "" : (reason || "PowerShell could not move the folder");
}

// Deletes directory tree `dir` (window stays responsive).
function finceptRemoveDir(dir)
{
    if (finceptCmdSafe(dir))
        return installer.performOperation("Execute", [finceptSystem32() + "\\cmd.exe", "/d", "/c", "rmdir", "/s",
                                                      "/q", dir, "&", "exit", "0"]);
    return finceptRunPowerShell("$ErrorActionPreference = 'SilentlyContinue'\n$d = " + finceptPsQuote(dir) + "\n" +
                                finceptPsRemoveTree("$d") + "exit 0\n");
}

// The previous version's folder, those left by an interrupted earlier update
// ("<folder>.previous-*" beside the install folder) and the old Add/Remove
// Programs entries, as install operations.
function finceptAddCleanupOperations()
{
    var target = installer.value("FinceptReplaceDir");
    var backup = installer.value("FinceptBackupDir");
    var keys = String(installer.value("FinceptOldUninstallKeys") || "").split("|");
    var dirs = [backup];
    var leaf = target.substring(target.lastIndexOf("\\") + 1);
    var parent = target.substring(0, target.lastIndexOf("\\"));
    var safe = finceptCmdSafe(target) && finceptCmdSafe(backup);
    if (safe) {
        var r = installer.execute(finceptSystem32() + "\\cmd.exe", ["/d", "/c", "dir", "/b", "/ad",
                                                                   target + ".previous-*"]);
        var names = r && r.length ? String(r[0]).split(/\r?\n/) : [];
        for (var n = 0; n < names.length; ++n) {
            var name = names[n].replace(/^\s+|\s+$/g, "");
            var dir = parent + "\\" + name;
            if (name.toLowerCase().indexOf((leaf + ".previous-").toLowerCase()) === 0 &&
                dir.toLowerCase() !== backup.toLowerCase() && finceptCmdSafe(dir))
                dirs.push(dir);
        }
        for (var d = 0; d < dirs.length; ++d) {
            component.addOperation("Execute", finceptSystem32() + "\\cmd.exe", "/d", "/c", "rmdir", "/s", "/q",
                                   dirs[d], "&", "exit", "0");
        }
        for (var k = 0; k < keys.length; ++k) {
            if (keys[k])
                component.addOperation("Execute", "{0,1}", finceptSystem32() + "\\reg.exe", "delete", keys[k], "/f");
        }
    } else {
        component.addOperation("Execute", finceptPowerShellExe(), "-NoLogo", "-NoProfile", "-NonInteractive",
                               "-ExecutionPolicy", "Bypass", "-EncodedCommand",
                               finceptEncode(finceptCleanupScript(target, keys)));
    }
    console.log("[Fincept] update: cleanup after success: " + dirs.join(", ") + (safe ? "" : " (+ stale)") +
                "; registry: " + (keys.join(", ") || "none"));
}

// rmdir is much faster than Remove-Item on large trees. The path reaches cmd
// through an environment variable inside quotes, so no character in it is
// interpreted by cmd; Remove-Item finishes whatever rmdir left.
function finceptPsRemoveTree(varName)
{
    return "  $env:FINCEPT_UPDATE_RMDIR = " + varName + "\n" +
        "  Start-Process -FilePath $env:ComSpec -ArgumentList '/d /c rmdir /s /q \"%FINCEPT_UPDATE_RMDIR%\"' " +
        "-NoNewWindow -Wait\n" +
        "  if (Test-Path -LiteralPath " + varName + ") { Remove-Item -LiteralPath " + varName + " -Recurse -Force }\n";
}

function finceptRestoreScript(target, backup)
{
    // Whatever is left in the target after IFW's rollback belongs to the
    // failed attempt; the previous version is complete in the backup folder.
    return "$ErrorActionPreference = 'Stop'\n" +
        "$t = " + finceptPsQuote(target) + "; $b = " + finceptPsQuote(backup) + "\n" +
        "try {\n" +
        "  if (-not (Test-Path -LiteralPath $b)) { exit 2 }\n" +
        "  if (Test-Path -LiteralPath $t) {\n" + finceptPsRemoveTree("$t") + "  }\n" +
        "  if (Test-Path -LiteralPath $t) { exit 1 }\n" +
        "  Move-Item -LiteralPath $b -Destination $t\n" +
        "  exit 0\n" +
        "} catch { exit 1 }\n";
}

function finceptCleanupScript(target, keys)
{
    var slash = target.lastIndexOf("\\");
    var parent = target.substring(0, slash < 3 ? 3 : slash);
    var prefix = target.substring(slash + 1) + ".previous-";
    var keyList = [];
    for (var i = 0; i < keys.length; ++i) {
        if (keys[i])
            keyList.push(finceptPsQuote(keys[i]));
    }
    // This update's backup and any left behind by an interrupted earlier one.
    return "$ErrorActionPreference = 'SilentlyContinue'; $ProgressPreference = 'SilentlyContinue'\n" +
        "$parent = " + finceptPsQuote(parent) + "; $prefix = " + finceptPsQuote(prefix) + "\n" +
        "foreach ($d in @(Get-ChildItem -LiteralPath $parent -Directory -Force -Filter ($prefix + '*'))) {\n" +
        "  if (-not $d.Name.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) { continue }\n" +
        "  Write-Output ('Removing the previous version: ' + $d.FullName)\n" +
        "  $old = $d.FullName\n" + finceptPsRemoveTree("$old") +
        "}\n" +
        "foreach ($k in @(" + keyList.join(", ") + ")) {\n" +
        "  Write-Output ('Removing the old Add/Remove Programs entry: ' + $k)\n" +
        "  Remove-Item -LiteralPath ('Registry::' + $k) -Recurse -Force\n" +
        "}\n" +
        "exit 0\n";
}

function finceptPowerShellExe()
{
    var root = installer.environmentVariable("SystemRoot");
    return (root ? root : "C:\\Windows") + "\\System32\\WindowsPowerShell\\v1.0\\powershell.exe";
}

// Runs `script` and returns true when it exits with 0. performOperation runs
// the process on a worker thread, so the window keeps painting meanwhile.
function finceptRunPowerShell(script)
{
    return installer.performOperation("Execute", [finceptPowerShellExe(), "-NoLogo", "-NoProfile", "-NonInteractive",
                                                  "-ExecutionPolicy", "Bypass", "-EncodedCommand",
                                                  finceptEncode(script)]);
}

// A PowerShell single-quoted literal. PowerShell also treats the typographic
// single quotes as quote characters, so they are doubled as well.
function finceptPsQuote(s)
{
    return "'" + String(s).replace(/['\u2018\u2019\u201A\u201B]/g, "$&$&") + "'";
}

// -EncodedCommand takes base64 of the UTF-16LE script text.
function finceptEncode(text)
{
    var table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    var bytes = [];
    for (var i = 0; i < text.length; ++i) {
        var c = text.charCodeAt(i);
        bytes.push(c & 0xff, (c >> 8) & 0xff);
    }
    var out = "";
    for (var j = 0; j < bytes.length; j += 3) {
        var n = (bytes[j] << 16) | ((j + 1 < bytes.length ? bytes[j + 1] : 0) << 8) |
                (j + 2 < bytes.length ? bytes[j + 2] : 0);
        out += table.charAt((n >> 18) & 63) + table.charAt((n >> 12) & 63) +
               (j + 1 < bytes.length ? table.charAt((n >> 6) & 63) : "=") +
               (j + 2 < bytes.length ? table.charAt(n & 63) : "=");
    }
    return out;
}

function finceptTempFile(tag)
{
    var dir = installer.environmentVariable("TEMP");
    return (dir ? dir : installer.value("TargetDir") + "/..") + "\\fincept-update-" + tag + "-" + Date.now() + ".txt";
}

function finceptReadAndDelete(path)
{
    var text = "";
    try {
        if (installer.fileExists(path)) {
            text = String(installer.readFile(path, "UTF-8") || "").replace(/^\uFEFF/, "").replace(/\s+$/, "");
            installer.performOperation("Delete", [path]);
        }
    } catch (e) {
        console.log("[Fincept] update: cannot read " + path + ": " + e);
    }
    return text;
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
    var markers = ["/.edition-root.json", "/profiles.json", "/data/fincept.db", "/workspace.db", "/venv-numpy2",
                   "/.setup_complete"];
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

    // Only this edition's names (see the header). The legacy com.fincept.terminal
    // root, HKCU\Software\Fincept\FinceptTerminal, %APPDATA%\fincept-terminal,
    // FinceptTerminal/* credentials and %TEMP%\fincept_* are shared with the
    // private build and are left alone.

    // 1. Main data root (all profiles) and a FINCEPT_DATA_DIR override
    removeDirWindows(localAppData + "/com.fincept.terminal.community");
    removeDirWindows(customDataRoot());

    // 2. Qt's per-application folders (QStandardPaths App*/Cache: shader cache)
    removeDirWindows(localAppData + "/FinceptCommunity/FinceptTerminalCommunity");
    removeDirIfEmptyWindows(localAppData + "/FinceptCommunity");
    removeDirWindows(localAppData + "/fincept-dev");

    // 3. Roaming per-application folder
    removeDirWindows(appData + "/FinceptCommunity/FinceptTerminalCommunity");
    removeDirIfEmptyWindows(appData + "/FinceptCommunity");

    // 4. Registry — QSettings default format on Windows
    runAndLog("reg.exe", ["delete", "HKCU\\Software\\FinceptCommunity\\FinceptTerminalCommunity", "/f"]);
    // The vendor key only when nothing is left in it: `reg delete /f` is
    // recursive. `reg query` prints no HKEY_ line for an empty key.
    runAndLog("cmd.exe", ["/c",
        "for /f %c in ('reg query \"HKCU\\Software\\FinceptCommunity\" 2^>nul ^| find /c \"HKEY_\"') do " +
        "(if %c==0 reg delete \"HKCU\\Software\\FinceptCommunity\" /f >nul 2>&1) & exit /b 0"]);

    // 4b. Coding-agent history. The bundled Python scripts keep their files in the
    //     data root (scripts/fincept_paths.py); the home folders older releases
    //     used (.fincept, .oecd_cache, .cache/fincept-terminal) carry no edition
    //     name, may hold another build's files, and are left alone.
    if (userProfile) {
        runAndLog("cmd.exe", ["/c",
            "for /d %d in (\"" + toWin(userProfile) + "\\.claude\\projects\\*com-fincept-terminal-community-agent-workspace*\") do " +
            "rmdir /s /q \"%d\" 2>nul & exit /b 0"]);
    }

    // 5. This edition's boot log (the log fallback when the data root is unwritable)
    runAndLog("cmd.exe", ["/c",
        "del /q /f \"" + toWin(tempDir) + "\\fincept-community-boot.log\" 2>nul & exit /b 0"]);

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

    // Only this edition's names (see the header): com.fincept.terminal,
    // com.fincept.FinceptTerminal.plist and the com.fincept.terminal Keychain
    // service are shared with the private build and are left alone.

    // 1. Main data root (all profiles), a FINCEPT_DATA_DIR override, Qt's App* folder
    removeDirPosix(home + "/Library/Application Support/com.fincept.terminal.community");
    removeDirPosix(customDataRoot());
    removeDirPosix(home + "/Library/Application Support/FinceptCommunity/FinceptTerminalCommunity");
    removeDirIfEmptyPosix(home + "/Library/Application Support/FinceptCommunity");
    removeDirPosix(home + "/Library/Application Support/fincept-dev");

    // 2. Preferences / plist: QSettings (organization FinceptCommunity) and the
    //    bundle-id domain. cfprefsd caches preferences: drop the domains first so
    //    the deleted files are not written back.
    runAndLog("/bin/bash", ["-c",
        "/usr/bin/defaults delete com.finceptcommunity.FinceptTerminalCommunity >/dev/null 2>&1; " +
        "/usr/bin/defaults delete com.fincept.terminal.community >/dev/null 2>&1; exit 0"]);
    removeFilePosix(home + "/Library/Preferences/com.finceptcommunity.FinceptTerminalCommunity.plist");
    removeFilePosix(home + "/Library/Preferences/com.fincept.terminal.community.plist");

    // 3. Caches (Qt shader cache lands in Caches/FinceptCommunity/FinceptTerminalCommunity)
    removeDirPosix(home + "/Library/Caches/com.fincept.terminal.community");
    removeDirPosix(home + "/Library/Caches/FinceptCommunity/FinceptTerminalCommunity");
    removeDirIfEmptyPosix(home + "/Library/Caches/FinceptCommunity");

    // 4. Saved application state, URL-session storage
    removeDirPosix(home + "/Library/Saved Application State/com.fincept.terminal.community.savedState");
    removeDirPosix(home + "/Library/HTTPStorages/com.fincept.terminal.community");

    // 4b. Folders the bundled Python scripts and coding agents create in $HOME
    removeHomeExtrasPosix(home);

    // 5. This edition's boot log
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

    // Only this edition's names (see the header): com.fincept.terminal,
    // ~/.config/Fincept, the libsecret schema com.fincept.terminal and
    // fincept-terminal.desktop are shared with the private build and left alone.

    // 1. Main data root (the app hard-codes ~/.local/share; XDG too, to be safe),
    //    a FINCEPT_DATA_DIR override, and Qt's App* folder
    removeDirPosix(xdgDat + "/com.fincept.terminal.community");
    removeDirPosix(home   + "/.local/share/com.fincept.terminal.community");
    removeDirPosix(customDataRoot());
    removeDirPosix(xdgDat + "/FinceptCommunity/FinceptTerminalCommunity");
    removeDirIfEmptyPosix(xdgDat + "/FinceptCommunity");
    removeDirPosix(xdgDat + "/fincept-dev");

    // 2. QSettings .conf file
    removeFilePosix(xdgCfg + "/FinceptCommunity/FinceptTerminalCommunity.conf");
    removeDirIfEmptyPosix(xdgCfg + "/FinceptCommunity");

    // 3. Cache dirs (Qt shader cache lands in FinceptCommunity/FinceptTerminalCommunity)
    removeDirPosix(xdgCch + "/com.fincept.terminal.community");
    removeDirPosix(xdgCch + "/FinceptCommunity/FinceptTerminalCommunity");
    removeDirIfEmptyPosix(xdgCch + "/FinceptCommunity");

    // 3c. Folders the bundled Python scripts and coding agents create in $HOME
    removeHomeExtrasPosix(home);

    // 4. Desktop entry (installed via CreateDesktopEntry at install time — IFW's
    //    own UNDO step removes it, but clean up any stale copies just in case).
    removeFilePosix(home + "/.local/share/applications/fincept-terminal-community.desktop");

    // 5. This edition's boot log
    removeTempPosix();

    // Screenshots the user saved (fincept_YYYYMMDD_HHMMSS.png in $HOME) are
    // the user's own files and are deliberately kept.
}

// ---------- Shared by mac + linux ----------

function removeHomeExtrasPosix(home)
{
    if (!home) return;
    // ~/.fincept, ~/.oecd_cache and ~/.cache/fincept-terminal (script data of
    // older releases) carry no edition name and may hold another build's files:
    // left alone. The scripts keep their files in the data root now.
    // ~/fincept-terminal/agents.db is the legacy agent repository the private
    // build imports from; this edition keeps its own in <data root>/finagent.
    runAndLog("/bin/bash", ["-c",
        "rm -rf \"" + shellEscape(home) + "\"/.claude/projects/*com-fincept-terminal-community-agent-workspace* 2>/dev/null; exit 0"]);
}

function removeTempPosix()
{
    // Only this edition's boot log. fincept_* temp files and the fincept-<hash>
    // instance sockets cannot be told apart from the private build's.
    runAndLog("/bin/bash", ["-c",
        "for t in /tmp \"${TMPDIR:-/tmp}\"; do " +
        "  rm -f \"$t\"/fincept-community-boot.log 2>/dev/null; " +
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
