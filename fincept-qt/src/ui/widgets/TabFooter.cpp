#include "ui/widgets/TabFooter.h"
#include "ui/theme/Theme.h"

namespace fincept::ui {

static constexpr const char* APP_NAME    = "Fincept Terminal";
static constexpr const char* APP_VERSION = "4.0.0";

static QLabel* separator() {
    auto* s = new QLabel("|");
    s->setStyleSheet(
        "color: #6b7280; font-size: 11px; background: transparent;"
        " font-family: 'Consolas','Courier New',monospace;");
    return s;
}

TabFooter::TabFooter(const QString& tab_name, QWidget* parent)
    : QWidget(parent)
{
    setStyleSheet("background: #1a1a1a; border-top: 2px solid #ea580c;");
    setFixedHeight(30);

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(16, 4, 16, 4);
    root->setSpacing(12);

    // Left: brand | tab name | info items
    auto* left = new QHBoxLayout;
    left->setSpacing(10);
    left->setContentsMargins(0, 0, 0, 0);

    brand_label_ = new QLabel(QString("%1 v%2").arg(APP_NAME, APP_VERSION).toUpper());
    brand_label_->setStyleSheet(
        "color: #ea580c; font-size: 12px; font-weight: bold;"
        " background: transparent;"
        " font-family: 'Consolas','Courier New',monospace;");
    left->addWidget(brand_label_);
    left->addWidget(separator());

    tab_label_ = new QLabel(tab_name);
    tab_label_->setStyleSheet(
        "color: #06b6d4; font-size: 11px; font-weight: bold;"
        " background: transparent;"
        " font-family: 'Consolas','Courier New',monospace;");
    left->addWidget(tab_label_);

    // Info items area (dynamically populated)
    info_layout_ = new QHBoxLayout;
    info_layout_->setSpacing(10);
    info_layout_->setContentsMargins(0, 0, 0, 0);
    left->addLayout(info_layout_);

    root->addLayout(left);
    root->addStretch();

    // Right: status
    status_label_ = new QLabel;
    status_label_->setStyleSheet(
        "color: #6b7280; font-size: 10px; background: transparent;"
        " font-family: 'Consolas','Courier New',monospace;");
    root->addWidget(status_label_);
}

void TabFooter::set_tab_name(const QString& name) {
    tab_label_->setText(name);
}

void TabFooter::set_status(const QString& status_text) {
    status_label_->setText(status_text);
}

void TabFooter::set_left_info(const QVector<FooterInfo>& items) {
    rebuild_info(items);
}

void TabFooter::rebuild_info(const QVector<FooterInfo>& items) {
    // Clear existing info items
    while (info_layout_->count() > 0) {
        auto* item = info_layout_->takeAt(0);
        if (item->widget()) delete item->widget();
        delete item;
    }

    if (items.isEmpty()) return;

    info_layout_->addWidget(separator());

    for (int i = 0; i < items.size(); ++i) {
        const auto& info = items[i];
        QString color = info.color.isEmpty() ? "#ffffff" : info.color;
        QString text  = info.value.isEmpty()
            ? info.label
            : QString("%1: %2").arg(info.label, info.value);

        auto* lbl = new QLabel(text);
        lbl->setStyleSheet(QString(
            "color: %1; font-size: 11px; background: transparent;"
            " font-family: 'Consolas','Courier New',monospace;")
            .arg(color));
        info_layout_->addWidget(lbl);

        if (i < items.size() - 1)
            info_layout_->addWidget(separator());
    }
}

} // namespace fincept::ui
