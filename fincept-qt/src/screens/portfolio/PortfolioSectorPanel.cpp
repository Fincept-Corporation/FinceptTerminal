// src/screens/portfolio/PortfolioSectorPanel.cpp
#include "screens/portfolio/PortfolioSectorPanel.h"

#include "screens/portfolio/PortfolioCorrelation.h"
#include "screens/portfolio/PortfolioFormat.h"
#include "services/portfolio/PortfolioService.h"
#include "ui/charts/DonutChart.h"
#include "ui/charts/MatrixHeatmap.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/PanelHeader.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QComboBox>
#include <QEvent>
#include <QLabel>
#include <QHBoxLayout>
#include <QMap>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

PortfolioSectorPanel::PortfolioSectorPanel(QWidget* parent) : QWidget(parent) {
    build_ui();
    ui::formatting::on_format_changed(this, [this]() { update_donut(); }); // slice values re-format
}

QColor PortfolioSectorPanel::sector_color(int index) {
    return ui::colors::chart_color(index);
}

void PortfolioSectorPanel::build_ui() {
    // One selector stylesheet, before any child exists (the theme manager's
    // re-style pass keeps it in step with theme / font changes).
    apply_styles();
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto header = ui::make_panel_header(tr("ALLOCATION"), this);
    title_label_ = header.title_label;
    dim_cb_ = new QComboBox(this);
    using AD = portfolio::AllocationDimension;
    for (const AD d : {AD::Sector, AD::AssetClass, AD::Region, AD::Currency})
        dim_cb_->addItem(pfmt::dimension_name(d), static_cast<int>(d));
    dim_cb_->setToolTip(tr("Group the donut by"));
    connect(dim_cb_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { update_donut(); });
    header.controls_slot->layout()->addWidget(dim_cb_);
    layout->addWidget(header.header);

    auto* split = new QSplitter(Qt::Vertical, this);
    split->setChildrenCollapsible(false);
    donut_ = new ui::DonutChart(split);
    donut_->set_max_slices(8);
    donut_->setMinimumHeight(ui::metrics::height(150));
    connect(donut_, &ui::DonutChart::slice_clicked, this, [this](const QString& id) {
        if (static_cast<portfolio::AllocationDimension>(dim_cb_->currentData().toInt()) !=
                portfolio::AllocationDimension::Sector ||
            id == QLatin1String("__other__"))
            return;
        select_sector(id == selected_sector_ ? QString() : id);
    });
    split->addWidget(donut_);

    auto* corr_box = new QWidget(split);
    auto* cv = new QVBoxLayout(corr_box);
    cv->setContentsMargins(0, 0, 0, 0);
    cv->setSpacing(0);
    corr_title_ = new QLabel(corr_box);
    corr_title_->setObjectName("pspCorrTitle");
    corr_title_->setContentsMargins(10, 4, 10, 2);
    cv->addWidget(corr_title_);
    auto* scroll = new QScrollArea(corr_box);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    corr_ = new ui::MatrixHeatmap(scroll);
    corr_->set_min_cell_size(14);
    scroll->setWidget(corr_);
    cv->addWidget(scroll, 1);
    auto* note_row = new QHBoxLayout;
    note_row->setContentsMargins(0, 0, 8, 0);
    note_row->setSpacing(6);
    corr_note_ = new QLabel(corr_box);
    corr_note_->setObjectName("pspCorrNote");
    corr_note_->setWordWrap(true);
    corr_note_->setContentsMargins(10, 2, 10, 4);
    note_row->addWidget(corr_note_, 1);
    corr_retry_btn_ = new QPushButton(tr("RETRY"), corr_box);
    corr_retry_btn_->setObjectName("pspRetry");
    corr_retry_btn_->setCursor(Qt::PointingHandCursor);
    corr_retry_btn_->setToolTip(tr("Download the price history again"));
    corr_retry_btn_->setVisible(false);
    connect(corr_retry_btn_, &QPushButton::clicked, this, [this]() {
        if (corr_retry_)
            corr_retry_();
    });
    note_row->addWidget(corr_retry_btn_, 0, Qt::AlignVCenter);
    cv->addLayout(note_row);
    split->addWidget(corr_box);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 1);
    layout->addWidget(split, 1);

    retranslateUi();
}

