#pragma once
// SensitivityTable — the shared two-axis sensitivity grid: DCF value over WACC x terminal growth,
// LBO IRR / MOIC over entry x exit multiple, accretion over premium x synergies, a property's IRR
// over exit cap x rent growth. Callers compute the matrix; this widget shows it.
//
//   auto* t = new ui::SensitivityTable(parent);
//   t->set_title(tr("Value per share"), tr("WACC x terminal growth"));
//   t->set_axes(tr("WACC"), {"8.0%", "9.0%", "10.0%"}, tr("Terminal growth"), {"2.0%", "2.5%", "3.0%"});
//   t->set_matrix(values, [](double v) { return money(v); });   // one metric
//   t->add_metric(tr("MOIC"), moic, [](double v) { return QString("%1x").arg(v, 0, 'f', 2); });  // more
//   t->set_base_cell(1, 1);                                     // outlined (and bold)
//
// Two presentations of the same numbers: TABLE (default: exact values, cells tinted on the chart
// kit's diverging / sequential scale, the base case outlined) and MAP (ui::MatrixHeatmap; the base
// row and column labels carry a marker). COPY puts tab-separated text on the clipboard (pastes as a
// grid into a spreadsheet), CSV saves a file (ui::ChartExport), headers() / rows() feed Send to Report.
// Colours are read from the live theme when painted; nothing appearance-derived is cached.

#include "ui/widgets/SensitivityGrid.h"

#include <QPixmap>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include <functional>

class QAbstractButton;
class QButtonGroup;
class QHBoxLayout;
class QLabel;
class QPushButton;
class QStackedWidget;
class QTableView;

namespace fincept::ui {

class MatrixHeatmap;
class SensitivityTableModel;

class SensitivityTable : public QWidget {
    Q_OBJECT
  public:
    using Formatter = std::function<QString(double)>;
    enum class View { Table, Map };

    explicit SensitivityTable(QWidget* parent = nullptr);
    ~SensitivityTable() override;

    void set_title(const QString& title, const QString& subtitle = QString());
    /// Axis titles and the label of every row / column (already formatted: "9.0%", "8.5x").
    void set_axes(const QString& row_title, const QStringList& row_labels, const QString& col_title,
                  const QStringList& col_labels);
    /// Numeric axes shown through `format` (empty = plain numbers).
    void set_axes(const QString& row_title, const QVector<double>& row_values, const QString& col_title,
                  const QVector<double>& col_values, const Formatter& row_format = {},
                  const Formatter& col_format = {});

    /// Replace every metric with this one ([row][col], NaN = not computed).
    void set_matrix(const QVector<QVector<double>>& values, const Formatter& format = {});
    /// Add a metric (a toggle button per metric appears from the second one).
    void add_metric(const SensitivityMetric& metric);
    void add_metric(const QString& name, const QVector<QVector<double>>& values, const Formatter& format = {},
                    bool higher_is_better = true);
    void clear();
    void set_current_metric(int index);
    int current_metric() const { return current_; }
    int metric_count() const { return static_cast<int>(grid_.metrics.size()); }

    /// Outline the base-case cell (-1, -1 = none). Diverging scales centre on it unless a metric
    /// sets its own midpoint.
    void set_base_cell(int row, int col);
    /// Formatter for metrics that have none of their own.
    void set_formatter(const Formatter& format);
    /// Text under the grid (units, method, "computed in one run of …").
    void set_note(const QString& note);
    void set_view(View view);
    View view() const { return view_; }
    /// Base file name for CSV / PNG ("dcf_sensitivity").
    void set_export_name(const QString& name) { export_name_ = name; }
    /// Hide COPY / CSV (when the host offers its own export row).
    void set_export_buttons_visible(bool on);

    // ── Read-out (copy / export / report) ──────────────────────────────────────
    const SensitivityGrid& grid() const { return grid_; }
    QString title() const;
    QString subtitle() const;
    QStringList headers() const;
    QVector<QStringList> rows() const; ///< current metric, formatted as shown
    QString to_tsv() const;
    bool is_empty() const { return grid_.is_empty(); }
    /// The grid as an image (the visible presentation).
    QPixmap grab_image();

    void copy_to_clipboard() const;
    bool save_csv();

    QSize sizeHint() const override;

  signals:
    void cell_clicked(int row, int col);
    void metric_changed(int index);

  protected:
    void changeEvent(QEvent* e) override;

  private:
    void build_ui();
    void rebuild_metric_buttons();
    void refresh();
    void refit();
    QString formatted(double v) const;

    SensitivityGrid grid_;
    int current_ = 0;
    View view_ = View::Table;
    Formatter default_format_;
    QString export_name_ = QStringLiteral("sensitivity");
    QString title_;
    QString subtitle_;

    QLabel* title_label_ = nullptr;
    QLabel* subtitle_label_ = nullptr;
    QLabel* axes_label_ = nullptr;
    QLabel* note_label_ = nullptr;
    QWidget* metric_box_ = nullptr;
    QHBoxLayout* metric_layout_ = nullptr;
    QButtonGroup* metric_group_ = nullptr;
    QPushButton* table_btn_ = nullptr;
    QPushButton* map_btn_ = nullptr;
    QPushButton* copy_btn_ = nullptr;
    QPushButton* csv_btn_ = nullptr;
    QStackedWidget* stack_ = nullptr;
    QTableView* table_ = nullptr;
    SensitivityTableModel* model_ = nullptr;
    MatrixHeatmap* map_ = nullptr;
};

} // namespace fincept::ui
