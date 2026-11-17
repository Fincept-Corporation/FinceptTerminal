#pragma once
// QuantLibResultView.h — results pane of the QuantLib screen.
//
// One response is shown three ways: SUMMARY (default) — the shared
// ui::ResultView: headline KPIs, the right chart for each series / matrix /
// distribution, sortable tables, collapsible sections for nested objects;
// TABLE — any section of the payload as a grid ("VIEW" selector), a virtual
// model over the JSON so every row is available without building a widget
// item per cell; JSON — the payload itself, formatted on a worker thread.
// TABLE and JSON are built only when first shown. Failures get an explicit
// state with the action that fixes them (UPGRADE for 403, VIEW PLANS for 402,
// RETRY for transient errors). COPY puts the current view on the clipboard;
// CSV exports the full section (written on a worker thread).

#include "services/quantlib/QuantLibAccess.h"
#include "ui/widgets/EmptyState.h"

#include <QDateTime>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QPair>
#include <QStringList>
#include <QWidget>

class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSortFilterProxyModel;
class QStackedWidget;
class QTableView;

namespace fincept::ui {
class LoadingOverlay;
class LoadingSkeleton;
class ResultView;
} // namespace fincept::ui

namespace fincept::screens {

class QlJsonTableModel;

class QuantLibResultView : public QWidget {
    Q_OBJECT
  public:
    explicit QuantLibResultView(QWidget* parent = nullptr);

    /// Designed state for an endpoint that is not run automatically — it costs a
    /// credit or needs Pro on the current plan, the user is signed out, or
    /// automatic runs are paused: what the calculation returns, the inputs a run
    /// would send, what it costs, and RUN (and UPGRADE / VIEW PLANS).
    struct Preview {
        QString endpoint;
        QString glyph;
        QString title;
        QString summary;
        QList<QPair<QString, QString>> inputs;
        QString inputs_heading;
        QString note;
        ui::EmptyState::Tone note_tone = ui::EmptyState::Tone::Neutral;
        QString run_text;             ///< text of the RUN action
        bool upgrade_primary = false; ///< UPGRADE TO PRO is the primary action, RUN the secondary
        bool plans_secondary = false; ///< VIEW PLANS as the secondary action
        QString status;               ///< toolbar status text ("Not run · 1 credit")
    };

    void show_idle();
    void show_preview(const Preview& preview);
    /// `cost_note` e.g. "1 credit" — shown while the call is in flight. With no
    /// result on screen the pane shows a skeleton of the result; over a result
    /// the result stays (dimmed) under the loading overlay.
    void show_loading(const QString& endpoint, const QString& cost_note);
    /// `as_of` valid = a remembered result computed at that time (the status
    /// says "as of …"); otherwise a fresh answer that took `elapsed_ms`.
    void show_result(const QString& endpoint, const QJsonObject& request, const QJsonValue& payload,
                     qint64 elapsed_ms, const QDateTime& as_of = QDateTime());
    /// A remembered result stays fully visible while a fresh copy is computed;
    /// only the status line says so.
    void set_refreshing(bool refreshing);
    void show_failure(const QString& endpoint, const services::QuantLibFailure& failure);
    /// Client-side validation failure (nothing was sent).
    void show_input_error(const QString& message);
    /// The in-flight call was cancelled. Returns true when the previous result
    /// is back on screen; false when there was none (the caller shows a preview).
    bool show_cancelled(const QString& endpoint);

    bool has_result() const { return has_result_; }
    /// Endpoint whose result / failure / preview the pane currently shows.
    QString shown_endpoint() const { return endpoint_; }

    void retranslate();

  signals:
    /// RETRY on a failure, RUN on a preview or on the idle state.
    void retry_requested();
    void upgrade_requested();

  private:
    enum Page { kMessagePage = 0, kSummaryPage = 1, kTablePage = 2, kJsonPage = 3, kErrorPage = 4, kSkeletonPage = 5 };
    /// What the EmptyState page's actions do in its current configuration.
    enum class EmptyAction { None, Run, Upgrade };
    enum class View { Summary, Table, Json };

    void build_sections();
    void render_current();
    void render_summary();
    void ensure_table();
    void ensure_json();
    void set_view(View view);
    void update_status();
    void set_result_controls_enabled(bool enabled);
    void stop_loading();
    void copy_current();
    void export_csv();
    int current_section() const;
    void show_error_page(const QString& title, const QString& tone, const QString& message, const QString& hint,
                         const QString& primary_text, bool show_retry);
    /// Reconfigures the EmptyState page completely (every field) and shows it.
    void show_empty(const QString& glyph, const QString& title, const QString& message,
                    const QList<QPair<QString, QString>>& details, const QString& details_heading,
                    const QString& note, ui::EmptyState::Tone note_tone, const QString& primary_text,
                    EmptyAction primary, const QString& secondary_text, EmptyAction secondary);
    void dispatch_empty_action(EmptyAction action);

    // Current response
    QString endpoint_;
    QJsonObject request_;
    QJsonValue payload_;
    qint64 elapsed_ms_ = 0;
    QList<QPair<QString, QJsonValue>> sections_;
    View view_ = View::Summary;
    bool has_result_ = false;
    bool has_failure_ = false;
    services::QuantLibFailure failure_;
    QDateTime as_of_;          ///< valid while a remembered result is shown
    bool refreshing_ = false;  ///< a fresh copy of the remembered result is being computed
    bool has_preview_ = false;
    Preview preview_;          ///< the preview on screen (re-applied on a language change)
    EmptyAction primary_action_ = EmptyAction::None;
    EmptyAction secondary_action_ = EmptyAction::None;

    // Lazy pages: which result / section they currently show.
    quint64 result_gen_ = 0;  ///< bumped for every new result
    quint64 table_gen_ = 0;   ///< result_gen_ the table model holds
    int table_section_ = -1;  ///< section index the table model holds
    quint64 json_gen_ = 0;    ///< result_gen_ the JSON page holds (or is formatting)
    QByteArray json_full_;    ///< the full formatted JSON of json_gen_ (COPY uses it)

    // Toolbar
    QLabel* title_ = nullptr;
    QLabel* endpoint_label_ = nullptr;
    QLabel* status_ = nullptr;
    QLabel* section_label_ = nullptr;
    QComboBox* section_combo_ = nullptr;
    QPushButton* summary_btn_ = nullptr;
    QPushButton* table_btn_ = nullptr;
    QPushButton* json_btn_ = nullptr;
    QPushButton* copy_btn_ = nullptr;
    QPushButton* csv_btn_ = nullptr;

    // Pages
    QStackedWidget* stack_ = nullptr;
    ui::EmptyState* empty_ = nullptr;       ///< idle / cancelled / preview states
    ui::LoadingSkeleton* skeleton_ = nullptr;
    ui::ResultView* summary_ = nullptr;
    QTableView* table_ = nullptr;
    QlJsonTableModel* table_model_ = nullptr;
    QSortFilterProxyModel* table_proxy_ = nullptr;
    QPlainTextEdit* json_ = nullptr;
    QLabel* error_title_ = nullptr;
    QLabel* error_text_ = nullptr;
    QLabel* error_hint_ = nullptr;
    QPushButton* error_primary_ = nullptr;
    QPushButton* error_retry_ = nullptr;
    ui::LoadingOverlay* overlay_ = nullptr;
};

} // namespace fincept::screens
