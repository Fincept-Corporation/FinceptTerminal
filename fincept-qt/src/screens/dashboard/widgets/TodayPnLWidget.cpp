#include "screens/dashboard/widgets/TodayPnLWidget.h"

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

namespace fincept::screens::widgets {

namespace {
QString fmt_pnl(double v) {
    const QString sign = v >= 0 ? "+" : "";
    if (std::abs(v) >= 1.0e7)
        return sign + QString::number(v / 1.0e7, 'f', 2) + "Cr";
    if (std::abs(v) >= 1.0e5)
        return sign + QString::number(v / 1.0e5, 'f', 2) + "L";
    return sign + QString::number(v, 'f', 2);
}
} // namespace

TodayPnLWidget::TodayPnLWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget("TODAY P&L", parent) {
    auto* vl = content_layout();
    vl->setContentsMargins(10, 8, 10, 8);
    vl->setSpacing(6);

    header_hint_ = new QLabel("—");
    vl->addWidget(header_hint_);

    total_pnl_label_ = new QLabel("—");
    total_pnl_label_->setAlignment(Qt::AlignCenter);
    total_pnl_label_->setObjectName("todayPnlHero");
    vl->addWidget(total_pnl_label_);

    auto* grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(2);

    auto make_row = [grid](int row, const QString& label_text, QLabel*& label_out, QLabel*& value_out) {
        label_out = new QLabel(label_text);
        label_out->setObjectName("todayPnlRowLabel");
        value_out = new QLabel("—");
        value_out->setObjectName("todayPnlRowValue");
        value_out->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(label_out, row, 0);
        grid->addWidget(value_out, row, 1);
    };

    make_row(0, "Day", day_pnl_label_, day_pnl_value_);
    make_row(1, "Realized", realized_pnl_label_, realized_pnl_value_);
    make_row(2, "Positions", positions_label_, positions_value_);

    vl->addLayout(grid);
    vl->addStretch(1);

    set_configurable(true);
    apply_styles();
    apply_config(cfg);
}

QJsonObject TodayPnLWidget::config() const {
    QJsonObject o;
    if (!account_id_.isEmpty())
        o.insert("account_id", account_id_);
    return o;
}

void TodayPnLWidget::apply_config(const QJsonObject& cfg) {
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

QString TodayPnLWidget::resolve_account_id() const {
    const auto active = trading::AccountManager::instance().active_accounts();
    return active.isEmpty() ? QString() : active.first().account_id;
}

void TodayPnLWidget::ensure_stream_running() {
    auto& mgr = trading::DataStreamManager::instance();
    if (!mgr.has_stream(account_id_))
        mgr.start_stream(account_id_);
}

void TodayPnLWidget::hub_resubscribe() {
    auto& hub = datahub::DataHub::instance();
    hub.unsubscribe(this);
    if (broker_id_.isEmpty() || account_id_.isEmpty())
        return;
    const QString topic = trading::broker_topic(broker_id_, account_id_, QStringLiteral("positions"));
    hub.subscribe(this, topic, [this](const QVariant& v) {
        if (!v.canConvert<QVector<trading::BrokerPosition>>())
            return;
        populate(v.value<QVector<trading::BrokerPosition>>());
    });
    hub_active_ = true;
}

void TodayPnLWidget::hub_unsubscribe_all() {
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
}

void TodayPnLWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!broker_id_.isEmpty() && !account_id_.isEmpty()) {
        ensure_stream_running();
        if (!hub_active_)
            hub_resubscribe();
    }
}

void TodayPnLWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    if (hub_active_)
        hub_unsubscribe_all();
}

void TodayPnLWidget::populate(const QVector<trading::BrokerPosition>& rows) {
    double total_pnl = 0.0;
    double day_pnl = 0.0;
    int open_positions = 0;
    for (const auto& p : rows) {
        total_pnl += p.pnl;
        day_pnl += p.day_pnl;
        if (std::abs(p.quantity) > 0.0)
            ++open_positions;
    }
    // Realized ≈ total P&L on zero-quantity rows (closed intraday)
    double realized = 0.0;
    for (const auto& p : rows) {
        if (std::abs(p.quantity) < 1e-9)
            realized += p.pnl;
    }

    const QColor total_col =
        total_pnl >= 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE();
    total_pnl_label_->setText(fmt_pnl(total_pnl));
    total_pnl_label_->setStyleSheet(
        QString("color:%1;font-size:24px;font-weight:700;background:transparent;padding:4px 0;")
            .arg(total_col.name()));

    day_pnl_value_->setText(fmt_pnl(day_pnl));
    day_pnl_value_->setStyleSheet(
        QString("color:%1;font-size:12px;font-weight:600;background:transparent;")
            .arg(day_pnl >= 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));

    realized_pnl_value_->setText(fmt_pnl(realized));
    realized_pnl_value_->setStyleSheet(
        QString("color:%1;font-size:12px;font-weight:600;background:transparent;")
            .arg(realized >= 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));

    positions_value_->setText(QString::number(open_positions));

    set_loading(false);
}

QDialog* TodayPnLWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle("Configure — Today P&L");
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

void TodayPnLWidget::on_theme_changed() {
    apply_styles();
}

void TodayPnLWidget::apply_styles() {
    header_hint_->setStyleSheet(
        QString("color:%1;font-size:9px;background:transparent;padding:2px 0;")
            .arg(ui::colors::TEXT_TERTIARY()));

    const QString row_label_css =
        QString("color:%1;font-size:10px;background:transparent;").arg(ui::colors::TEXT_TERTIARY());
    if (day_pnl_label_)
        day_pnl_label_->setStyleSheet(row_label_css);
    if (realized_pnl_label_)
        realized_pnl_label_->setStyleSheet(row_label_css);
    if (positions_label_)
        positions_label_->setStyleSheet(row_label_css);
    if (positions_value_)
        positions_value_->setStyleSheet(
            QString("color:%1;font-size:12px;font-weight:600;background:transparent;")
                .arg(ui::colors::TEXT_PRIMARY()));
}

} // namespace fincept::screens::widgets
