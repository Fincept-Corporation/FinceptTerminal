#include "screens/polymarket/PolymarketLeaderboard.h"

#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/NumericTableWidgetItem.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QVBoxLayout>

namespace pm_fmt = fincept::ui::formatting;

namespace fincept::screens::polymarket {

using namespace fincept::ui;
// Only the entry type: a using-directive for the whole namespace made PriceHistory (also in
// services::prediction) ambiguous in the other polymarket files of a unity batch.
using fincept::services::polymarket::LeaderboardEntry;

// The leaderboard is Polymarket's (the only venue with one): PnL and volume are
// in its settlement currency, USDC — the venue's own figures, never converted.
static const char* const kLbCurrency = "USDC";

// "USDC 1.5K" / "USDC 22.12M", abbreviated per the user's compact-number setting.
static QString lb_money_abbrev(double v) {
    return pm_fmt::money(v, QString::fromLatin1(kLbCurrency), pm_fmt::Compact::FromSettings);
}

// Signed: "+USDC 900.0K" / "-USDC 1.5K" (a loss used to read "$-1.5K").
static QString lb_pnl(double v) {
    return pm_fmt::signed_money(v, QString::fromLatin1(kLbCurrency), pm_fmt::Compact::FromSettings);
}

PolymarketLeaderboard::PolymarketLeaderboard(QWidget* parent) : QWidget(parent) {
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* header_bar = new QWidget;
    header_bar->setFixedHeight(30);
    header_bar->setStyleSheet(
        QString("background: %1; border-bottom: 1px solid %2;").arg(colors::BG_RAISED(), colors::BORDER_DIM()));
    auto* hbl = new QHBoxLayout(header_bar);
    hbl->setContentsMargins(12, 0, 8, 0);
    header_ = new QLabel(tr("LEADERBOARD"));
    header_->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-5) + "px; font-weight: 700; letter-spacing: 0.8px; "
                                   "background: transparent;")
                               .arg(colors::TEXT_SECONDARY()));
    hbl->addWidget(header_);
    hbl->addStretch(1);
    vl->addWidget(header_bar);

    table_ = new QTableWidget;
    // No TRADES column: the Data API leaderboard has no trade count, so it was a
    // permanent column of dashes.
    table_->setColumnCount(4);
    table_->setHorizontalHeaderLabels({tr("#"), tr("TRADER"), tr("PNL"), tr("VOLUME")});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setShowGrid(false);
    table_->setStyleSheet(QString("QTableWidget { background: %1; color: %2; border: none; font-size: " + fincept::ui::fonts::px(-4) + "px; }"
                                  "QTableWidget::item { padding: 3px 8px; border-bottom: 1px solid %3; }"
                                  "QTableWidget::item:selected { background: %4; color: %2; }"
                                  "QHeaderView::section { background: %5; color: %6; border: none;"
                                  "  border-bottom: 1px solid %3; padding: 5px 8px;"
                                  "  font-size: " + fincept::ui::fonts::px(-6) + "px; font-weight: 700; letter-spacing: 0.5px; }"
                                  "QScrollBar:vertical { background: %1; width: 4px; border: none; }"
                                  "QScrollBar::handle:vertical { background: %3; min-height: 20px; }"
                                  "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
                              .arg(colors::BG_BASE(), colors::TEXT_PRIMARY(), colors::BORDER_DIM(), colors::BG_HOVER(),
                                   colors::BG_RAISED(), colors::TEXT_SECONDARY()));
    table_->setAccessibleName(tr("Trader leaderboard"));
    table_->setToolTip(tr("Double-click a trader to open their Polymarket profile"));
    // trader_clicked was declared but never emitted. The wallet address rides on the
    // TRADER cell (Qt::UserRole) so it follows the row through column sorting.
    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        if (auto* it = table_->item(row, 1)) {
            const QString address = it->data(Qt::UserRole).toString();
            if (!address.isEmpty())
                emit trader_clicked(address);
        }
    });
    vl->addWidget(table_, 1);

    // The panel is shown whenever the adapter advertises has_leaderboard, but
    // nothing populates it yet — render an explicit empty state instead of a
    // blank grid that reads as a broken widget.
    show_empty_state(tr("Leaderboard data not available"));

    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { refresh_theme(); });
    // A number-format change re-renders the PNL / VOLUME cells from the values they carry (the
    // rows stay; the sort keys are the same numbers).
    pm_fmt::on_format_changed(this, [this]() {
        if (!table_ || table_->columnSpan(0, 0) > 1) // empty-state row
            return;
        for (int r = 0; r < table_->rowCount(); ++r) {
            if (auto* pnl_item = dynamic_cast<ui::NumericTableWidgetItem*>(table_->item(r, 2)))
                pnl_item->setText(lb_pnl(pnl_item->numeric_value()));
            if (auto* vol_item = dynamic_cast<ui::NumericTableWidgetItem*>(table_->item(r, 3)))
                vol_item->setText(lb_money_abbrev(qAbs(vol_item->numeric_value())));
        }
    });
}

