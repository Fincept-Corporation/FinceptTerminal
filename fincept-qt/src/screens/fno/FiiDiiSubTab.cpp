#include "screens/fno/FiiDiiSubTab.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/fno/FiiDiiChart.h"
#include "screens/fno/FnoFormat.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/NumericTableWidgetItem.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/StatTile.h"

#include <QDate>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHideEvent>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QShowEvent>
#include <QSplitter>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::screens::fno {

using fincept::services::options::FiiDiiDay;
using namespace fincept::ui;

namespace {

const QString kTopic = QStringLiteral("fno:fii_dii:daily");

namespace fmt = fincept::ui::formatting;

/// A table cell in crores of rupees (the column header names the unit). Net
/// flows carry their sign ("+1,234.56" / "-310.20") so the direction never
/// rests on colour alone; 0 = not published ("—").
QString fii_fmt_cr(double v, bool signed_) {
    if (v == 0)
        return fmt::placeholder();
    return signed_ ? fmt::signed_number(v, 2) : fmt::number(v, 2);
}

QColor color_for(double v) {
    if (v > 0)
        return QColor(colors::POSITIVE());
    if (v < 0)
        return QColor(colors::NEGATIVE());
    return QColor(colors::TEXT_SECONDARY());
}

/// 22px at the default font size, growing with the user's font.
int table_row_height() {
    return qMax(22, QFontMetrics(fonts::font()).height() + 6);
}

/// Signed rupee crores for a headline tile: "+₹1,234.56 Cr". The feed publishes
/// crores (FiiDiiTypes.h), so the amount is scaled back to rupees and printed in
/// the crore scale — the unit and its placement follow the user's settings, the
/// figure stays exact to two decimals of a crore.
QString fii_tile_cr(double v) {
    return fmt::signed_money(v * 1.0e7, fno_ccy(), fmt::Compact::LakhCrore, 2);
}

/// "FII Net (₹ Cr)".
QString fii_header_cr(const QString& title) {
    return fmt::header_scaled(title, fno_ccy(), fmt::Scale::Crore);
}

const char* const kFiiKpiKeys[] = {"fii", "dii", "fii5", "dii5", "fiim", "diim"};

} // namespace

FiiDiiSubTab::FiiDiiSubTab(QWidget* parent) : QWidget(parent) {
    setObjectName("fnoFiiDiiTab");
    setStyleSheet(QString("#fnoFiiDiiTab { background:%1; }"
                          "#fnoFiiHeader { background:%2; border-bottom:1px solid %3; }"
                          "#fnoFiiStatus { color:%4; font-size:" + fincept::ui::fonts::px(-4) + "px; background:transparent; }"
                          "#fnoFiiStatus[warn=\"true\"] { color:%9; }"
                          "#fnoFiiRefresh { background:%5; color:%8; border:none; padding:5px 12px; "
                          "                  font-size:" + fincept::ui::fonts::px(-5) + "px; font-weight:700; letter-spacing:0.5px; }"
                          "#fnoFiiRefresh:hover { background:%6; color:%2; }"
                          "QTableWidget { background:%1; color:%6; border:1px solid %3; "
                          "                gridline-color:%3; selection-background-color:%7; selection-color:%6; }"
                          "QHeaderView::section { background:%2; color:%4; border:none; "
                          "                       border-bottom:1px solid %3; padding:5px 8px; "
                          "                       font-size:" + fincept::ui::fonts::px(-5) + "px; font-weight:700; letter-spacing:0.4px; }")
                      .arg(colors::BG_BASE(),        // %1
                           colors::BG_RAISED(),      // %2
                           colors::BORDER_DIM(),     // %3
                           colors::TEXT_SECONDARY(), // %4
                           colors::AMBER(),          // %5
                           colors::TEXT_PRIMARY(),   // %6
                           colors::BG_HOVER(),       // %7
                           colors::TEXT_ON_ACCENT(), // %8
                           colors::DANGER()));       // %9 (source state, not a price move)

    setup_ui();
    connect(refresh_btn_, &QPushButton::clicked, this, &FiiDiiSubTab::on_refresh_clicked);
    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this,
            [this](const ThemeTokens&) { refresh_theme(); });
    // Row height follows the font — re-fit once the appearance change has settled.
    connect(&ThemeManager::instance(), &ThemeManager::appearance_settled, this, [this]() {
        if (table_)
            table_->verticalHeader()->setDefaultSectionSize(table_row_height());
    });
    // A currency / number-format change re-formats the rows already held.
    fmt::on_format_changed(this, [this]() {
        apply_headers();
        render_table();
        update_kpis();
    });
}

