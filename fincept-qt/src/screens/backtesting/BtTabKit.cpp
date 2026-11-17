// src/screens/backtesting/BtTabKit.cpp
#include "screens/backtesting/BtTabKit.h"

#include "ui/charts/ChartExport.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/PanelHeader.h"

#include <QAbstractItemModel>
#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QCoreApplication>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QPushButton>
#include <QShortcut>
#include <QSortFilterProxyModel>
#include <QStyle>
#include <QTableView>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens::bt {

namespace fmt = fincept::ui::formatting;

namespace {

QString bt_kit_tr(const char* text, const char* disambiguation = nullptr, int n = -1) {
    return QCoreApplication::translate("fincept::screens::bt::BtTabKit", text, disambiguation, n);
}

constexpr int kSortRole = Qt::UserRole + 1; // ui::LiveTableModel<...>::SortRole

void bt_kit_apply_row_height(QTableView* view) {
    const QFontMetrics fm(view->font());
    view->verticalHeader()->setDefaultSectionSize(std::max(18, fm.height() + 6));
}

void bt_kit_apply_column_chars(QTableView* view) {
    const QVariantList chars = view->property("btColumnChars").toList();
    if (chars.isEmpty())
        return;
    const QFontMetrics fm(view->font());
    const int avg = std::max(5, fm.averageCharWidth());
    for (int i = 0; i < chars.size(); ++i) {
        const int c = chars.at(i).toInt();
        if (c > 0)
            view->setColumnWidth(i, c * avg + 14);
    }
}

} // namespace

// ── Text ────────────────────────────────────────────────────────────────────

QString dash() {
    return fmt::placeholder();
}

QString pct(double fraction, int decimals, bool signed_) {
    if (!std::isfinite(fraction))
        return dash();
    return fmt::fraction_pct(fraction, decimals, signed_);
}

QString pp(double fraction_diff, int decimals) {
    if (!std::isfinite(fraction_diff))
        return dash();
    return fmt::pp(fraction_diff * 100.0, decimals, true);
}

QString ratio(double v, int decimals) {
    if (!std::isfinite(v))
        return dash();
    return fmt::number(v, decimals);
}

QString num(double v, int decimals) {
    if (!std::isfinite(v))
        return dash();
    return fmt::number(v, decimals);
}

QString money(double v, const QString& ccy, bool signed_) {
    if (!std::isfinite(v))
        return dash();
    return signed_ ? fmt::signed_money(v, ccy) : fmt::money(v, ccy);
}

QString money_compact(double v, const QString& ccy, bool signed_) {
    if (!std::isfinite(v))
        return dash();
    return signed_ ? fmt::signed_money(v, ccy, fmt::Compact::FromSettings)
                   : fmt::money(v, ccy, fmt::Compact::FromSettings);
}

QString price(double v, const QString& ccy) {
    if (!std::isfinite(v))
        return dash();
    fmt::InstrumentFmt inst;
    inst.currency = ccy;
    return fmt::price(v, inst);
}

QString quantity(double v) {
    if (!std::isfinite(v))
        return dash();
    return fmt::quantity(v, 4);
}

QString count(double v) {
    if (!std::isfinite(v) || v < 0)
        return dash();
    return fmt::number(std::round(v), 0);
}

QString bars(double v) {
    if (!std::isfinite(v) || v < 0)
        return dash();
    const int n = static_cast<int>(std::llround(v));
    return bt_kit_tr("%n bar(s)", nullptr, n);
}

QString exit_reason_text(const QString& id) {
    if (id.isEmpty())
        return dash();
    if (id == QLatin1String("signal"))
        return bt_kit_tr("Signal");
    if (id == QLatin1String("stop_loss"))
        return bt_kit_tr("Stop-loss");
    if (id == QLatin1String("take_profit"))
        return bt_kit_tr("Take-profit");
    if (id == QLatin1String("trailing_stop"))
        return bt_kit_tr("Trailing stop");
    if (id == QLatin1String("end_of_data"))
        return bt_kit_tr("End of data");
    if (id == QLatin1String("rebalance"))
        return bt_kit_tr("Rebalance");
    if (id == QLatin1String("open"))
        return bt_kit_tr("Open");
    if (id == QLatin1String("time_limit"))
        return bt_kit_tr("Time limit");
    if (id == QLatin1String("margin_call"))
        return bt_kit_tr("Margin call");
    QString s = id;
    s.replace(QLatin1Char('_'), QLatin1Char(' '));
    if (!s.isEmpty())
        s[0] = s[0].toUpper();
    return s;
}

