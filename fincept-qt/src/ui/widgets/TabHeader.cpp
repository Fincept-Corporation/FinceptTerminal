#include "ui/widgets/TabHeader.h"
#include "ui/theme/Theme.h"

namespace fincept::ui {

static constexpr const char* APP_NAME    = "Fincept Terminal";
static constexpr const char* APP_VERSION = "4.0.0";

TabHeader::TabHeader(const QString& title, const QString& subtitle, QWidget* parent)
    : QWidget(parent)
{
    // Orange accent bottom border, dark raised background
    setStyleSheet(QString(
        "background: #1a1a1a;"
        "border-bottom: 2px solid #ea580c;"));
    setFixedHeight(subtitle.isEmpty() ? 44 : 56);

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(16, 8, 16, 8);
    root->setSpacing(16);

    // Left: title column
    auto* left = new QVBoxLayout;
    left->setSpacing(4);
    left->setContentsMargins(0, 0, 0, 0);

    // Title row: "Title  |  Fincept Terminal v4.0.0"
    auto* title_row = new QHBoxLayout;
    title_row->setSpacing(8);
    title_row->setContentsMargins(0, 0, 0, 0);

    title_label_ = new QLabel(title);
    title_label_->setStyleSheet(
        "color: #ffffff; font-size: 16px; font-weight: bold;"
        " background: transparent;"
        " font-family: 'Consolas','Courier New',monospace;");
    title_row->addWidget(title_label_);

    auto* sep = new QLabel("|");
    sep->setStyleSheet(
        "color: #6b7280; font-size: 12px; background: transparent;"
        " font-family: 'Consolas','Courier New',monospace;");
    title_row->addWidget(sep);

    version_label_ = new QLabel(QString("%1 v%2").arg(APP_NAME, APP_VERSION));
    version_label_->setStyleSheet(
        "color: #ea580c; font-size: 11px; font-weight: bold;"
        " background: transparent;"
        " font-family: 'Consolas','Courier New',monospace;");
    title_row->addWidget(version_label_);

    title_row->addStretch();
    left->addLayout(title_row);

    // Subtitle (optional)
    subtitle_label_ = new QLabel(subtitle);
    subtitle_label_->setStyleSheet(
        "color: #6b7280; font-size: 11px; background: transparent;"
        " font-family: 'Consolas','Courier New',monospace;");
    subtitle_label_->setVisible(!subtitle.isEmpty());
    left->addWidget(subtitle_label_);

    root->addLayout(left, 1);

    // Right: actions area
    actions_layout_ = new QHBoxLayout;
    actions_layout_->setSpacing(8);
    actions_layout_->setContentsMargins(0, 0, 0, 0);
    root->addLayout(actions_layout_);
}

void TabHeader::set_title(const QString& title) {
    title_label_->setText(title);
}

void TabHeader::set_subtitle(const QString& subtitle) {
    subtitle_label_->setText(subtitle);
    subtitle_label_->setVisible(!subtitle.isEmpty());
}

} // namespace fincept::ui
