#include "screens/crypto_center/panels/MarketsListPanel.h"

#include "core/logging/Logger.h"
#include "services/prediction/PredictionExchangeAdapter.h"
#include "services/prediction/PredictionExchangeRegistry.h"
#include "services/prediction/fincept_internal/FinceptInternalAdapter.h"
#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHideEvent>
#include <QLabel>
#include <QLocale>
#include <QShowEvent>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens::panels {

namespace {

namespace pr = fincept::services::prediction;

QString font_stack() {
    return QStringLiteral(
        "'Consolas','Cascadia Mono','JetBrains Mono','SF Mono',monospace");
}

QString format_volume(double v) {
    if (v >= 1e9) return QStringLiteral("%1B").arg(QLocale::system().toString(v / 1e9, 'f', 1));
    if (v >= 1e6) return QStringLiteral("%1M").arg(QLocale::system().toString(v / 1e6, 'f', 1));
    if (v >= 1e3) return QStringLiteral("%1k").arg(QLocale::system().toString(v / 1e3, 'f', 0));
    return QLocale::system().toString(v, 'f', 0);
}

QString format_price(double p) {
    if (p < 0.0) p = 0.0;
    if (p > 1.0) p = 1.0;
    return QStringLiteral("%1").arg(QLocale::system().toString(p, 'f', 2));
}

pr::fincept_internal::FinceptInternalAdapter* fincept_adapter() {
    auto* base = pr::PredictionExchangeRegistry::instance().adapter(
        QStringLiteral("fincept"));
    return dynamic_cast<pr::fincept_internal::FinceptInternalAdapter*>(base);
}

} // namespace

MarketsListPanel::MarketsListPanel(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("marketsListPanel"));
    build_ui();
    apply_theme();
}

MarketsListPanel::~MarketsListPanel() = default;

// ── Layout ─────────────────────────────────────────────────────────────────

void MarketsListPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* head = new QWidget(this);
    head->setObjectName(QStringLiteral("marketsListHead"));
    head->setFixedHeight(34);
    auto* hl = new QHBoxLayout(head);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(0);

    title_ = new QLabel(QStringLiteral("MARKETS"), head);
    title_->setObjectName(QStringLiteral("marketsListTitle"));
    status_pill_ = new QLabel(QStringLiteral("DEMO"), head);
    status_pill_->setObjectName(QStringLiteral("marketsListStatusDemo"));

    hl->addWidget(title_);
    hl->addStretch();
    hl->addWidget(status_pill_);
    root->addWidget(head);

    table_ = new QTableWidget(this);
    table_->setObjectName(QStringLiteral("marketsListTable"));
    table_->setColumnCount(5);
    table_->setHorizontalHeaderLabels({
        tr("MARKET"), tr("YES"), tr("NO"), tr("24h VOL"), tr("EXPIRES")
    });
    table_->verticalHeader()->setVisible(false);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(false);
    table_->setShowGrid(false);
    table_->setMinimumHeight(220);

    auto* h = table_->horizontalHeader();
    h->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int i = 1; i <= 4; ++i) {
        h->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    }

    connect(table_, &QTableWidget::cellDoubleClicked, this,
            &MarketsListPanel::on_row_double_clicked);

    root->addWidget(table_, 1);

    footer_note_ = new QLabel(this);
    footer_note_->setObjectName(QStringLiteral("marketsListFooter"));
    footer_note_->setWordWrap(true);
    footer_note_->setContentsMargins(12, 8, 12, 12);
    footer_note_->setText(tr(
        "Demo dataset. Set `fincept.markets_endpoint` in SecureStorage and "
        "deploy the fincept_market Anchor program for live trading."));
    root->addWidget(footer_note_);
}

void MarketsListPanel::apply_theme() {
    using namespace ui::colors;
    const QString font = font_stack();

    const QString ss = QStringLiteral(
        "QWidget#marketsListPanel { background:%1; }"
        "QWidget#marketsListHead { background:%2; border-bottom:1px solid %3; }"
        "QLabel#marketsListTitle { color:%4; font-family:%5; font-size:11px;"
        "  font-weight:700; letter-spacing:1.2px; background:transparent; }"
        "QLabel#marketsListStatusLive { color:%6; font-family:%5; font-size:10px;"
        "  font-weight:700; letter-spacing:1.2px; background:transparent; }"
        "QLabel#marketsListStatusDemo { color:%7; font-family:%5; font-size:10px;"
        "  font-weight:700; letter-spacing:1.2px; background:transparent; }"
        "QLabel#marketsListStatusError { color:%8; font-family:%5; font-size:10px;"
        "  font-weight:700; letter-spacing:1.2px; background:transparent; }"
        "QTableWidget#marketsListTable { background:%1; color:%9;"
        "  font-family:%5; font-size:11px; gridline-color:%3; border:none; }"
        "QTableWidget#marketsListTable::item { padding:6px 10px; }"
        "QTableWidget#marketsListTable::item:selected { background:rgba(217,119,6,0.10);"
        "  color:%4; }"
        "QHeaderView::section { background:%2; color:%7; font-family:%5;"
        "  font-size:10px; font-weight:700; letter-spacing:1.2px; padding:6px 10px;"
        "  border:none; border-bottom:1px solid %3; }"
        "QLabel#marketsListFooter { color:%7; font-family:%5; font-size:10px;"
        "  background:transparent; }"
    )
        .arg(BG_BASE(),         // %1
             BG_SURFACE(),      // %2 (panel chrome bg lives outside; this matches table header)
             BORDER_DIM(),      // %3
             AMBER(),           // %4
             font,              // %5
             POSITIVE(),        // %6
             TEXT_TERTIARY(),   // %7
             NEGATIVE(),        // %8
             TEXT_PRIMARY());   // %9

    setStyleSheet(ss);
}

