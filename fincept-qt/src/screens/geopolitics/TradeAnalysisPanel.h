// src/screens/geopolitics/TradeAnalysisPanel.h
#pragma once
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QHash>
#include <QJsonObject>
#include <QLabel>
#include <QList>
#include <QPushButton>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

#include <functional>

namespace fincept::ui {
class AutoLoadPane;
} // namespace fincept::ui

namespace fincept::screens {

/// Trade geopolitics analysis — benefits/costs, restrictions, trading blocs,
/// barrier removal. Inputs on the left, the result on the right in the shared
/// AutoLoadPane: each analysis (sub-tab) is a pane view that runs its local
/// Python calculation with the inputs on screen the first time it is shown, and
/// remembers its last result (with those inputs) across restarts.
class TradeAnalysisPanel : public QWidget {
    Q_OBJECT
  public:
    explicit TradeAnalysisPanel(QWidget* parent = nullptr);

  protected:
    void changeEvent(QEvent* event) override;

  private slots:
    void on_trade_result(const QString& context, const QJsonObject& data);
    void on_error(const QString& context, const QString& message);

  private:
    void build_ui();
    void connect_service();
    /// Designed result view: headline KPIs (+ chart) for the analysis type in
    /// `context`, the script's qualitative sections as cards, raw JSON behind a
    /// toggle. Unknown shapes fall back to generic key/value cards.
    void display_result(const QString& context, const QJsonObject& data);
    void clear_results();
    void retranslateUi();
    /// Pane start: run analysis `tab` with the inputs on screen.
    void start_analysis(quint64 id);
    /// Pane render: `payload` = {"inputs": {...}, "result": {...}}.
    void render_payload(const QJsonValue& payload, bool from_cache);

    QTabWidget* tabs_ = nullptr;
    fincept::ui::AutoLoadPane* pane_ = nullptr;
    QVBoxLayout* results_layout_ = nullptr;
    QWidget* results_container_ = nullptr;
    QLabel* status_label_ = nullptr;

    // Per analysis (tab index): its inputs as the script's params, and how to put
    // remembered inputs back into the form.
    QVector<std::function<QJsonObject()>> params_fns_;
    QVector<std::function<void(const QJsonObject&)>> apply_fns_;
    /// Service context ("trade_benefits", ...) -> the pane run waiting for it.
    QHash<QString, quint64> pending_;
    /// The inputs each pending run was started with (stored with its result).
    QHash<QString, QJsonObject> pending_inputs_;

    // Static text widgets (cached for retranslateUi).
    QLabel* title_lbl_ = nullptr;
    QComboBox* type_combo_ = nullptr;
    QList<QPushButton*> run_buttons_;
    // Page-level hint labels paired with their English source string so
    // retranslateUi can re-apply them. (Per-field captions built via make_field
    // are translated at construction time.) Filled during build_ui().
    QVector<QPair<QLabel*, QString>> i18n_labels_;
};

} // namespace fincept::screens
