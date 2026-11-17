#include "screens/dashboard/widgets/MarginUsageWidget.h"

#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "trading/AccountManager.h"
#include "trading/BrokerRegistry.h"
#include "trading/BrokerTopic.h"
#include "trading/DataStreamManager.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QJsonObject>
#include <QLabel>
#include <QProgressBar>

#include <algorithm>
#include <cmath>

namespace fincept::screens::widgets {

namespace {
// An amount in the broker account's own currency (empty when the broker is not
// known: no unit). Full below 1 lakh / 1 million, abbreviated above in the user's
// scheme (Settings > Currency & Number Format; on Automatic lakh / crore only
// for rupee-family accounts — a USD broker once printed 150,000 as "1.50L").
QString mu_money(double v, const QString& ccy) {
    namespace mu_fmt = fincept::ui::formatting;
    const bool south_asian = mu_fmt::resolve_compact(mu_fmt::Compact::FromSettings, ccy) == mu_fmt::Compact::LakhCrore;
    const bool big = std::abs(v) >= (south_asian ? 1.0e5 : 1.0e6);
    return mu_fmt::money(v, ccy, big ? mu_fmt::Compact::FromSettings : mu_fmt::Compact::None);
}

// Margin usage is a risk state, not a price direction: the status colours.
QString margin_usage_color(int pct) {
    if (pct >= 80)
        return ui::colors::DANGER();
    if (pct >= 50)
        return ui::colors::WARNING();
    return ui::colors::SUCCESS();
}
} // namespace

MarginUsageWidget::MarginUsageWidget(const QJsonObject& cfg, QWidget* parent) : BaseWidget(tr("MARGIN USAGE"), parent) {
    auto* vl = content_layout();
    vl->setContentsMargins(10, 8, 10, 8);
    vl->setSpacing(6);

    header_hint_ = new QLabel("—");
    vl->addWidget(header_hint_);

    auto* grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(4);

    auto make_row = [this, grid](int row, const QString& label_text, QLabel*& value_out) {
        auto* lbl = new QLabel(label_text);
        lbl->setObjectName("marginUsageRowLabel");
        row_labels_.append(lbl);
        value_out = new QLabel("—");
        value_out->setObjectName("marginUsageRowValue");
        value_out->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(lbl, row, 0);
        grid->addWidget(value_out, row, 1);
    };

    make_row(0, tr("Available"), available_val_);
    make_row(1, tr("Used"), used_val_);
    make_row(2, tr("Total"), total_val_);
    make_row(3, tr("Collateral"), collateral_val_);

    vl->addLayout(grid);
    vl->addSpacing(4);

    usage_pct_label_ = new QLabel(tr("Usage: —"));
    usage_pct_label_->setObjectName("marginUsagePctLabel");
    vl->addWidget(usage_pct_label_);

    usage_bar_ = new QProgressBar(this);
    usage_bar_->setRange(0, 100);
    usage_bar_->setValue(0);
    usage_bar_->setTextVisible(false);
    usage_bar_->setFixedHeight(6);
    vl->addWidget(usage_bar_);

    vl->addStretch(1);

    // Title-bar refresh = "retry". It used to be wired to nothing, so the 20 s
    // "No data yet - click refresh to retry" prompt pointed at a dead button.
    // The account stream polls on its own cadence, so the only thing a retry can
    // do is (re)start that stream, show the loading state again, and ask the hub.
    connect(this, &BaseWidget::refresh_requested, this, [this]() {
        if (broker_id_.isEmpty() || account_id_.isEmpty())
            return;
        ensure_stream_running();
        if (last_usage_pct_ < 0)
            set_loading(true);
        datahub::DataHub::instance().request(balance_topic(), /*force=*/true);
        refresh_header_hint(); // a still-dead session says so again instead of spinning
    });

    // A dead broker session (expired token, connection error) used to end in the
    // generic "No data yet": the poll fails silently. Name it in the header and,
    // with nothing on screen, on the overlay.
    connect(&trading::AccountManager::instance(), &trading::AccountManager::connection_state_changed, this,
            [this](const QString& account_id, trading::ConnectionState) {
                if (account_id == account_id_)
                    refresh_header_hint();
            });

    // Settings > Currency & Number Format: re-format the hub's last funds, no fetch.
    ui::formatting::on_format_changed(this, [this]() {
        if (!hub_active_ || last_usage_pct_ < 0)
            return;
        const QVariant v = datahub::DataHub::instance().peek_raw(balance_topic());
        if (v.canConvert<trading::BrokerFunds>())
            populate(v.value<trading::BrokerFunds>());
    });

    set_configurable(true);
    apply_styles();
    apply_config(cfg);
}

QJsonObject MarginUsageWidget::config() const {
    QJsonObject o;
    if (!account_id_.isEmpty())
        o.insert("account_id", account_id_);
    return o;
}

void MarginUsageWidget::refresh_header_hint() {
    if (!header_hint_)
        return;
    if (account_id_.isEmpty()) {
        header_hint_->setText(tr("No active account — click gear to configure"));
        return;
    }
    // Account name, or name + why its broker session is unusable (recorded as
    // the balance topic's error so an empty tile says so on its overlay).
    header_hint_->setText(broker_account_hint(account_id_, balance_topic()));
}

QString MarginUsageWidget::balance_topic() const {
    if (broker_id_.isEmpty() || account_id_.isEmpty())
        return {};
    return trading::broker_topic(broker_id_, account_id_, QStringLiteral("balance"));
}

void MarginUsageWidget::apply_config(const QJsonObject& cfg) {
    account_id_ = cfg.value("account_id").toString();
    if (account_id_.isEmpty())
        account_id_ = resolve_account_id();

    broker_id_.clear();
    if (!account_id_.isEmpty())
        broker_id_ = trading::AccountManager::instance().get_account(account_id_).broker_id;
    refresh_header_hint();
    if (account_id_.isEmpty())
        show_no_account_state();
    else
        hide_empty_state();

    if (isVisible() && !broker_id_.isEmpty() && !account_id_.isEmpty()) {
        ensure_stream_running();
        hub_resubscribe();
    }
}

void MarginUsageWidget::show_no_account_state() {
    // Class C (needs a broker login): explain and point to Equity Trading, never
    // connect anything here. An account that becomes active fills the tile.
    show_no_broker_account(tr("This tile shows a broker account's funds — available, used margin, total, "
                              "collateral and how much of it is in use."),
                           [this]() { apply_config({}); });
}

QString MarginUsageWidget::resolve_account_id() const {
    const auto active = trading::AccountManager::instance().active_accounts();
    return active.isEmpty() ? QString() : active.first().account_id;
}

void MarginUsageWidget::ensure_stream_running() {
    auto& mgr = trading::DataStreamManager::instance();
    if (!mgr.has_stream(account_id_))
        mgr.start_stream(account_id_);
}

void MarginUsageWidget::hub_resubscribe() {
    auto& hub = datahub::DataHub::instance();
    hub.unsubscribe(this);
    if (broker_id_.isEmpty() || account_id_.isEmpty())
        return;
    const QString topic = balance_topic();
    if (last_usage_pct_ < 0)
        set_loading(true); // nothing shown yet — see OpenPositionsWidget::hub_resubscribe
    // Producer errors on the balance topic replace the spinner / mark the tile STALE.
    track_topics({topic});
    hub.subscribe(this, topic, [this, topic](const QVariant& v) {
        if (!v.canConvert<trading::BrokerFunds>())
            return;
        note_topic_value(topic);
        populate(v.value<trading::BrokerFunds>());
    });
    hub_active_ = true;
    refresh_header_hint();
}

void MarginUsageWidget::hub_unsubscribe_all() {
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
}

void MarginUsageWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!broker_id_.isEmpty() && !account_id_.isEmpty()) {
        ensure_stream_running();
        if (!hub_active_)
            hub_resubscribe();
    }
}

void MarginUsageWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    if (hub_active_)
        hub_unsubscribe_all();
}

void MarginUsageWidget::populate(const trading::BrokerFunds& funds) {
    // The account's own currency; unknown broker -> no unit (never a guessed one).
    const auto* broker = trading::BrokerRegistry::instance().get(broker_id_);
    const QString ccy = broker ? broker->profile().currency : QString();
    available_val_->setText(mu_money(funds.available_balance, ccy));
    used_val_->setText(mu_money(funds.used_margin, ccy));
    total_val_->setText(mu_money(funds.total_balance, ccy));
    collateral_val_->setText(mu_money(funds.collateral, ccy));

    const double denom = funds.total_balance > 0 ? funds.total_balance : (funds.available_balance + funds.used_margin);
    // The label shows the real ratio — an account past 100% (margin shortfall)
    // used to read a reassuring "Usage: 100%". Only the bar is clamped.
    int pct = 0;
    if (denom > 0)
        pct = std::max(0, static_cast<int>(std::lround((funds.used_margin / denom) * 100.0)));
    usage_bar_->setValue(std::min(pct, 100));
    last_usage_pct_ = pct;
    usage_pct_label_->setText(tr("Usage: %1%").arg(pct));
    usage_pct_label_->setToolTip(pct > 100 ? tr("Used margin exceeds the account balance") : QString());

    set_css_cached(usage_bar_, QString("QProgressBar{background:%1;border:none;border-radius:2px;}"
                                       "QProgressBar::chunk{background:%2;border-radius:2px;}")
                                   .arg(ui::colors::BG_RAISED(), margin_usage_color(pct)));

    set_loading(false);
}

