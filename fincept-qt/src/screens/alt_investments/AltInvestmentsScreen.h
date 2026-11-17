#pragma once
#include "screens/common/ISectionLinked.h"
#include "screens/common/IStatefulScreen.h"
#include "services/python_cli/PythonCliService.h"
#include "ui/widgets/ResultLayout.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFrame>
#include <QHash>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStringList>
#include <QVector>
#include <QVBoxLayout>
#include <QWidget>

#include <atomic>
#include <functional>
#include <memory>

namespace fincept::ui {
class AddToReportButton;
class AutoLoadPane;
class ResultView;
} // namespace fincept::ui

namespace fincept::report {
struct ReportPayload;
} // namespace fincept::report

namespace fincept::screens {

struct AltAnalyzer {
    QString id;          // Python CLI command (e.g. "high-yield")
    QString name;        // Display name
    QString description; // Short description shown in header
};

struct AltCategory {
    QString id;
    QString name;
    QString color;
    QString icon;
    QList<AltAnalyzer> analyzers;
};

/// Form field descriptor — drives dynamic form generation per analyzer and method.
struct AltField {
    QString key;   // JSON key sent to Python
    QString label; // Display label
    /// CashFlows: dated grid (date, type, amount, note) -> [{date, type, amount}].
    /// Series: grid of returns (optional date) + percent / decimal toggle -> [returns], [dates].
    enum Type { Text, Spin, Combo, Series, CashFlows } type = Spin;
    double default_val = 0.0;
    double min_val = 0.0;
    double max_val = 1e9;
    int decimals = 2;
    QString prefix;
    QString suffix;
    /// Text: default text. Combo: pipe-separated items (first = default).
    /// Series / CashFlows: default rows, ';'-separated rows of '|'-separated cells.
    QString combo_items;
    bool divide_100 = false; // divide UI value by 100 before sending (for % fields)
    /// Methods (cli.py --method) that read this field; empty = every method of the analyzer.
    QStringList methods;
    QString tooltip;

    bool applies_to(const QString& method) const { return methods.isEmpty() || methods.contains(method); }
};

/// One analysis method of an analyzer. A guide is general product text that is the same whatever
/// is entered: it is listed apart, shows no inputs and carries no score.
struct AltMethod {
    QString id;
    QString label;
    bool guide = false;
};

class AltInvestmentsScreen : public QWidget, public IStatefulScreen, public fincept::screens::ISectionLinked {
    Q_OBJECT
  public:
    explicit AltInvestmentsScreen(QWidget* parent = nullptr);
    ~AltInvestmentsScreen() override;

    // IStatefulScreen — the open category / analyzer, each analyzer's method and inputs.
    QVariantMap save_state() const override;
    void restore_state(const QVariantMap& state) override;
    QString state_key() const override { return "alt_investments"; }

    // ISectionLinked — the workbench modes by mode_id(): "analyzers" "private_markets" "real_estate"
    // "fund_analytics" "allocation" "credit_dd" ("mode:<id>" too), through set_mode. `focus` = an
    // analyzer category id or analyzer id: the ANALYZERS mode on that category / analyzer.
    bool open_section(const QString& section, const QString& focus) override;

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void changeEvent(QEvent* e) override;

  private slots:
    void on_category_changed(int index);
    void on_analyzer_changed(int index);
    void on_method_changed(int index);
    /// ANALYZE, or CANCEL while an analysis runs.
    void on_analyze();

  private:
    // ── Setup (Layout.cpp) ────────────────────────────────────────────────────
    void setup_ui();
    void retranslateUi();
    QWidget* create_header();
    QWidget* create_left_panel(QWidget* parent);
    QWidget* create_center_panel(QWidget* parent);
    QWidget* create_right_panel(QWidget* parent);
    QWidget* create_status_bar();
    /// ANALYZERS | PRIVATE MARKETS | REAL ESTATE | ... — the workbench panels (AltPanels.h) beside the
    /// analyzer catalogue; a panel is created the first time its mode is chosen.
    QWidget* create_mode_strip();
    void set_mode(int index);
    /// Stable key of mode `index` ("private_markets") and back (0 = analyzers when unknown).
    QString mode_id(int index) const;
    int mode_index(const QString& id) const;

