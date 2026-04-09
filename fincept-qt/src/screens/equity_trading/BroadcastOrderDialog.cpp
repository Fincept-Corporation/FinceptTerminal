// BroadcastOrderDialog.cpp — multi-account order broadcast
#include "screens/equity_trading/BroadcastOrderDialog.h"

#include "trading/AccountManager.h"
#include "trading/BrokerRegistry.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QMetaObject>
#include <QPointer>
#include <QScrollArea>
#include <QtConcurrent/QtConcurrent>

namespace fincept::screens::equity {

using namespace fincept::ui;
using namespace fincept::trading;

BroadcastOrderDialog::BroadcastOrderDialog(const trading::UnifiedOrder& order, QWidget* parent)
    : QDialog(parent), order_(order) {
    setWindowTitle("Broadcast Order");
    setMinimumSize(450, 380);
    setStyleSheet(QString("QDialog { background: %1; color: %2; }"
                          "QCheckBox { color: %2; font-size: 12px; spacing: 6px; }"
                          "QCheckBox::indicator { width: 14px; height: 14px; }"
                          "QLabel#header { color: %3; font-size: 14px; font-weight: 700; }"
                          "QLabel#orderInfo { color: %4; font-size: 11px; }"
                          "QLabel#resultOk { color: %5; font-size: 11px; }"
                          "QLabel#resultErr { color: %6; font-size: 11px; }"
                          "QPushButton { padding: 8px 16px; font-weight: 700; font-size: 12px; border-radius: 2px; }")
                      .arg(colors::BG_SURFACE, colors::TEXT_PRIMARY, colors::AMBER,
                           colors::TEXT_SECONDARY, colors::POSITIVE, colors::NEGATIVE));
    setup_ui();
}

void BroadcastOrderDialog::setup_ui() {
    auto* root = new QVBoxLayout(this);
    root->setSpacing(10);
    root->setContentsMargins(16, 16, 16, 16);

    // Header
    auto* header = new QLabel("BROADCAST ORDER");
    header->setObjectName("header");
    root->addWidget(header);

    // Order summary
    const QString side_str = order_.side == OrderSide::Buy ? "BUY" : "SELL";
    const QString type_str = order_type_str(order_.order_type);
    auto* info = new QLabel(QString("%1  %2  x%3  %4  @ %5")
                                .arg(side_str, order_.symbol)
                                .arg(order_.quantity)
                                .arg(type_str)
                                .arg(order_.price > 0 ? QString::number(order_.price, 'f', 2) : "MKT"));
    info->setObjectName("orderInfo");
    root->addWidget(info);

    // Separator
    auto* sep = new QWidget(this);
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background: %1;").arg(colors::BORDER_MED));
    root->addWidget(sep);

    // Select All
    select_all_cb_ = new QCheckBox("Select All");
    select_all_cb_->setStyleSheet(QString("QCheckBox { color: %1; font-weight: 700; }").arg(colors::AMBER));
    connect(select_all_cb_, &QCheckBox::toggled, this, &BroadcastOrderDialog::on_select_all);
    root->addWidget(select_all_cb_);

    // Scrollable account list
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(QString("QScrollArea { border: 1px solid %1; background: %2; }")
                              .arg(colors::BORDER_MED, colors::BG_BASE));

    auto* list_widget = new QWidget(this);
    auto* list_layout = new QVBoxLayout(list_widget);
    list_layout->setContentsMargins(8, 8, 8, 8);
    list_layout->setSpacing(4);

    const auto accounts = AccountManager::instance().active_accounts();
    for (const auto& account : accounts) {
        auto* broker = BrokerRegistry::instance().get(account.broker_id);
        const QString broker_name = broker ? broker->profile().display_name : account.broker_id;
        const QString mode_tag = account.trading_mode == "live" ? "[LIVE]" : "[PAPER]";
        const QString text = QString("%1  [%2]  %3").arg(account.display_name, broker_name, mode_tag);

        auto* cb = new QCheckBox(text);
        // Color-code by connection state
        if (account.state == ConnectionState::Connected)
            cb->setStyleSheet(QString("QCheckBox { color: %1; }").arg(colors::TEXT_PRIMARY));
        else
            cb->setStyleSheet(QString("QCheckBox { color: %1; }").arg(colors::TEXT_TERTIARY));

        list_layout->addWidget(cb);
        account_cbs_.append(cb);
        account_ids_.append(account.account_id);

        // Connect each checkbox to update Select All state
        connect(cb, &QCheckBox::toggled, this, [this](bool) {
            bool all_checked = true;
            for (auto* c : account_cbs_) {
                if (!c->isChecked()) {
                    all_checked = false;
                    break;
                }
            }
            select_all_cb_->blockSignals(true);
            select_all_cb_->setChecked(all_checked);
            select_all_cb_->blockSignals(false);
        });
    }

    list_layout->addStretch();
    scroll->setWidget(list_widget);
    root->addWidget(scroll, 1);

    // Results area (hidden initially)
    results_widget_ = new QWidget(this);
    results_layout_ = new QVBoxLayout(results_widget_);
    results_layout_->setContentsMargins(0, 0, 0, 0);
    results_layout_->setSpacing(2);
    results_widget_->hide();
    root->addWidget(results_widget_);

    // Status
    status_label_ = new QLabel("");
    status_label_->setObjectName("orderInfo");
    root->addWidget(status_label_);

    // Buttons
    auto* btn_row = new QHBoxLayout;
    btn_row->addStretch();

    auto* cancel_btn = new QPushButton("CANCEL");
    cancel_btn->setStyleSheet(QString("QPushButton { background: %1; color: %2; }")
                                  .arg(colors::BG_RAISED, colors::TEXT_PRIMARY));
    connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);
    btn_row->addWidget(cancel_btn);