QString date_text(const QString& iso) {
    const QString t = iso.trimmed();
    if (t.isEmpty())
        return dash();
    if (t.size() >= 19 && t.mid(11, 8) == QLatin1String("00:00:00"))
        return t.left(10);
    return t.size() >= 19 ? t.left(16) : t;
}

// ── Tones ───────────────────────────────────────────────────────────────────

const char* tone_key(Tone tone) {
    switch (tone) {
        case Tone::Pos:
            return "pos";
        case Tone::Neg:
            return "neg";
        case Tone::Warn:
            return "warn";
        case Tone::Dim:
            return "dim";
        case Tone::Accent:
            return "accent";
        case Tone::Ok:
            return "ok";
        case Tone::Danger:
            return "danger";
        case Tone::None:
            break;
    }
    return "";
}

Tone sign_tone(double v) {
    if (!std::isfinite(v) || v == 0)
        return Tone::None;
    return v > 0 ? Tone::Pos : Tone::Neg;
}

void set_tone(QWidget* w, Tone tone) {
    if (!w)
        return;
    const QString key = QString::fromLatin1(tone_key(tone));
    if (w->property("tone").toString() == key)
        return;
    w->setProperty("tone", key);
    w->style()->unpolish(w);
    w->style()->polish(w);
    w->update();
}

QColor tone_color(Tone tone) {
    using namespace fincept::ui;
    switch (tone) {
        case Tone::Pos:
            return QColor(colors::POSITIVE());
        case Tone::Neg:
            return QColor(colors::NEGATIVE());
        case Tone::Warn:
            return QColor(colors::WARNING());
        case Tone::Dim:
            return QColor(colors::TEXT_TERTIARY());
        case Tone::Accent:
            return QColor(colors::AMBER());
        case Tone::Ok:
            return QColor(colors::SUCCESS());
        case Tone::Danger:
            return QColor(colors::DANGER());
        case Tone::None:
            break;
    }
    return QColor(colors::TEXT_PRIMARY());
}

// ── KPI tiles ───────────────────────────────────────────────────────────────

QString format_value(double v, Unit unit, const QString& ccy) {
    if (!std::isfinite(v))
        return dash();
    switch (unit) {
        case Unit::Fraction:
            return pct(v, 2, false);
        case Unit::SignedFraction:
            return pct(v, 2, true);
        case Unit::PctPoints:
            return pp(v, 1);
        case Unit::Ratio:
            return ratio(v, 2);
        case Unit::Money:
            return money_compact(v, ccy, false);
        case Unit::SignedMoney:
            return money_compact(v, ccy, true);
        case Unit::Count:
            return count(v);
        case Unit::Bars:
            return bars(v);
        case Unit::Plain:
            break;
    }
    return num(v, 2);
}

ui::StatTile::Tone tile_tone(double v, Rule rule) {
    using T = ui::StatTile::Tone;
    if (!std::isfinite(v))
        return T::Neutral;
    switch (rule) {
        case Rule::Sign:
            return v > 0 ? T::Positive : (v < 0 ? T::Negative : T::Neutral);
        case Rule::Drawdown: {
            const double m = std::fabs(v);
            return m >= 0.25 ? T::Danger : (m >= 0.10 ? T::Warning : T::Neutral); // a breached limit = state
        }
        case Rule::BelowOne:
            return v < 1.0 ? T::Warning : T::Neutral;
        case Rule::Negative:
            return v < 0 ? T::Warning : T::Neutral;
        case Rule::None:
            break;
    }
    return T::Neutral;
}

void set_tile(ui::StatTile* tile, double v, Unit unit, Rule rule, const QString& ccy, const QString& why_blank) {
    if (!tile)
        return;
    if (!std::isfinite(v)) {
        tile->set_value(dash());
        tile->set_tone(ui::StatTile::Tone::Neutral);
        tile->set_state(ui::StatTile::State::Error,
                        why_blank.isEmpty() ? bt_kit_tr("Not reported for this run") : why_blank);
        return;
    }
    tile->set_state(ui::StatTile::State::Normal);
    tile->set_value(format_value(v, unit, ccy));
    tile->set_tone(tile_tone(v, rule));
}

void set_tile_metric(ui::StatTile* tile, const BtRun& run, const QString& path, Unit unit, Rule rule) {
    set_tile(tile, run.metric(path), unit, rule, run.currency, run.why_blank(path));
}

// ── Panels ──────────────────────────────────────────────────────────────────

