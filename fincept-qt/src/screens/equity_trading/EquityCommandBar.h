#pragma once
// EquityCommandBar — the Equity Trading screen's command bar, in the look of the
// shared ui::ScreenCommandBar (Markets / Backtesting / Notes):
//
//   [ACCOUNT ▾] [symbol search ……] venue · name · ccy │ $271.40 ▲ +2.31 (+0.86%)  OPEN
//   [● PAPER] ◐ DELAYED 15 MIN · YAHOO  [B BUY] [S SELL] [T TICKET] [A ACCOUNTS] … [⋯]
//
// The leading part holds widgets the shared bar has no slot for (account menu,
// symbol search box, price ribbon, mode chip, connection state); the actions are a
// real ui::ScreenCommandBar embedded at the end, so they paint, collapse into "⋯"
// and carry key-cap hints exactly as on every other dense screen. The screen owns
// the shortcuts; the bar only shows them. Nothing here talks to a service.

#include <QLabel>
#include <QPushButton>
#include <QWidget>

namespace fincept::ui {
class ScreenCommandBar;
}

namespace fincept::screens::equity {

class EquityTickerBar;

/// A label that never widens its row: it asks for its full text's width but
/// accepts any width down to a glyph and an ellipsis, showing the full text when
/// it fits, else the compact form, else the compact form elided. Callers keep the
/// full explanation in the tooltip.
class EquityFitLabel : public QLabel {
  public:
    explicit EquityFitLabel(QWidget* parent = nullptr);
    /// `compact` (optional) is what to show when `full` does not fit.
    void set_status_text(const QString& full, const QString& compact = QString());
    QString full_text() const { return full_; }
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

  protected:
    void resizeEvent(QResizeEvent* e) override;
    void changeEvent(QEvent* e) override;

  private:
    int text_width(const QString& s) const;
    void refit();
    QString full_;
    QString compact_;
};

/// A command-bar button painted from the live theme like ui::ScreenCommandBar's:
/// outlined, a key cap before the label, a chevron when it has a menu. The
/// dynamic property "mode" ("paper" / "live") turns it into the trading-mode chip:
/// PAPER in the SUCCESS state tone, LIVE filled in the DANGER state tone, each with
/// a dot — readable without colour. A property change needs only update().
/// It paints QPushButton::text() ("&&" shown as "&"), so setText() works as usual.
class EquityBarButton : public QPushButton {
  public:
    explicit EquityBarButton(QWidget* parent = nullptr);
    /// setText() with '&' kept literal (no mnemonic) + the accessible name.
    void set_label(const QString& text);
    QString label() const;
    void set_hint(const QString& hint);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

  protected:
    void paintEvent(QPaintEvent* e) override;
    bool event(QEvent* e) override;

  private:
    QString mode() const;
    int hint_width() const;
    int chevron_width() const;
    void refresh_accessible();
    QString hint_;
};

class EquityCommandBar : public QWidget {
    Q_OBJECT
  public:
    explicit EquityCommandBar(QWidget* parent = nullptr);

    EquityBarButton* account_button() const { return account_btn_; }
    /// Inserts the caller's symbol search box after the account button.
    void set_symbol_widget(QWidget* w);
    EquityFitLabel* instrument_label() const { return instrument_; }
    EquityTickerBar* ribbon() const { return ribbon_; }
    /// Venue session chip ("OPEN" / "CLOSED" / "24/7"); full sentence in the tooltip.
    void set_session(const QString& short_text, const QString& full_text, bool open);
    EquityBarButton* mode_button() const { return mode_btn_; }
    EquityFitLabel* connection_label() const { return conn_; }
    fincept::ui::ScreenCommandBar* actions() const { return actions_; }
    /// Actions that keep their room on a narrow bar (the leading items give way
    /// first); everything else collapses into "⋯", which always stays reachable.
    void reserve_actions(const QStringList& ids);

  protected:
    void resizeEvent(QResizeEvent* e) override;

  private:
    void refit_optional();
    void apply_reserve();
    QStringList reserved_;

    EquityBarButton* account_btn_ = nullptr;
    QWidget* symbol_slot_ = nullptr;
    EquityFitLabel* instrument_ = nullptr;
    QWidget* rule_ = nullptr;
    EquityTickerBar* ribbon_ = nullptr;
    QLabel* session_ = nullptr;
    EquityBarButton* mode_btn_ = nullptr;
    EquityFitLabel* conn_ = nullptr;
    fincept::ui::ScreenCommandBar* actions_ = nullptr;
    QString session_state_;
};

} // namespace fincept::screens::equity
