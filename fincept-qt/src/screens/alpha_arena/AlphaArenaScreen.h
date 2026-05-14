#pragma once
// AlphaArenaScreen — viewer over AlphaArenaEngine.
//
// Engine-side (AlphaArenaEngine, ModelDispatcher, OrderRouter, …) drives the
// competition. The screen subscribes to engine signals while visible, posts
// user intents (create / start / kill / halt), and delegates rendering to
// per-tab panels (LeaderboardPanel / ModelChatPanel / PositionsPanel /
// HitlPanel / RiskPanel / AuditPanel).
//
// Lifecycle (P3):
//   * showEvent  → connect engine signals + first refresh
//   * hideEvent  → disconnect engine signals; engine keeps running
//
// Reference: .grill-me/alpha-arena-production-refactor.md §Phase 7.

#include "screens/common/IStatefulScreen.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMetaObject>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVector>
#include <QWidget>

namespace fincept::screens::alpha_arena {

class LeaderboardPanel;
class ModelChatPanel;
class PositionsPanel;
class HitlPanel;
class RiskPanel;
class AuditPanel;

/// Lightweight descriptor for a user-configured LLM model entry.
struct ArenaModelEntry {
    QString display_name;
    QString provider;
    QString model_id;
    QString api_key;
    QString base_url;
    QString profile_id;
};

class AlphaArenaScreen : public QWidget, public IStatefulScreen {
    Q_OBJECT
  public:
    explicit AlphaArenaScreen(QWidget* parent = nullptr);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return QStringLiteral("alpha_arena"); }
    int state_version() const override { return 2; }

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private slots:
    void on_create_clicked();
    void on_start_clicked();
    void on_force_tick_clicked();
    void on_kill_all_clicked();
    void on_live_mode_toggle_clicked();
    void on_right_tab_changed(int idx);

    void on_engine_tick(int seq);
    void on_engine_decision_received(QString decision_id, QString agent_id);
    void on_engine_hitl_requested(QString approval_id, QString agent_id, QString summary);
    void on_engine_circuit_open(QString agent_id, QString reason);
    void on_engine_status_changed(QString competition_id, QString status);
    void on_engine_crash_recovery(QStringList competition_ids);

    void on_hitl_resolved(QString approval_id, bool approved);

    void on_countdown_tick();

  private:
    void build_ui();
    QWidget* build_header();
    QWidget* build_create_panel();
    QWidget* build_main();
    QWidget* build_right_stack();

    void connect_engine_signals();
    void disconnect_engine_signals();

    void populate_model_list();
    void apply_competition_id(const QString& id);
    void update_status_badge(const QString& status);
    void update_venue_badge();
    void show_disclaimer_if_live(bool live);
    void refresh_all_panels();

    // State.
    QString competition_id_;
    QString competition_status_;
    bool live_mode_engaged_ = false;
    QVector<ArenaModelEntry> model_entries_;

    // Engine connection handles (so we can disconnect cleanly in hideEvent).
    QVector<QMetaObject::Connection> engine_conns_;

    // Header.
    QLabel* venue_badge_ = nullptr;
    QLabel* status_badge_ = nullptr;
    QLabel* tick_label_ = nullptr;
    QLabel* countdown_label_ = nullptr;
    QPushButton* force_tick_btn_ = nullptr;
    QPushButton* kill_all_btn_ = nullptr;
    QPushButton* live_mode_btn_ = nullptr;
    QSpinBox* hot_cadence_spin_ = nullptr;
    QLabel* disclaimer_ = nullptr;

    // Create panel.
    QWidget* create_panel_ = nullptr;
    QLineEdit* comp_name_ = nullptr;
    QComboBox* comp_mode_ = nullptr;
    QRadioButton* venue_paper_ = nullptr;
    QRadioButton* venue_hl_ = nullptr;
    QRadioButton* venue_us_eq_ = nullptr;     // disabled — S2 stub
    QDoubleSpinBox* comp_capital_ = nullptr;
    QSpinBox* comp_cadence_ = nullptr;
    QListWidget* comp_instruments_ = nullptr;
    QListWidget* model_list_ = nullptr;
    QPushButton* create_btn_ = nullptr;
    QPushButton* start_btn_ = nullptr;

    // Main split panels.
    LeaderboardPanel* leaderboard_ = nullptr;
    ModelChatPanel* modelchat_ = nullptr;
    PositionsPanel* positions_ = nullptr;
    HitlPanel* hitl_ = nullptr;
    RiskPanel* risk_ = nullptr;
    AuditPanel* audit_ = nullptr;

    QStackedWidget* right_stack_ = nullptr;
    QList<QPushButton*> right_tab_btns_;

    // Visibility-driven countdown timer.
    QTimer* countdown_timer_ = nullptr;
};

} // namespace fincept::screens::alpha_arena

namespace fincept::screens {
// Backwards alias so the rest of the app's screen registry still finds us.
using AlphaArenaScreen = fincept::screens::alpha_arena::AlphaArenaScreen;
} // namespace fincept::screens
