#include "screens/dashboard/widgets/MarginUsageWidget.h"

#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "trading/AccountManager.h"
#include "trading/BrokerTopic.h"
#include "trading/DataStreamManager.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QJsonObject>
#include <QLabel>
#include <QProgressBar>

namespace fincept::screens::widgets {

namespace {
QString fmt_money(double v) {
    QString s;
    if (std::abs(v) >= 1.0e7)
        s = QString::number(v / 1.0e7, 'f', 2) + "Cr";
    else if (std::abs(v) >= 1.0e5)
        s = QString::number(v / 1.0e5, 'f', 2) + "L";
    else
        s = QString::number(v, 'f', 2);
    return s;
}
} // namespace

MarginUsageWidget::MarginUsageWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget("MARGIN USAGE", parent) {
    auto* vl = content_layout();
    vl->setContentsMargins(10, 8, 10, 8);
    vl->setSpacing(6);

    header_hint_ = new QLabel("—");
    vl->addWidget(header_hint_);

    auto* grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(4);

    auto make_row = [grid](int row, const QString& label_text, QLabel*& value_out) {
        auto* lbl = new QLabel(label_text);
        lbl->setObjectName("marginUsageRowLabel");
        value_out = new QLabel("—");
        value_out->setObjectName("marginUsageRowValue");
        value_out->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(lbl, row, 0);
        grid->addWidget(value_out, row, 1);
    };

    make_row(0, "Available", available_val_);
    make_row(1, "Used", used_val_);
    make_row(2, "Total", total_val_);
    make_row(3, "Collateral", collateral_val_);

    vl->addLayout(grid);
    vl->addSpacing(4);

    usage_pct_label_ = new QLabel("Usage: —");
    usage_pct_label_->setObjectName("marginUsagePctLabel");
    vl->addWidget(usage_pct_label_);

    usage_bar_ = new QProgressBar(this);
    usage_bar_->setRange(0, 100);
    usage_bar_->setValue(0);
    usage_bar_->setTextVisible(false);
    usage_bar_->setFixedHeight(6);
    vl->addWidget(usage_bar_);

    vl->addStretch(1);

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

void MarginUsageWidget::apply_config(const QJsonObject& cfg) {
    account_id_ = cfg.value("account_id").toString();
    if (account_id_.isEmpty())
        account_id_ = resolve_account_id();

    broker_id_.clear();
    if (!account_id_.isEmpty()) {
        const auto acct = trading::AccountManager::instance().get_account(account_id_);
        broker_id_ = acct.broker_id;
        header_hint_->setText(acct.display_name.isEmpty() ? account_id_ : acct.display_name);
    } else {
        header_hint_->setText("No active account — click gear to configure");
    }

    if (isVisible() && !broker_id_.isEmpty() && !account_id_.isEmpty()) {
        ensure_stream_running();
        hub_resubscribe();
    }
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
    const QString topic = trading::broker_topic(broker_id_, account_id_, QStringLiteral("balance"));
    hub.subscribe(this, topic, [this](const QVariant& v) {
        if (!v.canConvert<trading::BrokerFunds>())
            return;
        populate(v.value<trading::BrokerFunds>());
    });
    hub_active_ = true;
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
    available_val_->setText(fmt_money(funds.available_balance));
    used_val_->setText(fmt_money(funds.used_margin));
    total_val_->setText(fmt_money(funds.total_balance));
    collateral_val_->setText(fmt_money(funds.collateral));

    const double denom = funds.total_balance > 0 ? funds.total_balance
                                                 : (funds.available_balance + funds.used_margin);
    int pct = 0;
    if (denom > 0) {
        pct = qBound(0, int((funds.used_margin / denom) * 100.0 + 0.5), 100);
    }
    usage_bar_->setValue(pct);
    usage_pct_label_->setText(QString("Usage: %1%").arg(pct));

    QColor bar_color = ui::colors::POSITIVE();
    if (pct >= 80)
        bar_color = ui::colors::NEGATIVE();
    else if (pct >= 50)
        bar_color = ui::colors::WARNING();
    usage_bar_->setStyleSheet(
        QString("QProgressBar{background:%1;border:none;border-radius:2px;}"
                "QProgressBar::chunk{background:%2;border-radius:2px;}")
            .arg(ui::colors::BG_RAISED(), bar_color.name()));

    set_loading(false);
}

QDialog* MarginUsageWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle("Configure — Margin Usage");
    auto* form = new QFormLayout(dlg);

    auto* combo = new QComboBox(dlg);
    const auto accts = trading::AccountManager::instance().list_accounts();
    for (const auto& a : accts) {
        const QString label = a.display_name.isEmpty() ? (a.broker_id + " — " + a.account_id) : a.display_name;
        combo->addItem(label, a.account_id);
        if (a.account_id == account_id_)
            combo->setCurrentIndex(combo->count() - 1);
    }
    form->addRow("Broker account", combo);

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
    header_hint_->setStyleSheet(
        QString("color:%1;font-size:9px;background:transparent;padding:2px 0;")
            .arg(ui::colors::TEXT_TERTIARY()));

    const QString row_label_css =
        QString("color:%1;font-size:10px;background:transparent;").arg(ui::colors::TEXT_TERTIARY());
    const QString row_value_css =
        QString("color:%1;font-size:12px;font-weight:600;background:transparent;")
            .arg(ui::colors::TEXT_PRIMARY());
    for (QLabel* l : {available_val_, used_val_, total_val_, collateral_val_}) {
        if (l)
            l->setStyleSheet(row_value_css);
    }
    if (auto* parent = available_val_ ? available_val_->parentWidget() : nullptr) {
        parent->setStyleSheet(
            QString("QLabel#marginUsageRowLabel{%1} QLabel#marginUsageRowValue{%2}")
                .arg(row_label_css, row_value_css));
    }

    if (usage_pct_label_)
        usage_pct_label_->setStyleSheet(
            QString("color:%1;font-size:10px;font-weight:600;background:transparent;padding-top:4px;")
                .arg(ui::colors::TEXT_SECONDARY()));

    if (usage_bar_)
        usage_bar_->setStyleSheet(
            QString("QProgressBar{background:%1;border:none;border-radius:2px;}"
                    "QProgressBar::chunk{background:%2;border-radius:2px;}")
                .arg(ui::colors::BG_RAISED(), ui::colors::POSITIVE()));
}

} // namespace fincept::screens::widgets
