#pragma once
// DataWorkspace — what the Data Sources and Data Mapping tabs share:
//   * workspace_qss(): the one selector-based stylesheet both screens set on their root
//     before any child exists (theme tokens only; the theme manager's re-style pass keeps it
//     current). Widgets pick a look with the "dw" property (title, muted, primary, danger, …).
//   * TabularTableModel / TabularFilterProxy: model/view over a DataTable, with optional
//     per-cell marks (mapping errors: tinted cell + reason tooltip).
//   * ResultPane: a table result with a table picker (multi-table results), TABLE and CHART
//     views, CSV / JSON export, "Send to Data Mapping" and "Add to Report".

#include "core/report/ReportBlocks.h"
#include "services/data_mapping/MappingSpec.h"
#include "services/data_sources/SourceError.h"
#include "services/data_sources/TabularData.h"
#include "ui/widgets/EmptyState.h"

#include <QAbstractTableModel>
#include <QHash>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QString>
#include <QVector>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTabWidget;
class QTableView;
class QToolButton;

namespace fincept::ui {
class AddToReportButton;
class CandleChart;
class TimeSeriesChart;
} // namespace fincept::ui

namespace fincept::screens::datasources {

/// The shared stylesheet (set on the screen root before children are created).
QString workspace_qss();
/// Button / label text with '&' shown literally (QAbstractButton treats '&' as a mnemonic).
QString amp_safe(const QString& text);
/// Set the "dw" look property (before the widget is polished).
void set_look(QWidget* w, const char* look);
/// "just now" / "4 min ago" / "3 h ago" / "16 Nov 14:02" for a UTC "yyyy-MM-dd HH:mm:ss" time.
QString relative_time_text(const QString& utc_sql_time);

/// What a failure page offers. Every connector-contract error code maps to one action:
/// missing_key → ADD KEY (Settings > Credentials on that key), auth_failed → CHECK KEY / EDIT
/// SETTINGS, rate_limited → RETRY after a countdown, upstream_down → RETRY, bad_args → EDIT
/// INPUTS / EDIT SETTINGS, no_data → change the inputs, not_supported → the reason and the fix.
enum class ErrorAction { None, Retry, AddKey, EditSettings, EditInputs };
struct ErrorPage {
    fincept::ui::EmptyState::Spec spec;
    ErrorAction primary = ErrorAction::None;
    ErrorAction secondary = ErrorAction::None;
    int retry_after_s = -1; ///< rate_limited: the primary RETRY stays disabled this long
    QString env_key;        ///< AddKey: the credential to focus in Settings > Credentials
};
/// `catalogue`: a catalogue connector (keys live in Settings > Credentials) vs a saved
/// connection (credentials live in its own settings).
ErrorPage error_page(const fincept::services::datasources::SourceError& error, bool catalogue);
/// Settings > Credentials, focused on `env_key`.
void open_credentials(const QString& env_key, QWidget* context);

class TabularTableModel : public QAbstractTableModel {
    Q_OBJECT
  public:
    static constexpr int kSortRole = Qt::UserRole + 1;
    static constexpr int kMarkedRole = Qt::UserRole + 2;

    struct CellMark {
        QString message; ///< why the cell failed
        QString raw;     ///< the input text that failed (shown instead of the empty value)
    };
    static qint64 mark_key(int row, int column) { return (qint64(row) << 20) | qint64(column); }

    explicit TabularTableModel(QObject* parent = nullptr);

    void set_table(const fincept::services::datasources::DataTable& table);
    void set_marks(const QHash<qint64, CellMark>& marks);
    void clear();
    const fincept::services::datasources::DataTable& table() const { return table_; }
    bool row_marked(int row) const { return marked_rows_.contains(row); }

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    /// Repaint after a theme change (colours are read live in data()).
    void refresh_colours();

  private:
    fincept::services::datasources::DataTable table_;
    QHash<qint64, CellMark> marks_;
    QSet<int> marked_rows_;
};

/// Text filter (all terms must appear in the row) + "marked rows only".
class TabularFilterProxy : public QSortFilterProxyModel {
    Q_OBJECT
  public:
    explicit TabularFilterProxy(QObject* parent = nullptr);
    void set_text(const QString& text);
    void set_marked_only(bool on);

  protected:
    bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override;

  private:
    QStringList terms_;
    bool marked_only_ = false;
};

class ResultPane : public QWidget {
    Q_OBJECT
  public:
    /// `report_source` names the tab in report provenance ("Data Sources").
    explicit ResultPane(const QString& report_source, QWidget* parent = nullptr);

    /// Show `result` (keeps the chosen table when a table of the same name exists).
    void set_result(const fincept::services::datasources::TabularResult& result, const QString& title);
    void clear();
    bool has_result() const { return has_result_; }
    const fincept::services::datasources::TabularResult& result() const { return result_; }
    QString current_table_name() const;
    const fincept::services::datasources::DataTable* current_table() const;

    /// Enables SEND TO DATA MAPPING for this result (empty source kind disables it).
    void set_mapping_source(const fincept::services::datamapping::SourceRef& source, const QString& label);
    /// A short line under the bar (cache age, caveats); empty hides it.
    void set_note(const QString& text);

  signals:
    void table_changed(const QString& table_name);

  private:
    void show_table(int index);
    void update_chart_view();
    void render_chart();
    void export_csv();
    void export_json();
    void send_to_mapping();
    fincept::report::ReportPayload report_payload() const;

    QString report_source_;
    fincept::services::datasources::TabularResult result_;
    bool has_result_ = false;
    QString title_;
    int table_index_ = 0;
    fincept::services::datamapping::SourceRef mapping_source_;
    QString mapping_label_;

    QComboBox* table_pick_ = nullptr;
    QLabel* info_ = nullptr;
    QLabel* note_ = nullptr;
    QLineEdit* filter_ = nullptr;
    QToolButton* export_btn_ = nullptr;
    QPushButton* to_mapping_btn_ = nullptr;
    fincept::ui::AddToReportButton* to_report_ = nullptr;
    QTabWidget* tabs_ = nullptr;
    QTableView* view_ = nullptr;
    TabularTableModel* model_ = nullptr;
    TabularFilterProxy* proxy_ = nullptr;
    QComboBox* chart_column_ = nullptr;
    QStackedWidget* chart_stack_ = nullptr;
    fincept::ui::TimeSeriesChart* line_chart_ = nullptr;
    fincept::ui::CandleChart* candle_chart_ = nullptr;
    QLabel* chart_message_ = nullptr;
    int chart_time_col_ = -1;
    QVector<int> chart_value_cols_;
    bool chart_has_ohlc_ = false;
};

} // namespace fincept::screens::datasources
