#pragma once

#include "screens/IStatefulScreen.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QTimer>
#include <QWidget>
#include <QVector>

namespace fincept::screens {

/// Lightweight descriptor for a user-configured LLM model entry.
/// Populated from LlmConfigRepository providers and LlmProfileRepository profiles.
struct ArenaModelEntry {
    QString display_name; // shown in the list widget
    QString provider;     // e.g. "openai", "anthropic"
    QString model_id;     // e.g. "gpt-4o", "claude-sonnet-4-5-20250514"
    QString api_key;
    QString base_url;
    QString profile_id;   // non-empty when sourced from a named profile
};

/// Alpha Arena — AI trading competition platform.
/// Pits multiple LLM agents against each other in crypto or prediction markets.
/// Features: competition creation, leaderboard, performance charts, AI decisions,
/// HITL approval, sentiment analysis, portfolio metrics, grid strategies, research.
class AlphaArenaScreen : public QWidget, public IStatefulScreen {
    Q_OBJECT
  public:
    explicit AlphaArenaScreen(QWidget* parent = nullptr);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "alpha_arena"; }
    int state_version() const override { return 1; }

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private slots:
    void on_create_competition();
    void on_run_cycle();
    void on_toggle_auto_run();
    void on_reset();
    void on_right_tab_changed(int tab);
    void on_competition_type_changed(int index);
    void on_refresh_leaderboard();
    void on_past_competitions_toggle();
    void on_past_competition_clicked(QListWidgetItem* item);

  private:
    void setup_ui();
    QWidget* create_header();
    QWidget* create_create_panel();
    QWidget* create_controls_bar();
    QWidget* create_main_content();
    QWidget* create_leaderboard_panel();
    QWidget* create_right_panel();
    QWidget* create_past_competitions_panel();
    QWidget* create_status_bar();

    void run_python_action(const QString& action, const QJsonObject& params);
    void update_leaderboard(const QJsonArray& entries);
    void update_decisions(const QJsonArray& decisions);
    void load_past_competitions();
    void set_loading(bool loading);

    /// Repopulate model_list_ from configured LLM providers and profiles.
    /// Called on construction and whenever the screen becomes visible.
    void populate_model_list();

    // State
    QString competition_id_;
    QString competition_status_; // created, running, paused, completed, failed
    int cycle_count_ = 0;
    bool is_auto_running_ = false;
    bool loading_ = false;

    // Model entries sourced from LlmConfigRepository / LlmProfileRepository.
    // Index matches model_list_ row order.
    QVector<ArenaModelEntry> model_entries_;

    // Timer for auto-run
    QTimer* auto_timer_ = nullptr;

    // UI — Header
    QLabel* status_badge_ = nullptr;
    QLabel* cycle_label_ = nullptr;
    QLabel* price_label_ = nullptr;

    // UI — Create panel
    QWidget* create_panel_ = nullptr;
    QLineEdit* comp_name_ = nullptr;
    QComboBox* comp_type_ = nullptr;
    QComboBox* comp_symbol_ = nullptr;
    QComboBox* comp_mode_ = nullptr;
    QDoubleSpinBox* comp_capital_ = nullptr;
    QSpinBox* comp_interval_ = nullptr;
    QListWidget* model_list_ = nullptr;
    QPushButton* create_btn_ = nullptr;

    // UI — Controls
    QPushButton* run_btn_ = nullptr;
    QPushButton* auto_btn_ = nullptr;
    QPushButton* reset_btn_ = nullptr;
    QLabel* interval_label_ = nullptr;

    // UI — Leaderboard
    QTableWidget* leaderboard_table_ = nullptr;
    QLabel* leaderboard_cycle_ = nullptr;

    // UI — Right panel (7 tabs)
    QStackedWidget* right_stack_ = nullptr;
    QList<QPushButton*> right_tab_btns_;

    // Right tab: Decisions
    QListWidget* decisions_list_ = nullptr;

    // Right tab: Metrics
    QTableWidget* metrics_table_ = nullptr;

    // Right tab: other content
    QTextEdit* hitl_content_ = nullptr;
    QTextEdit* sentiment_content_ = nullptr;
    QTextEdit* grid_content_ = nullptr;
    QTextEdit* research_content_ = nullptr;
    QTextEdit* broker_content_ = nullptr;

    // UI — Past competitions
    QWidget* past_panel_ = nullptr;
    QListWidget* past_list_ = nullptr;
    bool past_visible_ = false;

    // UI — Status bar
    QLabel* status_comp_ = nullptr;
    QLabel* status_models_ = nullptr;
    QLabel* status_info_ = nullptr;
};

} // namespace fincept::screens
