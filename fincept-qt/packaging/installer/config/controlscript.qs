// controlscript.qs — Fincept Terminal QtIFW Controller script.
//
// Wizard-page automation for headless invocations (e.g.
// `FinceptMaintenanceTool.exe purge --confirm-command --accept-messages
// --default-answer`). Without this, IFW would block on each wizard page
// even when --confirm-command is set, because some pages (Introduction,
// ReadyForInstallation, Finished) only auto-advance for the *installer*
// path, not the uninstaller path.
//
// Reference: https://doc.qt.io/qtinstallerframework/noninteractive.html

function Controller() {
    // No-op constructor. Page callbacks below are what actually drive
    // headless flows. Defining a Controller at all is what makes IFW
    // load this file.
}

// ── Headless install path (rare in maintenance-tool context, but harmless) ──

Controller.prototype.IntroductionPageCallback = function() {
    if (gui && gui.clickButton)
        gui.clickButton(buttons.NextButton);
};

Controller.prototype.TargetDirectoryPageCallback = function() {
    if (gui && gui.clickButton)
        gui.clickButton(buttons.NextButton);
};

Controller.prototype.ComponentSelectionPageCallback = function() {
    if (gui && gui.clickButton)
        gui.clickButton(buttons.NextButton);
};

Controller.prototype.LicenseAgreementPageCallback = function() {
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
    if (gui && gui.clickButton)
        gui.clickButton(buttons.CommitButton);
};

Controller.prototype.PerformInstallationPageCallback = function() {
    if (gui && gui.clickButton)
        gui.clickButton(buttons.CommitButton);
};

Controller.prototype.FinishedPageCallback = function() {
    if (gui && gui.clickButton)
        gui.clickButton(buttons.FinishButton);
};
