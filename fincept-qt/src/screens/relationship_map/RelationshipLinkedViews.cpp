// src/screens/relationship_map/RelationshipLinkedViews.cpp
#include "screens/relationship_map/RelationshipLinkedViews.h"

#include "screens/relationship_map/RelationshipMapCharts.h"
#include "screens/relationship_map/RelationshipMapFormat.h"
#include "ui/charts/DonutChart.h"
#include "ui/charts/MatrixHeatmap.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/BarCellDelegate.h"
#include "ui/tables/SparklineDelegate.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/KeySortProxy.h"
#include "ui/widgets/LoadingSkeleton.h"

#include <QAbstractTableModel>
#include <QBrush>
#include <QDate>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QMap>
#include <QScopedValueRollback>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTabBar>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace fincept::relmap {

namespace {

namespace colors = fincept::ui::colors;
namespace fonts = fincept::ui::fonts;
namespace fmt = fincept::ui::formatting;

constexpr double kRlvNaN = std::numeric_limits<double>::quiet_NaN();
constexpr int kRlvNodeRole = Qt::UserRole + 1;  ///< the graph node id of the row ("" = none)
constexpr int kRlvSortRole = Qt::UserRole + 2;  ///< typed sort key (KeySortProxy)
constexpr int kRlvValueRole = Qt::UserRole + 3; ///< the raw number (in-cell bars)
constexpr int kRlvTableFont = -2;               ///< table text: base size - 2 px (matches the sheet)

/// How a cell prints its value.
enum class RlvFmt {
    Text,        ///< text as given
    Tag,         ///< a transaction kind: BUY / SELL with arrow + direction colour, others neutral
    Int,         ///< whole number (rank, age, count)
    Shares,      ///< share count, abbreviated
    Amount,      ///< company amount in the major unit of `ccy` (relmap::amount_text)
    Money,       ///< an amount in `ccy` exactly ("" = no symbol), abbreviated
    Price,       ///< per-share price in `ccy` as quoted
    Percent,     ///< percent units, unsigned
    PercentChg,  ///< percent units, signed, with arrow + direction colour
    Fraction,    ///< a fraction as a percentage, unsigned
    FractionChg, ///< a fraction as a percentage, signed + direction colour
    Ratio,       ///< a multiple: "12.3x"
    Corr,        ///< a coefficient: "0.85"
    Spark,       ///< painted by the sparkline delegate
};

bool rlv_numeric(RlvFmt f) {
    return f != RlvFmt::Text && f != RlvFmt::Tag && f != RlvFmt::Spark;
}

bool rlv_percent_like(RlvFmt f) {
    return f == RlvFmt::Percent || f == RlvFmt::PercentChg || f == RlvFmt::Fraction || f == RlvFmt::FractionChg;
}

bool rlv_has_currency(RlvFmt f) {
    return f == RlvFmt::Amount || f == RlvFmt::Money || f == RlvFmt::Price;
}

double rlv_pos(double v) {
    return std::isfinite(v) && v > 0 ? v : kRlvNaN;
}

double rlv_nz(double v) {
    return std::isfinite(v) && v != 0 ? v : kRlvNaN;
}

struct RlvCell {
    RlvFmt fmt = RlvFmt::Text;
    QString text; ///< Text: the text; Tag: the raw kind ("buy", "Stock Gift")
    double v = kRlvNaN;
    QString ccy;  ///< Amount / Money / Price
    QString tip;  ///< tooltip ("" = the text of a Text cell)
    int tone = 0; ///< Tag: +1 buy, -1 sell, 0 any other kind
};

struct RlvRow {
    QString node_id;
    QVector<RlvCell> cells;
    QVector<double> spark;
    bool accent = false; ///< the company's own row
};

struct RlvColumn {
    QString title;
    RlvFmt fmt = RlvFmt::Text;
    int chars = 10; ///< preferred width in average characters
    QString tip{};
};

RlvCell rlv_text(const QString& text, const QString& tip = QString()) {
    RlvCell c;
    c.text = text;
    c.tip = tip;
    return c;
}

RlvCell rlv_num(RlvFmt f, double v, const QString& ccy = QString()) {
    RlvCell c;
    c.fmt = f;
    c.v = v;
    c.ccy = ccy.trimmed();
    return c;
}

RlvCell rlv_tag(const QString& kind, const QString& tip = QString()) {
    RlvCell c;
    c.fmt = RlvFmt::Tag;
    c.text = kind.trimmed();
    c.tip = tip;
    const QString k = c.text.toLower();
    if (k == QLatin1String("buy") || k == QLatin1String("purchase"))
        c.tone = 1;
    else if (k == QLatin1String("sell") || k == QLatin1String("sale"))
        c.tone = -1;
    return c;
}

RlvCell rlv_spark_cell() {
    RlvCell c;
    c.fmt = RlvFmt::Spark;
    return c;
}

/// +1 / -1 when the value prints as a non-zero change, else 0.
int rlv_direction(const RlvCell& c) {
    if (!std::isfinite(c.v))
        return 0;
    if (c.fmt == RlvFmt::PercentChg)
        return c.v >= 0.005 ? 1 : (c.v <= -0.005 ? -1 : 0);
    if (c.fmt == RlvFmt::FractionChg)
        return c.v >= 0.0005 ? 1 : (c.v <= -0.0005 ? -1 : 0);
    return 0;
}

/// An amount in `ccy` exactly as reported ("" = no symbol), abbreviated per the user's scheme.
QString rlv_money(double v, const QString& ccy) {
    if (!std::isfinite(v))
        return fmt::placeholder();
    const QString c = ccy.trimmed();
    return c.isEmpty() ? fmt::compact(v) : fmt::money(v, c, fmt::Compact::FromSettings);
}

QString rlv_signed_volume(double v) {
    if (!std::isfinite(v))
        return fmt::placeholder();
    if (v > 0)
        return QStringLiteral("+") + fmt::volume(v);
    if (v < 0)
        return QStringLiteral("-") + fmt::volume(-v);
    return fmt::volume(0);
}

QString rlv_cell_text(const RlvCell& c) {
    switch (c.fmt) {
        case RlvFmt::Text:
            return c.text;
        case RlvFmt::Tag: {
            const QString up = c.text.toUpper();
            if (up.isEmpty())
                return fmt::placeholder();
            if (c.tone > 0)
                return QStringLiteral("▲ ") + up;
            if (c.tone < 0)
                return QStringLiteral("▼ ") + up;
            return up;
        }
        case RlvFmt::Int:
            return fmt::number(c.v, 0);
        case RlvFmt::Shares:
            return std::isfinite(c.v) && c.v > 0 ? fmt::volume(c.v) : fmt::placeholder();
        case RlvFmt::Amount:
            return std::isfinite(c.v) ? amount_text(c.v, c.ccy) : fmt::placeholder();
        case RlvFmt::Money:
            return rlv_money(c.v, c.ccy);
        case RlvFmt::Price:
            return std::isfinite(c.v) ? price_text(c.v, c.ccy) : fmt::placeholder();
        case RlvFmt::Percent:
            return fmt::pct(c.v, 2, false);
        case RlvFmt::PercentChg: {
            const QString t = fmt::pct(c.v, 2, true);
            const int d = rlv_direction(c);
            if (d > 0)
                return QStringLiteral("▲ ") + t;
            if (d < 0)
                return QStringLiteral("▼ ") + t;
            return t;
        }
        case RlvFmt::Fraction:
            return fmt::fraction_pct(c.v, 1, false);
        case RlvFmt::FractionChg:
            return fmt::fraction_pct(c.v, 1, true);
        case RlvFmt::Ratio:
            return fmt::ratio(c.v, 1);
        case RlvFmt::Corr:
            return fmt::number(c.v, 2);
        case RlvFmt::Spark:
            return QString();
    }
    return QString();
}

QVariant rlv_sort_key(const RlvCell& c) {
    switch (c.fmt) {
        case RlvFmt::Text:
            return c.text;
        case RlvFmt::Tag:
            return c.text.toLower();
        case RlvFmt::Spark:
            return QVariant();
        default:
            break;
    }
    return std::isfinite(c.v) ? QVariant(c.v) : QVariant();
}

/// Machine-readable value of a cell (CSV): percentages in percent units, "" when missing.
QString rlv_csv_value(const RlvCell& c) {
    switch (c.fmt) {
        case RlvFmt::Text:
        case RlvFmt::Tag:
            return c.text;
        case RlvFmt::Spark:
            return QString();
        case RlvFmt::Fraction:
        case RlvFmt::FractionChg:
            return fmt::export_number(c.v * 100.0);
        default:
            break;
    }
    return fmt::export_number(c.v);
}

QString rlv_csv_currency(const RlvCell& c) {
    if (!std::isfinite(c.v) || c.ccy.isEmpty())
        return QString();
    return c.fmt == RlvFmt::Amount ? fmt::major_currency(c.ccy) : c.ccy;
}

QString rlv_csv_field(const QString& s) {
    if (!s.contains(QLatin1Char(',')) && !s.contains(QLatin1Char('"')) && !s.contains(QLatin1Char('\n')) &&
        !s.contains(QLatin1Char('\r')))
        return s;
    QString q = s;
    q.replace(QLatin1String("\""), QLatin1String("\"\""));
    return QLatin1Char('"') + q + QLatin1Char('"');
}

QString rlv_row_key(const RlvRow& r) {
    if (!r.node_id.isEmpty())
        return r.node_id;
    QString key;
    for (int i = 0; i < r.cells.size() && i < 2; ++i)
        key += r.cells[i].text + QLatin1Char('\x1f');
    return key;
}

/// The rows of one linked table. Formats in data() (so a number-format change only needs
/// dataChanged, see fmt::refresh_model_on_format_change); rows are replaced in place when the
/// row set is unchanged (selection and scroll stay), else by a reset.
class RlvTableModel : public QAbstractTableModel {
  public:
    RlvTableModel(QVector<RlvColumn> columns, QObject* parent)
        : QAbstractTableModel(parent), columns_(std::move(columns)) {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : static_cast<int>(rows_.size());
    }
    int columnCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : static_cast<int>(columns_.size());
    }

    Qt::ItemFlags flags(const QModelIndex& index) const override {
        return index.isValid() ? (Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemNeverHasChildren) : Qt::NoItemFlags;
    }

