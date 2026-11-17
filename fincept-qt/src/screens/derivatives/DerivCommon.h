#pragma once
// DerivCommon — building blocks every Derivatives page uses: the screen's one
// stylesheet (selectors only, theme tokens only), input / card factories, number
// text through ui/formatting, a numeric-x line chart on the shared chart kit, and
// DerivResultBar — COPY · CSV · PNG · ADD TO REPORT above every result area.
//
// Layout helpers (the dense terminal look of Portfolio / Watchlist):
//   make_card   — a panel under a one-line ui::PanelHeader (tag chip + title, height from
//                 the font); spare height goes to the body, never to the header;
//   make_strip  — a one-line toolbar strip (surface background, hairline under it);
//   FormFit     — fixed-width cells of a compact form (fields sized from the input font,
//                 re-fitted on ThemeManager::appearance_settled);
//   compact_grid / dense_table — natural-width form columns, dense zebra tables.

#include "core/report/ReportBlocks.h"
#include "ui/charts/ChartExport.h"

#include <QPixmap>
#include <QPointF>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include <functional>
#include <tuple>

class QComboBox;
class QDateEdit;
class QDoubleSpinBox;
class QGridLayout;
class QHBoxLayout;
class QLabel;
class QPushButton;
class QSpinBox;
class QTableView;
class QVBoxLayout;

namespace fincept::ui {
class AddToReportButton;
class PanelHeader;
class TimeSeriesChart;
} // namespace fincept::ui

namespace fincept::screens::deriv {

/// The Derivatives screen's stylesheet (set once on the screen, before its children exist).
QString screen_style();

QDoubleSpinBox* make_spin(QWidget* parent, double value, double lo, double hi, int decimals,
                          const QString& suffix = QString());
QSpinBox* make_int_spin(QWidget* parent, int value, int lo, int hi, const QString& suffix = QString());
QComboBox* make_combo(QWidget* parent, const QStringList& items);
QDateEdit* make_date(QWidget* parent, const QDate& date);
/// `primary`: the accent-filled action of a card; otherwise a quiet outlined button. Both are
/// normal-height buttons at their natural width ('&' in `text` shows as '&').
QPushButton* make_button(QWidget* parent, const QString& text, bool primary);

/// A caption above an input in `grid` at (row, col). Returns the caption (keep it for retranslation).
QLabel* add_field(QGridLayout* grid, int row, int col, const QString& caption, QWidget* input, int col_span = 1);

/// A titled panel: a one-line header strip (ui::PanelHeader with a small accent tag chip before
/// the title) and a body layout that takes the panel's spare height (content at the top).
struct Card {
    QWidget* frame = nullptr;
    QLabel* tag = nullptr;
    QLabel* title = nullptr;
    QVBoxLayout* body = nullptr;
    ui::PanelHeader* header = nullptr; ///< add_control() puts widgets at its right end
};
Card make_card(QWidget* parent, const QString& tag, const QString& title);

QLabel* make_note(QWidget* parent, const QString& text);
QLabel* make_section(QWidget* parent, const QString& text);

/// A one-line toolbar strip of a page (object name "derivBar": surface background, hairline
/// under it, fixed height from its content). `layout` receives its QHBoxLayout.
QWidget* make_strip(QWidget* parent, QHBoxLayout** layout);

/// After the last field of a wide input row: the columns keep their natural width and the spare
/// width goes to an empty trailing column (no input stretched across the window).
void compact_grid(QGridLayout* grid);

/// The dense table look: zebra rows, no word wrap, row height from the table font (re-fitted on
/// ThemeManager::appearance_settled), no row numbers.
void dense_table(QTableView* table);

/// Fixed-width cells of a compact form. Every registered input (and its caption) is `span`
/// columns of field_px() wide — the width of `chars` characters of the input font plus the
/// box's padding — so the separate grids of one card line up and nothing stretches with the
/// window. Captions wrap. refit() re-applies the widths: call it on
/// ThemeManager::appearance_settled (never in a theme_changed handler).
class FormFit {
  public:
    static constexpr int kGap = 8; ///< horizontal spacing of every grid the form uses

