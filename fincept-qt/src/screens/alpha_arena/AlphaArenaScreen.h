#pragma once
// AlphaArenaScreen — nof1-style dashboard over fincept::arena::ArenaEngine.
// Pure viewer: engine runs even when this screen is closed. Screen id
// "alpha_arena" (CommandBar/ToolBar/WindowFrame registrations unchanged).
// Terminal layout: header + mids ticker/cadence strip, then a splitter body
// [equity chart | agent board] over the always-visible panel grid.
#include "screens/common/IStatefulScreen.h"
#include "services/alpha_arena/ArenaTypes.h"

#include <QHash>
#include <QMetaObject>
#include <QVariantMap>
#include <QVector>
#include <QWidget>

class QComboBox; class QLabel; class QPushButton; class QSpinBox; class QStackedWidget;
class QTimer;

namespace fincept::screens::alpha_arena {

class ArenaPanelGrid; class EquityCurveWidget; class LeaderboardCards;

class AlphaArenaScreen : public QWidget, public IStatefulScreen {
    Q_OBJECT
  public:
    explicit AlphaArenaScreen(QWidget* parent = nullptr);

    // IStatefulScreen (DockScreenRouter persists/restores lightweight UI state).
    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return QStringLiteral("alpha_arena"); }
    int state_version() const override { return 3; }

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    QWidget* build_header();
    QWidget* build_ticker_strip();       // mids ticker + cadence editor
    QWidget* build_empty_state();
    QWidget* build_dashboard();
    void connect_engine();
    void disconnect_engine();
    void reload_competitions();          // switcher combo
    void show_competition(const QString& id);
    void refresh_dashboard();            // chart + board + panels (full, per round)
    void update_ticker();                // mids ticker (every marks tick)
    void rebuild_board_light();          // board from cached agents + live account_view
    void on_new_competition();
    void update_badges(const QString& status);
    void maybe_offer_crash_recovery();   // Resume-or-End prompt (signal + showEvent)

    QString competition_id_;
    QVector<QMetaObject::Connection> conns_;
    QTimer* countdown_ = nullptr;
    // Light-refresh caches (filled by refresh_dashboard; used on marks ticks).
    QVector<fincept::arena::AgentRow> agents_cache_;
    QHash<QString, qint64> tokens_cache_;
    QHash<QString, double> last_mids_;   // previously displayed mids (ticker arrows)
    double cap_ = 0;
    // Header
    QLabel* venue_badge_ = nullptr;
    QLabel* status_badge_ = nullptr;
    QLabel* round_label_ = nullptr;
    QLabel* countdown_label_ = nullptr;
    QLabel* disclaimer_ = nullptr;
    QLabel* ticker_ = nullptr;
    QSpinBox* cadence_spin_ = nullptr;
    QPushButton* pause_btn_ = nullptr;
    QPushButton* force_btn_ = nullptr;
    QPushButton* kill_btn_ = nullptr;
    QComboBox* switcher_ = nullptr;
    // Body
    QStackedWidget* body_ = nullptr;     // 0 = empty state, 1 = dashboard
    EquityCurveWidget* chart_ = nullptr;
    LeaderboardCards* cards_ = nullptr;
    ArenaPanelGrid* grid_ = nullptr;
};

} // namespace fincept::screens::alpha_arena

namespace fincept::screens {
// Backwards alias so the rest of the app's screen registry still finds us.
using AlphaArenaScreen = fincept::screens::alpha_arena::AlphaArenaScreen;
} // namespace fincept::screens