    QVariant headerData(int column, Qt::Orientation orientation, int role = Qt::DisplayRole) const override {
        if (orientation != Qt::Horizontal || column < 0 || column >= columns_.size())
            return QVariant();
        const RlvColumn& c = columns_[column];
        switch (role) {
            case Qt::DisplayRole:
                return c.title;
            case Qt::ToolTipRole:
                return c.tip.isEmpty() ? c.title : c.tip;
            case Qt::TextAlignmentRole:
                return QVariant((rlv_numeric(c.fmt) ? (Qt::AlignRight | Qt::AlignVCenter) : (Qt::AlignLeft | Qt::AlignVCenter))
                                    .toInt());
            default:
                break;
        }
        return QVariant();
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || index.row() >= rows_.size() || index.column() >= columns_.size())
            return QVariant();
        const RlvRow& row = rows_[index.row()];
        const int col = index.column();
        const RlvCell cell = col < row.cells.size() ? row.cells[col] : RlvCell();
        switch (role) {
            case Qt::DisplayRole:
                return rlv_cell_text(cell);
            case Qt::ToolTipRole:
                if (!cell.tip.isEmpty())
                    return cell.tip;
                if (cell.fmt == RlvFmt::Text && !cell.text.isEmpty())
                    return cell.text;
                return QVariant();
            case Qt::TextAlignmentRole:
                return QVariant((rlv_numeric(cell.fmt) ? (Qt::AlignRight | Qt::AlignVCenter)
                                                       : (Qt::AlignLeft | Qt::AlignVCenter))
                                    .toInt());
            case Qt::ForegroundRole: {
                // Read at paint time: a theme switch repaints in the new colours.
                if (cell.fmt == RlvFmt::Tag) {
                    if (cell.tone > 0)
                        return QVariant::fromValue(QColor(colors::POSITIVE()));
                    if (cell.tone < 0)
                        return QVariant::fromValue(QColor(colors::NEGATIVE()));
                    return QVariant::fromValue(QColor(colors::TEXT_SECONDARY()));
                }
                const int d = rlv_direction(cell);
                if (d > 0)
                    return QVariant::fromValue(QColor(colors::POSITIVE()));
                if (d < 0)
                    return QVariant::fromValue(QColor(colors::NEGATIVE()));
                if (row.accent && col == 0)
                    return QVariant::fromValue(QColor(colors::AMBER()));
                if (rlv_numeric(cell.fmt) && !std::isfinite(cell.v))
                    return QVariant::fromValue(QColor(colors::TEXT_TERTIARY()));
                return QVariant();
            }
            case Qt::FontRole:
                if (row.accent)
                    return QVariant::fromValue(fonts::font(kRlvTableFont, QFont::Bold));
                return QVariant();
            case Qt::BackgroundRole:
                if (row.accent)
                    return QVariant::fromValue(QBrush(colors::tint_color(QColor(colors::AMBER()), 0.10)));
                return QVariant();
            case kRlvNodeRole:
                return row.node_id;
            case kRlvSortRole:
                return rlv_sort_key(cell);
            case kRlvValueRole:
                return cell.v;
            case ui::SparklineDelegate::kDefaultRole:
                if (cell.fmt == RlvFmt::Spark && row.spark.size() >= 2)
                    return QVariant::fromValue(row.spark);
                return QVariant();
            default:
                break;
        }
        return QVariant();
    }

    /// True when the row set changed (model reset); false when replaced in place.
    bool set_rows(QVector<RlvRow> rows) {
        bool same = rows.size() == rows_.size();
        for (qsizetype i = 0; same && i < rows.size(); ++i)
            same = rlv_row_key(rows[i]) == rlv_row_key(rows_[i]);
        if (same) {
            rows_ = std::move(rows);
            if (!rows_.isEmpty())
                emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
            return false;
        }
        beginResetModel();
        rows_ = std::move(rows);
        endResetModel();
        return true;
    }

    int row_of(const QString& id) const {
        if (id.isEmpty())
            return -1;
        for (int i = 0; i < rows_.size(); ++i)
            if (rows_[i].node_id == id)
                return i;
        return -1;
    }

    const QVector<RlvColumn>& columns() const { return columns_; }
    const QVector<RlvRow>& rows() const { return rows_; }

  private:
    QVector<RlvColumn> columns_;
    QVector<RlvRow> rows_;
};

const RlvTableModel* rlv_model(const QAbstractTableModel* m) {
    return static_cast<const RlvTableModel*>(m);
}

/// Puts `rows` into the table; after a reset the scroll position is put back once the view
/// has laid the new rows out.
void rlv_apply_rows(QTableView* view, QAbstractTableModel* model, QVector<RlvRow> rows) {
    const int v = view->verticalScrollBar()->value();
    const int h = view->horizontalScrollBar()->value();
    const bool reset = static_cast<RlvTableModel*>(model)->set_rows(std::move(rows));
    if (!reset || (v == 0 && h == 0))
        return;
    QTimer::singleShot(0, view, [view, v, h]() {
        view->verticalScrollBar()->setValue(v);
        view->horizontalScrollBar()->setValue(h);
    });
}

QTableView* rlv_make_view(QWidget* parent, const QString& accessible_name) {
    auto* v = new QTableView(parent);
    v->setObjectName(QStringLiteral("rlvTable"));
    v->setAccessibleName(accessible_name);
    v->setFrameShape(QFrame::NoFrame);
    v->verticalHeader()->hide();
    v->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    v->setAlternatingRowColors(true);
    v->setSelectionBehavior(QAbstractItemView::SelectRows);
    v->setSelectionMode(QAbstractItemView::SingleSelection);
    v->setEditTriggers(QAbstractItemView::NoEditTriggers);
    v->setShowGrid(false);
    v->setWordWrap(false);
    v->setTextElideMode(Qt::ElideRight);
    v->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    v->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    v->setCornerButtonEnabled(false);
    QHeaderView* h = v->horizontalHeader();
    h->setSectionResizeMode(QHeaderView::Interactive);
    h->setStretchLastSection(true);
    h->setHighlightSections(false);
    h->setSortIndicatorShown(true);
    return v;
}

ui::BarCellDelegate* rlv_bar_delegate(QObject* parent) {
    auto* d = new ui::BarCellDelegate(ui::BarCellDelegate::Mode::Bar, parent);
    d->set_value_role(kRlvValueRole);
    return d;
}

QLabel* rlv_label(const char* object_name, QWidget* parent, bool wrap = true) {
    auto* l = new QLabel(parent);
    l->setObjectName(QLatin1String(object_name));
    l->setTextFormat(Qt::PlainText);
    l->setWordWrap(wrap);
    l->setContentsMargins(8, 3, 8, 3);
    l->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    l->setVisible(false); // shown once it has text (rlv_set_text)
    return l;
}

QLabel* rlv_section(QWidget* parent, const QString& title) {
    QLabel* l = rlv_label("rlvSection", parent, false);
    l->setText(title.toUpper());
    l->setAccessibleName(title);
    l->setVisible(true);
    return l;
}

void rlv_set_text(QLabel* l, const QString& text, const QString& tip = QString()) {
    if (l->text() != text)
        l->setText(text);
    if (l->toolTip() != tip)
        l->setToolTip(tip);
    const bool on = !text.isEmpty();
    if (l->isVisibleTo(l->parentWidget()) != on)
        l->setVisible(on);
}

/// A pane: no margins, an optional section title on top. Returns its layout.
QVBoxLayout* rlv_pane(QWidget* pane, const QString& title) {
    pane->setObjectName(QStringLiteral("rlvPane"));
    auto* v = new QVBoxLayout(pane);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    if (!title.isEmpty())
        v->addWidget(rlv_section(pane, title));
    return v;
}

QSplitter* rlv_splitter(Qt::Orientation o, QWidget* parent) {
    auto* s = new QSplitter(o, parent);
    s->setObjectName(QStringLiteral("rlvSplit"));
    s->setChildrenCollapsible(false);
    s->setHandleWidth(1);
    return s;
}

QVector<double> rlv_spark(const CorrelationSet& c, const QString& symbol) {
    auto it = c.spark.constFind(symbol.trimmed().toUpper());
    if (it == c.spark.constEnd())
        it = c.spark.constFind(symbol);
    return it == c.spark.constEnd() ? QVector<double>() : it.value();
}

/// One token-built stylesheet for the whole widget (object-name selectors), set before any
/// child exists; the theme manager's re-style pass keeps it in step with theme and font.
QString rlv_stylesheet() {
    const QString base = colors::BG_BASE, surface = colors::BG_SURFACE, bdim = colors::BORDER_DIM,
                  tp = colors::TEXT_PRIMARY, ts = colors::TEXT_SECONDARY, tt = colors::TEXT_TERTIARY,
                  accent = colors::AMBER, abg = colors::ACCENT_BG, alt = colors::ROW_ALT;
    QString css;
    css += QStringLiteral("#rlvRoot,#rlvBody,#rlvPage,#rlvPane{background:%1;}").arg(base);
    css += QStringLiteral("#rlvStrip{background:%1;border:none;border-bottom:1px solid %2;}").arg(surface, bdim);
    css += QStringLiteral("QTabBar#rlvTabs{background:transparent;border:none;}");
    css += QStringLiteral("QTabBar#rlvTabs::tab{background:transparent;color:%1;padding:3px 10px;border:none;"
                          "border-bottom:2px solid transparent;font-weight:700;font-size:%2px;}")
               .arg(ts)
               .arg(ui::fonts::px(-3));
    css += QStringLiteral("QTabBar#rlvTabs::tab:selected{color:%1;border-bottom-color:%1;background:%2;}").arg(accent, base);
    css += QStringLiteral("QTabBar#rlvTabs::tab:hover{color:%1;}").arg(tp);
    css += QStringLiteral("QTabBar#rlvTabs QToolButton{background:%1;border:none;color:%2;}").arg(surface, ts);
    css += QStringLiteral("QLabel#rlvSource{color:%1;background:transparent;font-size:%2px;}")
               .arg(tt)
               .arg(ui::fonts::px(-4));
    css += QStringLiteral("QLabel#rlvCaption{color:%1;background:transparent;font-size:%2px;}")
               .arg(tt)
               .arg(ui::fonts::px(-3));
    css += QStringLiteral("QLabel#rlvHeadline{color:%1;background:transparent;font-weight:700;font-size:%2px;}")
               .arg(tp)
               .arg(ui::fonts::px(-1));
    css += QStringLiteral("QLabel#rlvSection{color:%1;background:transparent;font-weight:700;letter-spacing:1px;"
                          "font-size:%2px;}")
               .arg(tt)
               .arg(ui::fonts::px(-4));
    css += QStringLiteral("QTableView#rlvTable{background:%1;alternate-background-color:%2;border:none;color:%3;"
                          "selection-background-color:%4;selection-color:%3;font-size:%5px;}")
               .arg(base, alt, tp, abg)
               .arg(ui::fonts::px(-2));
    css += QStringLiteral("QTableView#rlvTable QHeaderView{background:%1;border:none;}").arg(surface);
    css += QStringLiteral("QTableView#rlvTable QHeaderView::section{background:%1;color:%2;border:none;"
                          "border-bottom:1px solid %3;padding:2px 6px;font-weight:700;font-size:%4px;}")
               .arg(surface, tt, bdim)
               .arg(ui::fonts::px(-4));
    css += QStringLiteral("QSplitter#rlvSplit::handle{background:%1;}").arg(bdim);
    return css;
}

} // namespace

// ═════════════════════════════════════════════════════════════════════════════
// Construction
// ═════════════════════════════════════════════════════════════════════════════

RelationshipLinkedViews::RelationshipLinkedViews(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("rlvRoot"));
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(rlv_stylesheet()); // once, before any child exists
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    pages_.resize(kPageCount);
    build_ui();
    apply_metrics();
    refresh_pages(); // nothing yet: every tab shows its loading skeleton

    // Font size / theme settled: row heights and column widths follow the font; the donut's
    // categorical colours are taken when its slices are set, so they are set again.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() {
        apply_metrics();
        if (has_data_)
            refresh_decor();
    });
    // Number / currency format changed: the models refresh themselves (dataChanged, no reset);
    // captions, tab counts and the charts are re-rendered from the data in memory.
    fmt::on_format_changed(this, [this]() {
        if (!has_data_)
            return;
        refresh_decor();
        sync_tabs();
    });
}