void FiiDiiSubTab::refresh_theme() {
    // Net-flow cells carry their sign colour as an item brush. The table is
    // sortable, so read each cell's own value rather than assuming row order.
    for (int i = 0; i < table_->rowCount(); ++i) {
        for (int c : {3, 6}) {
            if (auto* item = dynamic_cast<ui::NumericTableWidgetItem*>(table_->item(i, c)))
                item->setForeground(color_for(item->numeric_value()));
        }
    }
}

FiiDiiSubTab::~FiiDiiSubTab() {
    fincept::datahub::DataHub::instance().unsubscribe(this);
}

void FiiDiiSubTab::setup_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header strip ──────────────────────────────────────────────────────
    auto* header = new QWidget(this);
    header->setObjectName("fnoFiiHeader");
    auto* hlay = new QHBoxLayout(header);
    hlay->setContentsMargins(12, 8, 12, 8);
    hlay->setSpacing(8);
    lbl_status_ = new QLabel(tr("FII / DII flows — fetching…"), header);
    lbl_status_->setObjectName("fnoFiiStatus");
    refresh_btn_ = new QPushButton(tr("REFRESH"), header);
    refresh_btn_->setObjectName("fnoFiiRefresh");
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    hlay->addWidget(lbl_status_);
    hlay->addStretch(1);
    hlay->addWidget(refresh_btn_);
    root->addWidget(header);

    // ── Headline figures: today's net and the running 5-day / MTD sums ────
    auto* kpi_host = new QWidget(this);
    auto* kpi_lay = new QVBoxLayout(kpi_host);
    kpi_lay->setContentsMargins(12, 8, 12, 8);
    kpis_ = new ui::KpiStrip(kpi_host);
    kpis_->set_max_columns(6);
    for (const char* key : kFiiKpiKeys)
        kpis_->add_tile(QString::fromLatin1(key), QString())->set_value(fincept::ui::formatting::placeholder());
    label_kpis();
    kpi_lay->addWidget(kpis_);
    root->addWidget(kpi_host);

    // ── Body: table | chart split ─────────────────────────────────────────
    auto* split = new QSplitter(Qt::Horizontal, this);
    split->setHandleWidth(1);
    split->setChildrenCollapsible(false);

    table_ = new QTableWidget(split);
    table_->setColumnCount(7);
    apply_headers();
    table_->verticalHeader()->setVisible(false);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setShowGrid(false);
    table_->setFocusPolicy(Qt::NoFocus);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    // Interactive, sized once after each fill (render_table) — never measured per row.
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    table_->verticalHeader()->setDefaultSectionSize(table_row_height());
    // Header-click sorting (numbers by value, the ISO date as text); newest first by default.
    table_->horizontalHeader()->setSortIndicator(0, Qt::DescendingOrder);
    table_->setSortingEnabled(true);

    chart_ = new FiiDiiChart(split);

    split->addWidget(table_);
    split->addWidget(chart_);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 4);
    root->addWidget(split, 1);
}

QVariantMap FiiDiiSubTab::save_state() const {
    return {};
}
void FiiDiiSubTab::restore_state(const QVariantMap& /*state*/) {}

void FiiDiiSubTab::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (subscribed_)
        return;
    auto& hub = fincept::datahub::DataHub::instance();
    QPointer<FiiDiiSubTab> self = this;
    hub.subscribe(this, kTopic, [self](const QVariant& v) {
        if (!self)
            return;
        self->on_data_arrived(v);
    });
    hub.subscribe_errors(this, kTopic, [self](const QString& err) {
        if (!self)
            return;
        self->source_error_ = err;
        self->update_status_line();
    });
    hub.request(kTopic, /*force*/ false);
    subscribed_ = true;
}

void FiiDiiSubTab::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    if (!subscribed_)
        return;
    fincept::datahub::DataHub::instance().unsubscribe(this, kTopic);
    fincept::datahub::DataHub::instance().unsubscribe_errors(this, kTopic);
    subscribed_ = false;
}

void FiiDiiSubTab::on_refresh_clicked() {
    fincept::datahub::DataHub::instance().request(kTopic, /*force*/ true);
    lbl_status_->setText(tr("FII / DII flows — refreshing…"));
}

void FiiDiiSubTab::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void FiiDiiSubTab::retranslateUi() {
    if (refresh_btn_)
        refresh_btn_->setText(tr("REFRESH"));
    apply_headers();
    label_kpis();
}

