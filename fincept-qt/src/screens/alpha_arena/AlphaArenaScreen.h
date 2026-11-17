#pragma once
// AlphaArenaScreen — nof1-style dashboard over fincept::arena::ArenaEngine.
// Pure viewer: engine runs even when this screen is closed. Screen id
// "alpha_arena" (CommandBar/ToolBar/WindowFrame registrations unchanged).
// Terminal layout: header + mids ticker/cadence strip, then a splitter body
// [equity chart | agent board] over the always-visible panel grid.
#include "screens/common/ISectionLinked.h"
#include "screens/common/IStatefulScreen.h"
#include "services/alpha_arena/ArenaTypes.h"

#include <QHash>
#include <QMetaObject>
#include <QPointF>
#include <QVariantMap>
#include <QVector>
#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTimer;

namespace fincept::ui {
class EmptyState;
}

namespace fincept::screens::alpha_arena {

class ArenaPanelGrid;
class EquityCurveWidget;
class LeaderboardCards;

class AlphaArenaScreen : public QWidget, public IStatefulScreen, public fincept::screens::ISectionLinked {
    Q_OBJECT
  public:
    explicit AlphaArenaScreen(QWidget* parent = nullptr);

    // IStatefulScreen (DockScreenRouter persists/restores lightweight UI state).
    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return QStringLiteral("alpha_arena"); }
    int state_version() const override { return 3; }

    // ISectionLinked — no tabs (the panel grid is always visible): "competition:<id>" (picked in
    // the switcher as a user does), "new" (NEW COMPETITION: the wizard, which asks before anything
    // runs). `focus` = a competition id (with an empty section).
    bool open_section(const QString& section, const QString& focus) override;

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    QWidget* build_header();
    QWidget* build_ticker_strip(); // mids ticker + cadence editor
    QWidget* build_empty_state();
    void refresh_start_page(); // start page note: which models are ready (on show, no competition)
    QWidget* build_dashboard();
    void connect_engine();
    void disconnect_engine();
    void reload_competitions(); // switcher combo
    void show_competition(const QString& id);
    void refresh_dashboard();   // chart + board + panels (full, per round)
    void update_ticker();       // mids ticker (every marks tick)
    void render_ticker();       // ticker text from last_mids_ / ticker_dir_ (also on theme change)
    void rebuild_board_light(); // board from cached agents + live account_view
    void on_new_competition();
    void on_pause_resume();
    void on_kill_all();
    void set_killing(bool on); // KILL ALL in flight: controls disabled, "KILLING…"
    void update_badges(const QString& status);
    void maybe_offer_crash_recovery(); // Resume-or-End prompt (signal + showEvent)
    void restore_pending_approvals();  // re-show the engine's queued HITL approvals

    QString competition_id_;
    QString competition_status_; // status of the viewed competition (badges / RESUME)
    QVector<QMetaObject::Connection> conns_;
    QTimer* countdown_ = nullptr;
    bool killing_ = false;
    // Light-refresh caches (filled by refresh_dashboard; used on marks ticks).
    QVector<fincept::arena::AgentRow> agents_cache_;
    QHash<QString, qint64> tokens_cache_;
    QHash<QString, QVector<QPointF>> series_cache_; // agent → equity snapshots (ts, equity)
    QHash<QString, double> last_mids_;              // previously displayed mids (ticker arrows)
    QHash<QString, int> ticker_dir_;                // per coin: +1 up, -1 down, 0 flat vs the tick before
    double cap_ = 0;
    // live_mode of the competition currently on screen — gates the extra
    // confirmation on header actions that trigger real order flow.
    bool live_mode_ = false;
    // Header
    QLabel* venue_badge_ = nullptr;
    QLabel* status_badge_ = nullptr;
    QLabel* round_label_ = nullptr;
    QLabel* countdown_label_ = nullptr;
    QLabel* disclaimer_ = nullptr;
    QLabel* ticker_ = nullptr;
    QSpinBox* cadence_spin_ = nullptr;
    QPushButton* cadence_apply_btn_ = nullptr; // enabled with PAUSE/FORCE/KILL (update_badges)
    QPushButton* pause_btn_ = nullptr;
    QPushButton* force_btn_ = nullptr;
    QPushButton* kill_btn_ = nullptr;
    QComboBox* switcher_ = nullptr;
    // Body
    QStackedWidget* body_ = nullptr; // 0 = empty state, 1 = dashboard
    fincept::ui::EmptyState* start_state_ = nullptr; // page 0: no competition yet
    QString start_note_;                             // its note (set only when it changes)
    EquityCurveWidget* chart_ = nullptr;
    LeaderboardCards* cards_ = nullptr;
    ArenaPanelGrid* grid_ = nullptr;
};

} // namespace fincept::screens::alpha_arena

namespace fincept::screens {
// Backwards alias so the rest of the app's screen registry still finds us.
using AlphaArenaScreen = fincept::screens::alpha_arena::AlphaArenaScreen;
} // namespace fincept::screens
