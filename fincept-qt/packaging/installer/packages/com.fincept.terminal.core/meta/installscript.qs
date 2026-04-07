// installscript.qs -- Fincept Terminal QtIFW component script
// Handles: shortcuts on install, user data cleanup on uninstall
//
// Data locations cleaned on uninstall (when user confirms):
//   Windows:  %LOCALAPPDATA%\com.fincept.terminal\
//   macOS:    ~/Library/Application Support/com.fincept.terminal/
//   Linux:    ~/.local/share/com.fincept.terminal/
//   QSettings (registry/plist/conf), credentials, temp files

function Component()
{
    installer.installationFinished.connect(this, Component.prototype.installationFinished);

    if (installer.isUninstaller()) {
        installer.uninstallationStarted.connect(this, Component.prototype.uninstallationStarted);
    }
}

// ---------------------------------------------------------------------------
// Install: create platform shortcuts
// ---------------------------------------------------------------------------

Component.prototype.createOperations = function()
{
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

Component.prototype.installationFinished = function()
{
    // Reserved for future post-install actions (URL handlers, file associations)
};

// ---------------------------------------------------------------------------
// Uninstall: prompt for user data removal, then clean up
// ---------------------------------------------------------------------------

Component.prototype.uninstallationStarted = function()
{
    var result = QMessageBox.question(
        "fincept.uninstall.data",
        "Remove User Data?",
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

    if (result === QMessageBox.Yes) {
        cleanUserData();
    }
};

// ---------------------------------------------------------------------------
// Data cleanup implementation
// ---------------------------------------------------------------------------

function cleanUserData()
{
    // 1. Remove main data directory
    removeDataRoot();

    // 2. Remove legacy data directories (Windows only)
    removeLegacyDirs();

    // 3. Remove QSettings (registry / plist / conf)
    removeSettings();

    // 4. Remove credentials from OS secure storage
    removeCredentials();

    // 5. Clean temp files
    removeTempFiles();
}

// -- 1. Main data root --

function removeDataRoot()
{
    var dataRoot = "";

    if (systemInfo.kernelType === "winnt") {
        dataRoot = installer.environmentVariable("LOCALAPPDATA") +
                   "/com.fincept.terminal";
    } else if (systemInfo.kernelType === "darwin") {
        dataRoot = installer.environmentVariable("HOME") +
                   "/Library/Application Support/com.fincept.terminal";
    } else {
        dataRoot = installer.environmentVariable("HOME") +
                   "/.local/share/com.fincept.terminal";
    }

    if (installer.fileExists(dataRoot)) {
        removeDirectoryRecursive(dataRoot);
    }
}

// -- 2. Legacy directories (pre-v4 Windows paths) --

function removeLegacyDirs()
{
    if (systemInfo.kernelType !== "winnt") return;

    var localAppData = installer.environmentVariable("LOCALAPPDATA");

    var legacy1 = localAppData + "/Fincept/FinceptTerminal";
    if (installer.fileExists(legacy1)) {
        removeDirectoryRecursive(legacy1);
    }

    var legacy2 = localAppData + "/FinceptTerminal";
    if (installer.fileExists(legacy2)) {
        removeDirectoryRecursive(legacy2);
    }

    // Remove empty parent directory
    var finceptParent = localAppData + "/Fincept";
    if (installer.fileExists(finceptParent)) {
        installer.execute("cmd", ["/c", "rmdir", finceptParent.replace(/\//g, "\\")]);
    }
}

// -- 3. QSettings --

function removeSettings()
{
    if (systemInfo.kernelType === "winnt") {
        // Registry: HKCU\Software\Fincept\FinceptTerminal
        installer.execute("reg", ["delete",
            "HKCU\\Software\\Fincept\\FinceptTerminal", "/f"]);
        // Registry: HKCU\Software\Fincept\FinceptTerminal-Secure
        installer.execute("reg", ["delete",
            "HKCU\\Software\\Fincept\\FinceptTerminal-Secure", "/f"]);
        // Remove empty parent key if no other Fincept apps
        installer.execute("reg", ["delete",
            "HKCU\\Software\\Fincept", "/f"]);

    } else if (systemInfo.kernelType === "darwin") {
        var plistPath = installer.environmentVariable("HOME") +
            "/Library/Preferences/com.fincept.FinceptTerminal.plist";
        if (installer.fileExists(plistPath)) {
            installer.execute("rm", ["-f", plistPath]);
        }

    } else {
        // Linux: QSettings .conf files
        var home = installer.environmentVariable("HOME");
        var confPath = home + "/.config/Fincept/FinceptTerminal.conf";
        if (installer.fileExists(confPath)) {
            installer.execute("rm", ["-f", confPath]);
        }

        var secureConf = home + "/.config/Fincept/FinceptTerminal-Secure.conf";
        if (installer.fileExists(secureConf)) {
            installer.execute("rm", ["-f", secureConf]);
        }

        // Remove empty parent dir
        var configDir = home + "/.config/Fincept";
        if (installer.fileExists(configDir)) {
            installer.execute("rmdir", [configDir]);
        }
    }
}

// -- 4. Credentials --

function removeCredentials()
{
    if (systemInfo.kernelType === "winnt") {
        // Windows Credential Manager: enumerate and delete FinceptTerminal/* entries
        // cmdkey doesn't support wildcards, so use PowerShell to enumerate + delete
        installer.execute("powershell", [
            "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command",
            "$targets = cmdkey /list 2>&1 | Select-String 'FinceptTerminal/' | " +
            "ForEach-Object { ($_ -replace '.*Target:\\s*', '').Trim() }; " +
            "foreach ($t in $targets) { cmdkey /delete:$t 2>$null }"
        ]);

    } else if (systemInfo.kernelType === "darwin") {
        // macOS Keychain: delete all entries with service "com.fincept.terminal"
        // security delete-generic-password only removes one at a time, so loop
        installer.execute("/bin/bash", ["-c",
            "while /usr/bin/security delete-generic-password " +
            "-s 'com.fincept.terminal' 2>/dev/null; do :; done"
        ]);
    }

    // Linux: credentials are stored in FinceptTerminal-Secure.conf (already deleted above)
}

// -- 5. Temp files --

function removeTempFiles()
{
    if (systemInfo.kernelType === "winnt") {
        var tempDir = installer.environmentVariable("TEMP");
        installer.execute("cmd", ["/c",
            "del /q \"" + tempDir.replace(/\//g, "\\") + "\\fincept_*\" 2>nul"]);

    } else {
        installer.execute("/bin/bash", ["-c",
            "rm -f /tmp/fincept_* 2>/dev/null; " +
            "rm -f \"${TMPDIR:-/tmp}\"/fincept_* 2>/dev/null"
        ]);
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

function removeDirectoryRecursive(path)
{
    if (systemInfo.kernelType === "winnt") {
        installer.execute("cmd", ["/c", "rmdir", "/s", "/q",
            path.replace(/\//g, "\\")]);
    } else {
        installer.execute("/bin/rm", ["-rf", path]);
    }
}
