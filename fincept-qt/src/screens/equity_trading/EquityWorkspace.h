#pragma once
// EquityWorkspace — the Equity Trading screen's body: ONE row of three zones that
// re-flows with the window (rules: EquityLayoutPlan.h).
//
//   ┌ LEFT ────────┬ CENTRE ─────────────────────┬ RIGHT ───────────┐
//   │ watchlist    │ chart                       │ quote / depth    │
//   │ (or a rail)  ├─────────────────────────────┤──────────────────│
//   │              │ blotter                     │ order ticket     │
//   └──────────────┴─────────────────────────────┴──────────────────┘
//
// The screen builds each panel INSIDE its host (watchlist_host(), chart_host(), …)
// and never moves it again: every width class is the same widget tree with zones
// shown, hidden or resized — no re-parenting, so no re-styling on a resize.
//   Wide     all three zones.
//   Medium   narrower watchlist; the depth ladder collapses (fewer levels).
//   Narrow   the watchlist folds to a rail; the right column is a drawer the
//            screen opens for BUY / SELL / TICKET (Esc or ✕ closes it).
//   Compact  zone tabs (CHART · BLOTTER / WATCHLIST / TICKET), one zone at a time.
// Splitter positions are remembered per width class (save_layout / restore_layout);
// minimum sizes come from the font, so nothing is ever squeezed to a sliver.

#include "screens/equity_trading/EquityLayoutPlan.h"

#include <QHash>
#include <QList>
#include <QVariantMap>
#include <QWidget>

class QLabel;
class QSplitter;
class QTabBar;
class QToolButton;

namespace fincept::screens::equity {

class EquityWorkspace : public QWidget {
    Q_OBJECT
  public:
    enum class Zone { Centre = 0, Watchlist = 1, Ticket = 2 };

    explicit EquityWorkspace(QWidget* parent = nullptr);

    // Hosts (QVBoxLayout, no margins) the screen builds its panels into.
    QWidget* watchlist_host() const { return watchlist_host_; }
    QWidget* chart_host() const { return chart_host_; }
    QWidget* blotter_host() const { return blotter_host_; }
    QWidget* quote_host() const { return quote_host_; }
    QWidget* ticket_host() const { return ticket_host_; }
    /// Adds `w` (already a child of `host`) to the host's layout; `stretch` as QBoxLayout.
    static void place(QWidget* host, QWidget* w, int stretch = 0);

    EqWidthClass width_class() const { return plan_.cls; }
    const EqLayoutPlan& plan() const { return plan_; }

    /// The width the watchlist wants for its content (default width in Wide).
    void set_watchlist_preferred_width(int px);
    /// The width the right column needs at least (the ticket's own minimum).
    void set_right_minimum_width(int px);

    /// Left zone folded to the rail. A user choice is remembered for the current
    /// width class only (the class default returns in the others).
    void set_watchlist_collapsed(bool collapsed);
    bool watchlist_collapsed() const { return rail_shown_; }

    /// Make the order ticket reachable: opens the drawer (Narrow), switches the zone
    /// tab (Compact); a no-op where the ticket is always on screen.
    void reveal_ticket();
    /// Close the Narrow drawer (no-op otherwise).
    void close_drawer();
    bool drawer_open() const { return drawer_open_; }
    /// The ticket column is on screen right now.
    bool ticket_on_screen() const;
    /// Compact: show one zone. Elsewhere: Watchlist expands a folded rail.
    void reveal_zone(Zone zone);

    /// Splitter sizes per width class, rail choices, the compact tab.
    QVariantMap save_layout() const;
    void restore_layout(const QVariantMap& state);

  signals:
    void width_class_changed(fincept::screens::equity::EqWidthClass cls);
    void watchlist_collapsed_changed(bool collapsed);
    void drawer_changed(bool open);

  protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

  private:
    void apply(bool force);              // classify + apply the plan for the current width
    void apply_visibility();             // zones shown for the class / rail / drawer / tab
    void apply_main_sizes();             // saved sizes for the class, else defaults
    void apply_centre_sizes();
    void update_minimums();              // font-derived floors
    void remember_main_sizes();          // user drag -> current class
    void remember_centre_sizes();
    int char_w() const;
    int line_h() const;
    int rail_width() const;
    QList<int> main_minimums() const;
    QString key() const { return eq_width_class_key(plan_.cls); }

    EqLayoutPlan plan_;
    bool classified_ = false;
    bool rail_shown_ = false;
    bool drawer_open_ = false;
    int compact_zone_ = 0;
    int watchlist_pref_ = 0;
    int right_min_ = 0;
    bool applying_ = false;

    QTabBar* zone_tabs_ = nullptr;
    QSplitter* main_ = nullptr;
    QWidget* left_zone_ = nullptr;
    QWidget* rail_ = nullptr;
    QWidget* watchlist_host_ = nullptr;
    QSplitter* centre_ = nullptr;
    QWidget* chart_host_ = nullptr;
    QWidget* blotter_host_ = nullptr;
    QWidget* right_zone_ = nullptr;
    QWidget* drawer_bar_ = nullptr;
    QLabel* drawer_title_ = nullptr;
    QToolButton* drawer_close_ = nullptr;
    QWidget* quote_host_ = nullptr;
    QWidget* ticket_host_ = nullptr;

    // Per width class (eq_width_class_key): last user sizes and rail choice.
    QHash<QString, QList<int>> main_sizes_;
    QHash<QString, QList<int>> centre_sizes_;
    QHash<QString, bool> rail_choice_;
};

} // namespace fincept::screens::equity
