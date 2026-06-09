// src/screens/portfolio/views/AnalyticsSectorsView.cpp
#include "screens/portfolio/views/AnalyticsSectorsView.h"

#include "ui/theme/Theme.h"

#include <QChart>
#include <QTabBar>
#include <QChartView>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QPieSeries>
#include <QScrollArea>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

namespace {

// Balanced 16-color palette — same hues used by PortfolioSectorPanel so the
// two sector views share the same colour language.
const QColor kSectorPalette[] = {
    QColor("#0891b2"), QColor("#16a34a"), QColor("#d97706"), QColor("#ca8a04"),
    QColor("#9333ea"), QColor("#84cc16"), QColor("#0d9488"), QColor("#f97316"),
    QColor("#60a5fa"), QColor("#f43f5e"), QColor("#a78bfa"), QColor("#fbbf24"),
    QColor("#2dd4bf"), QColor("#fb923c"), QColor("#818cf8"), QColor("#e879f9"),
};

// Solid coloured rectangle used as a legend swatch / table colour dot.
class Swatch : public QWidget {
  public:
    Swatch(QColor c, int w, int h, QWidget* parent = nullptr) : QWidget(parent), color_(c) {
        setFixedSize(w, h);
    }
  protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(), color_);
    }
  private:
    QColor color_;
};

// Forwards mouse clicks on a card into a sector_selected emission on the
// parent view. Lives at file scope so MOC doesn't choke on local classes.
class SectorCardClickForwarder : public QObject {
  public:
    SectorCardClickForwarder(class AnalyticsSectorsView* owner, QString sector, QObject* parent)
        : QObject(parent), owner_(owner), sector_(std::move(sector)) {}
  protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;
  private:
    class AnalyticsSectorsView* owner_;
    QString sector_;
};

// English source keys returned by verdict_for_*. Translated to display strings
// in the rendering code below (kept English here so verdict_color can pattern-
// match without depending on the active locale).
[[maybe_unused]] constexpr const char* kVerdictDiversified  = "Diversified";
[[maybe_unused]] constexpr const char* kVerdictBalanced     = "Balanced";
[[maybe_unused]] constexpr const char* kVerdictConcentrated = "Concentrated";

QString verdict_for_hhi(double hhi) {
    // HHI convention uses fractions of 10,000; we use percent-squared so
    // divide by 100 to get the standard range. 1500-2500 = moderate, 2500+ = concentrated.
    if (hhi < 1500) return QStringLiteral("Diversified");
    if (hhi < 2500) return QStringLiteral("Balanced");
    return QStringLiteral("Concentrated");
}

QString verdict_for_top3(double pct) {
    if (pct < 50.0) return QStringLiteral("Diversified");
    if (pct < 75.0) return QStringLiteral("Balanced");
    return QStringLiteral("Concentrated");
}

QColor verdict_color(const QString& verdict) {
    if (verdict == QStringLiteral("Diversified")) return QColor(ui::colors::POSITIVE());
    if (verdict == QStringLiteral("Balanced")) return QColor(ui::colors::AMBER());
    return QColor(ui::colors::NEGATIVE());
}

} // namespace

// ============================================================================
// AnalyticsSectorsView
// ============================================================================

QColor AnalyticsSectorsView::sector_color(int index) {
    return kSectorPalette[index % (sizeof(kSectorPalette) / sizeof(kSectorPalette[0]))];
}

AnalyticsSectorsView::AnalyticsSectorsView(QWidget* parent) : QWidget(parent) {
    build_ui();
}

QString AnalyticsSectorsView::format_money(double v) const {
    QLocale loc = QLocale::system();
    QString body = loc.toString(v, 'f', 2);
    return currency_.isEmpty() ? body : QString("%1 %2").arg(currency_, body);
}

QString AnalyticsSectorsView::format_pct(double v, bool signed_) {
    QString body = QString::number(v, 'f', 2) + QStringLiteral("%");
    if (signed_ && v > 0)
        return QStringLiteral("+") + body;
    return body;
}

