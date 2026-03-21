#include "ui/widgets/SearchBar.h"
#include "ui/theme/Theme.h"

namespace fincept::ui {

SearchBar::SearchBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(32);
    setStyleSheet("background: transparent;");

    auto* hl = new QHBoxLayout(this);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(0);

    input_ = new QLineEdit;
    input_->setPlaceholderText("Enter command or search...");
    input_->setStyleSheet(
        QString("QLineEdit { background: %1; color: %2; border: 1px solid %3; "
                "padding: 4px 12px; font-size: 13px; } "
                "QLineEdit:focus { border-color: %4; }")
            .arg(colors::PANEL, colors::WHITE, colors::BORDER, colors::ORANGE));

    connect(input_, &QLineEdit::returnPressed, this, [this]() {
        emit search_submitted(input_->text());
    });
    connect(input_, &QLineEdit::textChanged, this, &SearchBar::text_changed);

    hl->addWidget(input_);
}

QString SearchBar::text() const { return input_->text(); }
void SearchBar::clear() { input_->clear(); }

} // namespace fincept::ui
