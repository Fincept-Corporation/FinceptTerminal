#pragma once
// AltDueDiligencePanel — "CREDIT & DUE DILIGENCE" mode of the Alternative Investments screen (A10):
//   PRIVATE CREDIT  one loan's yield, spread, duration, expected loss, loss-adjusted yield, cash-flow
//                   schedule and rate shocks (AltCreditMath.h: instant, local; runs on first show
//                   with the remembered inputs).
//   DUE DILIGENCE   named checklists from the templates in AltDdTemplates.h (PE, VC, private credit,
//                   real estate, hedge fund, digital asset): a status and a note per item, progress,
//                   saved locally (AltDdStore), exported as CSV / Markdown or sent to a report.
// Created by make_due_diligence_panel() (AltPanels.h) the first time the mode is chosen.

#include "services/alt_investments/AltDdStore.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointer>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QStackedWidget;
class QStandardItemModel;
class QTabWidget;
class QTimer;
class QTreeView;

namespace fincept::report {
struct ReportPayload;
}

namespace fincept::ui {
class AutoLoadPane;
class EmptyState;
class InputMemory;
class ResultView;
} // namespace fincept::ui

namespace fincept::screens::alt {

class AltDueDiligencePanel : public QWidget {
    Q_OBJECT
  public:
    explicit AltDueDiligencePanel(QWidget* parent = nullptr);
    ~AltDueDiligencePanel() override;

  private:
    // ── Private credit ──────────────────────────────────────────────────────
    QWidget* build_credit_tab(QWidget* parent);
    QJsonObject credit_payload() const;
    void update_credit_mode();
    fincept::report::ReportPayload credit_report() const;

    // ── Due diligence ───────────────────────────────────────────────────────
    QWidget* build_dd_tab(QWidget* parent);
    void load_checklists();
    void show_checklist(int index);
    void new_checklist(const QString& template_id);
    void rename_checklist();
    void delete_checklist();
    void on_item_edited();
    void save_current();
    void update_progress();
    QJsonArray items_from_model() const;
    QVector<QStringList> dd_rows() const;
    QString dd_markdown() const;
    fincept::report::ReportPayload dd_report() const;

    QTabWidget* tabs_ = nullptr;

    // credit
    QHash<QString, QDoubleSpinBox*> credit_inputs_;
    QComboBox* coupon_type_ = nullptr;
    QComboBox* frequency_ = nullptr;
    QLabel* base_note_ = nullptr;
    fincept::ui::AutoLoadPane* credit_pane_ = nullptr;
    fincept::ui::ResultView* credit_view_ = nullptr;
    fincept::ui::InputMemory* credit_memory_ = nullptr;
    QJsonObject credit_last_;
    quint64 base_fetch_ = 0;

    // due diligence
    QVector<fincept::services::alt::DdChecklist> lists_;
    int current_ = -1;
    QComboBox* list_combo_ = nullptr;
    QStackedWidget* dd_stack_ = nullptr; ///< 0 = empty state, 1 = the checklist
    fincept::ui::EmptyState* dd_empty_ = nullptr;
    QTreeView* tree_ = nullptr;
    QStandardItemModel* model_ = nullptr;
    QLabel* progress_ = nullptr;
    QLabel* save_note_ = nullptr;
    QList<QPushButton*> dd_actions_; ///< enabled only with a checklist on screen
    QTimer* save_timer_ = nullptr;
    bool filling_ = false;
};

} // namespace fincept::screens::alt