void RelationshipLinkedViews::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* strip = new QWidget(this);
    strip->setObjectName(QStringLiteral("rlvStrip"));
    strip->setAttribute(Qt::WA_StyledBackground, true);
    strip->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto* sl = new QHBoxLayout(strip);
    sl->setContentsMargins(4, 0, 8, 0);
    sl->setSpacing(8);
    tabs_ = new QTabBar(strip);
    tabs_->setObjectName(QStringLiteral("rlvTabs"));
    tabs_->setAccessibleName(tr("Relationship views"));
    tabs_->setUsesScrollButtons(true);
    tabs_->setExpanding(false);
    tabs_->setElideMode(Qt::ElideNone);
    tabs_->setDrawBase(false);
    tabs_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    sl->addWidget(tabs_, 1);
    source_ = new QLabel(strip);
    source_->setObjectName(QStringLiteral("rlvSource"));
    source_->setTextFormat(Qt::PlainText);
    source_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    source_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    source_->setMinimumWidth(1); // gives way to the tabs when narrow (full text in the tooltip)
    sl->addWidget(source_, 0, Qt::AlignVCenter);
    root->addWidget(strip);

    // One content area showing only the current page (hidden pages take no space, so the
    // area never takes the tallest page's size).
    body_ = new QWidget(this);
    body_->setObjectName(QStringLiteral("rlvBody"));
    body_->setAttribute(Qt::WA_StyledBackground, true);
    body_layout_ = new QVBoxLayout(body_);
    body_layout_->setContentsMargins(0, 0, 0, 0);
    body_layout_->setSpacing(0);
    root->addWidget(body_, 1);

    build_ownership();
    build_holder_page(kPageHolders, &holders_caption_, &t_holders_);
    build_holder_page(kPageFunds, &funds_caption_, &t_funds_);
    build_insiders();
    build_management();
    build_peers();
    build_correlation();
    build_subsidiaries();
    build_holdings();
    build_supply_chain();

    connect(tabs_, &QTabBar::currentChanged, this, [this](int index) {
        if (index < 0)
            return;
        const int page = tabs_->tabData(index).toInt();
        wanted_ = page;
        show_page(page);
    });
}

QWidget* RelationshipLinkedViews::make_page(int page, const QString& key, const char* section_id,
                                            const QString& title, const QString& what) {
    Page& pg = pages_[page];
    pg.key = key;
    pg.section = QString::fromLatin1(section_id);
    pg.title = title;
    pg.what = what;
    pg.root = new QWidget(body_);
    pg.root->setObjectName(QStringLiteral("rlvPage"));
    pg.root->setVisible(false);
    pg.root_layout = new QVBoxLayout(pg.root);
    pg.root_layout->setContentsMargins(0, 0, 0, 0);
    pg.root_layout->setSpacing(0);
    pg.content = new QWidget(pg.root);
    pg.content->setObjectName(QStringLiteral("rlvPane"));
    pg.root_layout->addWidget(pg.content, 1);
    body_layout_->addWidget(pg.root, 1);
    return pg.content;
}

int RelationshipLinkedViews::add_table(int page, QTableView* view, QAbstractTableModel* model) {
    auto* proxy = new ui::KeySortProxy(view);
    proxy->setSourceModel(model);
    proxy->setSortRole(kRlvSortRole);
    proxy->setDynamicSortFilter(true);
    view->setModel(proxy);
    // Source order (rank) until the user sorts; the sort survives every set_data().
    view->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
    view->setSortingEnabled(true);
    fmt::refresh_model_on_format_change(model);

    const int index = static_cast<int>(tables_.size());
    Table t;
    t.view = view;
    t.model = model;
    t.proxy = proxy;
    t.page = page;
    tables_.append(t);
    pages_[page].tables.append(index);
    connect(view, &QAbstractItemView::clicked, this, [this, index](const QModelIndex& i) { on_view_clicked(index, i); });
    connect(view, &QAbstractItemView::doubleClicked, this,
            [this, index](const QModelIndex& i) { on_view_double_clicked(index, i); });
    connect(view->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this, index](const QModelIndex& current, const QModelIndex&) { on_view_current_changed(index, current); });
    return index;
}

void RelationshipLinkedViews::build_ownership() {
    QWidget* content = make_page(kPageOwnership, QStringLiteral("ownership"), section::kHolders, tr("OWNERSHIP"),
                                 tr("ownership data"));
    QVBoxLayout* lay = rlv_pane(content, QString());
    QSplitter* split = rlv_splitter(Qt::Horizontal, content);
    lay->addWidget(split, 1);

    auto* left = new QWidget(split);
    split->addWidget(left);
    QVBoxLayout* lv = rlv_pane(left, tr("Breakdown"));
    donut_ = new ui::DonutChart(left);
    donut_->setAccessibleName(tr("Ownership breakdown"));
    donut_->set_max_slices(0);
    donut_->set_show_percent(false);
    donut_->set_value_formatter([](double v) { return fmt::pct(v, 1, false); });
    lv->addWidget(donut_, 1);
    connect(donut_, &ui::DonutChart::slice_clicked, this, [this](const QString& id) {
        if (id == QLatin1String("institutions"))
            open_view(QStringLiteral("holders"));
        else if (id == QLatin1String("insiders"))
            open_view(QStringLiteral("insiders"));
    });

    auto* right = new QWidget(split);
    split->addWidget(right);
    QVBoxLayout* rv = rlv_pane(right, QString());
    own_headline_ = rlv_label("rlvHeadline", right);
    rv->addWidget(own_headline_);
    own_caption_ = rlv_label("rlvCaption", right);
    rv->addWidget(own_caption_);
    rv->addWidget(rlv_section(right, tr("Top holders")));
    QTableView* view = rlv_make_view(right, tr("Top holders"));
    auto* model = new RlvTableModel({{tr("#"), RlvFmt::Int, 3, tr("Rank by % held")},
                                     {tr("Holder"), RlvFmt::Text, 30},
                                     {tr("Type"), RlvFmt::Text, 10},
                                     {tr("% held"), RlvFmt::Percent, 14, tr("Percent of shares outstanding")},
                                     {tr("Value"), RlvFmt::Amount, 11, tr("Position value")}},
                                    view);
    t_owners_ = add_table(kPageOwnership, view, model);
    view->setItemDelegateForColumn(3, rlv_bar_delegate(view));
    rv->addWidget(view, 1);
    split->setStretchFactor(0, 2);
    split->setStretchFactor(1, 3);
}

void RelationshipLinkedViews::build_holder_page(int page, QLabel** caption, int* table) {
    const bool funds = page == kPageFunds;
    QWidget* content = funds ? make_page(page, QStringLiteral("funds"), section::kHolders, tr("FUNDS"), tr("fund holders"))
                             : make_page(page, QStringLiteral("holders"), section::kHolders, tr("HOLDERS"),
                                         tr("institutional holders"));
    QVBoxLayout* lay = rlv_pane(content, QString());
    *caption = rlv_label("rlvCaption", content);
    lay->addWidget(*caption);
    QTableView* view = rlv_make_view(content, funds ? tr("Fund holders") : tr("Institutional holders"));
    auto* model =
        new RlvTableModel({{tr("#"), RlvFmt::Int, 3, tr("Rank by % held")},
                           {funds ? tr("Fund") : tr("Holder"), RlvFmt::Text, 32},
                           {tr("% held"), RlvFmt::Percent, 14, tr("Percent of shares outstanding")},
                           {tr("Shares"), RlvFmt::Shares, 9},
                           {tr("Value"), RlvFmt::Amount, 11, tr("Position value")},
                           {tr("Change"), RlvFmt::PercentChg, 11, tr("Change of the position since the previous report")},
                           {tr("Reported"), RlvFmt::Text, 10, tr("Date of the filing")}},
                          view);
    *table = add_table(page, view, model);
    view->setItemDelegateForColumn(2, rlv_bar_delegate(view));
    lay->addWidget(view, 1);
}

void RelationshipLinkedViews::build_insiders() {
    QWidget* content =
        make_page(kPageInsiders, QStringLiteral("insiders"), section::kInsiders, tr("INSIDERS"), tr("insiders"));
    QVBoxLayout* lay = rlv_pane(content, QString());
    insider_summary_ = rlv_label("rlvCaption", content);
    lay->addWidget(insider_summary_);
    QSplitter* split = rlv_splitter(Qt::Horizontal, content);
    lay->addWidget(split, 1);

    auto* left = new QWidget(split);
    split->addWidget(left);
    QVBoxLayout* lv = rlv_pane(left, tr("Roster"));
    QTableView* roster = rlv_make_view(left, tr("Insider roster"));
    auto* roster_model = new RlvTableModel({{tr("Insider"), RlvFmt::Text, 22},
                                            {tr("Title"), RlvFmt::Text, 20},
                                            {tr("Shares"), RlvFmt::Shares, 9},
                                            {tr("% held"), RlvFmt::Percent, 8, tr("Percent of shares outstanding")},
                                            {tr("Last"), RlvFmt::Tag, 11, tr("Last transaction")},
                                            {tr("Date"), RlvFmt::Text, 10, tr("Date of the last transaction")}},
                                           roster);
    t_roster_ = add_table(kPageInsiders, roster, roster_model);
    lv->addWidget(roster, 1);

    QSplitter* right = rlv_splitter(Qt::Vertical, split);
    split->addWidget(right);
    auto* act = new QWidget(right);
    right->addWidget(act);
    QVBoxLayout* av = rlv_pane(act, tr("Activity, last 12 months"));
    activity_ = new RelInsiderActivityChart(act);
    activity_->setAccessibleName(tr("Insider activity timeline"));
    av->addWidget(activity_, 1);
    activity_caption_ = rlv_label("rlvCaption", act);
    av->addWidget(activity_caption_);

    auto* tx = new QWidget(right);
    right->addWidget(tx);
    QVBoxLayout* tv = rlv_pane(tx, tr("Transactions"));
    QTableView* trades = rlv_make_view(tx, tr("Insider transactions"));
    auto* trades_model = new RlvTableModel({{tr("Date"), RlvFmt::Text, 10},
                                            {tr("Insider"), RlvFmt::Text, 22},
                                            {tr("Position"), RlvFmt::Text, 18},
                                            {tr("Kind"), RlvFmt::Tag, 11},
                                            {tr("Shares"), RlvFmt::Shares, 9},
                                            {tr("Value"), RlvFmt::Money, 10, tr("Value in the trade's currency")},
                                            {tr("Price"), RlvFmt::Price, 10, tr("Price per share in the trade's currency")}},
                                           trades);
    t_trades_ = add_table(kPageInsiders, trades, trades_model);
    tv->addWidget(trades, 1);

    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 1);
    right->setStretchFactor(0, 0);
    right->setStretchFactor(1, 1);
}

void RelationshipLinkedViews::build_management() {
    QWidget* content = make_page(kPageManagement, QStringLiteral("management"), section::kManagement, tr("MANAGEMENT"),
                                 tr("officers"));
    QVBoxLayout* lay = rlv_pane(content, QString());
    mgmt_caption_ = rlv_label("rlvCaption", content);
    lay->addWidget(mgmt_caption_);
    QTableView* view = rlv_make_view(content, tr("Officers"));
    auto* model = new RlvTableModel({{tr("Name"), RlvFmt::Text, 24},
                                     {tr("Title"), RlvFmt::Text, 28},
                                     {tr("Age"), RlvFmt::Int, 5},
                                     {tr("Pay"), RlvFmt::Amount, 10, tr("Total pay, last fiscal year")},
                                     {tr("Shares"), RlvFmt::Shares, 9, tr("Shares held (insider roster)")},
                                     {tr("% held"), RlvFmt::Percent, 8, tr("Percent of shares outstanding")},
                                     {tr("Last"), RlvFmt::Tag, 11, tr("Last transaction (insider roster)")},
                                     {tr("Date"), RlvFmt::Text, 10, tr("Date of the last transaction")}},
                                    view);
    t_mgmt_ = add_table(kPageManagement, view, model);
    lay->addWidget(view, 1);
}

