#pragma once
// EnterprisePromo — the single place the terminal makes upgrade offers.
//
//  * Fincept Terminal Enterprise — the private, closed-source edition the team
//    develops daily for funds, family offices and research desks. It is a
//    separate product with its own accounts and billing. Every in-app mention
//    (toolbar, status bar, dialog, dashboard strip, setup showcase, pricing
//    card, About/Help/Contact/Login/Settings/Launchpad, AI chat) reads its copy
//    and URLs from here.
//    In-app copy never quotes Enterprise prices: the app ships monthly while
//    the price list lives on the website, so surfaces link to pricing_url().
//  * The Pro plan of this terminal — billing::open_plans() is the one entry
//    point every "get more credits" surface uses.

#include <QDialog>
#include <QEvent>
#include <QString>
#include <QVector>
#include <QWidget>

class QCheckBox;
class QGraphicsOpacityEffect;
class QHideEvent;
class QLabel;
class QPropertyAnimation;
class QPushButton;
class QShowEvent;
class QTimer;

namespace fincept::ui {

namespace enterprise {

/// Canonical destinations. Keep in sync with README.md and
/// docs/COMMERCIAL_LICENSE.md — those are the published surfaces.
QString site_url();
QString pricing_url();
QString comparison_url();
QString signup_url();
QString demo_url();

/// `base` tagged with `ref=terminal-<placement>` so the website can attribute
/// visits to an in-app placement (toolbar, statusbar, popup, dashboard,
/// pricing, about, setup, ai, ...). Only fincept.in URLs are tagged; any other
/// URL (e.g. the demo booking page) and an empty placement return `base`.
QString url_with_ref(const QString& base, const QString& placement);

/// Opens `url` in the user's browser. Plain and ref-tagged URLs both work.
void open_url(const QString& url);

/// Visual weight of make_cta_button(). Primary is dark text on amber.
enum class CtaStyle { Primary, Outline, Ghost, Link, Quiet };

/// Self-styled call-to-action button that opens `url` (already ref-tagged by
/// the caller) when clicked; an empty `url` leaves the click to the caller.
/// Restyles itself on theme changes.
QPushButton* make_cta_button(const QString& text, const QString& url, CtaStyle style, QWidget* parent = nullptr);

/// Switch a make_cta_button() button to another style (no-op if unchanged).
void set_cta_style(QPushButton* btn, CtaStyle style);

/// Muted one-line mention ("<lead> <link ↗>") whose link opens in the
/// browser. Fill or re-translate it with set_mention_text().
QLabel* make_mention_label(QWidget* parent = nullptr);
void set_mention_text(QLabel* label, const QString& lead, const QString& link_text, const QString& url);

/// Per-release dismissal of an in-app placement (e.g. the dashboard strip):
/// dismissed placements come back with the next release.
bool placement_dismissed(const QString& placement);
void dismiss_placement(const QString& placement);

} // namespace enterprise

namespace billing {

/// Opens Plans & Pricing (Free / Pro / Enterprise) in the window that hosts
/// `origin`, or in the focused window when `origin` is null. Routes through
/// the "billing.plans" action — the same path as the toolbar plan badge and
/// the command palette (Ctrl+Shift+P) → "plans" — so it works from the dock UI
/// and from chat mode.
void open_plans(QWidget* origin = nullptr);

} // namespace billing

/// "Fincept Terminal Enterprise" dialog. Opened on demand (toolbar ENTERPRISE
/// button, Help menu, command palette → "enterprise") and, at most every 72 hours or
/// once per new release, as a reminder scheduled after sign-in/unlock.
class UpgradeDialog : public QDialog {
    Q_OBJECT
  public:
    /// `placement` tags the dialog's links (ref=terminal-<placement>).
    explicit UpgradeDialog(QWidget* parent = nullptr, const QString& placement = QString());

    /// Explicit user request — always shows (single instance: re-raises an
    /// open dialog). Non-blocking: heap-allocated, WA_DeleteOnClose, open().
    static void show_now(QWidget* parent, const QString& placement = QStringLiteral("app"));

    /// Cadence-gated reminder. Shows only when all of these hold:
    ///  * a window system is present (never headless / --smoke-test);
    ///  * the user has not chosen "Don't remind me until the next release"
    ///    for the running version;
    ///  * no UpgradeDialog is open and no other modal is active;
    ///  * `parent`'s window is visible, active, not minimised, not locked, and
    ///    the user is signed in;
    ///  * the running version differs from the one it was last shown in, or
    ///    at least 72 hours have passed since it was last shown.
    static void maybe_show(QWidget* parent);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void retranslateUi();
    void refresh_theme();

    QString placement_;
    QLabel* title_label_ = nullptr;
    QLabel* kicker_label_ = nullptr;
    QLabel* pitch_label_ = nullptr;
    QList<QLabel*> feature_labels_;
    QLabel* pricing_label_ = nullptr;
    QLabel* oss_note_label_ = nullptr;
    QCheckBox* dont_remind_check_ = nullptr;
    QPushButton* primary_btn_ = nullptr;
    QPushButton* compare_btn_ = nullptr;
    QPushButton* later_btn_ = nullptr;
};

/// Self-contained Enterprise showcase: header, an auto-rotating carousel of
/// five slides (data, multi-agent research, desks, controls, built for firms)
/// with dot indicators, and SEE ENTERPRISE / BOOK A DEMO actions. Rotates every
/// ~7 s only while visible. Designed for ~400×560 (the first-run setup
/// screen's right column) and reusable anywhere.
class EnterpriseShowcase : public QWidget {
    Q_OBJECT
  public:
    explicit EnterpriseShowcase(QWidget* parent = nullptr);

    /// Attribution tag for the links (ref=terminal-<placement>); default "setup".
    void set_placement(const QString& placement);

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;

  private:
    struct Slide {
        QString kicker;
        QString title;
        QString body;
    };
    QVector<Slide> slides() const;
    void retranslateUi();
    void refresh_theme();
    void show_slide(int index, bool animate);

    QString placement_ = QStringLiteral("setup");
    int current_ = 0;
    QLabel* header_ = nullptr;
    QWidget* slide_box_ = nullptr;
    QLabel* kicker_ = nullptr;
    QLabel* title_ = nullptr;
    QLabel* body_ = nullptr;
    QLabel* index_ = nullptr;
    QVector<QPushButton*> dots_;
    QPushButton* primary_btn_ = nullptr;
    QPushButton* demo_btn_ = nullptr;
    QLabel* note_ = nullptr;
    QTimer* rotate_timer_ = nullptr;
    QGraphicsOpacityEffect* fade_ = nullptr;
    QPropertyAnimation* fade_anim_ = nullptr;
};

/// Factory used by the first-run setup screen (and any other host).
QWidget* make_enterprise_showcase(QWidget* parent = nullptr);

} // namespace fincept::ui
