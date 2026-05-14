// src/screens/equity_trading/EquityTradingScreen_Holdings.cpp
//
// Broker→portfolio holdings import flow — yfinance symbol normalisation +
// the on_import_holdings_requested batch importer.
//
// Part of the partial-class split of EquityTradingScreen.cpp.

#include "screens/equity_trading/EquityTradingScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "core/symbol/SymbolContext.h"
#include "core/symbol/SymbolDragSource.h"
#include "screens/equity_trading/AccountManagementDialog.h"
#include "screens/equity_trading/BroadcastOrderDialog.h"
#include "screens/equity_trading/EquityBottomPanel.h"
#include "screens/equity_trading/EquityChart.h"
#include "screens/equity_trading/EquityOrderBook.h"
#include "screens/equity_trading/EquityOrderEntry.h"
#include "screens/equity_trading/EquityTickerBar.h"
#include "screens/equity_trading/EquityWatchlist.h"
#include "services/portfolio/PortfolioService.h"
#include "storage/repositories/SettingsRepository.h"
#include "trading/AccountManager.h"
#include "trading/BrokerRegistry.h"
#include "trading/DataStreamManager.h"
#include "trading/OrderMatcher.h"
#include "trading/PaperTrading.h"
#include "ui/theme/StyleSheets.h"
#include "ui/theme/Theme.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDate>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollBar>
#include <QSplitter>
#include <QStringListModel>
#include <QTableWidget>
#include <QVBoxLayout>

#include <memory>

#include <QtConcurrent/QtConcurrent>