void RelationshipLinkedViews::build_peers() {
    QWidget* content = make_page(kPagePeers, QStringLiteral("peers"), section::kPeers, tr("PEERS"), tr("peers"));
    QVBoxLayout* lay = rlv_pane(content, QString());
    peers_caption_ = rlv_label("rlvCaption", content);
    lay->addWidget(peers_caption_);
    QSplitter* split = rlv_splitter(Qt::Vertical, content);
    lay->addWidget(split, 1);

    QTableView* view = rlv_make_view(split, tr("Peer comparison"));
    split->addWidget(view);
    auto* model = new RlvTableModel(
        {{tr("Ticker"), RlvFmt::Text, 8},
         {tr("Name"), RlvFmt::Text, 22},
         {tr("Mkt cap"), RlvFmt::Amount, 10, tr("Market capitalisation in the company's own currency")},
         {tr("Price"), RlvFmt::Price, 10, tr("Last price in the company's quote currency")},
         {tr("P/E"), RlvFmt::Ratio, 7, tr("Trailing price / earnings")},
         {tr("EV/EBITDA"), RlvFmt::Ratio, 9, tr("Enterprise value / EBITDA")},
         {tr("Rev. growth"), RlvFmt::FractionChg, 9, tr("Revenue growth, year over year")},
         {tr("Gross m."), RlvFmt::Fraction, 8, tr("Gross margin")},
         {tr("Op. m."), RlvFmt::Fraction, 8, tr("Operating margin")},
         {tr("Net m."), RlvFmt::Fraction, 8, tr("Net margin")},
         {tr("ROE"), RlvFmt::Fraction, 8, tr("Return on equity")},
         {tr("52w"), RlvFmt::FractionChg, 8, tr("52-week price change")},
         {tr("ρ"), RlvFmt::Corr, 6, tr("Correlation of daily returns with the company over one year")},
         {tr("1y"), RlvFmt::Spark, 12, tr("Weekly closes over one year, rebased to 100")}},
        view);
    t_peers_ = add_table(kPagePeers, view, model);
    auto* spark = new ui::SparklineDelegate(view);
    ui::SparklineStyle style;
    style.baseline = 100.0; // rebased series: the dotted guide is the start of the year
    style.show_baseline = true;
    spark->set_spark_style(style);
    spark->set_width_chars(12);
    view->setItemDelegateForColumn(13, spark);

    auto* bullets_pane = new QWidget(split);
    split->addWidget(bullets_pane);
    QVBoxLayout* bv = rlv_pane(bullets_pane, tr("Against the peers"));
    bullets_ = new RelPeerBulletChart(bullets_pane);
    bullets_->setAccessibleName(tr("Company against its peers"));
    bv->addWidget(bullets_, 1);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 0);
}

void RelationshipLinkedViews::build_correlation() {
    QWidget* content = make_page(kPageCorrelation, QStringLiteral("correlation"), section::kCorrelation,
                                 tr("CORRELATION"), tr("correlations"));
    QVBoxLayout* lay = rlv_pane(content, QString());
    corr_caption_ = rlv_label("rlvCaption", content);
    lay->addWidget(corr_caption_);
    QSplitter* split = rlv_splitter(Qt::Horizontal, content);
    lay->addWidget(split, 1);

    heatmap_ = new ui::MatrixHeatmap(split);
    split->addWidget(heatmap_);
    heatmap_->setAccessibleName(tr("Correlation matrix"));
    heatmap_->set_diverging_scale(-1.0, 1.0, 0.0);
    heatmap_->set_show_values(true);
    heatmap_->set_value_formatter([](double v) { return fmt::number(v, 2); });
    connect(heatmap_, &ui::MatrixHeatmap::cell_clicked, this, [this](int, int col) {
        if (col < 0 || col >= heatmap_ids_.size())
            return;
        const QString id = heatmap_ids_[col];
        if (id.isEmpty())
            return;
        selected_id_ = id;
        highlight(id, -1, true);
        emit node_selected(id);
    });

    auto* right = new QWidget(split);
    split->addWidget(right);
    QVBoxLayout* rv = rlv_pane(right, tr("Against the company"));
    QTableView* view = rlv_make_view(right, tr("Correlation list"));
    auto* model = new RlvTableModel({{tr("Symbol"), RlvFmt::Text, 8},
                                     {tr("Name"), RlvFmt::Text, 20},
                                     {tr("Role"), RlvFmt::Text, 9},
                                     {tr("ρ"), RlvFmt::Corr, 6, tr("Correlation of daily returns with the company")},
                                     {tr("β"), RlvFmt::Corr, 6, tr("The company's beta against this symbol")},
                                     {tr("1y return"), RlvFmt::FractionChg, 9, tr("Total return over the window")}},
                                    view);
    t_corr_ = add_table(kPageCorrelation, view, model);
    rv->addWidget(view, 1);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);
}

void RelationshipLinkedViews::build_subsidiaries() {
    QWidget* content = make_page(kPageSubsidiaries, QStringLiteral("subsidiaries"), section::kSubsidiaries,
                                 tr("SUBSIDIARIES"), tr("subsidiaries"));
    QVBoxLayout* lay = rlv_pane(content, QString());
    subs_caption_ = rlv_label("rlvCaption", content);
    lay->addWidget(subs_caption_);
    QSplitter* split = rlv_splitter(Qt::Horizontal, content);
    lay->addWidget(split, 1);

    QTableView* subs = rlv_make_view(split, tr("Subsidiaries"));
    split->addWidget(subs);
    auto* subs_model =
        new RlvTableModel({{tr("Subsidiary"), RlvFmt::Text, 40}, {tr("Jurisdiction"), RlvFmt::Text, 20}}, subs);
    t_subs_ = add_table(kPageSubsidiaries, subs, subs_model);

    auto* right = new QWidget(split);
    split->addWidget(right);
    QVBoxLayout* rv = rlv_pane(right, tr("By jurisdiction"));
    QTableView* juris = rlv_make_view(right, tr("Subsidiaries by jurisdiction"));
    auto* juris_model =
        new RlvTableModel({{tr("Jurisdiction"), RlvFmt::Text, 20}, {tr("Count"), RlvFmt::Int, 12}}, juris);
    t_juris_ = add_table(kPageSubsidiaries, juris, juris_model);
    juris->setItemDelegateForColumn(1, rlv_bar_delegate(juris));
    rv->addWidget(juris, 1);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);
}

void RelationshipLinkedViews::build_holdings() {
    QWidget* content =
        make_page(kPageHoldings, QStringLiteral("holdings"), section::kFund, tr("HOLDINGS"), tr("fund holdings"));
    QVBoxLayout* lay = rlv_pane(content, QString());
    holdings_caption_ = rlv_label("rlvCaption", content);
    lay->addWidget(holdings_caption_);
    QSplitter* split = rlv_splitter(Qt::Horizontal, content);
    lay->addWidget(split, 1);

    QTableView* holdings = rlv_make_view(split, tr("Fund holdings"));
    split->addWidget(holdings);
    auto* holdings_model = new RlvTableModel({{tr("Symbol"), RlvFmt::Text, 8},
                                              {tr("Name"), RlvFmt::Text, 28},
                                              {tr("Weight"), RlvFmt::Percent, 14, tr("Percent of the fund")},
                                              {tr("Value"), RlvFmt::Amount, 11, tr("Weight x total assets")}},
                                             holdings);
    t_holdings_ = add_table(kPageHoldings, holdings, holdings_model);
    holdings->setItemDelegateForColumn(2, rlv_bar_delegate(holdings));

    auto* right = new QWidget(split);
    split->addWidget(right);
    QVBoxLayout* rv = rlv_pane(right, tr("Sectors"));
    QTableView* sectors = rlv_make_view(right, tr("Sector weights"));
    auto* sectors_model = new RlvTableModel(
        {{tr("Sector"), RlvFmt::Text, 18}, {tr("Weight"), RlvFmt::Percent, 14, tr("Percent of the fund")}}, sectors);
    t_sectors_ = add_table(kPageHoldings, sectors, sectors_model);
    sectors->setItemDelegateForColumn(1, rlv_bar_delegate(sectors));
    rv->addWidget(sectors, 1);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);
}

void RelationshipLinkedViews::build_supply_chain() {
    QWidget* content = make_page(kPageSupplyChain, QStringLiteral("supply_chain"), section::kSupplyChain,
                                 tr("SUPPLY CHAIN"), tr("customers and suppliers"));
    QVBoxLayout* lay = rlv_pane(content, QString());
    supply_caption_ = rlv_label("rlvCaption", content);
    lay->addWidget(supply_caption_);
    QTableView* view = rlv_make_view(content, tr("Customers and suppliers"));
    auto* model = new RlvTableModel({{tr("Name"), RlvFmt::Text, 30},
                                     {tr("Relationship"), RlvFmt::Text, 12},
                                     {tr("% of revenue"), RlvFmt::Percent, 10}},
                                    view);
    t_supply_ = add_table(kPageSupplyChain, view, model);
    lay->addWidget(view, 1);
}

void RelationshipLinkedViews::apply_metrics() {
    const QFontMetrics fm(fonts::font(kRlvTableFont));
    const int row_h = std::max(16, fm.height() + 6);
    const int avg = std::max(5, fm.averageCharWidth());
    for (const Table& t : tables_) {
        t.view->verticalHeader()->setDefaultSectionSize(row_h);
        const QVector<RlvColumn>& cols = rlv_model(t.model)->columns();
        QHeaderView* h = t.view->horizontalHeader();
        for (int c = 0; c < cols.size(); ++c)
            h->resizeSection(c, cols[c].chars * avg + 14);
    }
    if (donut_)
        donut_->setMinimumWidth(avg * 24);
    if (activity_)
        activity_->updateGeometry();
    if (bullets_)
        bullets_->updateGeometry();
}

// ═════════════════════════════════════════════════════════════════════════════
// Data
// ═════════════════════════════════════════════════════════════════════════════

void RelationshipLinkedViews::set_data(const RelationshipData& data, const RelGraphModel& model) {
    data_ = data;
    center_id_ = model.center_id.isEmpty() ? center_node_id(data.company.ticker) : model.center_id;
    graph_index_ = model.index;
    has_data_ = true;
    fill_ownership();
    fill_holders(kPageHolders, t_holders_, false);
    fill_holders(kPageFunds, t_funds_, true);
    fill_insiders();
    fill_management();
    fill_peers();
    fill_correlation();
    fill_subsidiaries();
    fill_holdings();
    fill_supply_chain();
    refresh_decor();
    refresh_pages();
    highlight(selected_id_, -1, false); // rows that came back after a reset are selected again
}

void RelationshipLinkedViews::fill_ownership() {
    struct Entry {
        const InstitutionalHolder* holder = nullptr;
        bool fund = false;
    };
    QVector<Entry> all;
    for (const auto& h : data_.institutional_holders)
        all.append({&h, false});
    for (const auto& h : data_.mutualfund_holders)
        all.append({&h, true});
    std::stable_sort(all.begin(), all.end(),
                     [](const Entry& a, const Entry& b) { return a.holder->percentage > b.holder->percentage; });
    QVector<RlvRow> rows;
    rows.reserve(all.size());
    int rank = 0;
    for (const Entry& e : all) {
        const InstitutionalHolder& h = *e.holder;
        RlvRow r;
        r.node_id = node_id(e.fund ? RelGroup::Funds : RelGroup::Institutions, h.name);
        r.cells = {rlv_num(RlvFmt::Int, ++rank),
                   rlv_text(holder_display_name(h.name), h.name),
                   rlv_text(e.fund ? tr("Fund") : tr("Institution")),
                   rlv_num(RlvFmt::Percent, rlv_pos(h.percentage)),
                   rlv_num(RlvFmt::Amount, rlv_pos(h.value), data_.company.currency)};
        rows.append(r);
    }
    const Table& t = tables_[t_owners_];
    rlv_apply_rows(t.view, t.model, std::move(rows));
    Page& pg = pages_[kPageOwnership];
    pg.has_rows = data_.major_holders.present || !all.isEmpty() || data_.company.institutional_percent > 0 ||
                  data_.company.insider_percent > 0;
    pg.count = -1;
}