QDialog* MarginUsageWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle(tr("Configure — Margin Usage"));
    auto* form = new QFormLayout(dlg);

    auto* combo = new QComboBox(dlg);
    const auto accts = trading::AccountManager::instance().list_accounts();
    for (const auto& a : accts) {
        const QString label = a.display_name.isEmpty() ? (a.broker_id + " — " + a.account_id) : a.display_name;
        combo->addItem(label, a.account_id);
        if (a.account_id == account_id_)
            combo->setCurrentIndex(combo->count() - 1);
    }
    form->addRow(tr("Broker account"), combo);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    form->addRow(buttons);

    connect(buttons, &QDialogButtonBox::accepted, dlg, [this, dlg, combo]() {
        const QString picked = combo->currentData().toString();
        if (!picked.isEmpty() && picked != account_id_) {
            QJsonObject cfg;
            cfg.insert("account_id", picked);
            apply_config(cfg);
            emit config_changed(cfg);
        }
        dlg->accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    return dlg;
}

void MarginUsageWidget::on_theme_changed() {
    apply_styles();
}

void MarginUsageWidget::apply_styles() {
    set_css_cached(header_hint_, 
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;background:transparent;padding:2px 0;").arg(ui::colors::TEXT_TERTIARY()));

    const QString row_label_css =
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;background:transparent;").arg(ui::colors::TEXT_TERTIARY());
    const QString row_value_css =
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-2) + "px;font-weight:600;background:transparent;").arg(ui::colors::TEXT_PRIMARY());
    for (QLabel* l : {available_val_, used_val_, total_val_, collateral_val_}) {
        if (l)
            set_css_cached(l, row_value_css);
    }
    if (auto* parent = available_val_ ? available_val_->parentWidget() : nullptr) {
        set_css_cached(parent, 
            QString("QLabel#marginUsageRowLabel{%1} QLabel#marginUsageRowValue{%2}").arg(row_label_css, row_value_css));
    }

    if (usage_pct_label_)
        set_css_cached(usage_pct_label_, 
            QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:600;background:transparent;padding-top:4px;")
                .arg(ui::colors::TEXT_SECONDARY()));

    if (usage_bar_)
        set_css_cached(usage_bar_, QString("QProgressBar{background:%1;border:none;border-radius:2px;}"
                                           "QProgressBar::chunk{background:%2;border-radius:2px;}")
                                       .arg(ui::colors::BG_RAISED(), margin_usage_color(std::max(last_usage_pct_, 0))));
}

void MarginUsageWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    set_title(tr("MARGIN USAGE"));
    // Actually re-translate the kept labels. The previous implementation
    // called hub_resubscribe(), which tears down and rebuilds every hub
    // subscription and does not touch a single string.
    const QStringList captions = {tr("Available"), tr("Used"), tr("Total"), tr("Collateral")};
    for (int i = 0; i < row_labels_.size() && i < captions.size(); ++i)
        row_labels_[i]->setText(captions[i]);
    if (usage_pct_label_)
        usage_pct_label_->setText(last_usage_pct_ >= 0 ? tr("Usage: %1%").arg(last_usage_pct_)
                                                       : tr("Usage: —"));
    refresh_header_hint();
    if (account_id_.isEmpty())
        show_no_account_state();
}

} // namespace fincept::screens::widgets