namespace fincept::screens {

using namespace fincept::trading;
using namespace fincept::screens::equity;

static QString yfinance_symbol_for(const QString& symbol, const QString& exchange) {
    static const QHash<QString, QString> suffix_map = {
        {"NSE", ".NS"},        {"BSE", ".BO"},  {"HKEX", ".HK"}, {"TSE", ".T"},
        {"KRX", ".KS"},        {"SGX", ".SI"},  {"ASX", ".AX"},  {"IDX", ".JK"},
        {"MYX", ".KL"},        {"SET", ".BK"},  {"PSE", ".PS"},  {"XETR", ".DE"},
        {"FWB", ".F"},         {"LSE", ".L"},   {"BME", ".MC"},  {"MIL", ".MI"},
        {"SIX", ".SW"},        {"TSX", ".TO"},  {"TSXV", ".V"},  {"BMFBOVESPA", ".SA"},
        {"BIST", ".IS"},       {"EGX", ".CA"},
    };
    const QString sym = symbol.trimmed().toUpper();
    const auto it = suffix_map.find(exchange.trimmed().toUpper());
    if (it == suffix_map.end() || sym.isEmpty() || sym.contains('.'))
        return sym;
    return sym + it.value();
}

void EquityTradingScreen::on_import_holdings_requested(const QVector<trading::BrokerHolding>& holdings) {
    if (holdings.isEmpty())
        return;
    if (focused_account_id_.isEmpty()) {
        QMessageBox::warning(this, "Import Holdings", "No account selected.");
        return;
    }
    const auto account = trading::AccountManager::instance().get_account(focused_account_id_);
    const QString broker_id = account.broker_id;
    auto* broker = trading::BrokerRegistry::instance().get(broker_id);
    const QString default_currency = (broker ? broker->profile().currency : QStringLiteral("USD"));
    const QString suggested_name = account.display_name.isEmpty()
                                       ? QString("%1 Holdings").arg(broker_id.toUpper())
                                       : QString("%1 - Holdings").arg(account.display_name);

    QDialog dlg(this);
    dlg.setWindowTitle("Import holdings into portfolio");
    dlg.setMinimumSize(780, 560);

    auto* v = new QVBoxLayout(&dlg);
    v->setContentsMargins(16, 16, 16, 16);
    v->setSpacing(10);

    auto* info = new QLabel(QString("Importing <b>%1</b> holdings from <b>%2</b>. "
                                    "Tickers are auto-mapped to Yahoo Finance format "
                                    "(NSE→.NS, BSE→.BO). Edit the <i>Yahoo Ticker</i> column "
                                    "if any symbol needs a manual override.")
                                .arg(holdings.size())
                                .arg(account.display_name.isEmpty() ? broker_id.toUpper()
                                                                    : account.display_name));
    info->setTextFormat(Qt::RichText);
    info->setWordWrap(true);
    v->addWidget(info);

    // ── Portfolio target selection ────────────────────────────────────────
    auto* mode_new = new QRadioButton("Create a new portfolio");
    auto* mode_existing = new QRadioButton("Add to existing portfolio");
    mode_new->setChecked(true);

    auto* mode_row = new QHBoxLayout;
    mode_row->addWidget(mode_new);
    mode_row->addWidget(mode_existing);
    mode_row->addStretch();
    v->addLayout(mode_row);

    auto* new_row = new QWidget;
    auto* new_form = new QFormLayout(new_row);
    new_form->setContentsMargins(18, 0, 0, 0);
    auto* name_input = new QLineEdit(suggested_name);
    auto* currency_input = new QLineEdit(default_currency);
    new_form->addRow("Name:", name_input);
    new_form->addRow("Currency:", currency_input);
    v->addWidget(new_row);

    auto* existing_combo = new QComboBox;
    existing_combo->setEnabled(false);
    existing_combo->addItem("Loading portfolios...");
    auto* existing_row = new QWidget;
    auto* existing_form = new QFormLayout(existing_row);
    existing_form->setContentsMargins(18, 0, 0, 0);
    existing_form->addRow("Portfolio:", existing_combo);
    existing_row->setVisible(false);
    v->addWidget(existing_row);

    // ── Per-row holdings table with editable yfinance ticker ─────────────
    auto* table = new QTableWidget;
    table->setColumnCount(6);
    table->setHorizontalHeaderLabels(
        {"Import", "Broker Symbol", "Exchange", "Yahoo Ticker (edit)", "Quantity", "Avg Price"});
    table->setRowCount(holdings.size());
    table->verticalHeader()->setVisible(false);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setEditTriggers(QAbstractItemView::AllEditTriggers);
    table->setAlternatingRowColors(true);
    table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    table->verticalScrollBar()->setSingleStep(8);
    {
        auto* hh = table->horizontalHeader();
        hh->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        hh->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        hh->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        hh->setSectionResizeMode(3, QHeaderView::Stretch);
        hh->setSectionResizeMode(4, QHeaderView::ResizeToContents);
        hh->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    }

    auto make_readonly = [](const QString& text, bool right_align = false) {
        auto* it = new QTableWidgetItem(text);
        it->setFlags(it->flags() & ~Qt::ItemIsEditable);
        if (right_align)
            it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        return it;
    };

    for (int r = 0; r < holdings.size(); ++r) {
        const auto& h = holdings[r];

        // Col 0: import checkbox (centered via a container widget)
        auto* cb_cell = new QWidget;
        auto* cb_lay = new QHBoxLayout(cb_cell);
        cb_lay->setContentsMargins(0, 0, 0, 0);
        auto* cb = new QCheckBox;
        cb->setChecked(h.quantity > 0 && !h.symbol.isEmpty());
        cb->setProperty("row_index", r);
        cb_lay->addWidget(cb, 0, Qt::AlignCenter);
        table->setCellWidget(r, 0, cb_cell);

        table->setItem(r, 1, make_readonly(h.symbol));
        table->setItem(r, 2, make_readonly(h.exchange));

        auto* yf_item = new QTableWidgetItem(yfinance_symbol_for(h.symbol, h.exchange));
        yf_item->setFlags(yf_item->flags() | Qt::ItemIsEditable);
        table->setItem(r, 3, yf_item);

        table->setItem(r, 4, make_readonly(QString::number(h.quantity, 'f', 2), true));
        table->setItem(r, 5, make_readonly(QString::number(h.avg_price, 'f', 2), true));
    }
    v->addWidget(table, 1);

    // Select/Deselect all helpers
    auto* select_row = new QHBoxLayout;
    auto* select_all_btn = new QPushButton("Select all");
    auto* deselect_all_btn = new QPushButton("Deselect all");
    select_row->addWidget(select_all_btn);
    select_row->addWidget(deselect_all_btn);
    select_row->addStretch();
    v->addLayout(select_row);

    auto set_all_checked = [table](bool on) {
        for (int r = 0; r < table->rowCount(); ++r) {
            if (auto* w = table->cellWidget(r, 0)) {
                if (auto* cb = w->findChild<QCheckBox*>())
                    cb->setChecked(on);
            }
        }
    };
    QObject::connect(select_all_btn, &QPushButton::clicked, &dlg, [set_all_checked]() { set_all_checked(true); });
    QObject::connect(deselect_all_btn, &QPushButton::clicked, &dlg, [set_all_checked]() { set_all_checked(false); });

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText("IMPORT");
    v->addWidget(buttons);

    auto sync_mode = [&]() {
        const bool is_new = mode_new->isChecked();
        new_row->setVisible(is_new);
        existing_row->setVisible(!is_new);
    };
    QObject::connect(mode_new, &QRadioButton::toggled, &dlg, sync_mode);
    QObject::connect(mode_existing, &QRadioButton::toggled, &dlg, sync_mode);
    sync_mode();

    // Load existing portfolios asynchronously.
    auto& ps = fincept::services::PortfolioService::instance();
    QPointer<QComboBox> combo_guard = existing_combo;
    QPointer<QRadioButton> existing_guard = mode_existing;
    auto conn = QObject::connect(
        &ps, &fincept::services::PortfolioService::portfolios_loaded, &dlg,
        [combo_guard, existing_guard](const QVector<fincept::portfolio::Portfolio>& list) {
            if (!combo_guard) return;
            combo_guard->clear();
            if (list.isEmpty()) {
                combo_guard->addItem("(no portfolios yet)");
                combo_guard->setEnabled(false);
                if (existing_guard) existing_guard->setEnabled(false);
            } else {
                for (const auto& p : list)
                    combo_guard->addItem(QString("%1 (%2)").arg(p.name, p.currency), p.id);
                combo_guard->setEnabled(true);
            }
        });
    ps.load_portfolios();

    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) {
        QObject::disconnect(conn);
        return;
    }
    QObject::disconnect(conn);