BtPanel make_panel(const QString& title, QWidget* parent, int body_margin) {
    BtPanel p;
    p.frame = new QFrame(parent);
    p.frame->setObjectName(QStringLiteral("btPanel"));
    p.frame->setAttribute(Qt::WA_StyledBackground, true);
    auto* v = new QVBoxLayout(p.frame);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    p.header = new ui::PanelHeader(title, p.frame);
    v->addWidget(p.header);
    p.body_widget = new QWidget(p.frame);
    p.body_widget->setObjectName(QStringLiteral("btPanelBody"));
    p.body = new QVBoxLayout(p.body_widget);
    p.body->setContentsMargins(body_margin, body_margin, body_margin, body_margin);
    p.body->setSpacing(6);
    v->addWidget(p.body_widget, 1);
    return p;
}

QLabel* make_note(const QString& text, QWidget* parent, Tone tone) {
    auto* l = new QLabel(text, parent);
    l->setObjectName(QStringLiteral("btNote"));
    l->setWordWrap(true);
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    set_tone(l, tone);
    return l;
}

QLabel* make_chip(const QString& text, Tone tone, QWidget* parent) {
    auto* l = new QLabel(text, parent);
    l->setObjectName(QStringLiteral("btChip"));
    set_tone(l, tone);
    return l;
}

QPushButton* make_tool_button(const QString& text, const QString& tooltip, QWidget* parent) {
    auto* b = new QPushButton(text, parent);
    b->setObjectName(QStringLiteral("btToolButton"));
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::TabFocus);
    if (!tooltip.isEmpty())
        b->setToolTip(tooltip);
    b->setAccessibleName(text);
    return b;
}

QWidget* make_segmented(const QStringList& labels, int checked, std::function<void(int)> on_change, QWidget* parent) {
    auto* box = new QWidget(parent);
    auto* h = new QHBoxLayout(box);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(0);
    auto* group = new QButtonGroup(box);
    group->setExclusive(true);
    for (int i = 0; i < labels.size(); ++i) {
        auto* b = new QPushButton(labels.at(i), box);
        b->setObjectName(QStringLiteral("btSeg"));
        b->setCheckable(true);
        b->setChecked(i == checked);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::TabFocus);
        b->setProperty("segPos", i == 0 ? QStringLiteral("first")
                                        : (i == labels.size() - 1 ? QStringLiteral("last") : QStringLiteral("mid")));
        group->addButton(b, i);
        h->addWidget(b);
    }
    if (on_change)
        QObject::connect(group, &QButtonGroup::idClicked, box, [on_change](int id) { on_change(id); });
    return box;
}

void set_segment(QWidget* segmented, int index) {
    if (!segmented)
        return;
    const auto buttons = segmented->findChildren<QPushButton*>(QString(), Qt::FindDirectChildrenOnly);
    for (int i = 0; i < buttons.size(); ++i)
        buttons.at(i)->setChecked(i == index);
}

// ── BtKvGrid ────────────────────────────────────────────────────────────────

BtKvGrid::BtKvGrid(QWidget* parent, int columns) : QWidget(parent), columns_(std::max(1, columns)) {
    grid_ = new QGridLayout(this);
    grid_->setContentsMargins(0, 0, 0, 0);
    grid_->setHorizontalSpacing(10);
    grid_->setVerticalSpacing(3);
}

void BtKvGrid::set_columns(int columns) {
    columns = std::max(1, columns);
    if (columns == columns_)
        return;
    columns_ = columns;
    relayout();
}

void BtKvGrid::relayout() {
    for (int i = 0; i < keys_.size(); ++i) {
        grid_->removeWidget(keys_[i]);
        grid_->removeWidget(values_[i]);
    }
    for (int c = 0; c < 2 * 4; ++c)
        grid_->setColumnStretch(c, 0);
    for (int i = 0; i < shown_; ++i) {
        const int row = i / columns_;
        const int pair = i % columns_;
        grid_->addWidget(keys_[i], row, pair * 2);
        grid_->addWidget(values_[i], row, pair * 2 + 1);
    }
    for (int p = 0; p < columns_; ++p)
        grid_->setColumnStretch(p * 2 + 1, 1);
}

void BtKvGrid::set_rows(const QVector<BtKv>& rows) {
    while (keys_.size() < rows.size()) {
        auto* k = new QLabel(this);
        k->setObjectName(QStringLiteral("btKvKey"));
        auto* v = new QLabel(this);
        v->setObjectName(QStringLiteral("btKvValue"));
        v->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        v->setTextInteractionFlags(Qt::TextSelectableByMouse);
        keys_.append(k);
        values_.append(v);
    }
    const bool layout_changed = rows.size() != shown_;
    for (int i = 0; i < keys_.size(); ++i) {
        const bool on = i < rows.size();
        if (on) {
            keys_[i]->setText(rows[i].key);
            values_[i]->setText(rows[i].value);
            values_[i]->setToolTip(rows[i].tip);
            set_tone(values_[i], rows[i].tone);
        }
        keys_[i]->setVisible(on);
        values_[i]->setVisible(on);
    }
    shown_ = static_cast<int>(rows.size());
    if (layout_changed)
        relayout();
}

