// controlscript.qs — Fincept Terminal QtIFW Controller script.
//
// Two jobs:
//
// 1. Wizard-page automation for HEADLESS / COMMAND-LINE invocations, e.g.
//      FinceptMaintenanceTool.exe purge --confirm-command --default-answer
//      FinceptTerminal-<v>-setup.exe install --confirm-command --default-answer
//    Without this, IFW would block on each wizard page even when --confirm-command
//    is set, because some pages (Introduction, ReadyForInstallation, Finished)
//    only auto-advance for the *installer* path, not the uninstaller path.
//
// 2. The in-app UPDATE MODE (Windows): the terminal's updater
//    (src/services/updater/UpdateInstaller_win.cpp) starts the new setup.exe as
//      FinceptTerminal-<v>-windows-x64-setup.exe FinceptUpdate=1 TargetDir=<install folder>
//    and quits. See "In-app update mode" below.
//
// CRITICAL: This file is embedded as the installer's <ControlScript> via
// CPACK_IFW_PACKAGE_CONTROL_SCRIPT (CMakeLists.txt). An embedded control
// script is loaded on EVERY run of the installer and the maintenance tool —
// interactive GUI runs included. Its page callbacks fire whenever a wizard
// page is shown, in BOTH interactive and command-line modes.
//
// Therefore every callback MUST bail out early in interactive GUI mode, or it
// auto-clicks "Next" the instant each page appears and the wizard flashes
// straight through to a silent install at the default location — the user
// never gets to see the Target Directory page, let alone choose a custom
// install path. The `if (!installer.isCommandLineInstance()) return;` guard at
// the top of each callback is exactly what keeps the interactive wizard usable
// while preserving headless automation (isCommandLineInstance() is true under
// --confirm-command / CLI, false in the GUI). The only GUI runs that are driven
// by this script are update-mode runs (FinceptUpdate=1), which the terminal
// starts itself. In the normal wizard it only watches the target folder, so
// that picking an existing installation of this edition updates it in place
// ("Upgrade from the wizard" below); every page stays the user's.
//
// References:
//   https://doc.qt.io/qtinstallerframework/noninteractive.html
//   https://doc.qt.io/qtinstallerframework/scripting-installer.html  (isCommandLineInstance)

// State of the in-app update mode and of the wizard upgrade (see "In-app
// update mode" and "Upgrade from the wizard" below).
var finceptUpdate = {
    active: false,          // FinceptUpdate=1 GUI installer run
    relaunchOnFinish: "",   // exe to start when Finish is clicked ("" = none)
    profile: "",            // profile the terminal ran on ("" = default)
    upgradeDir: "",         // wizard: folder of an existing installation the user picked
    targetHooked: false,    // wizard: target page line edit is being watched
    removeTargetDir: ""     // config value of RemoveTargetDir, restored after the target page
};

function Controller() {
    // Page callbacks below drive headless flows. Defining a Controller at all
    // is what makes IFW load this file.
    try {
        if (finceptUpdateRequested())
            finceptUpdateSetup();
        else if (installer.isInstaller() && interactiveGui())
            finceptWizardSetup();
    } catch (e) {
        // Never throw out of Controller(): IFW aborts before showing any UI.
        console.log("[Fincept] Controller() error: " + e);
    }
}

// Returns true only for interactive GUI runs, where the user drives the wizard
// themselves and we must NOT auto-advance any page. Defends against IFW builds
// that predate isCommandLineInstance() (4.0+) by treating a missing method as
// "interactive" — the safe default, since auto-clicking a GUI wizard is the bug
// we are fixing. Project ships with IFW 4.7/4.8 where the method exists.
function interactiveGui() {
    if (typeof installer.isCommandLineInstance !== "function")
        return true;
    return !installer.isCommandLineInstance();
}

// ── Headless install / maintenance-tool path ─────────────────────────────────
// In interactive GUI mode each of these returns immediately so the user sees
// and controls every page (Target Directory, components, license, etc.).
// Update mode (FinceptUpdate=1) has its own branch at the top of the callbacks
// it needs.