void FiiDiiSubTab::apply_headers() {
    if (!table_)
        return;
    // Every flow column is in crores of rupees (FiiDiiTypes.h).
    table_->setHorizontalHeaderLabels({tr("Date"), fii_header_cr(tr("FII Buy")), fii_header_cr(tr("FII Sell")),
                                       fii_header_cr(tr("FII Net")), fii_header_cr(tr("DII Buy")),
                                       fii_header_cr(tr("DII Sell")), fii_header_cr(tr("DII Net"))});
}

void FiiDiiSubTab::label_kpis() {
    if (!kpis_)
        return;
    const QString labels[] = {tr("FII NET"), tr("DII NET"), tr("FII 5-DAY"), tr("DII 5-DAY"), tr("FII MTD"), tr("DII MTD")};
    for (int i = 0; i < 6; ++i)
        if (auto* t = kpis_->tile(QString::fromLatin1(kFiiKpiKeys[i])))
            t->set_label(labels[i]);
}

void FiiDiiSubTab::update_kpis() {
    if (!kpis_)
        return;
    using Tone = ui::StatTile::Tone;
    auto tone_of = [](double v) { return v > 0 ? Tone::Positive : (v < 0 ? Tone::Negative : Tone::Neutral); };
    if (rows_.isEmpty()) {
        for (const char* key : kFiiKpiKeys) {
            if (auto* t = kpis_->tile(QString::fromLatin1(key))) {
                t->set_value(fincept::ui::formatting::placeholder());
                t->set_tone(Tone::Neutral);
                t->clear_sparkline();
                if (source_error_.isEmpty())
                    t->set_state(ui::StatTile::State::Normal);
                else
                    t->set_state(ui::StatTile::State::Error, source_error_);
            }
        }
        return;
    }
    // rows_ is ascending by date: the last row is the latest session.
    const FiiDiiDay& last = rows_.last();
    const QDate last_date = QDate::fromString(last.date_iso, Qt::ISODate);
    double fii5 = 0, dii5 = 0, fiim = 0, diim = 0;
    int n5 = 0, nm = 0;
    QVector<double> fii_spark, dii_spark;
    for (int i = rows_.size() - 1; i >= 0; --i) {
        const FiiDiiDay& d = rows_.at(i);
        if (n5 < 5) {
            fii5 += d.fii_net;
            dii5 += d.dii_net;
            ++n5;
        }
        const QDate dd = QDate::fromString(d.date_iso, Qt::ISODate);
        if (last_date.isValid() && dd.isValid() && dd.year() == last_date.year() && dd.month() == last_date.month()) {
            fiim += d.fii_net;
            diim += d.dii_net;
            ++nm;
        }
    }
    for (int i = std::max<qsizetype>(0, rows_.size() - 20); i < rows_.size(); ++i) {
        fii_spark << rows_.at(i).fii_net;
        dii_spark << rows_.at(i).dii_net;
    }
    struct TileData {
        const char* key;
        double value;
        QString sub;
        const QVector<double>* spark;
    };
    const TileData tiles[] = {
        {"fii", last.fii_net, last.date_iso, &fii_spark},
        {"dii", last.dii_net, last.date_iso, &dii_spark},
        {"fii5", fii5, tr("%n session(s)", "", n5), &fii_spark},
        {"dii5", dii5, tr("%n session(s)", "", n5), &dii_spark},
        {"fiim", fiim, tr("%n session(s) this month", "", nm), &fii_spark},
        {"diim", diim, tr("%n session(s) this month", "", nm), &dii_spark},
    };
    for (const auto& td : tiles) {
        auto* t = kpis_->tile(QString::fromLatin1(td.key));
        if (!t)
            continue;
        t->set_value(fii_tile_cr(td.value));
        t->set_tone(tone_of(td.value));
        t->set_sub_label(td.sub);
        t->set_sparkline(*td.spark, 0.0);
        // Cached figures behind a failed fetch are still shown, marked stale.
        if (source_error_.isEmpty())
            t->set_state(ui::StatTile::State::Normal);
        else
            t->set_state(ui::StatTile::State::Stale, source_error_);
    }
}

void FiiDiiSubTab::on_data_arrived(const QVariant& v) {
    if (!v.canConvert<QVector<FiiDiiDay>>())
        return;
    apply_data(v.value<QVector<FiiDiiDay>>());
}