    // ── Forms (Forms.cpp) ─────────────────────────────────────────────────────
    /// Rebuild the input fields for the current analyzer + method from the stored values.
    void rebuild_form();
    /// Fill method_combo_ (computed methods first, product guides grouped after a separator).
    void populate_methods(const QString& analyzer_id);
    QString current_analyzer_id() const;
    QString current_analyzer_name() const;
    AltMethod current_method() const;
    /// The stored display values of `analyzer_id` (defaults filled in on first use).
    QJsonObject& inputs_for(const QString& analyzer_id);
    void store_input(const QString& key, const QJsonValue& value);
    void reset_inputs_to_example();
    /// The JSON cli.py receives for the shown fields. `problems` collects what was left out.
    QJsonObject collect_form_data(QStringList* problems) const;
    QJsonObject input_echo() const; ///< label -> shown value, for copy / export
    void mark_result_stale();

    // ── Execution and results (Analysis.cpp) ──────────────────────────────────
    // The result area is the shared ui::AutoLoadPane (never blank): one view per
    // analyzer + method. Its first show displays the remembered result (memory,
    // else the persistent cache) and runs the analysis with the inputs on screen
    // (the user's persisted inputs, else the example); ANALYZE runs it again;
    // leaving the view (another analyzer / method / screen) cancels a default run;
    // a reply is matched to the pane's request id, so it never draws under
    // another view.
    /// The pane starts request `request_id` of its view: run cli.py (or answer from
    /// the 10-minute result cache) and deliver / fail it.
    void run_analysis(quint64 request_id);
    /// Stop the running analysis (if any); what is on screen stays.
    void cancel_analysis();
    /// Point the pane at the selected analyzer + method.
    void select_view();
    /// "<analyzer id>|<method id>" — the pane view of the selection.
    QString view_key() const;
    /// The result bar's title for the selection (analyzer · method · name).
    QString selection_title() const;
    /// ANALYZE / CANCEL follow whether the view on screen is running.
    void update_running();
    /// The pane's input signature for the view on screen: ui::autoload::signature_of the form JSON
    /// cli.py receives. A remembered result with the same inputs is not re-run; other inputs show
    /// the pane's "Inputs changed — RUN AGAIN" notice.
    void update_signature();
    /// Draw a payload of the pane (fresh or remembered) for the selection.
    void render_result(const QJsonValue& payload);
    void display_result(const QJsonObject& result, const QString& command, const AltMethod& method);
    /// An analyzer's in-band "cannot compute" shown inside the result.
    void display_error(const QString& error);
    void set_loading(bool loading);
    void add_analyzer_widgets(const QString& command, const QJsonObject& metrics);
    void update_actions();
    /// Plain-language text for a failure reported by Python or the runner.
    static QString plain_error(const QString& raw);
    /// Unit of an analyzer metric, decided by its name and (for amounts vs rates) magnitude:
    /// the analyzers report rates as fractions and amounts in currency.
    static fincept::ui::result::Unit result_unit_for(const QString& key, double value);
    fincept::ui::result::Options result_options(const QString& command) const;

    // Export of the shown result
    struct ExportTable {
        QString title;
        QStringList headers;
        QVector<QStringList> rows;
    };
    /// The shown result as (section, item, value) rows plus its tables.
    QVector<ExportTable> export_tables() const;
    QString export_base_name() const;
    void copy_result();
    void export_csv();
    void export_image();
    /// The shown result for the Report Builder: title, inputs echo, figures and tables.
    fincept::report::ReportPayload report_payload() const;

    // ── Live data prefill (Live.cpp, A4) ──────────────────────────────────────
    // A "LIVE DATA" strip at the top of forms that have a free source: Treasury par yield at the
    // bond's maturity, the 3-month bill, a futures curve, a stock / preferred quote, a coin's market
    // data, a commodity's positioning and price history. It runs only on a click (rate-limited free
    // sources), fills the fields (the user can still overwrite them) and says where each figure came
    // from and how old it is. A reply for an analyzer no longer on screen is dropped.
    /// The strip for the analyzer + method, or nullptr when it has no free source.
    QWidget* build_live_strip(const QString& analyzer_id, const AltMethod& method, QWidget* parent);
    void run_live_action(const QString& action);
    /// Fill a shown numeric field (no-op when the field is not on screen).
    bool set_live_value(const QString& key, double value);
    void set_live_note(const QString& text, bool problem);
    void set_live_busy(bool busy);
    /// Stop a prefill in flight and drop its reply.
    void cancel_live();
    /// Remember a strip input (symbol, root, coin) with the analyzer's inputs, outside the form data.
    void store_live(const QString& key, const QJsonValue& value);