Controller.prototype.IntroductionPageCallback = function() {
    if (finceptUpdate.active) {
        finceptUpdateIntroduction();
        return;
    }
    if (interactiveGui()) return;
    if (gui && gui.clickButton)
        gui.clickButton(buttons.NextButton);
};

Controller.prototype.TargetDirectoryPageCallback = function() {
    if (interactiveGui()) {         // GUI: let the user choose the install path
        finceptWizardWatchTarget();
        return;
    }
    if (gui && gui.clickButton)
        gui.clickButton(buttons.NextButton);
};

Controller.prototype.ComponentSelectionPageCallback = function() {
    finceptWizardLeftTargetPage();
    if (interactiveGui()) return;
    if (gui && gui.clickButton)
        gui.clickButton(buttons.NextButton);
};

Controller.prototype.LicenseAgreementPageCallback = function() {
    finceptWizardLeftTargetPage();
    if (interactiveGui()) return;
    // Per Qt IFW docs example: explicitly check the accept radio before
    // clicking Next, otherwise the Next button stays disabled and silent
    // installs hang. Guard each access — `pageWidgetByObjectName` returns
    // null if the page isn't in the wizard for this run (e.g. uninstall
    // doesn't show a license page).
    if (!gui || !gui.pageWidgetByObjectName) return;
    var page = gui.pageWidgetByObjectName("LicenseAgreementPage");
    if (page && typeof page.AcceptLicenseRadioButton !== "undefined") {
        page.AcceptLicenseRadioButton.checked = true;
    }
    if (gui.clickButton)
        gui.clickButton(buttons.NextButton);
};

Controller.prototype.StartMenuDirectoryPageCallback = function() {
    // Only reached in update mode when the Start Menu folder could not be
    // worked out up front (see finceptUpdateSetup): accept the default.
    if (finceptUpdate.active) {
        gui.clickButton(buttons.NextButton, 100);
        return;
    }
    finceptWizardLeftTargetPage();
};

Controller.prototype.ReadyForInstallationPageCallback = function() {
    if (finceptUpdate.active) {
        gui.clickButton(buttons.CommitButton, 200);
        return;
    }
    finceptWizardLeftTargetPage();
    finceptWizardReadyText();
    if (interactiveGui()) return;
    if (gui && gui.clickButton)
        gui.clickButton(buttons.CommitButton);
};

Controller.prototype.PerformInstallationPageCallback = function() {
    if (finceptUpdate.active) return;   // the installation starts by itself
    if (interactiveGui()) return;
    if (gui && gui.clickButton)
        gui.clickButton(buttons.CommitButton);
};

Controller.prototype.FinishedPageCallback = function() {
    if (finceptUpdate.active) {
        finceptUpdateFinishedPage();
        return;
    }
    finceptWizardFinishedPage();
    if (interactiveGui()) return;
    if (gui && gui.clickButton)
        gui.clickButton(buttons.FinishButton);
};

// ── In-app update mode ───────────────────────────────────────────────────────
// Started by the terminal with FinceptUpdate=1 TargetDir=<install folder> (IFW
// stores every key=value argument as an installer value). It replaces the
// installation in that folder in place, with no clicks:
//
//   * The pages the user answered when installing (target folder, components,
//     licence, Start Menu folder) are skipped. IFW's target-folder check would
//     refuse a folder that already holds an installation, and the update must
//     replace exactly that folder. The Introduction page stays (IFW loads the
//     components when it is left) and is advanced automatically.
//   * installationStarted (below): the progress page is up; wait — window
//     responsive, Cancel works — until the running terminal has exited, note
//     the old installation's Add/Remove Programs entry so the new installation
//     registers under the same key (never two entries), and pick the name the
//     old folder is moved to.
//   * installscript.qs, beginInstallation: after IFW has asked for elevation
//     (UAC) when the folder needs it and before any file is unpacked, the old
//     folder is renamed to "<folder>.previous-<time>" (same volume: instant).
//     IFW then installs into the empty folder. The last install operation
//     deletes the old folder and any stale Add/Remove Programs entries for it;
//     if the installation fails, IFW rolls its files back and the old folder is
//     renamed back, so the previous version keeps working.
//   * Success: the window closes by itself and the new version is started as
//     the user who ran the update (the installer's GUI process is never the
//     elevated one). Failure: the window stays open and says what happened;
//     Finish reopens the previous version when it is intact.
//
// User data never lives in the install folder (it is in the per-user data
// root), and the uninstall cleanup in installscript.qs never runs here.