void PortfolioSectorPanel::apply_styles() {
    setStyleSheet(QString("#pspCorrTitle { color:%1; font-size:%2px; font-weight:700; letter-spacing:1px; }"
                          "#pspCorrNote { color:%3; font-size:%4px; }"
                          "QComboBox { background:%5; color:%6; border:1px solid %7; padding:1px 6px; font-size:%4px; }"
                          "QPushButton#pspRetry { background:transparent; color:%8; border:1px solid %9;"
                          "  padding:1px 8px; font-size:%4px; font-weight:700; }"
                          "QPushButton#pspRetry:hover { border-color:%8; }")
                      .arg(ui::colors::TEXT_SECONDARY(), ui::fonts::px(-4), ui::colors::TEXT_TERTIARY(),
                           ui::fonts::px(-4), ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(),
                           ui::colors::BORDER_DIM(), ui::colors::AMBER(), ui::colors::BORDER_MED()));
}

void PortfolioSectorPanel::set_holdings(const QVector<portfolio::HoldingWithQuote>& holdings) {
    holdings_ = holdings;
    update_donut();
    update_correlation();
}

void PortfolioSectorPanel::set_currency(const QString& currency) {
    if (!currency.isEmpty())
        currency_ = currency;
    update_donut();
}

void PortfolioSectorPanel::set_allocation(const portfolio::AllocationReport& report) {
    report_ = report;
    have_report_ = true;
    update_donut();
}

void PortfolioSectorPanel::set_correlation(const QHash<QString, double>& matrix) {
    corr_matrix_ = matrix;
    corr_pending_ = false;
    corr_error_.clear();
    update_correlation();
}

void PortfolioSectorPanel::set_correlation_pending(bool pending) {
    if (corr_pending_ == pending)
        return;
    corr_pending_ = pending;
    if (pending)
        corr_error_.clear(); // a new run replaces the old failure
    update_correlation();
}

void PortfolioSectorPanel::set_correlation_error(const QString& message) {
    if (corr_error_ == message && !corr_pending_)
        return;
    corr_error_ = message;
    if (!message.isEmpty())
        corr_pending_ = false;
    update_correlation();
}

void PortfolioSectorPanel::select_sector(const QString& sector) {
    selected_sector_ = sector;
    emit sector_selected(sector);
    update_donut();
}

