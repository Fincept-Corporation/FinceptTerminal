// controlscript.qs — Fincept Terminal QtIFW Controller script.
//
// Wizard-page automation for HEADLESS / COMMAND-LINE invocations only, e.g.
//   FinceptMaintenanceTool.exe purge --confirm-command --accept-messages --default-answer
//   FinceptTerminal-<v>-setup.exe install --confirm-command --default-answer
// Without this, IFW would block on each wizard page even when --confirm-command
// is set, because some pages (Introduction, ReadyForInstallation, Finished)
// only auto-advance for the *installer* path, not the uninstaller path.
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
// --confirm-command / CLI, false in the GUI).
//
// References:
//   https://doc.qt.io/qtinstallerframework/noninteractive.html
//   https://doc.qt.io/qtinstallerframework/scripting-installer.html  (isCommandLineInstance)

function Controller() {
    // No-op constructor. Page callbacks below are what actually drive
    // headless flows. Defining a Controller at all is what makes IFW
    // load this file.
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

Controller.prototype.IntroductionPageCallback = function() {
    if (interactiveGui()) return;
    if (gui && gui.clickButton)
        gui.clickButton(buttons.NextButton);
};

Controller.prototype.TargetDirectoryPageCallback = function() {
    if (interactiveGui()) return;   // GUI: let the user choose the install path
    if (gui && gui.clickButton)
        gui.clickButton(buttons.NextButton);
};

Controller.prototype.ComponentSelectionPageCallback = function() {
    if (interactiveGui()) return;
    if (gui && gui.clickButton)
        gui.clickButton(buttons.NextButton);
};

Controller.prototype.LicenseAgreementPageCallback = function() {
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

Controller.prototype.ReadyForInstallationPageCallback = function() {
    if (interactiveGui()) return;
    if (gui && gui.clickButton)
        gui.clickButton(buttons.CommitButton);
};

Controller.prototype.PerformInstallationPageCallback = function() {
    if (interactiveGui()) return;
    if (gui && gui.clickButton)
        gui.clickButton(buttons.CommitButton);
};

Controller.prototype.FinishedPageCallback = function() {
    if (interactiveGui()) return;
    if (gui && gui.clickButton)
        gui.clickButton(buttons.FinishButton);
};
