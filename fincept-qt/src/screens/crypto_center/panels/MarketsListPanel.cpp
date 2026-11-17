#include "screens/crypto_center/panels/MarketsListPanel.h"
#include "screens/crypto_center/CryptoCenterText.h"

#include "core/logging/Logger.h"
#include "services/prediction/PredictionExchangeAdapter.h"
#include "services/prediction/PredictionExchangeRegistry.h"
#include "services/prediction/fincept_internal/FinceptInternalAdapter.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/EmptyState.h"

#include <QDate>
#include <QDateTime>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHideEvent>
#include <QLabel>
#include <QShowEvent>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

namespace cc_fmt = fincept::ui::formatting;

namespace fincept::screens::panels {

namespace {

namespace pr = fincept::services::prediction;

using fincept::screens::crypto_center_text::font_stack; // CryptoCenterText.h (one definition)

/// Traded volume, abbreviated per the user's compact-number setting.
QString format_volume(double v) {
    return cc_fmt::volume(v, cc_fmt::Compact::FromSettings);
}

/// A YES / NO price: a probability 0–1, not money.
QString mlp_format_price(double p) {
    if (p < 0.0)
        p = 0.0;
    if (p > 1.0)
        p = 1.0;
    return cc_fmt::number(p, 2);
}

pr::fincept_internal::FinceptInternalAdapter* fincept_adapter() {
    auto* base = pr::PredictionExchangeRegistry::instance().adapter(QStringLiteral("fincept"));
    return dynamic_cast<pr::fincept_internal::FinceptInternalAdapter*>(base);
}

} // namespace

MarketsListPanel::MarketsListPanel(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("marketsListPanel"));
    build_ui();
    apply_theme();
    // A number-format change re-renders the volumes / prices of the list on screen.
    cc_fmt::on_format_changed(this, [this]() {
        if (!shown_markets_.isEmpty())
            rebuild_table(shown_markets_);
    });
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

    title_ = new QLabel(tr("MARKETS"), head);
    title_->setObjectName(QStringLiteral("marketsListTitle"));
    status_pill_ = new QLabel(tr("DEMO"), head);
    status_pill_->setObjectName(QStringLiteral("marketsListStatusDemo"));

    hl->addWidget(title_);
    hl->addStretch();
    hl->addWidget(status_pill_);
    root->addWidget(head);

    table_ = new QTableWidget(this);
    table_->setObjectName(QStringLiteral("marketsListTable"));
    table_->setColumnCount(5);
    table_->setHorizontalHeaderLabels({tr("MARKET"), tr("YES"), tr("NO"), tr("24h VOL"), tr("EXPIRES")});
    table_->verticalHeader()->setVisible(false);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(false);
    table_->setShowGrid(false);
    table_->setMinimumHeight(220);

    auto* h = table_->horizontalHeader();
    h->setSectionResizeMode(0, QHeaderView::Stretch);
    // Interactive, fitted once after each fill (rebuild_table) — a ResizeToContents
    // header re-measures the column on every cell written.
    for (int i = 1; i <= 4; ++i) {
        h->setSectionResizeMode(i, QHeaderView::Interactive);
    }

    connect(table_, &QTableWidget::cellDoubleClicked, this, &MarketsListPanel::on_row_double_clicked);

    root->addWidget(table_, 1);

    footer_note_ = new QLabel(this);
    footer_note_->setObjectName(QStringLiteral("marketsListFooter"));
    footer_note_->setWordWrap(true);
    footer_note_->setContentsMargins(12, 8, 12, 12);
    footer_note_->setText(tr("SAMPLE MARKETS — illustrative questions and prices, not live and not tradable. "
                             "Fincept prediction markets are not launched yet."));
    root->addWidget(footer_note_);
}

void MarketsListPanel::apply_theme() {
    using namespace ui::colors;
    const QString font = font_stack();

    const QString ss = QString("QWidget#marketsListPanel { background:%1; }"
                                      "QWidget#marketsListHead { background:%2; border-bottom:1px solid %3; }"
                                      "QLabel#marketsListTitle { color:%4; font-family:%5; font-size:" + fincept::ui::fonts::px(-3) + "px;"
                                      "  font-weight:700; letter-spacing:1.2px; background:transparent; }"
                                      "QLabel#marketsListStatusLive { color:%6; font-family:%5; font-size:" + fincept::ui::fonts::px(-4) + "px;"
                                      "  font-weight:700; letter-spacing:1.2px; background:transparent; }"
                                      "QLabel#marketsListStatusDemo { color:%4; font-family:%5; font-size:" + fincept::ui::fonts::px(-4) + "px;"
                                      "  font-weight:700; letter-spacing:1.2px; background:" + ui::colors::alpha(AMBER, 0.10) + ";"
                                      "  border:1px solid %4; padding:2px 8px; }"
                                      "QLabel#marketsListStatusError { color:%8; font-family:%5; font-size:" + fincept::ui::fonts::px(-4) + "px;"
                                      "  font-weight:700; letter-spacing:1.2px; background:transparent; }"
                                      "QTableWidget#marketsListTable { background:%1; color:%9;"
                                      "  font-family:%5; font-size:" + fincept::ui::fonts::px(-3) + "px; gridline-color:%3; border:none; }"
                                      "QTableWidget#marketsListTable::item { padding:6px 10px; }"
                                      "QTableWidget#marketsListTable::item:selected { background:" + ui::colors::alpha(AMBER, 0.10) + ";"
                                      "  color:%4; }"
                                      "QHeaderView::section { background:%2; color:%7; font-family:%5;"
                                      "  font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; letter-spacing:1.2px; padding:6px 10px;"
                                      "  border:none; border-bottom:1px solid %3; }"
                                      "QLabel#marketsListFooter { color:%7; font-family:%5; font-size:" + fincept::ui::fonts::px(-4) + "px;"
                                      "  background:transparent; }")
                           .arg(BG_BASE(),       // %1
                                BG_SURFACE(),    // %2 (panel chrome bg lives outside; this matches table header)
                                BORDER_DIM(),    // %3
                                AMBER(),         // %4
                                font,            // %5
                                SUCCESS(),       // %6
                                TEXT_TERTIARY(), // %7
                                DANGER(),        // %8
                                TEXT_PRIMARY()); // %9

    setStyleSheet(ss);
}