void FiiDiiSubTab::apply_data(const QVector<FiiDiiDay>& rows) {
    rows_ = rows;
    chart_->set_data(rows);
    render_table();

    // A publish is a completed fetch; the service follows it with an error when the
    // source itself returned nothing, which sets source_error_ again right after.
    source_error_.clear();
    update_status_line();
}

void FiiDiiSubTab::render_table() {
    if (!table_)
        return;
    const QVector<FiiDiiDay>& rows = rows_;
    // The upstream feed publishes only the NET figure per day, so the Buy / Sell columns
    // were a wall of dashes. Show them only when some row actually carries a value.
    bool any_gross = false;
    for (const FiiDiiDay& d : rows) {
        if (d.fii_buy != 0 || d.fii_sell != 0 || d.dii_buy != 0 || d.dii_sell != 0) {
            any_gross = true;
            break;
        }
    }
    for (int col : {1, 2, 4, 5})
        table_->setColumnHidden(col, !any_gross);

    // Rows are written by index: sorting off while filling, restored after.
    table_->setSortingEnabled(false);
    table_->setRowCount(rows.size());
    auto put = [&](int r, int c, const QString& txt, const QColor& fg = QColor()) {
        auto* item = new QTableWidgetItem(txt);
        item->setTextAlignment(c == 0 ? Qt::AlignCenter : (Qt::AlignRight | Qt::AlignVCenter));
        if (fg.isValid())
            item->setForeground(fg);
        table_->setItem(r, c, item);
    };
    // Numeric cells sort by value, not by their "+1,234.56" text.
    auto put_num = [&](int r, int c, double v, bool signed_, const QColor& fg = QColor()) {
        auto* item = new ui::NumericTableWidgetItem(fii_fmt_cr(v, signed_), v);
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        if (fg.isValid())
            item->setForeground(fg);
        table_->setItem(r, c, item);
    };
    // Render newest at top of the table — flip the ascending vector here.
    for (int i = 0; i < rows.size(); ++i) {
        const FiiDiiDay& d = rows.at(rows.size() - 1 - i);
        put(i, 0, d.date_iso);
        put_num(i, 1, d.fii_buy, false);
        put_num(i, 2, d.fii_sell, false);
        put_num(i, 3, d.fii_net, true, color_for(d.fii_net));
        put_num(i, 4, d.dii_buy, false);
        put_num(i, 5, d.dii_sell, false);
        put_num(i, 6, d.dii_net, true, color_for(d.dii_net));
    }
    table_->setSortingEnabled(true);
    if (!rows.isEmpty())
        table_->resizeColumnToContents(0); // the date column, once per fill
}

void FiiDiiSubTab::update_status_line() {
    if (!lbl_status_)
        return;
    QString text;
    bool warn = false;
    if (rows_.isEmpty()) {
        if (!source_error_.isEmpty()) {
            text = tr("FII / DII flows — SOURCE UNAVAILABLE (%1). No cached data to show.").arg(source_error_);
            warn = true;
        } else {
            text = tr("FII / DII flows — no data yet. Try refreshing after 6 PM IST.");
        }
    } else {
        const QString as_of = rows_.last().date_iso;
        const QDate as_of_date = QDate::fromString(as_of, Qt::ISODate);
        const qint64 age_days = as_of_date.isValid() ? as_of_date.daysTo(QDate::currentDate()) : -1;
        if (!source_error_.isEmpty()) {
            text = tr("FII / DII flows — SOURCE UNAVAILABLE (%1)   ·   showing cached data as of %2 (%3 days old)")
                       .arg(source_error_, as_of)
                       .arg(age_days);
            warn = true;
        } else if (age_days > 4) {
            // Over a long weekend + one holiday the latest print is at most 4 days old.
            text = tr("FII / DII flows — data as of %1 is %2 days old (source may be unavailable)   ·   %3 days cached")
                       .arg(as_of)
                       .arg(age_days)
                       .arg(rows_.size());
            warn = true;
        } else {
            text = tr("FII / DII flows — data as of %1   ·   %2 days cached").arg(as_of).arg(rows_.size());
        }
    }
    lbl_status_->setText(text);
    // Warning colour through a property of the stylesheet set once in the constructor.
    if (lbl_status_->property("warn").toBool() != warn) {
        lbl_status_->setProperty("warn", warn);
        lbl_status_->style()->unpolish(lbl_status_);
        lbl_status_->style()->polish(lbl_status_);
    }
    update_kpis(); // the tiles follow the data and the source state
}

} // namespace fincept::screens::fno