void AnalyticsSectorsView::build_ui() {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    tabs_ = new QTabWidget;
    tabs_->tabBar()->setElideMode(Qt::ElideNone);
    tabs_->tabBar()->setExpanding(false);
    tabs_->tabBar()->setUsesScrollButtons(false);
    tabs_->setDocumentMode(true);
    tabs_->setStyleSheet(
        QString("QTabWidget::pane { border:0; background:%1; }"
                "QTabBar::tab { background:%2; color:%3; padding:8px 18px; border:0;"
                "  border-bottom:2px solid transparent; font-size:%6px; font-weight:700;"
                "  letter-spacing:1px; text-transform:uppercase; }"
                "QTabBar::tab:selected { color:%4; border-bottom:2px solid %4;"
                "  background:rgba(217,119,6,0.08); }"
                "QTabBar::tab:hover:!selected { color:%5; }")
            .arg(ui::colors::BG_BASE(), ui::colors::BG_SURFACE(), ui::colors::TEXT_SECONDARY(),
                 ui::colors::AMBER(), ui::colors::TEXT_PRIMARY())
            .arg(ui::fonts::font_px(-3)));

    overview_tab_index_ = tabs_->addTab(build_overview_tab(), tr("OVERVIEW"));
    correlation_tab_index_ = tabs_->addTab(build_correlation_tab(), tr("CORRELATION"));
    root->addWidget(tabs_);
}