void RelationshipLinkedViews::fill_holders(int page, int table, bool funds) {
    const QVector<InstitutionalHolder>& list = funds ? data_.mutualfund_holders : data_.institutional_holders;
    QVector<const InstitutionalHolder*> sorted;
    sorted.reserve(list.size());
    for (const auto& h : list)
        sorted.append(&h);
    std::stable_sort(sorted.begin(), sorted.end(), [](const InstitutionalHolder* a, const InstitutionalHolder* b) {
        return a->percentage > b->percentage;
    });
    QVector<RlvRow> rows;
    rows.reserve(sorted.size());
    int rank = 0;
    for (const InstitutionalHolder* h : sorted) {
        QString tip = h->name;
        if (!h->fund_family.isEmpty() && h->fund_family != h->name)
            tip += QLatin1Char('\n') + tr("Family: %1").arg(h->fund_family);
        RlvRow r;
        r.node_id = node_id(funds ? RelGroup::Funds : RelGroup::Institutions, h->name);
        r.cells = {rlv_num(RlvFmt::Int, ++rank),
                   rlv_text(holder_display_name(h->name), tip),
                   rlv_num(RlvFmt::Percent, rlv_pos(h->percentage)),
                   rlv_num(RlvFmt::Shares, rlv_pos(h->shares)),
                   rlv_num(RlvFmt::Amount, rlv_pos(h->value), data_.company.currency),
                   rlv_num(RlvFmt::PercentChg, std::isfinite(h->change_percent) ? h->change_percent : kRlvNaN),
                   rlv_text(h->date_reported)};
        rows.append(r);
    }
    const Table& t = tables_[table];
    rlv_apply_rows(t.view, t.model, std::move(rows));
    Page& pg = pages_[page];
    pg.has_rows = !list.isEmpty();
    pg.count = static_cast<int>(list.size());
}

QString RelationshipLinkedViews::insider_id(const QString& roster_name) const {
    const QString n = roster_name.trimmed();
    if (n.isEmpty())
        return QString();
    // An officer on the roster is drawn as the officer (Management), as on the graph.
    for (const auto& o : data_.officers)
        if (same_person(o.name, n))
            return node_id(RelGroup::Management, o.name);
    for (const auto& ih : data_.insider_holders)
        if (ih.name.trimmed().compare(n, Qt::CaseInsensitive) == 0)
            return node_id(RelGroup::Insiders, ih.name);
    return QString();
}

void RelationshipLinkedViews::fill_insiders() {
    QHash<QString, QString> ids; // insider name -> node id (names repeat across trades)
    const auto id_of = [this, &ids](const QString& name) {
        const auto it = ids.constFind(name);
        if (it != ids.constEnd())
            return it.value();
        const QString id = insider_id(name);
        ids.insert(name, id);
        return id;
    };

    QVector<const InsiderHolder*> roster;
    roster.reserve(data_.insider_holders.size());
    for (const auto& ih : data_.insider_holders)
        roster.append(&ih);
    std::stable_sort(roster.begin(), roster.end(),
                     [](const InsiderHolder* a, const InsiderHolder* b) { return a->shares > b->shares; });
    QVector<RlvRow> rows;
    rows.reserve(roster.size());
    for (const InsiderHolder* ih : roster) {
        RlvRow r;
        r.node_id = id_of(ih->name);
        r.cells = {rlv_text(ih->name),
                   rlv_text(ih->title),
                   rlv_num(RlvFmt::Shares, rlv_pos(ih->shares)),
                   rlv_num(RlvFmt::Percent, rlv_pos(ih->percentage)),
                   rlv_tag(ih->last_transaction, ih->last_transaction_text),
                   rlv_text(ih->last_date)};
        rows.append(r);
    }
    const Table& rt = tables_[t_roster_];
    rlv_apply_rows(rt.view, rt.model, std::move(rows));

    QVector<RlvRow> trades;
    trades.reserve(data_.insider_trades.size());
    for (const RelInsiderTrade& t : data_.insider_trades) {
        QString tip = t.text;
        if (t.ownership == QLatin1String("D"))
            tip += (tip.isEmpty() ? QString() : QStringLiteral("\n")) + tr("Direct ownership");
        else if (t.ownership == QLatin1String("I"))
            tip += (tip.isEmpty() ? QString() : QStringLiteral("\n")) + tr("Indirect ownership");
        RlvRow r;
        r.node_id = id_of(t.insider);
        r.cells = {rlv_text(t.date),
                   rlv_text(t.insider),
                   rlv_text(t.position),
                   rlv_tag(t.kind, tip),
                   rlv_num(RlvFmt::Shares, rlv_pos(std::isfinite(t.shares) ? std::abs(t.shares) : kRlvNaN)),
                   rlv_num(RlvFmt::Money, rlv_pos(t.value), t.currency),
                   rlv_num(RlvFmt::Price, rlv_pos(t.price), t.currency)};
        trades.append(r);
    }
    const Table& tt = tables_[t_trades_];
    rlv_apply_rows(tt.view, tt.model, std::move(trades));

    Page& pg = pages_[kPageInsiders];
    pg.has_rows = !data_.insider_holders.isEmpty() || !data_.insider_trades.isEmpty() || data_.insider_summary.present;
    pg.count = data_.insider_holders.isEmpty() ? -1 : static_cast<int>(data_.insider_holders.size());
}

void RelationshipLinkedViews::fill_management() {
    const int year = QDate::currentDate().year();
    const QString pay_ccy = statement_currency(data_.company.financial_currency, data_.company.currency);
    QVector<RlvRow> rows;
    rows.reserve(data_.officers.size());
    for (const CompanyOfficer& o : data_.officers) {
        const InsiderHolder* ih = nullptr;
        for (const auto& h : data_.insider_holders) {
            if (same_person(o.name, h.name)) {
                ih = &h;
                break;
            }
        }
        RlvRow r;
        r.node_id = node_id(RelGroup::Management, o.name);
        r.cells = {rlv_text(o.name),
                   rlv_text(o.title),
                   rlv_num(RlvFmt::Int, o.year_born > 0 ? static_cast<double>(year - o.year_born) : kRlvNaN),
                   rlv_num(RlvFmt::Amount, rlv_pos(o.total_pay), pay_ccy),
                   rlv_num(RlvFmt::Shares, ih ? rlv_pos(ih->shares) : kRlvNaN),
                   rlv_num(RlvFmt::Percent, ih ? rlv_pos(ih->percentage) : kRlvNaN),
                   rlv_tag(ih ? ih->last_transaction : QString(), ih ? ih->last_transaction_text : QString()),
                   rlv_text(ih ? ih->last_date : QString())};
        rows.append(r);
    }
    const Table& t = tables_[t_mgmt_];
    rlv_apply_rows(t.view, t.model, std::move(rows));
    Page& pg = pages_[kPageManagement];
    pg.has_rows = !data_.officers.isEmpty();
    pg.count = static_cast<int>(data_.officers.size());
}

void RelationshipLinkedViews::fill_peers() {
    const CompanyInfo& co = data_.company;
    const CorrelationSet& corr = data_.correlation;
    const QString ticker = co.ticker.trimmed().toUpper();
    QVector<RlvRow> rows;
    rows.reserve(data_.peers.size() + 1);
    if (!ticker.isEmpty()) {
        const double net = data_.margins.net != 0 ? data_.margins.net : co.profit_margins;
        RlvRow r;
        r.node_id = center_id_;
        r.accent = true;
        r.cells = {rlv_text(ticker, tr("%1 (this company)").arg(co.name.isEmpty() ? ticker : co.name)),
                   rlv_text(co.name),
                   rlv_num(RlvFmt::Amount, rlv_pos(co.market_cap), co.currency),
                   rlv_num(RlvFmt::Price, rlv_pos(co.current_price), co.currency),
                   rlv_num(RlvFmt::Ratio, rlv_pos(co.pe_ratio)),
                   rlv_num(RlvFmt::Ratio, rlv_pos(data_.enterprise.ev_to_ebitda)),
                   rlv_num(RlvFmt::FractionChg, rlv_nz(co.revenue_growth)),
                   rlv_num(RlvFmt::Fraction, rlv_nz(data_.margins.gross)),
                   rlv_num(RlvFmt::Fraction, rlv_nz(data_.margins.operating)),
                   rlv_num(RlvFmt::Fraction, rlv_nz(net)),
                   rlv_num(RlvFmt::Fraction, rlv_nz(co.roe)),
                   rlv_num(RlvFmt::FractionChg, rlv_nz(data_.technicals.week52_change_pct)),
                   rlv_num(RlvFmt::Corr, kRlvNaN),
                   rlv_spark_cell()};
        r.spark = rlv_spark(corr, ticker);
        rows.append(r);
    }
    for (const PeerCompany& p : data_.peers) {
        RlvRow r;
        r.node_id = node_id(RelGroup::Peers, p.ticker);
        QString tip = p.name.isEmpty() ? p.ticker : p.name;
        const QString where = p.industry.isEmpty() ? p.sector : p.industry;
        if (!where.isEmpty())
            tip += QLatin1Char('\n') + where;
        r.cells = {rlv_text(p.ticker, tip),
                   rlv_text(p.name),
                   rlv_num(RlvFmt::Amount, rlv_pos(p.market_cap), p.currency),
                   rlv_num(RlvFmt::Price, rlv_pos(p.current_price), p.currency),
                   rlv_num(RlvFmt::Ratio, rlv_pos(p.pe_ratio)),
                   rlv_num(RlvFmt::Ratio, rlv_pos(p.ev_to_ebitda)),
                   rlv_num(RlvFmt::FractionChg, rlv_nz(p.revenue_growth)),
                   rlv_num(RlvFmt::Fraction, rlv_nz(p.gross_margins)),
                   rlv_num(RlvFmt::Fraction, rlv_nz(p.operating_margins)),
                   rlv_num(RlvFmt::Fraction, rlv_nz(p.profit_margins)),
                   rlv_num(RlvFmt::Fraction, rlv_nz(p.roe)),
                   rlv_num(RlvFmt::FractionChg, rlv_nz(p.week52_change)),
                   rlv_num(RlvFmt::Corr, corr.value(ticker, p.ticker)),
                   rlv_spark_cell()};
        r.spark = rlv_spark(corr, p.ticker);
        rows.append(r);
    }
    const Table& t = tables_[t_peers_];
    rlv_apply_rows(t.view, t.model, std::move(rows));
    Page& pg = pages_[kPagePeers];
    pg.has_rows = !data_.peers.isEmpty();
    pg.count = static_cast<int>(data_.peers.size());
}

QString RelationshipLinkedViews::id_for_symbol(const QString& symbol) const {
    const QString s = symbol.trimmed().toUpper();
    if (s.isEmpty())
        return QString();
    const QString role = data_.correlation.roles.value(symbol);
    if (role == QLatin1String("self") || s == data_.company.ticker.trimmed().toUpper())
        return center_id_;
    const QString peer = node_id(RelGroup::Peers, s);
    const QString corr = node_id(RelGroup::Correlated, s);
    const QString hold = node_id(RelGroup::Holdings, s);
    if (role == QLatin1String("index") || role == QLatin1String("sector_etf"))
        return corr;
    for (const QString& id : {peer, hold, corr})
        if (graph_index_.contains(id))
            return id;
    for (const auto& p : data_.peers)
        if (p.ticker.trimmed().toUpper() == s)
            return node_id(RelGroup::Peers, p.ticker);
    for (const auto& h : data_.fund.holdings)
        if (h.symbol.trimmed().toUpper() == s)
            return node_id(RelGroup::Holdings, h.symbol);
    return role == QLatin1String("peer") ? peer : QString();
}