// ── Tables ──────────────────────────────────────────────────────────────────

void setup_table(QTableView* view, const QString& export_name) {
    if (!view)
        return;
    view->setObjectName(QStringLiteral("btTable"));
    view->verticalHeader()->hide();
    view->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    bt_kit_apply_row_height(view);
    view->setAlternatingRowColors(true);
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    view->setShowGrid(false);
    view->setWordWrap(false);
    view->setTextElideMode(Qt::ElideRight);
    view->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    view->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    view->horizontalHeader()->setStretchLastSection(true);
    view->horizontalHeader()->setHighlightSections(false);
    view->horizontalHeader()->setSortIndicatorShown(true);
    view->setSortingEnabled(true);
    ui::ChartExport::install_table_context_menu(view, export_name);

    // Ctrl+C: the selected rows as tab-separated text (header first).
    auto* copy = new QShortcut(QKeySequence::Copy, view);
    copy->setContext(Qt::WidgetWithChildrenShortcut);
    QObject::connect(copy, &QShortcut::activated, view, [view]() {
        const QAbstractItemModel* m = view->model();
        if (!m || !view->selectionModel())
            return;
        QModelIndexList rows = view->selectionModel()->selectedRows();
        std::sort(rows.begin(), rows.end(), [](const QModelIndex& a, const QModelIndex& b) { return a.row() < b.row(); });
        if (rows.isEmpty())
            return;
        QStringList lines;
        QStringList head;
        for (int c = 0; c < m->columnCount(); ++c)
            head << m->headerData(c, Qt::Horizontal).toString();
        lines << head.join(QLatin1Char('\t'));
        for (const QModelIndex& r : rows) {
            QStringList cells;
            for (int c = 0; c < m->columnCount(); ++c)
                cells << m->index(r.row(), c).data(Qt::DisplayRole).toString();
            lines << cells.join(QLatin1Char('\t'));
        }
        QApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
    });

    // Row height and widths follow the font (after the appearance has settled, never in theme_changed).
    QObject::connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, view, [view]() {
        bt_kit_apply_row_height(view);
        bt_kit_apply_column_chars(view);
    });
}

void set_column_chars(QTableView* view, const QVector<int>& chars) {
    if (!view)
        return;
    QVariantList list;
    for (int c : chars)
        list << c;
    view->setProperty("btColumnChars", list);
    bt_kit_apply_column_chars(view);
}

QSortFilterProxyModel* make_sort_proxy(QAbstractItemModel* source, QObject* parent) {
    auto* p = new QSortFilterProxyModel(parent);
    p->setSourceModel(source);
    p->setSortRole(kSortRole);
    p->setDynamicSortFilter(false);
    p->setFilterCaseSensitivity(Qt::CaseInsensitive);
    return p;
}

// ── Charts ──────────────────────────────────────────────────────────────────

ui::TimeSeriesChart* make_time_chart(QWidget* parent, const QString& export_name) {
    auto* c = new ui::TimeSeriesChart(parent);
    c->set_utc(true);
    c->set_crosshair_enabled(true);
    c->set_zoom_enabled(true);
    c->set_transparent_background(true);
    c->set_export_name(export_name);
    return c;
}

QVector<QPointF> to_points(const QVector<double>& t, const QVector<double>& v, double scale, int max_points) {
    QVector<double> xs, ys;
    xs.reserve(t.size());
    ys.reserve(t.size());
    const int n = static_cast<int>(std::min(t.size(), v.size()));
    for (int i = 0; i < n; ++i) {
        if (std::isfinite(t[i]) && std::isfinite(v[i])) {
            xs.append(t[i]);
            ys.append(v[i] * scale);
        }
    }
    QVector<double> tx, ty;
    services::backtest::bt_thin(xs, ys, max_points, &tx, &ty);
    QVector<QPointF> out;
    out.reserve(tx.size());
    for (int i = 0; i < tx.size(); ++i)
        out.append(QPointF(tx[i], ty[i]));
    return out;
}

std::function<QString(double)> fraction_formatter(int decimals) {
    return [decimals](double v) { return pct(v, decimals, false); };
}

std::function<QString(double)> money_formatter(const QString& ccy) {
    return [ccy](double v) { return fmt::axis_money(v, ccy); };
}

std::function<QString(double)> number_formatter(int decimals) {
    return [decimals](double v) { return num(v, decimals); };
}

// ── BtRunView ───────────────────────────────────────────────────────────────

BtRunView::BtRunView(BtSelection* selection, QWidget* parent) : QWidget(parent), selection_(selection) {}

} // namespace fincept::screens::bt