QWidget* AnalyticsSectorsView::build_overview_tab() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(QString("QScrollArea { background:%1; border:none; }"
                                  "QScrollBar:vertical { width:6px; background:%1; }"
                                  "QScrollBar::handle:vertical { background:%2; min-height:20px; }")
                              .arg(ui::colors::BG_BASE(), ui::colors::BORDER_DIM()));

    auto* page = new QWidget;
    page->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    auto* col = new QVBoxLayout(page);
    col->setContentsMargins(16, 14, 16, 14);
    col->setSpacing(14);

    const QString text1 = ui::colors::TEXT_PRIMARY();
    const QString text2 = ui::colors::TEXT_SECONDARY();
    const QString text3 = ui::colors::TEXT_TERTIARY();
    const QString border = ui::colors::BORDER_DIM();
    const QString surface = ui::colors::BG_SURFACE();
    const QString amber = ui::colors::AMBER();

    // ── KPI strip ────────────────────────────────────────────────────────────
    auto* kpi = new QWidget(page);
    kpi->setStyleSheet(QString("background:%1; border:1px solid %2;").arg(surface, border));
    auto* kpi_lay = new QHBoxLayout(kpi);
    kpi_lay->setContentsMargins(18, 10, 18, 10);
    kpi_lay->setSpacing(28);

    auto make_kpi = [&](const QString& label, QLabel*& out_value, QLabel*& out_label) {
        auto* box = new QVBoxLayout;
        box->setSpacing(2);
        out_label = new QLabel(label);
        out_label->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; letter-spacing:1.5px;")
                                     .arg(text3).arg(ui::fonts::font_px(-4)));
        box->addWidget(out_label);
        out_value = new QLabel("—");
        out_value->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700;")
                                     .arg(text1).arg(ui::fonts::font_px(2)));
        box->addWidget(out_value);
        kpi_lay->addLayout(box);
    };
    make_kpi(tr("SECTORS"), kpi_sectors_, kpi_sectors_label_);
    make_kpi(tr("POSITIONS"), kpi_positions_, kpi_positions_label_);
    make_kpi(tr("MARKET VALUE"), kpi_market_value_, kpi_market_value_label_);
    make_kpi(tr("P&L"), kpi_pnl_, kpi_pnl_label_);
    kpi_lay->addStretch();
    col->addWidget(kpi);

    // ── Donut + sector table row ─────────────────────────────────────────────
    auto* main_row = new QHBoxLayout;
    main_row->setSpacing(16);

    // Donut card
    auto* donut_card = new QWidget(page);
    donut_card->setStyleSheet(QString("background:%1; border:1px solid %2;").arg(surface, border));
    donut_card->setFixedWidth(320);
    auto* donut_lay = new QVBoxLayout(donut_card);
    donut_lay->setContentsMargins(14, 14, 14, 14);
    donut_lay->setSpacing(8);

    donut_title_ = new QLabel(tr("SECTOR ALLOCATION"));
    donut_title_->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; letter-spacing:1.5px;")
                                    .arg(text3).arg(ui::fonts::font_px(-4)));
    donut_lay->addWidget(donut_title_);

    // Chart itself
    auto* chart = new QChart;
    chart->setBackgroundBrush(Qt::transparent);
    chart->setMargins(QMargins(0, 0, 0, 0));
    chart->legend()->setVisible(false);
    chart->setAnimationOptions(QChart::NoAnimation);
    donut_view_ = new QChartView(chart);
    donut_view_->setRenderHint(QPainter::Antialiasing);
    donut_view_->setFixedHeight(220);
    donut_view_->setStyleSheet("border:none; background:transparent;");

    // Overlay label at the centre of the donut. The chart widget is in a
    // vertical stack here; we stack the label on top of the chart using a
    // QWidget parented to the chart view.
    donut_center_ = new QLabel(donut_view_);
    donut_center_->setAlignment(Qt::AlignCenter);
    donut_center_->setStyleSheet(QString("background:transparent; color:%1;"
                                         "font-size:%2px; font-weight:700; letter-spacing:.5px;")
                                     .arg(text1).arg(ui::fonts::font_px(-1)));

    donut_lay->addWidget(donut_view_);

    // Legend
    legend_container_ = new QWidget(donut_card);
    legend_container_->setStyleSheet("background:transparent;");
    auto* legend_lay = new QVBoxLayout(legend_container_);
    legend_lay->setContentsMargins(0, 4, 0, 0);
    legend_lay->setSpacing(4);
    donut_lay->addWidget(legend_container_);
    donut_lay->addStretch();

    main_row->addWidget(donut_card);

    // Sector table card
    auto* table_card = new QWidget(page);
    table_card->setStyleSheet(QString("background:%1; border:1px solid %2;").arg(surface, border));
    auto* table_lay = new QVBoxLayout(table_card);
    table_lay->setContentsMargins(14, 14, 14, 14);
    table_lay->setSpacing(8);

    table_title_ = new QLabel(tr("SECTOR BREAKDOWN"));
    table_title_->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; letter-spacing:1.5px;")
                                    .arg(text3).arg(ui::fonts::font_px(-4)));
    table_lay->addWidget(table_title_);

    sector_table_ = new QTableWidget;
    sector_table_->setColumnCount(7);
    sector_table_->setHorizontalHeaderLabels(
        {"", tr("SECTOR"), tr("POS"), tr("MKT VAL"), tr("WT"), tr("P&L"), tr("P&L%")});
    sector_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    sector_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    sector_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    sector_table_->setShowGrid(false);
    sector_table_->verticalHeader()->setVisible(false);
    sector_table_->horizontalHeader()->setStretchLastSection(false);
    sector_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    sector_table_->setStyleSheet(
        QString("QTableWidget { background:%1; color:%2; border:none;"
                "  font-family:%3; font-size:%4px; gridline-color:transparent; }"
                "QTableWidget::item { padding:6px 8px; border-bottom:1px solid %5; }"
                "QTableWidget::item:selected { background:rgba(217,119,6,0.10); color:%2; }"
                "QHeaderView::section { background:%6; color:%2; border:none;"
                "  border-bottom:1px solid %7; padding:6px 8px;"
                "  font-size:%8px; font-weight:700; letter-spacing:1px; }")
            .arg(surface, text1, ui::fonts::DATA_FAMILY)
            .arg(ui::fonts::font_px(-2))
            .arg(border, ui::colors::BG_BASE(), amber)
            .arg(ui::fonts::font_px(-4)));
    sector_table_->setColumnWidth(0, 14);
    sector_table_->setColumnWidth(2, 50);
    sector_table_->setColumnWidth(3, 130);
    sector_table_->setColumnWidth(4, 70);
    sector_table_->setColumnWidth(5, 110);
    sector_table_->setColumnWidth(6, 80);
    connect(sector_table_, &QTableWidget::cellClicked, this, [this](int row, int) {
        auto* item = sector_table_->item(row, 1);
        if (item)
            emit sector_selected(item->text());
    });
    table_lay->addWidget(sector_table_);

    main_row->addWidget(table_card, 1);
    col->addLayout(main_row);

    // ── Performers row ───────────────────────────────────────────────────────
    performers_row_ = new QWidget(page);
    auto* perf_lay = new QHBoxLayout(performers_row_);
    perf_lay->setContentsMargins(0, 0, 0, 0);
    perf_lay->setSpacing(12);
    col->addWidget(performers_row_);

    // ── Concentration row ────────────────────────────────────────────────────
    concentration_row_ = new QWidget(page);
    auto* conc_lay = new QHBoxLayout(concentration_row_);
    conc_lay->setContentsMargins(0, 0, 0, 0);
    conc_lay->setSpacing(12);
    col->addWidget(concentration_row_);

    col->addStretch();

    scroll->setWidget(page);
    return scroll;
}

