#pragma once
// EnterprisePromo — the single place the terminal talks about Fincept Terminal
// Enterprise (the private, closed-source edition).
//
// This open-source build is AGPL-3.0 and Fincept no longer sells a commercial
// licence for it; commercial, institutional and academic use is served by
// Enterprise instead. Every surface that mentions that — the toolbar UPGRADE
// button, the startup dialog, the pricing screen banner — reads its copy and
// its URLs from here so pricing lives in exactly one file.

#include <QDialog>
#include <QEvent>
#include <QString>

class QCheckBox;
class QLabel;
class QPushButton;

namespace fincept::ui {

namespace enterprise {

/// Canonical destinations. Keep in sync with README.md and
/// docs/COMMERCIAL_LICENSE.md — those three are the published surfaces.
QString site_url();
QString pricing_url();
QString comparison_url();
QString signup_url();
QString demo_url();

/// Opens `url` in the user's browser.
void open_url(const QString& url);

} // namespace enterprise

/// Modal "Upgrade to Enterprise" dialog. Shown once per launch at startup
/// (unless suppressed) and on demand from the toolbar UPGRADE button.
class UpgradeDialog : public QDialog {
    Q_OBJECT
  public:
    explicit UpgradeDialog(QWidget* parent = nullptr);

    /// False once the user has ticked "Don't show this again".
    static bool startup_prompt_enabled();
    static void set_startup_prompt_enabled(bool enabled);

    /// Non-blocking show, heap-allocated with WA_DeleteOnClose. Used by both
    /// the toolbar button and the startup hook so neither spins a nested
    /// event loop inside a timer callback.
    static void show_now(QWidget* parent);

    /// Startup entry point. No-ops when the user has suppressed the prompt or
    /// when the platform has no window system (headless CI, --smoke-test),
    /// where a modal would hang the run.
    static void maybe_show_at_startup(QWidget* parent);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void retranslateUi();

    QLabel* title_label_ = nullptr;
    QLabel* kicker_label_ = nullptr;
    QLabel* pitch_label_ = nullptr;
    QLabel* price_label_ = nullptr;
    QLabel* oss_note_label_ = nullptr;
    QList<QLabel*> feature_labels_;
    QCheckBox* dont_show_check_ = nullptr;
    QPushButton* primary_btn_ = nullptr;
    QPushButton* compare_btn_ = nullptr;
    QPushButton* later_btn_ = nullptr;
};

/// Inline Enterprise banner for embedding at the top of a screen (currently
/// the pricing screen). Caller takes ownership via the returned widget's
/// parent, as usual for Qt.
QWidget* make_enterprise_banner(QWidget* parent = nullptr);

} // namespace fincept::ui
