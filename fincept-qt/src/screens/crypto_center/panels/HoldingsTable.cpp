#include "screens/crypto_center/panels/HoldingsTable.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "services/wallet/TokenMetadataService.h"
#include "services/wallet/WalletService.h"
#include "services/wallet/WalletTypes.h"
#include "storage/secure/SecureStorage.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QHideEvent>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QShowEvent>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace fincept::screens::panels {

namespace {

QString font_stack() {
    return QStringLiteral(
        "'Consolas','Cascadia Mono','JetBrains Mono','SF Mono',monospace");
}

QString format_amount(double v, int max_dp = 4) {
    if (v == 0.0) return QStringLiteral("0");
    if (v < 0.0001) return QLocale::system().toString(v, 'g', 4);
    return QLocale::system().toString(v, 'f', max_dp);
}

QString format_usd(double v) {
    if (v < 0.0) return QStringLiteral("—");
    if (v >= 1.0 || v == 0.0) {
        return QStringLiteral("$%1").arg(QLocale::system().toString(v, 'f', 2));
    }
    return QStringLiteral("$%1").arg(QLocale::system().toString(v, 'f', 6));
}

QString format_price(double v) {
    if (v <= 0.0) return QStringLiteral("—");
    if (v >= 1.0) {
        return QStringLiteral("$%1").arg(QLocale::system().toString(v, 'f', 4));
    }
    return QStringLiteral("$%1").arg(QLocale::system().toString(v, 'f', 8));
}

QString format_pct(double v) {
    return QStringLiteral("%1%").arg(QLocale::system().toString(v, 'f', 1));
}

} // namespace

HoldingsTable::HoldingsTable(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("holdingsTable"));

    // Read persisted "show unverified" flag.
    auto r = SecureStorage::instance().retrieve(
        QStringLiteral("wallet.show_unverified_tokens"));
    if (r.is_ok()) {
        show_unverified_ =
            r.value().compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0;
    }

    build_ui();
    apply_theme();

    auto& svc = fincept::wallet::WalletService::instance();
    connect(&svc, &fincept::wallet::WalletService::wallet_connected, this,
            &HoldingsTable::on_wallet_connected);
    connect(&svc, &fincept::wallet::WalletService::wallet_disconnected, this,
            &HoldingsTable::on_wallet_disconnected);

    connect(&fincept::wallet::TokenMetadataService::instance(),
            &fincept::wallet::TokenMetadataService::metadata_changed, this,
            &HoldingsTable::on_metadata_changed);

    if (svc.is_connected()) {
        on_wallet_connected(svc.current_pubkey(), svc.state().label);
    }
}

HoldingsTable::~HoldingsTable() = default;

void HoldingsTable::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    table_ = new QTableWidget(this);
    table_->setObjectName(QStringLiteral("holdingsTableView"));
    table_->setColumnCount(5);
    table_->setHorizontalHeaderLabels(
        {QStringLiteral("TOKEN"), QStringLiteral("BALANCE"),
         QStringLiteral("PRICE"), QStringLiteral("USD VALUE"),
         QStringLiteral("% OF PORT")});
    table_->verticalHeader()->setVisible(false);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setShowGrid(false);
    table_->setAlternatingRowColors(false);
    table_->setSortingEnabled(false); // we sort manually at refresh
    root->addWidget(table_, 1);

    auto* footer_row = new QWidget(this);
    footer_row->setObjectName(QStringLiteral("holdingsTableFooter"));
    footer_row->setFixedHeight(30);
    auto* fl = new QHBoxLayout(footer_row);
    fl->setContentsMargins(12, 0, 12, 0);
    fl->setSpacing(8);
    footer_ = new QLabel(QStringLiteral("—"), footer_row);
    footer_->setObjectName(QStringLiteral("holdingsTableFooterText"));
    show_all_button_ = new QPushButton(
        show_unverified_ ? tr("Hide unverified") : tr("Show all"), footer_row);
    show_all_button_->setObjectName(QStringLiteral("holdingsTableShowAll"));
    show_all_button_->setFixedHeight(22);
    show_all_button_->setCursor(Qt::PointingHandCursor);
    fl->addWidget(footer_);
    fl->addStretch(1);
    fl->addWidget(show_all_button_);
    root->addWidget(footer_row);

    connect(table_, &QTableWidget::cellClicked, this, [this](int row, int) {
        auto* it = table_->item(row, 0);
        if (!it) return;
        const auto mint = it->data(Qt::UserRole).toString();
        if (!mint.isEmpty()) emit select_token(mint);
    });
    connect(show_all_button_, &QPushButton::clicked, this,
            &HoldingsTable::on_show_all_clicked);
}