QWidget* AnalyticsSectorsView::build_correlation_tab() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(QString("QScrollArea { background:%1; border:none; }")
                              .arg(ui::colors::BG_BASE()));

    corr_panel_ = new QWidget;
    corr_panel_->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    auto* lay = new QVBoxLayout(corr_panel_);
    lay->setContentsMargins(16, 14, 16, 14);
    lay->setSpacing(10);

    corr_title_ = new QLabel(tr("HOLDINGS CORRELATION MATRIX"));
    corr_title_->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; letter-spacing:1.5px;")
                                   .arg(ui::colors::TEXT_TERTIARY())
                                   .arg(ui::fonts::font_px(-4)));
    lay->addWidget(corr_title_);

    corr_note_ = new QLabel(
        tr("Top-10 holdings by weight. Pearson correlation of daily returns over "
           "the trailing 30 trading days (from real price history)."));
    corr_note_->setWordWrap(true);
    corr_note_->setStyleSheet(QString("color:%1; font-size:%2px;")
                                   .arg(ui::colors::TEXT_TERTIARY())
                                   .arg(ui::fonts::font_px(-3)));
    lay->addWidget(corr_note_);

    auto* card = new QWidget(corr_panel_);
    card->setStyleSheet(QString("background:%1; border:1px solid %2;")
                             .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    auto* card_lay = new QVBoxLayout(card);
    card_lay->setContentsMargins(16, 16, 16, 16);
    corr_grid_ = new QGridLayout;
    corr_grid_->setSpacing(2);
    card_lay->addLayout(corr_grid_);
    lay->addWidget(card);
    lay->addStretch();

    scroll->setWidget(corr_panel_);
    return scroll;
}

void AnalyticsSectorsView::set_data(const portfolio::PortfolioSummary& summary, const QString& currency) {
    summary_ = summary;
    currency_ = currency;
    has_data_ = true;
    update_overview();
    update_correlation();
}

void AnalyticsSectorsView::set_correlation(const QHash<QString, double>& matrix) {
    correlation_matrix_ = matrix;
    update_correlation();
}

void AnalyticsSectorsView::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void AnalyticsSectorsView::retranslateUi() {
    if (tabs_) {
        if (overview_tab_index_ >= 0)    tabs_->setTabText(overview_tab_index_, tr("OVERVIEW"));
        if (correlation_tab_index_ >= 0) tabs_->setTabText(correlation_tab_index_, tr("CORRELATION"));
    }
    if (kpi_sectors_label_)      kpi_sectors_label_->setText(tr("SECTORS"));
    if (kpi_positions_label_)    kpi_positions_label_->setText(tr("POSITIONS"));
    if (kpi_market_value_label_) kpi_market_value_label_->setText(tr("MARKET VALUE"));
    if (kpi_pnl_label_)          kpi_pnl_label_->setText(tr("P&L"));
    if (donut_title_) donut_title_->setText(tr("SECTOR ALLOCATION"));
    if (table_title_) table_title_->setText(tr("SECTOR BREAKDOWN"));
    if (corr_title_)  corr_title_->setText(tr("HOLDINGS CORRELATION MATRIX"));
    if (corr_note_)
        corr_note_->setText(tr("Top-10 holdings by weight. Pearson correlation of daily returns over "
                               "the trailing 30 trading days (from real price history)."));

    if (sector_table_)
        sector_table_->setHorizontalHeaderLabels(
            {QString(), tr("SECTOR"), tr("POS"), tr("MKT VAL"), tr("WT"), tr("P&L"), tr("P&L%")});

    // re-render dynamic content so donut centre text, performer card labels,
    // concentration verdict strings, and correlation "Need 2+ holdings" message
    // pick up new locale
    if (has_data_) {
        update_overview();
        update_correlation();
    }
}

QVector<AnalyticsSectorsView::SectorInfo> AnalyticsSectorsView::compute_sectors() const {
    QHash<QString, SectorInfo> map;
    for (const auto& h : summary_.holdings) {
        QString sec = h.sector.isEmpty() ? QStringLiteral("Unclassified") : h.sector;
        auto& info = map[sec];
        info.name = sec;
        info.weight += h.weight;
        info.market_value += h.market_value;
        info.cost_basis += h.cost_basis;
        info.pnl += h.unrealized_pnl;
        info.count++;
        info.holdings.append(h);
    }

    QVector<SectorInfo> result;
    result.reserve(map.size());
    for (auto it = map.begin(); it != map.end(); ++it) {
        auto v = it.value();
        v.pnl_percent = v.cost_basis > 0 ? (v.pnl / v.cost_basis) * 100.0 : 0.0;
        result.append(v);
    }

    std::sort(result.begin(), result.end(),
              [](const SectorInfo& a, const SectorInfo& b) { return a.weight > b.weight; });
    return result;
}

void AnalyticsSectorsView::update_overview() {
    auto sectors = compute_sectors();
    update_kpi_strip(sectors);
    update_donut(sectors);
    update_sector_table(sectors);
    update_performers(sectors);
    update_concentration(sectors);
}