void RelationshipLinkedViews::fill_correlation() {
    const CorrelationSet& c = data_.correlation;
    const QString ticker = data_.company.ticker.trimmed().toUpper();
    heatmap_ids_.clear();
    QVector<RlvRow> rows;
    rows.reserve(c.symbols.size());
    for (const QString& sym : c.symbols) {
        const QString id = id_for_symbol(sym);
        heatmap_ids_.append(id);
        const QString role = c.roles.value(sym);
        QString role_text = role;
        if (role == QLatin1String("self"))
            role_text = tr("Company");
        else if (role == QLatin1String("peer"))
            role_text = tr("Peer");
        else if (role == QLatin1String("index"))
            role_text = tr("Index");
        else if (role == QLatin1String("sector_etf"))
            role_text = tr("Sector ETF");
        RlvRow r;
        r.node_id = id;
        r.accent = role == QLatin1String("self");
        r.cells = {rlv_text(sym),
                   rlv_text(c.labels.value(sym, sym)),
                   rlv_text(role_text),
                   rlv_num(RlvFmt::Corr, c.value(ticker, sym)),
                   rlv_num(RlvFmt::Corr, c.beta.value(sym, kRlvNaN)),
                   rlv_num(RlvFmt::FractionChg, c.returns.value(sym, kRlvNaN))};
        rows.append(r);
    }
    const Table& t = tables_[t_corr_];
    rlv_apply_rows(t.view, t.model, std::move(rows));
    Page& pg = pages_[kPageCorrelation];
    pg.has_rows = c.present && !c.symbols.isEmpty();
    pg.count = pg.has_rows ? static_cast<int>(c.symbols.size()) : -1;
}

void RelationshipLinkedViews::fill_subsidiaries() {
    QVector<RlvRow> rows;
    rows.reserve(data_.subsidiaries.size());
    QMap<QString, int> per_place;
    for (const SubsidiaryEntry& s : data_.subsidiaries) {
        RlvRow r;
        r.node_id = node_id(RelGroup::Subsidiaries, s.name);
        r.cells = {rlv_text(s.name), rlv_text(s.jurisdiction)};
        rows.append(r);
        const QString place = s.jurisdiction.trimmed();
        per_place[place.isEmpty() ? tr("Not stated") : place] += 1;
    }
    const Table& t = tables_[t_subs_];
    rlv_apply_rows(t.view, t.model, std::move(rows));

    QVector<QPair<QString, int>> places;
    for (auto it = per_place.constBegin(); it != per_place.constEnd(); ++it)
        places.append({it.key(), it.value()});
    std::stable_sort(places.begin(), places.end(),
                     [](const QPair<QString, int>& a, const QPair<QString, int>& b) { return a.second > b.second; });
    QVector<RlvRow> counts;
    counts.reserve(places.size());
    for (const auto& pl : places) {
        RlvRow r;
        r.cells = {rlv_text(pl.first), rlv_num(RlvFmt::Int, pl.second)};
        counts.append(r);
    }
    const Table& j = tables_[t_juris_];
    rlv_apply_rows(j.view, j.model, std::move(counts));

    Page& pg = pages_[kPageSubsidiaries];
    pg.has_rows = !data_.subsidiaries.isEmpty();
    pg.count = static_cast<int>(data_.subsidiaries.size());
}

void RelationshipLinkedViews::fill_holdings() {
    const FundProfile& f = data_.fund;
    QVector<const FundHolding*> sorted;
    sorted.reserve(f.holdings.size());
    for (const auto& h : f.holdings)
        sorted.append(&h);
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const FundHolding* a, const FundHolding* b) { return a->weight > b->weight; });
    QVector<RlvRow> rows;
    rows.reserve(sorted.size());
    for (const FundHolding* h : sorted) {
        RlvRow r;
        r.node_id = node_id(RelGroup::Holdings, h->symbol.isEmpty() ? h->name : h->symbol);
        const double w = std::isfinite(h->weight) && h->weight > 0 ? h->weight : kRlvNaN;
        r.cells = {rlv_text(h->symbol),
                   rlv_text(h->name.isEmpty() ? h->symbol : h->name),
                   rlv_num(RlvFmt::Percent, w * 100.0),
                   rlv_num(RlvFmt::Amount, f.total_assets > 0 ? w * f.total_assets : kRlvNaN, data_.company.currency)};
        rows.append(r);
    }
    const Table& t = tables_[t_holdings_];
    rlv_apply_rows(t.view, t.model, std::move(rows));

    QVector<RlvRow> sectors;
    sectors.reserve(f.sector_weights.size());
    for (const auto& [key, weight] : f.sector_weights) {
        if (!(weight > 0))
            continue;
        QString label = key;
        label.replace(QLatin1Char('_'), QLatin1Char(' '));
        if (!label.isEmpty())
            label[0] = label[0].toUpper();
        RlvRow r;
        r.node_id = node_id(RelGroup::Sectors, key);
        r.cells = {rlv_text(label), rlv_num(RlvFmt::Percent, weight * 100.0)};
        sectors.append(r);
    }
    const Table& s = tables_[t_sectors_];
    rlv_apply_rows(s.view, s.model, std::move(sectors));

    Page& pg = pages_[kPageHoldings];
    pg.has_rows = !f.holdings.isEmpty() || !f.sector_weights.isEmpty();
    pg.count = f.holdings.isEmpty() ? -1 : static_cast<int>(f.holdings.size());
}

void RelationshipLinkedViews::fill_supply_chain() {
    QVector<RlvRow> rows;
    rows.reserve(data_.supply_chain.size());
    for (const SupplyChainEntity& e : data_.supply_chain) {
        QString rel = e.relationship;
        if (rel == QLatin1String("customer"))
            rel = tr("Customer");
        else if (rel == QLatin1String("supplier"))
            rel = tr("Supplier");
        RlvRow r;
        r.node_id = node_id(RelGroup::SupplyChain, e.name);
        r.cells = {rlv_text(e.name), rlv_text(rel), rlv_num(RlvFmt::Percent, rlv_pos(e.revenue_pct))};
        rows.append(r);
    }
    const Table& t = tables_[t_supply_];
    rlv_apply_rows(t.view, t.model, std::move(rows));
    Page& pg = pages_[kPageSupplyChain];
    pg.has_rows = !data_.supply_chain.isEmpty();
    pg.count = pg.has_rows ? static_cast<int>(data_.supply_chain.size()) : -1;
}