void PortfolioSectorPanel::update_donut() {
    using AD = portfolio::AllocationDimension;
    const auto dim = static_cast<AD>(dim_cb_->currentData().toInt());
    QVector<ui::DonutSlice> slices;
    if (dim == AD::Sector || !have_report_) {
        // Sector straight from the holdings (always available), valued positions only.
        QMap<QString, double> by;
        for (const auto& h : holdings_)
            if (h.valued() && h.market_value > 0)
                by[h.sector.isEmpty() ? tr("Unclassified") : h.sector] += h.market_value;
        QVector<QPair<QString, double>> v;
        for (auto it = by.constBegin(); it != by.constEnd(); ++it)
            v.append({it.key(), it.value()});
        std::sort(v.begin(), v.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
        for (const auto& [name, value] : v)
            slices.append({name, name == selected_sector_ ? QStringLiteral("▶ %1").arg(name) : name, value, {}});
    } else {
        for (const auto& b : report_.breakdowns) {
            if (b.dimension != dim)
                continue;
            for (const auto& s : b.slices)
                slices.append({s.key, pfmt::slice_label(dim, s.key), s.value, {}});
        }
    }
    const QString ccy = currency_;
    donut_->set_value_formatter([ccy](double v) { return pfmt::money_c(v, ccy); });
    donut_->set_slices(slices);
    donut_->set_center_text(pfmt::dimension_name(dim).toUpper());
    donut_->set_state(slices.isEmpty() ? ui::PaintedChart::Empty : ui::PaintedChart::Ready,
                      tr("No valued positions yet."));
    donut_->setToolTip(dim == AD::Sector ? tr("Click a sector to filter the positions table") : QString());
}

void PortfolioSectorPanel::update_correlation() {
    QStringList syms;
    for (const auto& h : holdings_)
        syms << h.symbol;
    // Rebuild only when the matrix, the symbol set or the load state changed (not on
    // every price poll).
    QString sig = syms.join(QLatin1Char(','));
    sig += QString::number(corr_matrix_.size());
    sig += corr_pending_ ? QStringLiteral("|p|") : QStringLiteral("|-|");
    sig += corr_error_;
    for (auto it = corr_matrix_.constBegin(); it != corr_matrix_.constEnd(); ++it)
        sig += QString::number(it.value(), 'f', 3);
    if (sig == corr_signature_)
        return;
    corr_signature_ = sig;

    const auto m = pfcorr::build(corr_matrix_, syms);
    if (m.labels.size() < 2) {
        // Loading only while a run is really in flight; a failed run says why (with
        // RETRY); otherwise there is nothing to correlate.
        int valued = 0;
        for (const auto& h : holdings_)
            if (h.valued())
                ++valued;
        corr_->clear();
        const bool can_load = valued >= 2;
        if (can_load && !corr_error_.isEmpty()) {
            corr_->set_state(ui::PaintedChart::Error, tr("Price history could not be downloaded: %1").arg(corr_error_));
            corr_note_->setText(tr("Correlation needs the daily price history of the holdings."));
        } else if (can_load && corr_pending_) {
            corr_->set_state(ui::PaintedChart::Loading, tr("Downloading price history…"));
            corr_note_->clear();
        } else {
            corr_->set_state(ui::PaintedChart::Empty, tr("Needs two or more holdings with price history."));
            corr_note_->clear();
        }
        corr_retry_btn_->setVisible(can_load && !corr_error_.isEmpty());
        return;
    }
    corr_retry_btn_->setVisible(false);
    corr_->set_state(ui::PaintedChart::Ready);
    pfcorr::show(corr_, m);
    // The matrix sits in a scroll area that only gave it the height left over: the
    // rotated column labels took all of it and no cell was visible. Ask for room for
    // the labels plus a readable row per holding — the area scrolls when short.
    const QFontMetrics lfm(ui::fonts::font(-3));
    int label_w = 0;
    for (const QString& s : m.labels)
        label_w = std::max(label_w, lfm.horizontalAdvance(s));
    corr_->setMinimumHeight(std::min(label_w, 120) + 16 + static_cast<int>(m.labels.size()) * ui::metrics::height(16) +
                            ui::metrics::height(28));
    // One line under the matrix; the explanation lives in the tooltip (it used to take
    // four lines of the panel's little height).
    corr_note_->setText(tr("Avg pair %1 · highest %2 (%3) · lowest %4 (%5)")
                            .arg(pfmt::num(m.avg_pair, 2), m.max_pair, pfmt::num(m.max_r, 2), m.min_pair,
                                 pfmt::num(m.min_r, 2)));
    corr_note_->setToolTip(tr("Red: move together (less diversification) · green: move opposite. Pearson correlation "
                              "of daily returns in the base currency."));
}

void PortfolioSectorPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void PortfolioSectorPanel::retranslateUi() {
    if (title_label_)
        title_label_->setText(tr("ALLOCATION"));
    using AD = portfolio::AllocationDimension;
    for (int i = 0; i < dim_cb_->count(); ++i)
        dim_cb_->setItemText(i, pfmt::dimension_name(static_cast<AD>(dim_cb_->itemData(i).toInt())));
    corr_title_->setText(tr("CORRELATION — %1 TRADING DAYS").arg(services::PortfolioService::kCorrelationWindowDays));
    corr_signature_.clear();
    update_donut();
    update_correlation();
}

} // namespace fincept::screens