void AnalyticsSectorsView::update_kpi_strip(const QVector<SectorInfo>& sectors) {
    kpi_sectors_->setText(QString::number(sectors.size()));
    kpi_positions_->setText(QString::number(summary_.total_positions));
    kpi_market_value_->setText(format_money(summary_.total_market_value));
    const QString sign = summary_.total_unrealized_pnl >= 0 ? "+" : "";
    kpi_pnl_->setText(QString("%1%2  (%3)")
                           .arg(sign)
                           .arg(QLocale::system().toString(summary_.total_unrealized_pnl, 'f', 2))
                           .arg(format_pct(summary_.total_unrealized_pnl_percent, true)));
    kpi_pnl_->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700;")
                                 .arg(summary_.total_unrealized_pnl >= 0 ? ui::colors::POSITIVE()
                                                                         : ui::colors::NEGATIVE())
                                 .arg(ui::fonts::font_px(2)));
}

void AnalyticsSectorsView::update_donut(const QVector<SectorInfo>& sectors) {
    auto* chart = donut_view_->chart();
    chart->removeAllSeries();

    auto* series = new QPieSeries;
    series->setHoleSize(0.60);
    series->setPieSize(0.95);
    for (int i = 0; i < sectors.size(); ++i) {
        auto* slice = series->append(sectors[i].name, sectors[i].weight);
        slice->setColor(sector_color(i));
        slice->setBorderColor(QColor(ui::colors::BG_BASE()));
        slice->setBorderWidth(2);
    }
    chart->addSeries(series);

    connect(series, &QPieSeries::clicked, this, [this](QPieSlice* slice) {
        if (slice) emit sector_selected(slice->label());
    });

    // Center label
    donut_center_->setText(tr("%1\nsectors").arg(sectors.size()));
    donut_center_->adjustSize();
    auto reposition_center = [this]() {
        if (!donut_view_ || !donut_center_)
            return;
        donut_center_->move((donut_view_->width() - donut_center_->width()) / 2,
                            (donut_view_->height() - donut_center_->height()) / 2);
    };
    reposition_center();

    // Rebuild legend
    if (auto* old = legend_container_->layout()) {
        QLayoutItem* item;
        while ((item = old->takeAt(0)) != nullptr) {
            if (item->widget())
                item->widget()->deleteLater();
            delete item;
        }
        delete old;
    }
    auto* legend_lay = new QVBoxLayout(legend_container_);
    legend_lay->setContentsMargins(0, 4, 0, 0);
    legend_lay->setSpacing(4);

    int shown = std::min(sectors.size(), qsizetype{8});
    for (int i = 0; i < shown; ++i) {
        auto* row = new QHBoxLayout;
        row->setSpacing(8);
        row->addWidget(new Swatch(sector_color(i), 10, 10, legend_container_));

        auto* name = new QLabel(sectors[i].name, legend_container_);
        name->setStyleSheet(QString("color:%1; font-size:%2px; background:transparent;")
                                 .arg(ui::colors::TEXT_PRIMARY())
                                 .arg(ui::fonts::font_px(-3)));
        row->addWidget(name, 1);

        auto* wt = new QLabel(format_pct(sectors[i].weight, false), legend_container_);
        wt->setAlignment(Qt::AlignRight);
        wt->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; background:transparent;")
                               .arg(ui::colors::AMBER())
                               .arg(ui::fonts::font_px(-3)));
        row->addWidget(wt);

        legend_lay->addLayout(row);
    }
    if (sectors.size() > shown) {
        auto* more = new QLabel(tr("+%1 more").arg(sectors.size() - shown), legend_container_);
        more->setStyleSheet(QString("color:%1; font-size:%2px; background:transparent;")
                                 .arg(ui::colors::TEXT_TERTIARY())
                                 .arg(ui::fonts::font_px(-3)));
        legend_lay->addWidget(more);
    }
}