// ── Lifecycle ──────────────────────────────────────────────────────────────

void MarketsListPanel::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (subscribed_)
        return;
    auto* adapter = fincept_adapter();
    if (!adapter) {
        set_status_error(tr("FinceptInternalAdapter not registered"), QStringLiteral("adapter"));
        if (table_->rowCount() == 0)
            show_state_page(true, tr("The Fincept markets service is not available in this build."));
        return;
    }
    set_status_demo(adapter->is_demo_mode());
    connect(adapter, &fincept::services::prediction::PredictionExchangeAdapter::markets_ready, this,
            &MarketsListPanel::on_markets_ready);
    connect(adapter, &fincept::services::prediction::PredictionExchangeAdapter::error_occurred, this,
            &MarketsListPanel::on_error);
    subscribed_ = true;
    adapter->list_markets({}, {}, /*limit=*/50, /*offset=*/0);
}

void MarketsListPanel::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    if (!subscribed_)
        return;
    auto* adapter = fincept_adapter();
    if (adapter) {
        disconnect(adapter, nullptr, this, nullptr);
    }
    subscribed_ = false;
}

void MarketsListPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void MarketsListPanel::retranslateUi() {
    if (title_)
        title_->setText(tr("MARKETS"));
    if (table_) {
        table_->setHorizontalHeaderLabels({tr("MARKET"), tr("YES"), tr("NO"), tr("24h VOL"), tr("EXPIRES")});
    }
    // The footer is state-dependent (demo note / empty / order-entry hint) and is
    // only the fixed demo text while the pill reads DEMO.
    if (footer_note_ && status_pill_ && status_pill_->objectName() == QLatin1String("marketsListStatusDemo"))
        footer_note_->setText(tr("SAMPLE MARKETS — illustrative questions and prices, not live and not tradable. "
                                 "Fincept prediction markets are not launched yet."));
    // Re-apply the status pill text using its objectName as the state flag so
    // the live DEMO/LIVE/ERROR state survives the locale switch.
    if (status_pill_) {
        const QString on = status_pill_->objectName();
        if (on == QLatin1String("marketsListStatusError"))
            status_pill_->setText(tr("ERROR"));
        else if (on == QLatin1String("marketsListStatusLive"))
            status_pill_->setText(tr("● LIVE"));
        else
            status_pill_->setText(tr("DEMO"));
    }
}

// ── Adapter callbacks ──────────────────────────────────────────────────────

void MarketsListPanel::on_markets_ready(const QVector<pr::PredictionMarket>& markets) {
    // A successful list clears a previous ERROR pill. The pill used to stay red
    // after a retry succeeded (set_status_demo was only called once, on show),
    // and the footer kept announcing "Demo dataset" even against a live endpoint.
    if (auto* adapter = fincept_adapter())
        set_status_demo(adapter->is_demo_mode());
    status_pill_->setToolTip(QString());
    shown_markets_ = markets;
    rebuild_table(markets);
    if (markets.isEmpty()) {
        footer_note_->setText(tr("No markets are listed yet."));
        footer_note_->show();
        show_state_page(false, tr("Prediction markets that settle in $FNCPT appear here once they are listed."));
    } else {
        hide_state_page();
    }
}

void MarketsListPanel::on_error(const QString& context, const QString& message) {
    set_status_error(message, context);
    // Rows already on screen stay (the pill says ERROR, its tooltip why); with none,
    // an error page with RETRY instead of a bare header.
    if (table_->rowCount() == 0)
        show_state_page(true, message);
}

void MarketsListPanel::request_markets() {
    auto* adapter = fincept_adapter();
    if (!adapter || !subscribed_)
        return; // not shown / not available: showEvent lists them
    adapter->list_markets({}, {}, /*limit=*/50, /*offset=*/0);
}

