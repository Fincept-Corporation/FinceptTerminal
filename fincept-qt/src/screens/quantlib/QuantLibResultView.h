#pragma once
// QuantLibResultView.h — results pane of the QuantLib screen.
//
// Shows one response as a table, a chart (when it carries numeric series) or
// raw JSON; arrays inside an object response are selectable as their own
// tables ("VIEW" selector). Failures get an explicit state with the action that
// fixes them (UPGRADE for 403, VIEW PLANS for 402, RETRY for transient errors).
// COPY puts the current view on the clipboard; CSV exports the full section —
// never the row-capped table.

#include "services/quantlib/QuantLibAccess.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QPair>
#include <QPointF>
#include <QStringList>
#include <QWidget>

class QChartView;
class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QTableWidget;

namespace fincept::screens {

class QuantLibResultView : public QWidget {
    Q_OBJECT
  public:
    explicit QuantLibResultView(QWidget* parent = nullptr);

    void show_idle();
    /// `cost_note` e.g. "1 credit" — shown while the call is in flight.
    void show_loading(const QString& endpoint, const QString& cost_note);
    void show_result(const QString& endpoint, const QJsonObject& request, const QJsonValue& payload,
                     qint64 elapsed_ms);
    void show_failure(const QString& endpoint, const services::QuantLibFailure& failure);
    /// Client-side validation failure (nothing was sent).
    void show_input_error(const QString& message);

    void retranslate();

  signals:
    void retry_requested();
    void upgrade_requested();

  private:
    enum Page { kMessagePage = 0, kTablePage = 1, kChartPage = 2, kJsonPage = 3, kErrorPage = 4 };
    enum class View { Table, Chart, Json };

    struct Series {
        QString name;
        QList<QPointF> points;
    };

    void build_sections();
    void render_current();
    void render_table(const QJsonValue& section);
    QList<Series> chart_series(const QJsonValue& section, QString* x_title) const;
    void render_chart(const QList<Series>& series, const QString& x_title);
    void render_json();
    void set_view(View view);
    void set_result_controls_enabled(bool enabled);
    void copy_current();
    void export_csv();
    /// Full (uncapped) rows of `section` as text, with column headers.
    QList<QStringList> section_rows(const QJsonValue& section, QStringList* headers) const;
    void show_error_page(const QString& title, const QString& tone, const QString& message, const QString& hint,
                         const QString& primary_text, bool show_retry);

    // Current response
    QString endpoint_;
    QJsonObject request_;
    QJsonValue payload_;
    qint64 elapsed_ms_ = 0;
    QList<QPair<QString, QJsonValue>> sections_;
    View view_ = View::Table;
    bool has_result_ = false;
    bool has_failure_ = false;
    services::QuantLibFailure failure_;

    // Toolbar
    QLabel* title_ = nullptr;
    QLabel* endpoint_label_ = nullptr;
    QLabel* status_ = nullptr;
    QLabel* section_label_ = nullptr;
    QComboBox* section_combo_ = nullptr;
    QPushButton* table_btn_ = nullptr;
    QPushButton* chart_btn_ = nullptr;
    QPushButton* json_btn_ = nullptr;
    QPushButton* copy_btn_ = nullptr;
    QPushButton* csv_btn_ = nullptr;

    // Pages
    QStackedWidget* stack_ = nullptr;
    QLabel* message_ = nullptr;
    QTableWidget* table_ = nullptr;
    QChartView* chart_view_ = nullptr;
    QPlainTextEdit* json_ = nullptr;
    QLabel* error_title_ = nullptr;
    QLabel* error_text_ = nullptr;
    QLabel* error_hint_ = nullptr;
    QPushButton* error_primary_ = nullptr;
    QPushButton* error_retry_ = nullptr;
};

} // namespace fincept::screens