void AnalyticsSectorsView::update_sector_table(const QVector<SectorInfo>& sectors) {
    sector_table_->setRowCount(sectors.size());
    sector_table_->clearSelection();

    for (int r = 0; r < sectors.size(); ++r) {
        const auto& s = sectors[r];
        sector_table_->setRowHeight(r, 30);

        // Col 0 — color swatch
        sector_table_->setCellWidget(r, 0, new Swatch(sector_color(r), 10, 10, sector_table_));

        auto set_text = [&](int col, const QString& text, QColor color, Qt::Alignment align) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(align | Qt::AlignVCenter);
            item->setForeground(color);
            sector_table_->setItem(r, col, item);
        };

        set_text(1, s.name, QColor(ui::colors::TEXT_PRIMARY()), Qt::AlignLeft);
        set_text(2, QString::number(s.count), QColor(ui::colors::TEXT_SECONDARY()), Qt::AlignRight);
        set_text(3, format_money(s.market_value), QColor(ui::colors::TEXT_PRIMARY()), Qt::AlignRight);
        set_text(4, format_pct(s.weight), QColor(ui::colors::AMBER()), Qt::AlignRight);

        const QColor pnl_color = s.pnl >= 0 ? QColor(ui::colors::POSITIVE()) : QColor(ui::colors::NEGATIVE());
        const QString pnl_text = (s.pnl >= 0 ? QStringLiteral("+") : QString()) +
                                 QLocale::system().toString(s.pnl, 'f', 2);
        set_text(5, pnl_text, pnl_color, Qt::AlignRight);
        set_text(6, format_pct(s.pnl_percent, true), pnl_color, Qt::AlignRight);
    }
}

void AnalyticsSectorsView::update_performers(const QVector<SectorInfo>& sectors) {
    // Clear existing cards
    if (auto* lay = performers_row_->layout()) {
        QLayoutItem* it;
        while ((it = lay->takeAt(0)) != nullptr) {
            if (it->widget())
                it->widget()->deleteLater();
            delete it;
        }
    }

    if (sectors.isEmpty())
        return;

    auto* lay = qobject_cast<QHBoxLayout*>(performers_row_->layout());
    if (!lay) {
        lay = new QHBoxLayout(performers_row_);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(12);
    }

    // Identify interesting sectors
    SectorInfo largest = sectors.first();         // already weight-sorted desc
    SectorInfo smallest = sectors.last();
    SectorInfo best = sectors.first();
    SectorInfo worst = sectors.first();
    for (const auto& s : sectors) {
        if (s.pnl_percent > best.pnl_percent) best = s;
        if (s.pnl_percent < worst.pnl_percent) worst = s;
    }

    struct Card {
        QString label;
        const SectorInfo* s;
        QColor accent;
        QString metric_text;
    };
    const QVector<Card> cards = {
        {tr("LARGEST"),  &largest,  QColor(ui::colors::AMBER()),    format_pct(largest.weight)},
        {tr("SMALLEST"), &smallest, QColor(ui::colors::TEXT_SECONDARY()), format_pct(smallest.weight)},
        {tr("BEST"),     &best,     QColor(ui::colors::POSITIVE()), format_pct(best.pnl_percent, true)},
        {tr("WORST"),    &worst,    QColor(ui::colors::NEGATIVE()), format_pct(worst.pnl_percent, true)},
    };

    for (const auto& c : cards) {
        auto* card = new QWidget(performers_row_);
        card->setCursor(Qt::PointingHandCursor);
        card->setStyleSheet(QString("QWidget { background:%1; border:1px solid %2; }"
                                    "QWidget:hover { border:1px solid %3; }")
                                 .arg(ui::colors::BG_SURFACE())
                                 .arg(ui::colors::BORDER_DIM())
                                 .arg(c.accent.name()));
        auto* cl = new QVBoxLayout(card);
        cl->setContentsMargins(14, 10, 14, 10);
        cl->setSpacing(4);

        auto* label = new QLabel(c.label, card);
        label->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700;"
                                     "letter-spacing:1.5px; background:transparent;")
                                 .arg(c.accent.name())
                                 .arg(ui::fonts::font_px(-4)));
        cl->addWidget(label);

        auto* name = new QLabel(c.s->name, card);
        name->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; background:transparent;")
                                 .arg(ui::colors::TEXT_PRIMARY())
                                 .arg(ui::fonts::font_px()));
        cl->addWidget(name);

        auto* metric = new QLabel(c.metric_text, card);
        metric->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700;"
                                      "font-family:%3; background:transparent;")
                                   .arg(c.accent.name())
                                   .arg(ui::fonts::font_px(1))
                                   .arg(ui::fonts::DATA_FAMILY));
        cl->addWidget(metric);

        auto* sub = new QLabel(tr("%1 positions").arg(c.s->count), card);
        sub->setStyleSheet(QString("color:%1; font-size:%2px; background:transparent;")
                                .arg(ui::colors::TEXT_TERTIARY())
                                .arg(ui::fonts::font_px(-3)));
        cl->addWidget(sub);

        // Click to filter blotter by this sector
        card->installEventFilter(new SectorCardClickForwarder(this, c.s->name, card));

        lay->addWidget(card, 1);
    }
}