void MarketsListPanel::show_state_page(bool error, const QString& message) {
    if (!state_page_) {
        state_page_ = new ui::EmptyState(this);
        state_page_->set_compact(true);
        if (auto* root = qobject_cast<QVBoxLayout*>(layout()))
            root->insertWidget(root->indexOf(table_) + 1, state_page_, 1);
        connect(state_page_, &ui::EmptyState::primary_clicked, this, &MarketsListPanel::request_markets);
    }
    state_page_->apply(error ? ui::EmptyState::error_spec(message, tr("Markets unavailable"))
                             : ui::EmptyState::no_data_spec(tr("No markets listed yet"), message));
    table_->hide();
    state_page_->show();
}

void MarketsListPanel::hide_state_page() {
    if (state_page_)
        state_page_->hide();
    table_->show();
}

void MarketsListPanel::on_row_double_clicked(int row, int /*column*/) {
    auto* item = table_->item(row, 0);
    if (!item)
        return;
    const auto market_id = item->data(Qt::UserRole).toString();
    if (market_id.isEmpty())
        return;
    LOG_DEBUG("MarketsListPanel", QString("market double-clicked: %1").arg(market_id));
    // The order-entry panel is part of Phase 4 polish and has no UI yet. Say so
    // on screen — a double-click that silently does nothing reads as a broken
    // row. (The log line above keeps the seam testable.)
    footer_note_->setText(tr("Trading on \"%1\" opens once the fincept_market program is deployed — "
                             "order entry is not available yet.")
                              .arg(item->text()));
    footer_note_->show();
}

// ── Rendering ──────────────────────────────────────────────────────────────

void MarketsListPanel::set_status_demo(bool demo) {
    if (demo) {
        status_pill_->setText(tr("DEMO"));
        status_pill_->setObjectName(QStringLiteral("marketsListStatusDemo"));
        footer_note_->setText(tr("SAMPLE MARKETS — illustrative questions and prices, not live and not tradable. "
                                 "Fincept prediction markets are not launched yet."));
        footer_note_->show();
    } else {
        status_pill_->setText(tr("● LIVE"));
        status_pill_->setObjectName(QStringLiteral("marketsListStatusLive"));
        footer_note_->hide();
    }
    status_pill_->style()->unpolish(status_pill_);
    status_pill_->style()->polish(status_pill_);
}

void MarketsListPanel::set_status_error(const QString& message, const QString& operation) {
    status_pill_->setText(tr("ERROR"));
    status_pill_->setObjectName(QStringLiteral("marketsListStatusError"));
    status_pill_->setToolTip(message);
    status_pill_->style()->unpolish(status_pill_);
    status_pill_->style()->polish(status_pill_);
    LOG_WARN("MarketsListPanel",
             logf("Fincept markets request failed",
                  {{"operation", operation.isEmpty() ? QStringLiteral("list_markets") : operation}, {"error", message}}));
}

void MarketsListPanel::rebuild_table(const QVector<pr::PredictionMarket>& markets) {
    table_->setUpdatesEnabled(false);
    table_->setRowCount(markets.size());
    const QDate today = QDate::currentDate();
    int row = 0;
    for (const auto& m : markets) {
        // The head pill alone is easy to miss next to a row of YES/NO prices:
        // a sample market is labelled on the row itself, so a fabricated
        // "Fed cuts … 0.62" can never be read as a live quote.
        const bool is_demo = m.extras.value(QStringLiteral("is_demo")).toBool();
        auto* q = new QTableWidgetItem(is_demo ? tr("SAMPLE · %1").arg(m.question) : m.question);
        q->setData(Qt::UserRole, m.key.market_id);
        table_->setItem(row, 0, q);

        const double yes = m.outcomes.size() >= 1 ? m.outcomes[0].price : 0.0;
        const double no = m.outcomes.size() >= 2 ? m.outcomes[1].price : (1.0 - yes);

        auto* yes_item = new QTableWidgetItem(mlp_format_price(yes));
        auto* no_item = new QTableWidgetItem(mlp_format_price(no));
        auto* vol_item = new QTableWidgetItem(format_volume(m.volume));
        yes_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        no_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        vol_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table_->setItem(row, 1, yes_item);
        table_->setItem(row, 2, no_item);
        table_->setItem(row, 3, vol_item);

        // An expiry already in the past next to a quoted price is a stale
        // market, whatever the source says about "active" — flag it.
        const QDate end = QDate::fromString(m.end_date_iso.left(10), Qt::ISODate);
        const bool expired = end.isValid() && end < today;
        table_->setItem(row, 4, new QTableWidgetItem(expired ? tr("%1 · expired").arg(m.end_date_iso)
                                                             : m.end_date_iso));
        if (is_demo) {
            for (int c = 0; c < table_->columnCount(); ++c) {
                if (auto* cell = table_->item(row, c))
                    cell->setToolTip(tr("Sample market — illustrative price and volume, not live data and "
                                        "not tradable."));
            }
        }
        ++row;
    }
    // Filled: fit YES / NO / VOL / EXPIRES once (MARKET stretches).
    if (!markets.isEmpty()) {
        for (int c = 1; c <= 4; ++c)
            table_->resizeColumnToContents(c);
    }
    table_->setUpdatesEnabled(true);
}

} // namespace fincept::screens::panels