    place_btn_ = new QPushButton(QString("PLACE %1").arg(order_.side == OrderSide::Buy ? "BUY" : "SELL"));
    place_btn_->setStyleSheet(
        QString("QPushButton { background: %1; color: %2; }")
            .arg(order_.side == OrderSide::Buy ? colors::POSITIVE : colors::NEGATIVE, colors::BG_BASE));
    connect(place_btn_, &QPushButton::clicked, this, &BroadcastOrderDialog::on_place_order);
    btn_row->addWidget(place_btn_);

    root->addLayout(btn_row);
}

void BroadcastOrderDialog::on_select_all(bool checked) {
    for (auto* cb : account_cbs_)
        cb->setChecked(checked);
}

void BroadcastOrderDialog::on_place_order() {
    // Collect selected account IDs
    QStringList selected;
    for (int i = 0; i < account_cbs_.size(); ++i) {
        if (account_cbs_[i]->isChecked())
            selected.append(account_ids_[i]);
    }

    if (selected.isEmpty()) {
        status_label_->setText("Select at least one account");
        status_label_->setStyleSheet(QString("color: %1;").arg(colors::NEGATIVE));
        return;
    }

    // Disable UI during placement
    place_btn_->setEnabled(false);
    select_all_cb_->setEnabled(false);
    for (auto* cb : account_cbs_)
        cb->setEnabled(false);

    status_label_->setText(QString("Placing order across %1 account(s)...").arg(selected.size()));
    status_label_->setStyleSheet(QString("color: %1;").arg(colors::AMBER));

    // Run broadcast on background thread (P1: never block UI)
    QPointer<BroadcastOrderDialog> self = this;
    auto order_copy = order_;
    QtConcurrent::run([self, selected, order_copy]() {
        auto results = UnifiedTrading::instance().broadcast_order(selected, order_copy);
        QMetaObject::invokeMethod(
            self,
            [self, results]() {
                if (!self)
                    return;
                self->show_results(results);
                emit self->broadcast_completed(results);
            },
            Qt::QueuedConnection);
    });
}

void BroadcastOrderDialog::show_results(const QVector<UnifiedTrading::BroadcastResult>& results) {
    // Clear old results
    while (results_layout_->count() > 0) {
        auto* item = results_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    int success_count = 0;
    int fail_count = 0;

    for (const auto& r : results) {
        const QString mode_str = r.response.mode.isEmpty() ? "" : QString(" [%1]").arg(r.response.mode.toUpper());
        QString text;
        QString obj_name;
        if (r.response.success) {
            text = QString("%1 %2 %3: %4")
                       .arg(QString::fromUtf8("\xe2\x9c\x93"), r.display_name, mode_str, // checkmark
                            r.response.order_id.isEmpty() ? r.response.message : "Order #" + r.response.order_id);
            obj_name = "resultOk";
            ++success_count;
        } else {
            text = QString("%1 %2 %3: %4")
                       .arg(QString::fromUtf8("\xe2\x9c\x97"), r.display_name, mode_str, r.response.message); // X mark
            obj_name = "resultErr";
            ++fail_count;
        }
        auto* label = new QLabel(text);
        label->setObjectName(obj_name);
        label->setWordWrap(true);
        results_layout_->addWidget(label);
    }

    results_widget_->show();

    // Update status
    if (fail_count == 0) {
        status_label_->setText(QString("All %1 orders placed successfully").arg(success_count));
        status_label_->setStyleSheet(QString("color: %1;").arg(colors::POSITIVE));
    } else if (success_count == 0) {
        status_label_->setText(QString("All %1 orders failed").arg(fail_count));
        status_label_->setStyleSheet(QString("color: %1;").arg(colors::NEGATIVE));
    } else {
        status_label_->setText(QString("%1 succeeded, %2 failed").arg(success_count).arg(fail_count));
        status_label_->setStyleSheet(QString("color: %1;").arg(colors::WARNING));
    }

    // Re-enable close
    place_btn_->setText("DONE");
    place_btn_->setEnabled(true);
    disconnect(place_btn_, &QPushButton::clicked, this, &BroadcastOrderDialog::on_place_order);
    connect(place_btn_, &QPushButton::clicked, this, &QDialog::accept);
}

} // namespace fincept::screens::equity