void AnalyticsSectorsView::update_concentration(const QVector<SectorInfo>& sectors) {
    if (auto* lay = concentration_row_->layout()) {
        QLayoutItem* it;
        while ((it = lay->takeAt(0)) != nullptr) {
            if (it->widget())
                it->widget()->deleteLater();
            delete it;
        }
    }
    auto* lay = qobject_cast<QHBoxLayout*>(concentration_row_->layout());
    if (!lay) {
        lay = new QHBoxLayout(concentration_row_);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(12);
    }

    if (sectors.isEmpty())
        return;

    // HHI: Σ (weight%)² over all sectors. Units already in percent-squared.
    double hhi = 0.0;
    for (const auto& s : sectors)
        hhi += s.weight * s.weight;

    double top3 = 0.0;
    for (int i = 0; i < std::min(qsizetype{3}, sectors.size()); ++i)
        top3 += sectors[i].weight;

    struct Box {
        QString title;
        QString value;
        QString verdict;
        QString sub;
    };
    const QVector<Box> boxes = {
        {tr("HHI CONCENTRATION"), QString::number(hhi, 'f', 0),
         verdict_for_hhi(hhi),
         tr("Herfindahl index across sectors (lower = more diversified)")},
        {tr("TOP-3 CONCENTRATION"), format_pct(top3),
         verdict_for_top3(top3),
         tr("Weight of the three largest sectors (%1)").arg(
             [&]() {
                 QStringList parts;
                 for (int i = 0; i < std::min(qsizetype{3}, sectors.size()); ++i)
                     parts << sectors[i].name;
                 return parts.join(", ");
             }())},
    };

    for (const auto& b : boxes) {
        auto* card = new QWidget(concentration_row_);
        card->setStyleSheet(QString("background:%1; border:1px solid %2;")
                                 .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
        auto* cl = new QVBoxLayout(card);
        cl->setContentsMargins(16, 12, 16, 12);
        cl->setSpacing(6);

        auto* title = new QLabel(b.title, card);
        title->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700;"
                                     "letter-spacing:1.5px; background:transparent;")
                                 .arg(ui::colors::TEXT_TERTIARY())
                                 .arg(ui::fonts::font_px(-4)));
        cl->addWidget(title);

        auto* value_row = new QHBoxLayout;
        value_row->setSpacing(10);
        auto* val = new QLabel(b.value, card);
        val->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700;"
                                   "font-family:%3; background:transparent;")
                                .arg(ui::colors::TEXT_PRIMARY())
                                .arg(ui::fonts::font_px(4))
                                .arg(ui::fonts::DATA_FAMILY));
        value_row->addWidget(val);

        // Translate the English verdict key to display string. verdict_color
        // still pattern-matches on the English source (stable across locales).
        QString verdict_display = b.verdict;
        if (b.verdict == QStringLiteral("Diversified"))       verdict_display = tr("Diversified");
        else if (b.verdict == QStringLiteral("Balanced"))     verdict_display = tr("Balanced");
        else if (b.verdict == QStringLiteral("Concentrated")) verdict_display = tr("Concentrated");
        auto* verdict = new QLabel(verdict_display, card);
        const QColor vc = verdict_color(b.verdict);
        verdict->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700;"
                                       "letter-spacing:1.5px; padding:4px 10px;"
                                       "background:rgba(%3,%4,%5,0.15); border:1px solid %1;")
                                    .arg(vc.name())
                                    .arg(ui::fonts::font_px(-3))
                                    .arg(vc.red()).arg(vc.green()).arg(vc.blue()));
        verdict->setAlignment(Qt::AlignCenter);
        value_row->addWidget(verdict);
        value_row->addStretch();
        cl->addLayout(value_row);

        auto* sub = new QLabel(b.sub, card);
        sub->setWordWrap(true);
        sub->setStyleSheet(QString("color:%1; font-size:%2px; background:transparent;")
                                .arg(ui::colors::TEXT_TERTIARY())
                                .arg(ui::fonts::font_px(-3)));
        cl->addWidget(sub);

        lay->addWidget(card, 1);
    }
}