    explicit FormFit(int chars = 14) : chars_(chars) {}
    /// An input and its caption (may be null), `span` columns wide.
    void add(QWidget* input, QLabel* caption, int span = 1);
    /// Another widget that must be exactly `span` columns wide (a note, a status line, a table).
    void add_wide(QWidget* w, int span);
    void refit() const;
    int field_px() const;
    int span_px(int span) const;

  private:
    struct Entry {
        QPointer<QWidget> w;
        int span = 1;
    };
    QVector<Entry> entries_;
    int chars_;
};

// ── Numbers (always through ui/formatting) ───────────────────────────────────
QString num(double v, int decimals);
QString signed_num(double v, int decimals);
QString money(double v, const QString& ccy);
QString signed_money(double v, const QString& ccy);
QString vol_pct(double sigma, int decimals = 2); ///< 0.2345 → "23.45%"
QString pct_points(double percent, int decimals = 2);  ///< 1.5 → "1.50%" (unsigned)

// ── Charts ───────────────────────────────────────────────────────────────────
/// One series of a numeric-x line chart.
struct LineSpec {
    QString name;
    QVector<QPointF> points;
    bool dashed = false;
    int palette = -1; ///< −1 = next categorical colour
    bool muted = false;
};
/// A TimeSeriesChart with a plain numeric x axis (no presets, no time formatting),
/// built under `parent`; `markers` are (x, y, label) points on the first series' axis.
ui::TimeSeriesChart* make_xy_chart(QWidget* parent, const QString& title, const QString& x_title,
                                   const QVector<LineSpec>& lines,
                                   const QVector<std::tuple<double, double, QString>>& markers = {},
                                   const std::function<QString(double)>& x_fmt = {},
                                   const std::function<QString(double)>& y_fmt = {});

// ── Results toolbar ──────────────────────────────────────────────────────────
class DerivResultBar : public QWidget {
    Q_OBJECT
  public:
    using TableFn = std::function<ui::ChartExport::Table()>;
    using ImageFn = std::function<QPixmap()>;
    using PayloadFn = std::function<report::ReportPayload()>;

    explicit DerivResultBar(const QString& export_name, QWidget* parent = nullptr);

    void set_table(TableFn fn);
    void set_image(ImageFn fn);
    void set_payload(PayloadFn fn);
    void set_export_name(const QString& name) { export_name_ = name; }

  protected:
    void changeEvent(QEvent* e) override;

  private:
    void retranslate();
    void copy();
    void save_csv();
    void save_png();

    QString export_name_;
    TableFn table_;
    ImageFn image_;
    QPushButton* copy_btn_ = nullptr;
    QPushButton* csv_btn_ = nullptr;
    QPushButton* png_btn_ = nullptr;
    ui::AddToReportButton* report_btn_ = nullptr;
};

/// Remembers which widget shows which source text so a page can re-apply every
/// caption, button, tooltip and combo item on QEvent::LanguageChange. Sources are
/// marked with QT_TR_NOOP at the call site (context = the page class). Button texts
/// show '&' literally.
class TrBook {
  public:
    explicit TrBook(const char* context) : context_(context) {}
    QString text(const char* src) const;
    void label(QLabel* w, const char* src);
    void button(QPushButton* w, const char* src);
    void tooltip(QWidget* w, const char* src);
    void combo(QComboBox* w, const QVector<const char*>& items);
    void apply() const;

  private:
    struct Entry {
        QPointer<QWidget> w;
        int kind = 0; // 0 label, 1 button, 2 tooltip, 3 combo
        QVector<const char*> src;
    };
    const char* context_;
    QVector<Entry> entries_;
};

/// TSV text of a table (header row first) for the clipboard.
QString table_tsv(const ui::ChartExport::Table& t);

/// PNG bytes of a pixmap (for report payloads).
QByteArray png_bytes(const QPixmap& pm);

} // namespace fincept::screens::deriv
