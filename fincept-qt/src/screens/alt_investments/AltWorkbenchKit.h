#pragma once
// AltWorkbenchKit — small shared pieces of the Alternative Investments workbench panels (private
// markets, real estate, fund analytics): one selector stylesheet for a panel root, a read-only table
// model, the COPY / CSV / IMAGE / ADD TO REPORT row above a result, and number text through
// ui::formatting. File-scope names carry the alt_wb_ / AltWb prefix (unity-build safe).

#include "core/report/ReportBlocks.h"

#include <QAbstractTableModel>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include <functional>
#include <limits>

class QLabel;
class QPushButton;
class QTableView;

namespace fincept::ui {
class AddToReportButton;
}

namespace fincept::screens::alt {

/// Selector stylesheet for a workbench panel whose root object is named "altWbRoot". Set it on the
/// root before any child exists; every colour / size is a theme token (ThemeRestyle rewrites it).
QString alt_wb_style();

/// Read-only rows of display text. `numeric` columns are right-aligned; `tones` (optional, per cell:
/// "", "good", "bad", "warn") colour state text with SUCCESS / DANGER / WARNING.
class AltWbRowsModel : public QAbstractTableModel {
  public:
    explicit AltWbRowsModel(QObject* parent = nullptr);
    void set_rows(const QStringList& headers, const QVector<QStringList>& rows, const QVector<int>& numeric_cols = {},
                  const QVector<QStringList>& tones = {});
    void clear();
    const QStringList& headers() const { return headers_; }
    const QVector<QStringList>& rows() const { return rows_; }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

  private:
    QStringList headers_;
    QVector<QStringList> rows_;
    QVector<int> numeric_;
    QVector<QStringList> tones_;
};

/// A QTableView (object name "altWbTable") over `model`: sortable off, no edit, row height from the
/// font, right-click export (CSV / image) under `export_name`. Columns stretch; never resized to
/// contents on a fill.
QTableView* alt_wb_make_table(QWidget* parent, AltWbRowsModel* model, const QString& export_name);
/// Fit the view's height to its rows (at most `max_rows` visible before it scrolls).
void alt_wb_fit_table(QTableView* view, int max_rows = 14);

/// The action row above a result: title on the left; COPY, CSV, IMAGE and ADD TO REPORT on the right.
struct AltWbActions {
    QWidget* row = nullptr;
    QLabel* title = nullptr;
    QLabel* status = nullptr; ///< for the result pane's "as of …" (AutoLoadPane::set_status_label)
    QPushButton* copy = nullptr;
    QPushButton* csv = nullptr;
    QPushButton* image = nullptr;
    fincept::ui::AddToReportButton* report = nullptr;
};
AltWbActions alt_wb_make_actions(QWidget* parent, const QString& title);
/// Enable / disable the export actions together.
void alt_wb_set_actions_enabled(const AltWbActions& a, bool on);

QPushButton* alt_wb_button(const QString& text, bool primary, QWidget* parent);
QLabel* alt_wb_label(const QString& text, const char* object_name, QWidget* parent);

// ── Numbers ──────────────────────────────────────────────────────────────────
constexpr double kAltWbNaN = std::numeric_limits<double>::quiet_NaN();
/// JSON number -> double; null / missing / text -> NaN.
double alt_wb_num(const QJsonValue& v);
/// Money in `ccy` (empty = the display currency), compact per the user's scheme; "-" for NaN.
QString alt_wb_money(double v, const QString& ccy, bool compact = true);
/// A fraction as percent ("5.57%"); "-" for NaN. `signed_` adds "+" to positives.
QString alt_wb_pct(double fraction, int decimals = 2, bool signed_ = false);
/// "1.33x"; "-" for NaN.
QString alt_wb_multiple(double v, int decimals = 2);
/// Plain grouped number; "-" for NaN.
QString alt_wb_number(double v, int decimals = 2);
/// "yyyy-MM-dd" -> epoch ms at local midnight (NaN when unreadable).
double alt_wb_date_ms(const QString& iso);

// ── Export ───────────────────────────────────────────────────────────────────
struct AltWbExport {
    QString title;
    QVector<QPair<QString, QString>> figures; ///< headline figures (label, value)
    QStringList headers;                      ///< main table
    QVector<QStringList> rows;
    QVector<QPair<QString, QString>> inputs;  ///< assumptions behind the result
    QString notes;
};
/// Tab-separated text of the figures, inputs and the table (pastes into a spreadsheet).
QString alt_wb_tsv(const AltWbExport& e);
void alt_wb_copy(const AltWbExport& e);
/// CSV: figures and inputs as leading "Figure, Value" rows, a blank row, then the table.
bool alt_wb_save_csv(const AltWbExport& e, const QString& base_name, QWidget* dialog_parent);
/// A report payload with the figures as KPIs, the table, the inputs echo, the notes and (optional)
/// chart images grabbed from `charts` (title, widget).
fincept::report::ReportPayload alt_wb_payload(const AltWbExport& e,
                                              const QVector<QPair<QString, QWidget*>>& charts = {});

} // namespace fincept::screens::alt