void AnalyticsSectorsView::update_correlation() {
    // Clear existing grid contents
    if (corr_grid_) {
        QLayoutItem* it;
        while ((it = corr_grid_->takeAt(0)) != nullptr) {
            if (it->widget())
                it->widget()->deleteLater();
            delete it;
        }
    }

    if (summary_.holdings.size() < 2) {
        auto* msg = new QLabel(tr("Need 2+ holdings for correlation analysis"), corr_panel_);
        msg->setAlignment(Qt::AlignCenter);
        msg->setStyleSheet(QString("color:%1; font-size:%2px; padding:40px; background:transparent;")
                                .arg(ui::colors::TEXT_TERTIARY())
                                .arg(ui::fonts::font_px()));
        corr_grid_->addWidget(msg, 0, 0);
        return;
    }

    // Real correlation matrix is fetched asynchronously (PortfolioService::
    // fetch_correlation → correlation_computed). Until it arrives, show a
    // loading state rather than fabricating values from day-change signs.
    if (correlation_matrix_.isEmpty()) {
        auto* msg = new QLabel(tr("Computing correlations from price history…"), corr_panel_);
        msg->setAlignment(Qt::AlignCenter);
        msg->setStyleSheet(QString("color:%1; font-size:%2px; padding:40px; background:transparent;")
                                .arg(ui::colors::TEXT_TERTIARY())
                                .arg(ui::fonts::font_px()));
        corr_grid_->addWidget(msg, 0, 0);
        return;
    }

    auto sorted = summary_.holdings;
    std::sort(sorted.begin(), sorted.end(),
              [](const auto& a, const auto& b) { return a.weight > b.weight; });
    int n = static_cast<int>(std::min(qsizetype{10}, sorted.size()));

    auto make_header = [](const QString& s, bool rotate) {
        auto* lbl = new QLabel(s);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setFixedHeight(rotate ? 46 : 24);
        lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700;"
                                   "letter-spacing:.5px; background:transparent;")
                                .arg(ui::colors::TEXT_SECONDARY())
                                .arg(ui::fonts::font_px(-3)));
        return lbl;
    };

    // Corner
    corr_grid_->addWidget(make_header(QString(), false), 0, 0);
    // Top headers
    for (int c = 0; c < n; ++c)
        corr_grid_->addWidget(make_header(sorted[c].symbol.left(6), false), 0, c + 1);

    for (int r = 0; r < n; ++r) {
        corr_grid_->addWidget(make_header(sorted[r].symbol.left(6), false), r + 1, 0);

        for (int c = 0; c < n; ++c) {
            // Real Pearson correlation looked up from the service matrix,
            // keyed "SYMBOL_A|SYMBOL_B". Try both orderings; if a pair is
            // missing (e.g. no price history for a symbol) show a blank cell
            // rather than inventing a value.
            const QString& sa = sorted[r].symbol;
            const QString& sb = sorted[c].symbol;
            bool have = false;
            double corr = 0.0;
            if (r == c) {
                corr = 1.0;
                have = true;
            } else {
                const QString key = sa + "|" + sb;
                const QString rkey = sb + "|" + sa;
                if (correlation_matrix_.contains(key)) {
                    corr = correlation_matrix_.value(key);
                    have = true;
                } else if (correlation_matrix_.contains(rkey)) {
                    corr = correlation_matrix_.value(rkey);
                    have = true;
                }
            }

            auto* cell = new QLabel(have ? QString::number(corr, 'f', 2) : QStringLiteral("—"));
            cell->setAlignment(Qt::AlignCenter);
            cell->setFixedSize(54, 26);

            QColor bg;
            QString fg;
            if (!have) {
                bg = QColor(ui::colors::BORDER_DIM());
                fg = ui::colors::TEXT_TERTIARY();
            } else if (r == c) {
                bg = QColor(ui::colors::AMBER());
                fg = ui::colors::BG_BASE();
            } else if (corr > 0) {
                int alpha = static_cast<int>(std::min(1.0, std::abs(corr)) * 180);
                bg = QColor(22, 163, 74, alpha);
                fg = ui::colors::TEXT_PRIMARY();
            } else {
                int alpha = static_cast<int>(std::min(1.0, std::abs(corr)) * 180);
                bg = QColor(220, 38, 38, alpha);
                fg = ui::colors::TEXT_PRIMARY();
            }

            cell->setStyleSheet(QString("background:rgba(%1,%2,%3,%4); color:%5;"
                                        "font-size:%6px; font-weight:700; font-family:%7;")
                                    .arg(bg.red())
                                    .arg(bg.green())
                                    .arg(bg.blue())
                                    .arg(bg.alpha())
                                    .arg(fg)
                                    .arg(ui::fonts::font_px(-3))
                                    .arg(ui::fonts::DATA_FAMILY));

            corr_grid_->addWidget(cell, r + 1, c + 1);
        }
    }
}

bool SectorCardClickForwarder::eventFilter(QObject* obj, QEvent* ev) {
    if (ev->type() == QEvent::MouseButtonRelease && owner_)
        emit owner_->sector_selected(sector_);
    return QObject::eventFilter(obj, ev);
}

} // namespace fincept::screens