function finceptUpdateRequested() {
    return installer.isInstaller() && interactiveGui() && installer.value("FinceptUpdate") === "1";
}

function finceptLog(text) {
    console.log("[Fincept] update: " + text);
}

function finceptNative(path) {
    var p = String(path || "").replace(/\//g, "\\");
    while (p.length > 3 && p.charAt(p.length - 1) === "\\")
        p = p.substring(0, p.length - 1);
    return p;
}

function finceptSystem32() {
    var root = installer.environmentVariable("SystemRoot");
    return (root ? root : "C:\\Windows") + "\\System32";
}

// The page title of the progress page; IFW replaces it with its own messages
// ("Creating Maintenance Tool", ...) as the installation goes on.
function finceptTitle(text) {
    try {
        installer.titleMessageChanged(text);
    } catch (e) {
        finceptLog("cannot set the title: " + e);
    }
}

// Waits about `ms` milliseconds WITHOUT blocking the window: performOperation
// runs the operation on a worker thread and keeps the event loop turning.
function finceptSleep(ms) {
    var started = Date.now();
    var pings = Math.max(2, Math.round(ms / 1000) + 1);
    installer.performOperation("Execute", [finceptSystem32() + "\\PING.EXE", "-n", String(pings), "127.0.0.1"]);
    if (Date.now() - started < ms / 4) {
        // ping did not wait (blocked or missing): never spin in a busy loop
        installer.execute(finceptSystem32() + "\\cmd.exe", ["/d", "/c", "ping -n 2 127.0.0.1 >nul"]);
    }
}

function finceptUpdateSetup() {
    finceptUpdate.active = true;
    var target = finceptNative(installer.value("TargetDir"));
    finceptLog("mode on, target=" + target + ", version=" + installer.value("ProductVersion"));

    installer.setDefaultPageVisible(QInstaller.TargetDirectory, false);
    installer.setDefaultPageVisible(QInstaller.ComponentSelection, false);
    installer.setDefaultPageVisible(QInstaller.LicenseCheck, false);

    // The Start Menu page turns the folder name into a full path when it is
    // left; a hidden page would leave @StartMenuDir@ relative (the shortcut
    // would land in the current directory). Do what the page does.
    var menuDir = String(installer.value("StartMenuDir") || "");
    var programs = installer.value("AllUsers") === "true" ? installer.value("AllUsersStartMenuProgramsPath")
                                                          : installer.value("UserStartMenuProgramsPath");
    if (menuDir && !/^[A-Za-z]:[\\\/]/.test(menuDir) && programs) {
        installer.setValue("StartMenuDir", programs + "\\" + menuDir);
        installer.setDefaultPageVisible(QInstaller.StartMenuSelection, false);
    } else if (/^[A-Za-z]:[\\\/]/.test(menuDir)) {
        installer.setDefaultPageVisible(QInstaller.StartMenuSelection, false);
    }

    installer.installationStarted.connect(finceptUpdateStarted);
    installer.installationFinished.connect(finceptUpdateInstallationFinished);
    gui.finishButtonClicked.connect(finceptUpdateFinishClicked);

    // The new version must start on the profile the terminal ran on: data
    // and the single-instance key are per profile, and without --profile it
    // would open on "default" (and hand itself to a "default" instance that
    // may be running). Terminals that pass FinceptProfile say so; for others
    // read it from the terminal's command line while it is still running
    // (it waits for this window before it quits).
    finceptUpdate.profile = String(installer.value("FinceptProfile") || "");
    if (!finceptUpdate.profile)
        finceptUpdate.profile = finceptDetectProfile(target);
    finceptLog("the new version will start on profile '" + (finceptUpdate.profile || "default") + "'");
}

// --profile of the FinceptTerminal.exe running from `target`, or "". Runs
// before the window exists, so the short wait blocks nothing visible.
function finceptDetectProfile(target) {
    var script = "$ErrorActionPreference = 'SilentlyContinue'; [Console]::OutputEncoding = [Text.Encoding]::UTF8\n" +
        "$exe = " + finceptPsQuote(target + "\\FinceptTerminal.exe") + "\n" +
        "$p = @(Get-CimInstance Win32_Process -Filter \"Name='FinceptTerminal.exe'\" | " +
        "Where-Object { $_.ExecutablePath -eq $exe })[0]\n" +
        "if ($p -and $p.CommandLine -match '(?:^|\\s)--profile\\s+(?:\"([^\"]*)\"|(\\S+))') {\n" +
        "  if ($Matches[1]) { $Matches[1] } else { $Matches[2] }\n" +
        "}\n";
    var root = installer.environmentVariable("SystemRoot");
    var ps = (root ? root : "C:\\Windows") + "\\System32\\WindowsPowerShell\\v1.0\\powershell.exe";
    try {
        var r = installer.execute(ps, ["-NoLogo", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass",
                                       "-EncodedCommand", finceptEncode(script)], "", "UTF-8", "UTF-8");
        if (r && r.length >= 1)
            return String(r[0]).replace(/^\s+|\s+$/g, "");
    } catch (e) {
        finceptLog("cannot read the terminal's profile: " + e);
    }
    return "";
}

// A PowerShell single-quoted literal (typographic single quotes count too).
// Same as in installscript.qs; the two scripts cannot share code.
function finceptPsQuote(s) {
    return "'" + String(s).replace(/['\u2018\u2019\u201A\u201B]/g, "$&$&") + "'";
}

// base64 of the UTF-16LE text, as -EncodedCommand wants it (as in installscript.qs).
function finceptEncode(text) {
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

function finceptUpdateIntroduction() {
    var target = finceptNative(installer.value("TargetDir"));
    var page = gui.currentPageWidget();
    try {
        if (page && page.MessageLabel)
            page.MessageLabel.setText("Fincept Terminal is being updated to version " +
                                      installer.value("ProductVersion") + " in\n" + target +
                                      "\n\nYour data and settings are kept. This window closes by itself " +
                                      "and the new version starts when the update is done.");
    } catch (e) {
        finceptLog("cannot set the introduction text: " + e);
    }
    // Deferred: this callback runs before the window is first shown.
    gui.clickButton(buttons.NextButton, 300);
}

// Runs at the very start of the installation, on the progress page, before
// IFW creates the target folder or unpacks anything.
function finceptUpdateStarted() {
    try {
        var target = finceptNative(installer.value("TargetDir"));
        if (!finceptWaitUntilClosed(target, 60000)) {
            finceptLog("stopped: the terminal is still running or the user cancelled");
            installer.setValue("FinceptUpdateResult", "notStarted");
            if (installer.status !== QInstaller.Canceled)
                installer.interrupt();   // IFW stops before unpacking; nothing changed
            return;
        }
        if (installer.fileExists(target + "\\FinceptMaintenanceTool.exe") && target.length > 3) {
            finceptNoteOldUninstallEntries(target);
            installer.setValue("FinceptReplaceDir", target);
            installer.setValue("FinceptBackupDir", finceptBackupPath(target));
            finceptLog("old installation will be moved to " + installer.value("FinceptBackupDir"));
        } else {
            finceptLog("no installation in " + target + " - installing fresh");
        }
        finceptTitle("Updating Fincept Terminal to version " + installer.value("ProductVersion"));
    } catch (e) {
        finceptLog("installationStarted error: " + e);
    }
}

// True once no program of the installation in `target` runs any more. The
// terminal quits right after starting the update; give it `firstAskMs`, then
// ask. False when the user gives up (or cancels the installation).
function finceptWaitUntilClosed(target, firstAskMs) {
    var exes = ["FinceptTerminal.exe", "QtWebEngineProcess.exe", "yt-dlp.exe"];
    var limit = firstAskMs;
    var since = Date.now();
    var announced = false;
    for (;;) {
        var running = [];
        for (var i = 0; i < exes.length; ++i) {
            if (installer.isProcessRunning(target + "\\" + exes[i]))
                running.push(exes[i]);
        }
        if (running.length === 0)
            return true;
        if (installer.status === QInstaller.Canceled)
            return false;
        if (!announced) {
            finceptTitle("Waiting for Fincept Terminal to close");
            finceptLog("waiting for " + running.join(", "));
            announced = true;
        }
        if (Date.now() - since > limit) {
            var answer = QMessageBox.question("fincept.update.stillRunning", "Fincept Terminal is still running",
                "Fincept Terminal has not closed yet, so it cannot be updated:\n" + target + "\n\n" +
                "Close every Fincept Terminal window, then click Retry.\n" +
                "Click Cancel to stop the update. Nothing will be changed.",
                QMessageBox.Retry | QMessageBox.Cancel, QMessageBox.Retry);
            if (answer !== QMessageBox.Retry)
                return false;
            since = Date.now();
            limit = 30000;
        }
        finceptSleep(1000);
    }
}

// The Add/Remove Programs entries that point at the installation in `target`.
// The new installation registers under the first HKCU entry's key (IFW reuses
// ProductUUID as the key name), so the entry is updated, not duplicated; any
// other entry for this folder is removed after a successful update.
function finceptNoteOldUninstallEntries(target) {
    var needle = target + "\\FinceptMaintenanceTool.exe";
    var roots = ["HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
                 "HKLM\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall"];
    var keys = [];
    for (var r = 0; r < roots.length; ++r) {
        var result = installer.execute(finceptSystem32() + "\\reg.exe", ["query", roots[r], "/s", "/f", needle, "/d"]);
        if (!result || result.length === 0)
            continue;
        var lines = String(result[0]).split(/\r?\n/);
        for (var l = 0; l < lines.length; ++l) {
            var line = lines[l].replace(/^\s+|\s+$/g, "");
            if (line.indexOf("HKEY_") === 0 && keys.indexOf(line) < 0)
                keys.push(line);
        }
    }
    var reused = "";
    for (var k = 0; k < keys.length && !reused; ++k) {
        var guid = keys[k].substring(keys[k].lastIndexOf("\\") + 1);
        if (keys[k].indexOf("HKEY_CURRENT_USER\\") === 0 && /^\{[0-9A-Fa-f-]{36}\}$/.test(guid)) {
            reused = keys[k];
            installer.setValue("ProductUUID", guid);
        }
    }
    var others = [];
    for (var o = 0; o < keys.length; ++o) {
        if (keys[o] !== reused)
            others.push(keys[o]);
    }
    installer.setValue("FinceptOldUninstallKeys", others.join("|"));
    finceptLog("Add/Remove Programs entries for this folder: " + (keys.length ? keys.join(", ") : "none") +
               (reused ? "; reusing " + reused : ""));
}

function finceptBackupPath(target) {
    var d = new Date();
    function two(n) { return (n < 10 ? "0" : "") + n; }
    var stamp = d.getFullYear() + two(d.getMonth() + 1) + two(d.getDate()) + "-" + two(d.getHours()) +
                two(d.getMinutes()) + two(d.getSeconds());
    var path = target + ".previous-" + stamp;
    for (var n = 2; installer.fileExists(path); ++n)
        path = target + ".previous-" + stamp + "-" + n;
    return path;
}

function finceptUpdateInstallationFinished() {
    finceptLog("installation finished, status=" + installer.status);
    // IFW switches to the Finished page by itself unless "Show Details" is
    // open (it is by default with --verbose); then this click does it. On the
    // Finished page itself the click does nothing.
    gui.clickButton(buttons.CommitButton, 800);
}

// What the Finished page says after an update that did not complete.
// `reopen`: the previous version is intact and Finish starts it again.
function finceptFailureText(target, reopen) {
    var result = installer.value("FinceptUpdateResult");
    var again = reopen ? "\n\nClick Finish to open it again." : "";
    if (result === "restored")
        return "The update could not be installed. Your previous version of Fincept Terminal has been put back " +
               "and works as before." + again;
    if (result === "restoreFailed")
        return "The update could not be installed, and your previous version could not be put back " +
               "automatically.\n\nIt was kept in\n" + installer.value("FinceptBackupDir") + "\n\nRename that " +
               "folder to\n" + target + "\nor run the Fincept Terminal setup again.";
    if (result === "notStarted")
        return "The update was not installed. Nothing has been changed; Fincept Terminal in\n" + target +
               "\nis still the previous version.";
    return "The update could not be installed. Nothing has been changed; your previous version of Fincept " +
           "Terminal is still installed." + again;
}

function finceptSetFinishedText(text) {
    var page = gui.currentPageWidget();
    try {
        if (page && page.MessageLabel)
            page.MessageLabel.setText(text);
    } catch (e) {
        finceptLog("cannot set the finished text: " + e);
    }
}

function finceptUpdateFinishedPage() {
    var target = finceptNative(installer.value("TargetDir"));
    var exe = target + "\\FinceptTerminal.exe";
    if (installer.status === QInstaller.Success) {
        finceptSetFinishedText("Fincept Terminal has been updated to version " + installer.value("ProductVersion") +
                               ". It is starting now.");
        // Start it now, while this window still owns the foreground: Windows
        // lets a program started by the foreground window come to the front.
        // Started on Finish, its window appeared after this installer had
        // gone and opened behind whatever window was next. This window closes
        // a few seconds later, once the terminal's own window is up.
        finceptStartTerminal(exe);
        finceptUpdate.relaunchOnFinish = "";
        gui.clickButton(buttons.FinishButton, 4000);
    } else {
        var intact = installer.value("FinceptUpdateResult") !== "restoreFailed" && installer.fileExists(exe) &&
                     !installer.isProcessRunning(exe);
        finceptUpdate.relaunchOnFinish = intact ? exe : "";
        finceptSetFinishedText(finceptFailureText(target, intact));
    }
    finceptLog("finished page, status=" + installer.status + ", relaunch=" + finceptUpdate.relaunchOnFinish);
}

// ── Upgrade from the wizard ──────────────────────────────────────────────────
// A user who runs a newer setup.exe and picks the folder of an existing
// installation of this edition gets the same in-place update (old folder moved
// aside, new files installed, old folder removed — or put back on failure)
// instead of IFW's "already contains an installation" refusal. Folders of any
// other installation (another product, the private build) are still refused.
//
// IFW 4.x skips that target-folder check while RemoveTargetDir is false, so it
// is false exactly while such a folder is selected on the target page and set
// back on every later page and when the installation starts (the value also
// decides whether uninstalling removes the folder).

function finceptWizardSetup() {
    finceptUpdate.removeTargetDir = String(installer.value("RemoveTargetDir") || "true");
    installer.installationStarted.connect(finceptWizardInstallationStarted);
    gui.finishButtonClicked.connect(finceptUnprotectListAfterExit);
}

// The folder holds an installation made by an installer with this name.
function finceptIsThisEditionInstalled(dir) {
    if (!dir || dir.length <= 3 || !installer.fileExists(dir + "\\FinceptMaintenanceTool.exe") ||
        !installer.fileExists(dir + "\\components.xml"))
        return false;
    var name = installer.value("ProductName");
    var xml = String(installer.readFile(dir + "\\components.xml", "UTF-8") || "");
    return !!name && xml.indexOf("<ApplicationName>" + name + "</ApplicationName>") >= 0;
}

function finceptWizardWatchTarget() {
    if (!finceptUpdate.removeTargetDir)
        return;   // not an interactive installer run
    var page = gui.pageWidgetByObjectName("TargetDirectoryPage");
    if (!page || !page.TargetDirectoryLineEdit)
        return;
    if (!finceptUpdate.targetHooked) {
        page.TargetDirectoryLineEdit.textChanged.connect(finceptWizardTargetChanged);
        finceptUpdate.targetHooked = true;
    }
    finceptWizardTargetChanged(page.TargetDirectoryLineEdit.text);
}

function finceptWizardTargetChanged(text) {
    var dir = finceptNative(text);
    var upgrade = finceptIsThisEditionInstalled(dir);
    if (upgrade !== (finceptUpdate.upgradeDir !== ""))
        finceptLog(upgrade ? "wizard: " + dir + " holds an installation of this edition - will update it in place"
                           : "wizard: no installation of this edition selected");
    finceptUpdate.upgradeDir = upgrade ? dir : "";
    installer.setValue("RemoveTargetDir", upgrade ? "false" : finceptUpdate.removeTargetDir);
}

function finceptWizardLeftTargetPage() {
    if (finceptUpdate.removeTargetDir && installer.value("RemoveTargetDir") !== finceptUpdate.removeTargetDir)
        installer.setValue("RemoveTargetDir", finceptUpdate.removeTargetDir);
}

function finceptWizardReadyText() {
    if (!finceptUpdate.upgradeDir)
        return;
    var page = gui.currentPageWidget();
    try {
        if (page && page.MessageLabel)
            page.MessageLabel.setText("Fincept Terminal in\n" + finceptUpdate.upgradeDir + "\nwill be updated to " +
                                      "version " + installer.value("ProductVersion") + ". Your data and settings " +
                                      "are kept.\n\nClick Install to continue.");
    } catch (e) {
        finceptLog("cannot set the ready text: " + e);
    }
}

function finceptWizardInstallationStarted() {
    try {
        finceptWizardLeftTargetPage();
        var target = finceptNative(installer.value("TargetDir"));
        if (!finceptUpdate.upgradeDir || target.toLowerCase() !== finceptUpdate.upgradeDir.toLowerCase() ||
            !finceptIsThisEditionInstalled(target))
            return;
        if (!finceptWaitUntilClosed(target, 1500)) {
            installer.setValue("FinceptUpdateResult", "notStarted");
            if (installer.status !== QInstaller.Canceled)
                installer.interrupt();
            return;
        }
        finceptNoteOldUninstallEntries(target);
        installer.setValue("FinceptReplaceDir", target);
        installer.setValue("FinceptBackupDir", finceptBackupPath(target));
        finceptLog("wizard: old installation will be moved to " + installer.value("FinceptBackupDir"));
    } catch (e) {
        finceptLog("wizard installationStarted error: " + e);
    }
}

function finceptWizardFinishedPage() {
    if (!finceptUpdate.upgradeDir || installer.status === QInstaller.Success)
        return;
    finceptSetFinishedText(finceptFailureText(finceptUpdate.upgradeDir, false));
}

// After a failed update, Finish reopens the previous version (the click gives
// this process the foreground, which the new window inherits).
function finceptUpdateFinishClicked() {
    finceptUnprotectListAfterExit();
    if (finceptUpdate.relaunchOnFinish)
        finceptStartTerminal(finceptUpdate.relaunchOnFinish);
}

// Starts the terminal from this (never elevated) GUI process on the profile it
// ran on, with its install folder as working directory (like the shortcut).
function finceptStartTerminal(exe) {
    var dir = exe.substring(0, exe.lastIndexOf("\\"));
    var profile = finceptUpdate.profile;
    var args = profile && profile.toLowerCase() !== "default" ? ["--profile", profile] : [];
    var ok = installer.executeDetached(exe, args, dir);
    finceptLog("started " + exe + " " + args.join(" ") + ": " + ok);
}

// installscript.qs made a restored installation's components.xml read-only
// so that this installer's exit cannot overwrite it; clear the flag a few
// seconds after we are gone. Best effort: if the window is closed another
// way the flag stays, which nothing depends on.
function finceptUnprotectListAfterExit() {
    var list = installer.value("FinceptProtectedList");
    if (!list || /[&|<>^%!(),;="]/.test(list))
        return;
    installer.executeDetached(finceptSystem32() + "\\cmd.exe",
                              ["/d", "/c", "ping", "-n", "5", "127.0.0.1", ">nul", "&",
                               finceptSystem32() + "\\attrib.exe", "-R", list]);
}