    // Collect selected rows with their (possibly edited) yfinance tickers.
    // We carry the broker-native pair (symbol + exchange) alongside the
    // yfinance form so PortfolioService can route live quote calls through
    // the broker instead of yfinance for portfolios linked to a connected
    // account. See migration v022.
    struct ImportRow {
        QString symbol;          // yfinance-format ("RELIANCE.NS"). Canonical key.
        QString broker_symbol;   // broker-native ("RELIANCE")
        QString exchange;        // "NSE" / "BSE" / etc.
        double quantity = 0;
        double avg_price = 0;
    };
    QVector<ImportRow> rows;
    rows.reserve(holdings.size());
    for (int r = 0; r < holdings.size(); ++r) {
        auto* w = table->cellWidget(r, 0);
        auto* cb = w ? w->findChild<QCheckBox*>() : nullptr;
        if (!cb || !cb->isChecked())
            continue;
        const QString yf = table->item(r, 3)->text().trimmed().toUpper();
        if (yf.isEmpty())
            continue;
        rows.push_back({yf,
                        holdings[r].symbol,    // broker-native ticker as returned by broker
                        holdings[r].exchange,
                        holdings[r].quantity,
                        holdings[r].avg_price});
    }
    if (rows.isEmpty()) {
        QMessageBox::information(this, "Import Holdings", "Nothing selected to import.");
        return;
    }

    auto do_add_assets = [rows](const QString& portfolio_id) {
        auto& svc = fincept::services::PortfolioService::instance();
        const QString today = QDate::currentDate().toString(Qt::ISODate);
        for (const auto& row : rows) {
            if (row.quantity <= 0 || row.symbol.isEmpty())
                continue;
            svc.add_asset(portfolio_id, row.symbol, row.quantity, row.avg_price, today,
                          row.broker_symbol, row.exchange);
        }
    };

    if (mode_new->isChecked()) {
        const QString name = name_input->text().trimmed();
        if (name.isEmpty()) {
            QMessageBox::warning(this, "Import Holdings", "Portfolio name is required.");
            return;
        }
        const QString currency = currency_input->text().trimmed().isEmpty()
                                     ? default_currency
                                     : currency_input->text().trimmed().toUpper();
        QPointer<EquityTradingScreen> self = this;
        auto once = std::make_shared<QMetaObject::Connection>();
        const int count = rows.size();
        *once = connect(&ps, &fincept::services::PortfolioService::portfolio_created, this,
                        [self, once, do_add_assets, name, count](const fincept::portfolio::Portfolio& p) {
                            if (!self || p.name != name)
                                return;
                            QObject::disconnect(*once);
                            do_add_assets(p.id);
                            QMessageBox::information(self, "Import Holdings",
                                                     QString("Imported %1 holdings into portfolio \"%2\".")
                                                         .arg(count)
                                                         .arg(p.name));
                        });
        // Stamp the new portfolio with the broker account_id so live quotes
        // route through the broker (PortfolioService::build_summary).
        ps.create_portfolio(name, account.display_name, currency,
                            "Imported from " + broker_id.toUpper(),
                            account.account_id);
    } else {
        const QString pid = existing_combo->currentData().toString();
        if (pid.isEmpty()) {
            QMessageBox::warning(this, "Import Holdings", "Select a portfolio first.");
            return;
        }
        do_add_assets(pid);
        QMessageBox::information(this, "Import Holdings",
                                 QString("Imported %1 holdings.").arg(rows.size()));
    }
}

// ── IGroupLinked ─────────────────────────────────────────────────────────────

} // namespace fincept::screens