/// Captions and charts — everything that holds formatted text outside the models; called on
/// set_data, on a number-format change and once the appearance has settled.
void RelationshipLinkedViews::refresh_decor() {
    const CompanyInfo& co = data_.company;
    const MajorHolders& mh = data_.major_holders;

    // ── Ownership ──
    double inst = mh.present ? mh.institutions_pct : co.institutional_percent;
    double ins = mh.present ? mh.insiders_pct : co.insider_percent;
    if (!std::isfinite(inst) || inst < 0)
        inst = 0;
    if (!std::isfinite(ins) || ins < 0)
        ins = 0;
    if (inst > 0 || ins > 0) {
        const double rest = std::max(0.0, 1.0 - inst - ins);
        QVector<ui::DonutSlice> slices;
        slices.append({QStringLiteral("institutions"), tr("Institutions"), inst * 100.0, QColor()});
        slices.append({QStringLiteral("insiders"), tr("Insiders"), ins * 100.0, QColor()});
        slices.append({QStringLiteral("retail"), tr("Retail & other"), rest * 100.0, QColor()});
        donut_->set_slices(slices);
        donut_->set_center_text(tr("INSTITUTIONS"), fmt::fraction_pct(inst, 1, false));
        donut_->set_state(ui::PaintedChart::Ready);
    } else {
        donut_->clear();
        donut_->set_state(ui::PaintedChart::Empty, tr("No ownership breakdown reported"));
    }
    QStringList head;
    if (mh.present && mh.institutions_count > 0 && inst > 0)
        head << tr("%1 institutions hold %2 of the shares")
                    .arg(fmt::number(mh.institutions_count, 0), fmt::fraction_pct(inst, 1, false));
    else if (inst > 0)
        head << tr("Institutions hold %1 of the shares").arg(fmt::fraction_pct(inst, 1, false));
    if (mh.present && mh.institutions_float_pct > 0)
        head << tr("%1 of the float").arg(fmt::fraction_pct(mh.institutions_float_pct, 1, false));
    if (ins > 0)
        head << tr("insiders %1").arg(fmt::fraction_pct(ins, 1, false));
    rlv_set_text(own_headline_, head.join(QStringLiteral(" · ")));
    rlv_set_text(own_caption_, pages_[kPageOwnership].has_rows
                                   ? tr("Mutual funds are a subset of the institutions slice, not a slice of their own; "
                                        "retail & other is the rest of the shares outstanding. The list ranks "
                                        "institutions and funds by the share of the company each holds.")
                                   : QString());

    // ── Holders / funds ──
    const auto holders_text = [&](const QVector<InstitutionalHolder>& list, bool funds) -> QString {
        if (list.isEmpty())
            return QString();
        double pct = 0;
        double value = 0;
        for (const auto& h : list) {
            if (std::isfinite(h.percentage) && h.percentage > 0)
                pct += h.percentage;
            if (std::isfinite(h.value) && h.value > 0)
                value += h.value;
        }
        const QString n = fmt::number(static_cast<double>(list.size()), 0);
        QString s = funds ? tr("These %1 funds hold %2 of the shares outstanding, worth %3. Funds are part of the "
                               "institutions, not in addition to them.")
                                .arg(n, fmt::pct(pct, 2, false), amount_text(value, co.currency))
                          : tr("These %1 institutions hold %2 of the shares outstanding, worth %3.")
                                .arg(n, fmt::pct(pct, 2, false), amount_text(value, co.currency));
        if (!funds && mh.present && mh.institutions_count > list.size())
            s += QLatin1Char(' ') +
                 tr("%1 institutions report holdings in all.").arg(fmt::number(mh.institutions_count, 0));
        return s;
    };
    rlv_set_text(holders_caption_, holders_text(data_.institutional_holders, false));
    rlv_set_text(funds_caption_, holders_text(data_.mutualfund_holders, true));

    // ── Insiders ──
    const RelInsiderSummary& sum = data_.insider_summary;
    QString summary;
    if (sum.present) {
        const QString window = (sum.window.isEmpty() || sum.window == QLatin1String("6m")) ? tr("Last 6 months")
                                                                                           : sum.window;
        summary = tr("%1: %2 buys (%3 shares) · %4 sells (%5 shares) · net %6 shares (%7 of insider holdings)")
                      .arg(window, fmt::number(sum.purchases_count, 0), fmt::volume(sum.purchases_shares),
                           fmt::number(sum.sales_count, 0), fmt::volume(sum.sales_shares),
                           rlv_signed_volume(sum.net_shares), fmt::fraction_pct(sum.net_pct, 1, true));
        if (sum.buy_value > 0 || sum.sell_value > 0)
            summary += QStringLiteral(" · ") + tr("bought %1, sold %2 on the open market")
                                                   .arg(rlv_money(sum.buy_value, sum.currency),
                                                        rlv_money(sum.sell_value, sum.currency));
    }
    rlv_set_text(insider_summary_, summary);
    const QDate end = data_.fetched_at.isValid() ? data_.fetched_at.toUTC().date() : QDate::currentDate();
    activity_->set_trades(data_.insider_trades, end);
    QString activity = activity_->currency().isEmpty()
                           ? tr("Open-market buys (up) and sells (down) by value per month; the line is the "
                                "cumulative net. Grants, gifts and option exercises are not counted.")
                           : tr("Open-market buys (up) and sells (down) by value per month, in %1; the line is the "
                                "cumulative net. Grants, gifts and option exercises are not counted.")
                                 .arg(activity_->currency());
    if (activity_->excluded_count() > 0)
        activity += QLatin1Char(' ') +
                    tr("%n trade(s) in another currency left out.", nullptr, activity_->excluded_count());
    rlv_set_text(activity_caption_, activity);

    // ── Management ──
    const QString pay_ccy = statement_currency(co.financial_currency, co.currency);
    rlv_set_text(mgmt_caption_, data_.officers.isEmpty()
                                    ? QString()
                                    : (pay_ccy.isEmpty()
                                           ? tr("Pay: total compensation for the last fiscal year. Shares and the last "
                                                "transaction come from the insider roster when the officer is on it.")
                                           : tr("Pay: total compensation for the last fiscal year, in %1. Shares and "
                                                "the last transaction come from the insider roster when the officer "
                                                "is on it.")
                                                 .arg(pay_ccy)));

    // ── Peers ──
    QString peer_text;
    const QString ps = data_.peer_source;
    if (ps.startsWith(QLatin1String("screen:")))
        peer_text = tr("Peers: companies in the same industry (Yahoo industry screen).");
    else if (ps.startsWith(QLatin1String("industry:")))
        peer_text = tr("Peers: curated list for %1.").arg(ps.mid(9).replace(QLatin1Char('_'), QLatin1Char(' ')));
    else if (ps == QLatin1String("generic_fallback"))
        peer_text = tr("Peers: no industry peers were found; these are large caps, not comparable companies.");
    if (!data_.peers.isEmpty()) {
        if (!peer_text.isEmpty())
            peer_text += QLatin1Char(' ');
        peer_text += tr("Market cap and price in each company's own currency; ρ is the correlation of daily returns "
                        "with the company.");
    }
    rlv_set_text(peers_caption_, peer_text);

    QVector<RelBulletMetric> metrics;
    const auto add_metric = [&](const QString& label, bool fraction, double company, auto peer_value) {
        RelBulletMetric m;
        m.label = label;
        m.fraction = fraction;
        m.company = company;
        m.peers.reserve(data_.peers.size());
        for (const PeerCompany& p : data_.peers)
            m.peers.append(peer_value(p));
        metrics.append(m);
    };
    const double net_margin = data_.margins.net != 0 ? data_.margins.net : co.profit_margins;
    add_metric(tr("P/E"), false, rlv_pos(co.pe_ratio), [](const PeerCompany& p) { return rlv_pos(p.pe_ratio); });
    add_metric(tr("EV/EBITDA"), false, rlv_pos(data_.enterprise.ev_to_ebitda),
               [](const PeerCompany& p) { return rlv_pos(p.ev_to_ebitda); });
    add_metric(tr("Revenue growth"), true, rlv_nz(co.revenue_growth),
               [](const PeerCompany& p) { return rlv_nz(p.revenue_growth); });
    add_metric(tr("Net margin"), true, rlv_nz(net_margin), [](const PeerCompany& p) { return rlv_nz(p.profit_margins); });
    add_metric(tr("ROE"), true, rlv_nz(co.roe), [](const PeerCompany& p) { return rlv_nz(p.roe); });
    add_metric(tr("52-week change"), true, rlv_nz(data_.technicals.week52_change_pct),
               [](const PeerCompany& p) { return rlv_nz(p.week52_change); });
    bullets_->set_metrics(co.ticker.trimmed().toUpper(), metrics);

    // ── Correlation ──
    const CorrelationSet& c = data_.correlation;
    if (c.present && !c.symbols.isEmpty()) {
        heatmap_->set_matrix(c.symbols, c.symbols, c.matrix);
        heatmap_->set_state(ui::PaintedChart::Ready);
        const QString interval = c.interval.isEmpty() || c.interval == QLatin1String("1d")
                                     ? tr("Daily")
                                     : (c.interval == QLatin1String("1wk") ? tr("Weekly") : c.interval);
        rlv_set_text(corr_caption_,
                     tr("%1 returns, %2, %3 observations, as of %4. Click a cell to select its column's symbol.")
                         .arg(interval, c.window.isEmpty() ? QStringLiteral("1y") : c.window,
                              fmt::number(c.observations, 0), c.as_of.isEmpty() ? fmt::placeholder() : c.as_of));
    } else {
        heatmap_->clear();
        heatmap_->set_state(ui::PaintedChart::Empty, tr("No correlations computed"));
        rlv_set_text(corr_caption_, QString());
    }

    // ── Subsidiaries ──
    if (!data_.subsidiaries.isEmpty()) {
        const FilingRef& f = data_.subsidiaries_filing;
        QStringList parts;
        parts << tr("%n subsidiaries listed", nullptr, static_cast<int>(data_.subsidiaries.size()));
        if (!f.form.isEmpty() && !f.filed.isEmpty())
            parts << tr("Form %1 filed %2").arg(f.form, f.filed);
        else if (!f.form.isEmpty())
            parts << tr("Form %1").arg(f.form);
        parts << (f.form.startsWith(QLatin1String("20-F")) ? tr("SEC EDGAR Exhibit 8") : tr("SEC EDGAR Exhibit 21"));
        rlv_set_text(subs_caption_, parts.join(QStringLiteral(" · ")), f.url);
    } else {
        rlv_set_text(subs_caption_, QString());
    }

    // ── Holdings ──
    const FundProfile& fp = data_.fund;
    QStringList hparts;
    if (!fp.holdings.isEmpty()) {
        double w = 0;
        for (const auto& h : fp.holdings)
            if (std::isfinite(h.weight) && h.weight > 0)
                w += h.weight;
        hparts << tr("Top %1 holdings = %2 of the fund")
                      .arg(fmt::number(static_cast<double>(fp.holdings.size()), 0), fmt::fraction_pct(w, 1, false));
    }
    if (fp.total_assets > 0)
        hparts << tr("total assets %1").arg(amount_text(fp.total_assets, co.currency));
    if (!fp.category.isEmpty())
        hparts << fp.category;
    if (!fp.family.isEmpty())
        hparts << fp.family;
    rlv_set_text(holdings_caption_, pages_[kPageHoldings].has_rows ? hparts.join(QStringLiteral(" · ")) : QString());

    // ── Supply chain ──
    rlv_set_text(supply_caption_, data_.supply_chain.isEmpty()
                                      ? QString()
                                      : tr("Customers and suppliers as reported; share of revenue where disclosed."));
}

// ═════════════════════════════════════════════════════════════════════════════
// Pages and tabs
// ═════════════════════════════════════════════════════════════════════════════

bool RelationshipLinkedViews::page_visible(int page) const {
    const Page& pg = pages_[page];
    const RelSectionState st = data_.section(pg.section).state;
    if (st == RelSectionState::NotApplicable && !pg.has_rows)
        return false;
    if (!has_data_)
        return page != kPageHoldings; // nothing known yet: the company layout, loading
    const bool active = pg.has_rows || st == RelSectionState::Loading || section_failed(st);
    if (data_.has_no_relationships())
        return (page == kPagePeers || page == kPageCorrelation) && active;
    if (data_.is_fund()) {
        switch (page) {
            case kPageHoldings:
            case kPagePeers:
            case kPageCorrelation:
                return true;
            case kPageOwnership:
            case kPageHolders:
            case kPageFunds:
                return pg.has_rows; // holders of the fund itself, when the source lists them
            default:
                return pg.has_rows;
        }
    }
    return page != kPageHoldings;
}

QVector<int> RelationshipLinkedViews::page_order() const {
    const int company[] = {kPageOwnership, kPageHolders,      kPageFunds,    kPageInsiders,   kPageManagement,
                           kPagePeers,     kPageCorrelation,  kPageSubsidiaries, kPageHoldings, kPageSupplyChain};
    const int fund[] = {kPageHoldings, kPageOwnership,   kPageHolders,      kPageFunds,      kPageInsiders,
                        kPageManagement, kPagePeers,     kPageCorrelation,  kPageSubsidiaries, kPageSupplyChain};
    const bool is_fund = has_data_ && data_.is_fund();
    QVector<int> out;
    for (int i = 0; i < kPageCount; ++i) {
        const int p = is_fund ? fund[i] : company[i];
        if (pages_[p].shown)
            out.append(p);
    }
    return out;
}

void RelationshipLinkedViews::refresh_pages() {
    for (int p = 0; p < kPageCount; ++p) {
        Page& pg = pages_[p];
        const RelSectionState st = data_.section(pg.section).state;
        if (pg.has_rows)
            pg.mode = PageMode::Content;
        else if (!has_data_ || st == RelSectionState::Loading)
            pg.mode = PageMode::Loading;
        else if (section_failed(st))
            pg.mode = PageMode::Failed;
        else
            pg.mode = PageMode::NoData;
        pg.shown = page_visible(p);
        apply_page_mode(p);
    }
    sync_tabs();
}

void RelationshipLinkedViews::apply_page_mode(int page) {
    Page& pg = pages_[page];
    QWidget* show = pg.content;
    if (pg.mode == PageMode::Loading) {
        if (!pg.skeleton) {
            pg.skeleton = new ui::LoadingSkeleton(ui::LoadingSkeleton::Shape::Table, pg.root);
            pg.skeleton->setVisible(false);
            pg.root_layout->addWidget(pg.skeleton, 1);
        }
        pg.skeleton->set_message(tr("Loading %1…").arg(pg.what));
        show = pg.skeleton;
    } else if (pg.mode != PageMode::Content) {
        if (!pg.state) {
            pg.state = new ui::EmptyState(pg.root);
            pg.state->setVisible(false);
            pg.root_layout->addWidget(pg.state, 1);
            connect(pg.state, &ui::EmptyState::primary_clicked, this, [this, page]() {
                if (pages_[page].mode == PageMode::Failed)
                    emit retry_requested(QStringList{pages_[page].section});
            });
        }
        const RelSectionStatus st = data_.section(pg.section);
        ui::EmptyState::Spec spec;
        if (pg.mode == PageMode::Failed) {
            const QString source = st.source.isEmpty() ? tr("The source") : st.source;
            QString title;
            QString message = st.note;
            if (st.state == RelSectionState::Timeout) {
                title = tr("Timed out");
                if (message.isEmpty())
                    message = tr("%1 did not answer in time.").arg(source);
            } else if (st.state == RelSectionState::Skipped) {
                title = tr("Not loaded");
                if (message.isEmpty())
                    message = tr("The time budget ran out before this section was fetched.");
            } else {
                title = tr("Could not load %1").arg(pg.what);
                if (message.isEmpty())
                    message = tr("%1 returned an error.").arg(source);
            }
            spec = ui::EmptyState::error_spec(message, title, tr("RETRY"));
        } else if (page == kPageSupplyChain) {
            spec = ui::EmptyState::no_data_spec(
                tr("Customers and suppliers are not available from free sources"),
                tr("Supplier and customer lists come from paid datasets; filings name major customers only in prose, "
                   "so nothing is estimated here."));
            spec.glyph = QStringLiteral("↔");
            spec.glyph_tone = ui::EmptyState::Tone::Info;
        } else {
            spec = ui::EmptyState::no_data_spec(tr("No %1 reported").arg(pg.what),
                                                st.note.isEmpty() ? tr("The source returned nothing for this instrument.")
                                                                  : st.note);
            spec.glyph_tone = ui::EmptyState::Tone::Neutral;
        }
        pg.state->apply(spec);
        show = pg.state;
    }
    QWidget* const parts[] = {pg.content, pg.skeleton, pg.state};
    for (QWidget* w : parts)
        if (w && w != show && w->isVisibleTo(pg.root))
            w->setVisible(false);
    if (show && !show->isVisibleTo(pg.root))
        show->setVisible(true);
}

