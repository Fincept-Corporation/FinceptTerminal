#pragma once
// NewCompetitionWizard — 2-step modal (spec §6.2). Step 1: provider pane
// (every catalog provider) → model pane (models of the selected provider,
// ready/needs-key gating); checks survive provider switches via
// selected_models_. Step 2: arena settings + Advanced (custom prompt, venue).
// Finish = create + auto-start (caller does both).
#include "services/alpha_arena/ArenaModelRegistry.h"
#include "services/alpha_arena/ArenaTypes.h"
#include <QDialog>
#include <QMap>

class QListWidget; class QListWidgetItem; class QLineEdit; class QDoubleSpinBox;
class QSpinBox; class QPlainTextEdit; class QRadioButton; class QPushButton;
class QStackedWidget; class QLabel;

namespace fincept::screens::alpha_arena {

class NewCompetitionWizard : public QDialog {
    Q_OBJECT
  public:
    explicit NewCompetitionWizard(QWidget* parent = nullptr);
    fincept::arena::ArenaConfig config() const;   // valid after Accepted
  private slots:
    void on_provider_row_changed();
    void on_model_item_changed(QListWidgetItem* it);
  private:
    void build_step1(); void build_step2();
    void populate_models();                            // both panes, from the registry
    void populate_model_pane(const QString& provider); // right pane only
    void refresh_provider_decorations();               // dots, counts, needs-key
    void update_selection_summary();
    QString current_provider() const;
    void update_nav();
    void on_next_or_create();
    bool engage_live_gates();   // signer + geofence + LiveModeGateDialog

    QStackedWidget* steps_ = nullptr;
    QListWidget* provider_list_ = nullptr;
    QListWidget* model_list_ = nullptr;
    QLabel* model_hint_ = nullptr;
    QLabel* selection_summary_ = nullptr;
    /// Single source of truth for cross-provider selection, keyed
    /// "provider/model_id" (QMap → deterministic agent order in the config).
    QMap<QString, fincept::arena::ArenaModelOption> selected_models_;
    /// Registry options grouped by lowercase provider id (rebuilt by populate_models).
    QMap<QString, QVector<fincept::arena::ArenaModelOption>> options_by_provider_;
    bool repopulating_ = false;   // guards itemChanged during programmatic rebuilds
    QLineEdit* name_ = nullptr;
    QListWidget* instruments_ = nullptr;
    QDoubleSpinBox* capital_ = nullptr;
    QSpinBox* cadence_ = nullptr;
    QDoubleSpinBox* max_lev_ = nullptr;
    QPlainTextEdit* custom_prompt_ = nullptr;
    QRadioButton* venue_paper_ = nullptr;
    QRadioButton* venue_live_ = nullptr;
    QPushButton* back_ = nullptr;
    QPushButton* next_ = nullptr;
    fincept::arena::ArenaConfig cfg_;
};

} // namespace fincept::screens::alpha_arena