void HoldingsTable::apply_theme() {
    using namespace ui::colors;
    const QString font = font_stack();
    const QString ss = QStringLiteral(
        "QWidget#holdingsTable { background:%1; }"
        "QTableWidget#holdingsTableView { background:%1; color:%2; gridline-color:%3;"
        "  border:none; selection-background-color:%4; selection-color:%2;"
        "  font-family:%5; font-size:11px; }"
        "QTableWidget#holdingsTableView::item { padding:6px 10px; border-bottom:1px solid %3; }"
        "QHeaderView::section { background:%6; color:%7; padding:6px 10px;"
        "  border:none; border-bottom:1px solid %3; font-family:%5;"
        "  font-size:9px; font-weight:700; letter-spacing:1.4px; }"
        "QWidget#holdingsTableFooter { background:%6; border-top:1px solid %3; }"
        "QLabel#holdingsTableFooterText { color:%7; font-family:%5; font-size:10px;"
        "  background:transparent; }"
        "QPushButton#holdingsTableShowAll { background:transparent; color:%8;"
        "  border:1px solid %3; font-family:%5; font-size:10px; font-weight:700;"
        "  letter-spacing:1.2px; padding:0 10px; }"
        "QPushButton#holdingsTableShowAll:hover { color:%2; border-color:%9; }"
    )
        .arg(BG_BASE(),         // %1
             TEXT_PRIMARY(),    // %2
             BORDER_DIM(),      // %3
             BG_HOVER(),        // %4
             font,              // %5
             BG_RAISED(),       // %6
             TEXT_TERTIARY(),   // %7
             AMBER(),           // %8
             BORDER_BRIGHT());  // %9
    setStyleSheet(ss);
}

void HoldingsTable::on_wallet_connected(const QString& pubkey, const QString& /*label*/) {
    current_pubkey_ = pubkey;
    if (isVisible()) {
        refresh_subscription();
    }
}

void HoldingsTable::on_wallet_disconnected() {
    auto& hub = fincept::datahub::DataHub::instance();
    hub.unsubscribe(this);
    current_balance_topic_.clear();
    price_topic_.clear();
    price_usd_.clear();
    current_pubkey_.clear();
    latest_balance_ = {};
    rebuild_table();
}

void HoldingsTable::refresh_subscription() {
    auto& hub = fincept::datahub::DataHub::instance();
    if (!current_balance_topic_.isEmpty()) {
        hub.unsubscribe(this, current_balance_topic_);
        current_balance_topic_.clear();
    }
    if (current_pubkey_.isEmpty()) return;
    current_balance_topic_ = QStringLiteral("wallet:balance:%1").arg(current_pubkey_);
    hub.subscribe(this, current_balance_topic_,
                  [this](const QVariant& v) { on_balance_update(v); });
    hub.request(current_balance_topic_, /*force=*/true);
}

void HoldingsTable::on_balance_update(const QVariant& v) {
    if (!v.canConvert<fincept::wallet::WalletBalance>()) return;
    latest_balance_ = v.value<fincept::wallet::WalletBalance>();
    resubscribe_prices();
    rebuild_table();
}

void HoldingsTable::resubscribe_prices() {
    auto& hub = fincept::datahub::DataHub::instance();

    // Build the desired set of mints (SOL + every held token).
    QSet<QString> wanted;
    wanted.insert(QString::fromLatin1(fincept::wallet::kWrappedSolMint));
    for (const auto& t : latest_balance_.tokens) {
        if (!t.mint.isEmpty()) wanted.insert(t.mint);
    }

    // Unsubscribe from mints we no longer hold.
    for (auto it = price_topic_.begin(); it != price_topic_.end();) {
        if (!wanted.contains(it.key())) {
            hub.unsubscribe(this, it.value());
            price_usd_.remove(it.key());
            it = price_topic_.erase(it);
        } else {
            ++it;
        }
    }

    // Subscribe to anything new.
    for (const auto& mint : wanted) {
        if (price_topic_.contains(mint)) continue;
        const auto topic = QStringLiteral("market:price:token:%1").arg(mint);
        price_topic_.insert(mint, topic);
        hub.subscribe(this, topic, [this, mint](const QVariant& v) {
            on_price_update(mint, v);
        });
        hub.request(topic, /*force=*/true);
    }
}

void HoldingsTable::on_price_update(const QString& mint, const QVariant& v) {
    if (!v.canConvert<fincept::wallet::TokenPrice>()) return;
    const auto p = v.value<fincept::wallet::TokenPrice>();
    if (!p.valid) return;
    price_usd_[mint] = p.usd;
    rebuild_table();
}

void HoldingsTable::on_metadata_changed() {
    // Symbols/icons may have flipped from truncated-mint to canonical;
    // rebuild so the cells show the right labels.
    rebuild_table();
}