    quint64 live_request_ = 0;
    std::function<void()> live_cancel_;
    QPointer<QLabel> live_note_;
    QPointer<QWidget> live_input_;
    QList<QPointer<QPushButton>> live_buttons_;

    // ── Modes (Layout.cpp) ────────────────────────────────────────────────────
    QStackedWidget* mode_stack_ = nullptr;
    QList<QPushButton*> mode_btns_;
    QVector<QWidget*> mode_pages_; ///< nullptr until the mode is first shown (page 0 = the analyzers)
    int active_mode_ = 0;

    // ── Category / analyzer data ──────────────────────────────────────────────
    QList<AltCategory> categories_;
    int active_category_ = 0;
    int active_analyzer_ = 0;
    bool loading_ = false;
    bool restoring_ = false;

    // Per-analyzer state (persisted): method id and display values of every field.
    QHash<QString, QString> methods_;
    QHash<QString, QJsonObject> inputs_;

    // Running analysis (its request id is the pane's)
    std::shared_ptr<std::atomic<bool>> cancel_token_;

    // Last shown result (for copy / export)
    QJsonObject last_result_;
    QString last_command_;
    AltMethod last_method_;
    QJsonObject last_inputs_echo_;
    QString last_title_;
    bool result_stale_ = false;

    // Fixed chrome labels (cached for retranslateUi)
    QLabel* header_title_ = nullptr;
    QLabel* header_sub_ = nullptr;
    QLabel* header_badge_ = nullptr;
    QLabel* left_title_ = nullptr;
    QLabel* combo_label_ = nullptr;
    QLabel* method_label_ = nullptr;
    QLabel* form_title_ = nullptr;
    QLabel* right_title_ = nullptr;
    QLabel* status_left_ = nullptr;

    // Left panel
    QList<QPushButton*> cat_btns_;

    // Centre (inputs)
    QLabel* center_title_ = nullptr;
    QLabel* center_desc_ = nullptr;
    QComboBox* analyzer_combo_ = nullptr;
    QComboBox* method_combo_ = nullptr; // cli.py --method for the selected analyzer
    QLabel* method_note_ = nullptr;     // what the selected method uses / that it is a guide
    QWidget* form_container_ = nullptr; // holds dynamic form rows
    QVBoxLayout* form_layout_ = nullptr;
    QPushButton* analyze_btn_ = nullptr;
    QPushButton* reset_btn_ = nullptr;
    QLabel* form_problems_ = nullptr;

    // Dynamic form fields (rebuilt per analyzer + method)
    QList<AltField> current_fields_;
    QHash<QString, QPointer<QWidget>> field_widgets_; // key -> editor

    // Right panel — result
    fincept::ui::AutoLoadPane* pane_ = nullptr; // never-blank result area (one view per analyzer + method)
    QLabel* result_status_ = nullptr;           // the pane's "as of 14:02 · refreshing…"
    QLabel* result_title_ = nullptr;
    QLabel* stale_note_ = nullptr;
    QPushButton* copy_btn_ = nullptr;
    QPushButton* csv_btn_ = nullptr;
    QPushButton* image_btn_ = nullptr;
    fincept::ui::AddToReportButton* report_btn_ = nullptr; ///< shared Send-to-Report action
    QLabel* verdict_badge_ = nullptr;
    QLabel* verdict_rating_ = nullptr;
    QLabel* verdict_rec_ = nullptr;
    fincept::ui::ResultView* result_view_ = nullptr; // KPIs, charts, tables of the analysis

    // Status bar
    QLabel* status_category_ = nullptr;
    QLabel* status_analyzer_ = nullptr;
};

} // namespace fincept::screens