// ── Lifecycle ──────────────────────────────────────────────────────────────

void MarketsListPanel::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (subscribed_) return;
    auto* adapter = fincept_adapter();
    if (!adapter) {
        set_status_error(tr("FinceptInternalAdapter not registered"));
        return;
    }
    set_status_demo(adapter->is_demo_mode());
    connect(adapter,
            &fincept::services::prediction::PredictionExchangeAdapter::markets_ready,
            this, &MarketsListPanel::on_markets_ready);
    connect(adapter,
            &fincept::services::prediction::PredictionExchangeAdapter::error_occurred,
            this, &MarketsListPanel::on_error);
    subscribed_ = true;
    adapter->list_markets({}, {}, /*limit=*/50, /*offset=*/0);
}

void MarketsListPanel::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    if (!subscribed_) return;
    auto* adapter = fincept_adapter();
    if (adapter) {
        disconnect(adapter, nullptr, this, nullptr);
    }
    subscribed_ = false;
}

// ── Adapter callbacks ──────────────────────────────────────────────────────

void MarketsListPanel::on_markets_ready(
    const QVector<pr::PredictionMarket>& markets) {
    rebuild_table(markets);
}

void MarketsListPanel::on_error(const QString& context, const QString& message) {
    Q_UNUSED(context);
    set_status_error(message);
}

void MarketsListPanel::on_row_double_clicked(int row, int /*column*/) {
    auto* item = table_->item(row, 0);
    if (!item) return;
    const auto market_id = item->data(Qt::UserRole).toString();
    if (market_id.isEmpty()) return;
    LOG_INFO("MarketsListPanel",
             QString("market double-clicked: %1").arg(market_id));
    // The order-entry panel is part of Phase 4 polish — wired via a
    // signal in the next iteration. For now, surface the click via the
    // log so the seam is testable.
}

// ── Rendering ──────────────────────────────────────────────────────────────

void MarketsListPanel::set_status_demo(bool demo) {
    if (demo) {
        status_pill_->setText(QStringLiteral("DEMO"));
        status_pill_->setObjectName(QStringLiteral("marketsListStatusDemo"));
    } else {
        status_pill_->setText(QStringLiteral("● LIVE"));
        status_pill_->setObjectName(QStringLiteral("marketsListStatusLive"));
    }
    status_pill_->style()->unpolish(status_pill_);
    status_pill_->style()->polish(status_pill_);
}

void MarketsListPanel::set_status_error(const QString& message) {
    status_pill_->setText(QStringLiteral("ERROR"));
    status_pill_->setObjectName(QStringLiteral("marketsListStatusError"));
    status_pill_->setToolTip(message);
    status_pill_->style()->unpolish(status_pill_);
    status_pill_->style()->polish(status_pill_);
    LOG_WARN("MarketsListPanel", QString("error: %1").arg(message));
}

void MarketsListPanel::rebuild_table(
    const QVector<pr::PredictionMarket>& markets) {
    table_->setRowCount(markets.size());
    int row = 0;
    for (const auto& m : markets) {
        auto* q = new QTableWidgetItem(m.question);
        q->setData(Qt::UserRole, m.key.market_id);
        table_->setItem(row, 0, q);

        const double yes = m.outcomes.size() >= 1 ? m.outcomes[0].price : 0.0;
        const double no = m.outcomes.size() >= 2 ? m.outcomes[1].price : (1.0 - yes);

        table_->setItem(row, 1, new QTableWidgetItem(format_price(yes)));
        table_->setItem(row, 2, new QTableWidgetItem(format_price(no)));
        table_->setItem(row, 3, new QTableWidgetItem(format_volume(m.volume)));
        table_->setItem(row, 4, new QTableWidgetItem(m.end_date_iso));
        ++row;
    }
}

} // namespace fincept::screens::panels