void RelationshipLinkedViews::sync_tabs() {
    const QVector<int> order = page_order();
    bool same = tabs_->count() == order.size();
    for (int i = 0; same && i < order.size(); ++i)
        same = tabs_->tabData(i).toInt() == order[i];
    int target = -1;
    if (wanted_ >= 0 && order.contains(wanted_))
        target = wanted_;
    else if (current_ >= 0 && order.contains(current_))
        target = current_;
    else if (!order.isEmpty())
        target = order.first();
    {
        const QSignalBlocker block(tabs_);
        if (!same) {
            while (tabs_->count() > 0)
                tabs_->removeTab(tabs_->count() - 1);
            for (int p : order)
                tabs_->setTabData(tabs_->addTab(QString()), p);
        }
        for (int i = 0; i < order.size(); ++i) {
            const Page& pg = pages_[order[i]];
            QString text = pg.title;
            if (pg.count > 0)
                text += QLatin1Char(' ') + fmt::number(pg.count, 0);
            text.replace(QLatin1String("&"), QLatin1String("&&"));
            if (tabs_->tabText(i) != text)
                tabs_->setTabText(i, text);
        }
        const int ti = static_cast<int>(order.indexOf(target));
        if (ti >= 0 && tabs_->currentIndex() != ti)
            tabs_->setCurrentIndex(ti);
    }
    // No tab applies (an index, a currency pair): one designed page instead of a blank area.
    const bool nothing = has_data_ && order.isEmpty();
    if (nothing && !none_) {
        none_ = new ui::EmptyState(body_);
        none_->setVisible(false);
        body_layout_->addWidget(none_, 1);
        ui::EmptyState::Spec spec = ui::EmptyState::no_data_spec(
            tr("No relationships for this instrument"),
            tr("Holders, insiders, officers, peers and filings exist for companies and funds only."));
        spec.glyph_tone = ui::EmptyState::Tone::Neutral;
        none_->apply(spec);
    }
    if (none_ && none_->isVisibleTo(body_) != nothing)
        none_->setVisible(nothing);
    show_page(target);
}

void RelationshipLinkedViews::show_page(int page) {
    for (int i = 0; i < pages_.size(); ++i)
        if (i != page && pages_[i].root && pages_[i].root->isVisibleTo(body_))
            pages_[i].root->setVisible(false);
    if (page >= 0 && !pages_[page].root->isVisibleTo(body_))
        pages_[page].root->setVisible(true);
    const bool changed = page != current_;
    current_ = page;
    update_source_label();
    if (changed && page >= 0 && !quiet_)
        emit view_changed(pages_[page].key);
}

void RelationshipLinkedViews::update_source_label() {
    QString text;
    if (current_ >= 0 && has_data_) {
        const RelSectionStatus st = data_.section(pages_[current_].section);
        QStringList parts;
        if (!st.source.isEmpty())
            parts << st.source;
        if (!st.as_of.isEmpty())
            parts << tr("as of %1").arg(st.as_of);
        text = parts.join(QStringLiteral(" · "));
    }
    if (source_->text() != text) {
        source_->setText(text);
        source_->setToolTip(text);
    }
}

bool RelationshipLinkedViews::open_view(const QString& key) {
    for (int p = 0; p < pages_.size(); ++p) {
        if (pages_[p].key != key)
            continue;
        if (!pages_[p].shown)
            return false;
        int ti = -1;
        for (int i = 0; i < tabs_->count(); ++i)
            if (tabs_->tabData(i).toInt() == p)
                ti = i;
        if (ti < 0)
            return false;
        wanted_ = p;
        {
            const QSignalBlocker block(tabs_);
            tabs_->setCurrentIndex(ti);
        }
        show_page(p);
        return true;
    }
    return false;
}

QString RelationshipLinkedViews::current_view() const {
    return current_ >= 0 ? pages_[current_].key : QString();
}

QStringList RelationshipLinkedViews::view_keys() const {
    QStringList out;
    for (int p : page_order())
        out << pages_[p].key;
    return out;
}

// ═════════════════════════════════════════════════════════════════════════════
// Selection
// ═════════════════════════════════════════════════════════════════════════════

void RelationshipLinkedViews::on_view_clicked(int table, const QModelIndex& index) {
    if (!index.isValid())
        return;
    const QString id = index.data(kRlvNodeRole).toString();
    if (id.isEmpty())
        return; // a row with no node on the graph (a jurisdiction count)
    selected_id_ = id;
    highlight(id, table, false);
    emit node_selected(id);
}

void RelationshipLinkedViews::on_view_double_clicked(int table, const QModelIndex& index) {
    Q_UNUSED(table);
    if (!index.isValid())
        return;
    const QString id = index.data(kRlvNodeRole).toString();
    if (!id.isEmpty())
        emit node_activated(id);
}

void RelationshipLinkedViews::on_view_current_changed(int table, const QModelIndex& index) {
    // Keyboard navigation only: a mouse press is reported by clicked() on release.
    if (syncing_ || !index.isValid())
        return;
    if (!tables_[table].view->hasFocus() || QGuiApplication::mouseButtons() != Qt::NoButton)
        return;
    on_view_clicked(table, index);
}

void RelationshipLinkedViews::highlight(const QString& id, int except_table, bool scroll) {
    const QScopedValueRollback<bool> guard(syncing_, true);
    for (int i = 0; i < tables_.size(); ++i) {
        if (i == except_table)
            continue;
        const Table& t = tables_[i];
        QItemSelectionModel* sm = t.view->selectionModel();
        if (!sm)
            continue;
        const int src = id.isEmpty() ? -1 : rlv_model(t.model)->row_of(id);
        const QModelIndex pi = src >= 0 ? t.proxy->mapFromSource(t.model->index(src, 0)) : QModelIndex();
        if (!pi.isValid()) {
            if (sm->hasSelection() || sm->currentIndex().isValid())
                sm->clear();
            continue;
        }
        if (scroll) {
            sm->setCurrentIndex(pi, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            t.view->scrollTo(pi, QAbstractItemView::EnsureVisible);
        } else if (!sm->isRowSelected(pi.row(), pi.parent())) {
            sm->select(pi, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        }
    }
}

bool RelationshipLinkedViews::page_lists(int page, const QString& id) const {
    for (int t : pages_[page].tables)
        if (rlv_model(tables_[t].model)->row_of(id) >= 0)
            return true;
    return false;
}

int RelationshipLinkedViews::page_for_node(const QString& id) const {
    if (id.isEmpty())
        return -1;
    if (current_ >= 0 && page_lists(current_, id))
        return current_;
    const int colon = static_cast<int>(id.indexOf(QLatin1Char(':')));
    const QString prefix = colon > 0 ? id.left(colon) : QString();
    const QString rest = colon > 0 ? id.mid(colon + 1) : QString();
    const bool group_node = prefix == QLatin1String("more") || prefix == QLatin1String("ph");
    RelGroup g = RelGroup::Count;
    if (group_node) {
        group_from_key(rest, &g);
    } else if (prefix == QLatin1String("inst")) {
        g = RelGroup::Institutions;
    } else if (prefix == QLatin1String("fund")) {
        g = RelGroup::Funds;
    } else if (prefix == QLatin1String("ins")) {
        g = RelGroup::Insiders;
    } else if (prefix == QLatin1String("mgmt")) {
        g = RelGroup::Management;
    } else if (prefix == QLatin1String("peer") || prefix == QLatin1String("co")) {
        g = RelGroup::Peers;
    } else if (prefix == QLatin1String("hold") || prefix == QLatin1String("sector")) {
        g = RelGroup::Holdings;
    } else if (prefix == QLatin1String("corr")) {
        g = RelGroup::Correlated;
    } else if (prefix == QLatin1String("sub")) {
        g = RelGroup::Subsidiaries;
    } else if (prefix == QLatin1String("sc")) {
        g = RelGroup::SupplyChain;
    }
    int preferred = -1;
    switch (g) {
        case RelGroup::Institutions:
            preferred = kPageHolders;
            break;
        case RelGroup::Funds:
            preferred = kPageFunds;
            break;
        case RelGroup::Insiders:
            preferred = kPageInsiders;
            break;
        case RelGroup::Management:
            preferred = kPageManagement;
            break;
        case RelGroup::Peers:
            preferred = kPagePeers;
            break;
        case RelGroup::Holdings:
        case RelGroup::Sectors:
            preferred = kPageHoldings;
            break;
        case RelGroup::Correlated:
            preferred = kPageCorrelation;
            break;
        case RelGroup::Subsidiaries:
            preferred = kPageSubsidiaries;
            break;
        case RelGroup::SupplyChain:
            preferred = kPageSupplyChain;
            break;
        case RelGroup::Count:
            break;
    }
    if (preferred >= 0 && pages_[preferred].shown && (group_node || page_lists(preferred, id)))
        return preferred;
    for (int p : page_order())
        if (page_lists(p, id))
            return p;
    return -1;
}

void RelationshipLinkedViews::set_selected(const QString& id, bool switch_tab) {
    selected_id_ = id;
    if (switch_tab && !id.isEmpty()) {
        const int p = page_for_node(id);
        if (p >= 0 && p != current_) {
            const QScopedValueRollback<bool> quiet(quiet_, true); // the contract: no signal
            open_view(pages_[p].key);
        }
    }
    highlight(id, -1, true);
}

// ═════════════════════════════════════════════════════════════════════════════
// CSV
// ═════════════════════════════════════════════════════════════════════════════

QString RelationshipLinkedViews::table_csv(int table) const {
    const Table& t = tables_[table];
    const RlvTableModel* m = rlv_model(t.model);
    const QVector<RlvColumn>& cols = m->columns();
    QStringList lines;
    QStringList head;
    for (const RlvColumn& c : cols) {
        if (c.fmt == RlvFmt::Spark)
            continue;
        const bool unit = rlv_percent_like(c.fmt) && !c.title.contains(QLatin1Char('%'));
        head << rlv_csv_field(unit ? tr("%1 (%)").arg(c.title) : c.title);
        if (rlv_has_currency(c.fmt))
            head << rlv_csv_field(tr("%1 currency").arg(c.title));
    }
    lines << head.join(QLatin1Char(','));
    // In the order on screen (the user's sort).
    for (int r = 0; r < t.proxy->rowCount(); ++r) {
        const int src = t.proxy->mapToSource(t.proxy->index(r, 0)).row();
        if (src < 0 || src >= m->rows().size())
            continue;
        const RlvRow& row = m->rows()[src];
        QStringList cells;
        for (int c = 0; c < cols.size(); ++c) {
            if (cols[c].fmt == RlvFmt::Spark)
                continue;
            const RlvCell cell = c < row.cells.size() ? row.cells[c] : RlvCell();
            cells << rlv_csv_field(rlv_csv_value(cell));
            if (rlv_has_currency(cols[c].fmt))
                cells << rlv_csv_field(rlv_csv_currency(cell));
        }
        lines << cells.join(QLatin1Char(','));
    }
    return lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

QString RelationshipLinkedViews::correlation_csv() const {
    const CorrelationSet& c = data_.correlation;
    QStringList lines;
    QStringList head{tr("symbol")};
    for (const QString& s : c.symbols)
        head << rlv_csv_field(s);
    lines << head.join(QLatin1Char(','));
    for (int i = 0; i < c.symbols.size(); ++i) {
        QStringList cells{rlv_csv_field(c.symbols[i])};
        for (int j = 0; j < c.symbols.size(); ++j) {
            const double v = (i < c.matrix.size() && j < c.matrix[i].size()) ? c.matrix[i][j] : kRlvNaN;
            cells << fmt::export_number(v);
        }
        lines << cells.join(QLatin1Char(','));
    }
    return lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

QString RelationshipLinkedViews::current_csv() const {
    if (current_ < 0)
        return QString();
    const Page& pg = pages_[current_];
    if (pg.mode != PageMode::Content)
        return QString();
    if (current_ == kPageCorrelation)
        return correlation_csv();
    if (pg.tables.isEmpty())
        return QString();
    return table_csv(pg.tables.first());
}

} // namespace fincept::relmap