void HoldingsTable::on_show_all_clicked() {
    show_unverified_ = !show_unverified_;
    SecureStorage::instance().store(
        QStringLiteral("wallet.show_unverified_tokens"),
        show_unverified_ ? QStringLiteral("true") : QStringLiteral("false"));
    show_all_button_->setText(show_unverified_ ? tr("Hide unverified")
                                               : tr("Show all"));
    rebuild_table();
}

void HoldingsTable::rebuild_table() {
    // Build the unified rows list: one synthetic SOL row + each TokenHolding,
    // optionally filtered by verified + sorted by USD value desc.
    struct Row {
        QString mint;
        QString symbol;
        double  ui_amount;
        double  usd_price;
        double  usd_value;
        bool    verified;
    };
    QVector<Row> rows;
    rows.reserve(latest_balance_.tokens.size() + 1);

    if (latest_balance_.sol_lamports > 0) {
        Row r;
        r.mint = QString::fromLatin1(fincept::wallet::kWrappedSolMint);
        r.symbol = QStringLiteral("SOL");
        r.ui_amount = latest_balance_.sol();
        r.usd_price = price_usd_.value(r.mint, -1.0);
        r.usd_value = (r.usd_price >= 0.0) ? r.ui_amount * r.usd_price : -1.0;
        r.verified = true;
        rows.append(std::move(r));
    }
    for (const auto& t : latest_balance_.tokens) {
        if (!show_unverified_ && !t.verified) continue;
        Row r;
        r.mint = t.mint;
        r.symbol = t.symbol.isEmpty() ? t.mint.left(10) : t.symbol;
        r.ui_amount = t.ui_amount();
        r.usd_price = price_usd_.value(t.mint, -1.0);
        r.usd_value = (r.usd_price >= 0.0) ? r.ui_amount * r.usd_price : -1.0;
        r.verified = t.verified;
        rows.append(std::move(r));
    }

    // Sort: known USD value desc; unknown to bottom alphabetically by symbol.
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
        if ((a.usd_value < 0.0) != (b.usd_value < 0.0)) return a.usd_value >= 0.0;
        if (a.usd_value != b.usd_value) return a.usd_value > b.usd_value;
        return a.symbol < b.symbol;
    });

    // Total = sum of priced rows only.
    double total = 0.0;
    int unpriced = 0;
    for (const auto& r : rows) {
        if (r.usd_value >= 0.0) total += r.usd_value;
        else ++unpriced;
    }

    table_->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const auto& r = rows[i];
        // Column 0: TOKEN — symbol + verified-or-not hint
        auto* sym = new QTableWidgetItem(r.symbol);
        sym->setData(Qt::UserRole, r.mint);
        if (!r.verified) {
            sym->setToolTip(tr("Unverified mint: %1").arg(r.mint));
        }
        table_->setItem(i, 0, sym);
        // Column 1: BALANCE
        auto* bal = new QTableWidgetItem(format_amount(r.ui_amount, 4));
        bal->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table_->setItem(i, 1, bal);
        // Column 2: PRICE
        auto* prc = new QTableWidgetItem(
            r.usd_price >= 0.0 ? format_price(r.usd_price) : QStringLiteral("—"));
        prc->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table_->setItem(i, 2, prc);
        // Column 3: USD VALUE
        auto* val = new QTableWidgetItem(
            r.usd_value >= 0.0 ? format_usd(r.usd_value) : QStringLiteral("—"));
        val->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table_->setItem(i, 3, val);
        // Column 4: % OF PORT
        const double pct = (total > 0.0 && r.usd_value >= 0.0)
                               ? (100.0 * r.usd_value / total)
                               : 0.0;
        auto* pctc = new QTableWidgetItem(
            r.usd_value >= 0.0 ? format_pct(pct) : QStringLiteral("—"));
        pctc->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table_->setItem(i, 4, pctc);
    }

    // Footer summary: total + counts.
    int verified = 0, unverified_total = 0;
    for (const auto& t : latest_balance_.tokens) {
        if (t.verified) ++verified;
        else ++unverified_total;
    }
    if (latest_balance_.sol_lamports > 0) ++verified; // SOL is always verified
    QString footer_text = tr("TOTAL %1  ·  %2 verified")
                              .arg(format_usd(total))
                              .arg(verified);
    if (unverified_total > 0) {
        footer_text += tr("  ·  %1 unverified%2")
                           .arg(unverified_total)
                           .arg(show_unverified_ ? QString() : tr(" hidden"));
    }
    if (unpriced > 0) {
        footer_text += tr("  ·  %1 without price").arg(unpriced);
    }
    footer_->setText(footer_text);
}

void HoldingsTable::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (!current_pubkey_.isEmpty()) {
        refresh_subscription();
    }
}

void HoldingsTable::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    fincept::datahub::DataHub::instance().unsubscribe(this);
    current_balance_topic_.clear();
    price_topic_.clear();
}

} // namespace fincept::screens::panels
