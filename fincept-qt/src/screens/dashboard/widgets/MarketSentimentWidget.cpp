#include "screens/dashboard/widgets/MarketSentimentWidget.h"

#include "services/markets/RiskCompositeService.h"
#include "ui/charts/Gauge.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/BarCellDelegate.h"
#include "ui/tables/DataTable.h"
#include "ui/theme/Theme.h"

#include <QFontMetrics>
#include <QHeaderView>
#include <QLabel>
#include <QResizeEvent>
#include <QVBoxLayout>

#include <algorithm>

#include <cmath>

namespace fincept::screens::widgets {

namespace {
namespace rr_fmt = fincept::ui::formatting;
using fincept::services::RiskCompositeService;
constexpr int kRrZRole = Qt::UserRole + 11;
enum RrCol { RrName = 0, RrReading, RrZ, RrCount };
} // namespace

MarketSentimentWidget::MarketSentimentWidget(QWidget* parent)
    : BaseWidget(tr("RISK-ON / RISK-OFF"), parent, ui::colors::INFO()) {
    gauge_ = new ui::Gauge(this);
    gauge_->set_style(ui::Gauge::Style::SemiCircle);
    gauge_->set_range(0, 100);
    gauge_->set_zones({{0, 35, ui::GaugeZone::Tone::Down, tr("Risk-off")},
                       {35, 45, ui::GaugeZone::Tone::Neutral, tr("Leaning risk-off")},
                       {45, 55, ui::GaugeZone::Tone::Neutral, tr("Neutral")},
                       {55, 65, ui::GaugeZone::Tone::Neutral, tr("Leaning risk-on")},
                       {65, 100, ui::GaugeZone::Tone::Up, tr("Risk-on")}});
    gauge_->set_value_formatter([](double v) { return rr_fmt::number(v, 0); });
    gauge_->setMinimumHeight(90);
    content_layout()->addWidget(gauge_, 1);

    regime_ = new QLabel(this);
    regime_->setAlignment(Qt::AlignCenter);
    regime_->setWordWrap(true);
    content_layout()->addWidget(regime_);

    table_ = new ui::DataTable(this);
    table_->set_headers({tr("SIGNAL"), tr("READING"), tr("Z (±2)")});
    // SIGNAL sized to its text, the Z bar a fixed width (sized in render() from
    // the font), READING takes the rest and elides — sizing READING to its text
    // pushed the Z column off a 4-column tile.
    table_->horizontalHeader()->setStretchLastSection(false);
    table_->horizontalHeader()->setSectionResizeMode(RrName, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(RrReading, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(RrZ, QHeaderView::Fixed);
    table_->setTextElideMode(Qt::ElideRight);
    table_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* zbar = new ui::BarCellDelegate(ui::BarCellDelegate::Mode::SignedBar, table_);
    zbar->set_value_role(kRrZRole);
    zbar->set_range(-2.0, 2.0);
    table_->setItemDelegateForColumn(RrZ, zbar);
    // Exactly as tall as its rows (render()); the gauge takes the remaining space,
    // so no signal row is ever cut off at the bottom of the tile.
    content_layout()->addWidget(table_, 0);

    footer_ = new QLabel(this);
    footer_->setWordWrap(true);
    footer_->setToolTip(RiskCompositeService::methodology());
    content_layout()->addWidget(footer_);

    connect(&RiskCompositeService::instance(), &RiskCompositeService::updated, this, [this]() {
        schedule_render([this]() { render(); });
    });
    connect(this, &BaseWidget::refresh_requested, this, []() { RiskCompositeService::instance().refresh(); });
    on_theme_changed();
    set_loading(true);
}

void MarketSentimentWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    RiskCompositeService::instance().attach(this);
    render();
}

void MarketSentimentWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    RiskCompositeService::instance().detach(this);
}

void MarketSentimentWidget::render() {
    const auto& c = RiskCompositeService::instance().current();
    if (c.inputs.isEmpty())
        return; // nothing delivered yet: keep the spinner
    set_loading(false);
    int available = 0;
    for (const auto& s : c.inputs)
        available += s.available() ? 1 : 0;

    if (c.complete) {
        gauge_->set_value(c.score);
        gauge_->set_caption(c.regime);
        regime_->setText(tr("%1 · score %2 of 100").arg(c.regime, rr_fmt::number(c.score, 0)));
    } else {
        gauge_->set_value(std::numeric_limits<double>::quiet_NaN());
        gauge_->set_caption(tr("Incomplete"));
        regime_->setText(tr("No score until all inputs are in (%1 of %2) — missing: %3")
                             .arg(available)
                             .arg(c.inputs.size())
                             .arg(c.missing.join(QStringLiteral(", "))));
    }

    table_->setRowCount(c.inputs.size());
    {
        const QFontMetrics fm(table_->font());
        table_->setColumnWidth(RrZ, std::max(fm.horizontalAdvance(QStringLiteral("Z (±2)")) + 24,
                                             fm.horizontalAdvance(QStringLiteral("+0.00")) * 2 + 16));
    }
    for (int i = 0; i < c.inputs.size(); ++i) {
        const auto& s = c.inputs[i];
        auto put = [&](int col, const QString& text) {
            auto* it = table_->item(i, col);
            if (!it) {
                it = new QTableWidgetItem;
                table_->setItem(i, col, it);
            }
            it->setText(text);
            it->setForeground(QColor(s.available() ? ui::colors::TEXT_PRIMARY() : ui::colors::TEXT_TERTIARY()));
            QString tip = s.description + QStringLiteral("\n") + s.symbols.join(QStringLiteral(", "));
            if (!s.error.isEmpty())
                tip += QStringLiteral("\n") + s.error;
            it->setToolTip(tip);
            return it;
        };
        put(RrName, s.name);
        put(RrReading, s.raw_text.isEmpty() ? rr_fmt::placeholder() : s.raw_text);
        auto* z = put(RrZ, s.available() ? rr_fmt::signed_number(s.z, 2) : rr_fmt::placeholder());
        z->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        z->setData(kRrZRole, s.available() ? QVariant(s.z) : QVariant());
    }
    footer_->setText((c.as_of.isValid() ? tr("Daily closes to %1").arg(rr_fmt::calendar_date(c.as_of))
                                        : tr("Daily closes")) +
                     QStringLiteral(" · ") + tr("hover for the method"));
    fit_table_height(); // after the texts above: their heights decide the room left
}

void MarketSentimentWidget::resizeEvent(QResizeEvent* e) {
    BaseWidget::resizeEvent(e);
    fit_table_height();
}

void MarketSentimentWidget::fit_table_height() {
    const int rows = table_->rowCount();
    if (rows <= 0)
        return;
    const int row_h = table_->verticalHeader()->defaultSectionSize();
    const int head_h = table_->horizontalHeader()->sizeHint().height();
    const int frame = 2 * table_->frameWidth() + 2;
    const int full = rows * row_h + head_h + frame;
    // As tall as its rows when the tile has the room. When it has not (a large
    // font: gauge, caption, five rows and a two-line footer are taller than the
    // tile) the layout used to overflow, and the footer was drawn across the
    // last rows. The table then gets the whole rows that fit and scrolls.
    int h = full;
    const QRect area = content_layout()->contentsRect();
    if (area.height() > 0 && area.width() > 0) {
        const auto text_h = [&area](const QLabel* l) {
            return l->isHidden() ? 0 : std::max(l->heightForWidth(area.width()), l->minimumSizeHint().height());
        };
        const int others = gauge_->minimumHeight() + text_h(regime_) + text_h(footer_) +
                           3 * std::max(0, content_layout()->spacing());
        const int room = area.height() - others;
        if (room < full)
            h = head_h + frame + std::max(1, (room - head_h - frame) / row_h) * row_h;
    }
    table_->setVerticalScrollBarPolicy(h < full ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    if (table_->minimumHeight() != h || table_->maximumHeight() != h)
        table_->setFixedHeight(h);
}

void MarketSentimentWidget::on_theme_changed() {
    set_css_cached(regime_, QString("color:%1;font-size:%2px;font-weight:bold;background:transparent;")
                               .arg(ui::colors::TEXT_PRIMARY())
                               .arg(ui::fonts::px(-3)));
    set_css_cached(footer_, QString("color:%1;font-size:%2px;padding:0 6px 2px 6px;background:transparent;")
                               .arg(ui::colors::TEXT_TERTIARY())
                               .arg(ui::fonts::px(-4)));
    render();
}

void MarketSentimentWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    set_title(tr("RISK-ON / RISK-OFF"));
    table_->set_headers({tr("SIGNAL"), tr("READING"), tr("Z (±2)")});
    footer_->setToolTip(RiskCompositeService::methodology());
    render();
}

} // namespace fincept::screens::widgets