void PolymarketLeaderboard::refresh_theme() {
    if (!table_)
        return;
    if (table_->columnSpan(0, 0) > 1) { // empty-state row
        if (auto* it = table_->item(0, 0))
            it->setForeground(QColor(colors::TEXT_DIM()));
        return;
    }
    for (int r = 0; r < table_->rowCount(); ++r) {
        if (auto* pnl_item = table_->item(r, 2))
            pnl_item->setForeground(
                QColor(pnl_item->data(Qt::UserRole).toDouble() >= 0 ? colors::POSITIVE() : colors::NEGATIVE()));
    }
}

void PolymarketLeaderboard::show_empty_state(const QString& message) {
    if (!table_)
        return;
    table_->setSortingEnabled(false);
    table_->setRowCount(1);
    auto* item = new QTableWidgetItem(message);
    item->setTextAlignment(Qt::AlignCenter);
    item->setForeground(QColor(colors::TEXT_DIM()));
    item->setFlags(Qt::ItemIsEnabled);
    table_->setItem(0, 0, item);
    table_->setSpan(0, 0, 1, table_->columnCount());
}

void PolymarketLeaderboard::set_entries(const QVector<LeaderboardEntry>& entries) {
    table_->clearSpans();
    if (entries.isEmpty()) {
        show_empty_state(tr("No leaderboard entries"));
        return;
    }
    table_->setSortingEnabled(false);
    table_->setRowCount(entries.size());

    for (int i = 0; i < entries.size(); ++i) {
        const auto& e = entries[i];

        // Numeric DisplayRole so the sort below orders 2 before 10 (a QString rank sorted
        // lexicographically — and descending by default — scrambled the board).
        auto* rank_item = new QTableWidgetItem;
        rank_item->setData(Qt::DisplayRole, e.rank > 0 ? e.rank : i + 1);
        rank_item->setTextAlignment(Qt::AlignCenter);
        table_->setItem(i, 0, rank_item);

        auto* name_item = new QTableWidgetItem(e.display_name);
        name_item->setData(Qt::UserRole, e.address);
        table_->setItem(i, 1, name_item);

        // PNL / VOLUME sort by value (text sorting put "+$900.0K" above "+$22.1M").
        auto* pnl_item = new ui::NumericTableWidgetItem(lb_pnl(e.pnl), e.pnl);
        pnl_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        pnl_item->setData(Qt::UserRole, e.pnl); // sign re-read by refresh_theme()
        pnl_item->setForeground(QColor(e.pnl >= 0 ? colors::POSITIVE() : colors::NEGATIVE()));
        table_->setItem(i, 2, pnl_item);

        auto* vol_item = new ui::NumericTableWidgetItem(lb_money_abbrev(qAbs(e.volume)), e.volume);
        vol_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table_->setItem(i, 3, vol_item);
    }

    table_->resizeColumnsToContents();
    // setSortingEnabled(true) sorts by the header's current indicator, which defaults
    // to column 0 *descending* — start on rank ascending so #1 is on top.
    table_->horizontalHeader()->setSortIndicator(0, Qt::AscendingOrder);
    table_->setSortingEnabled(true);
}

void PolymarketLeaderboard::set_unavailable(const QString& reason) {
    table_->clearSpans();
    show_empty_state(reason.isEmpty() ? tr("Leaderboard unavailable") : tr("Leaderboard unavailable: %1").arg(reason));
}

void PolymarketLeaderboard::set_loading(bool loading) {
    if (loading) {
        table_->clearSpans();
        table_->setRowCount(0);
        show_empty_state(tr("Loading…"));
    }
}

void PolymarketLeaderboard::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void PolymarketLeaderboard::retranslateUi() {
    if (header_)
        header_->setText(tr("LEADERBOARD"));
    if (table_)
        table_->setHorizontalHeaderLabels({tr("#"), tr("TRADER"), tr("PNL"), tr("VOLUME")});
    // Row data refreshes via set_entries().
}

} // namespace fincept::screens::polymarket
