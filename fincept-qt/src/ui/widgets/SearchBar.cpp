#include "ui/widgets/SearchBar.h"

#include "ui/theme/Theme.h"

namespace fincept::ui {

SearchBar::SearchBar(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(32); // grows with the input for a large base font
    setStyleSheet("background: transparent;");

    auto* hl = new QHBoxLayout(this);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(0);

    input_ = new QLineEdit;
    input_->setPlaceholderText(tr("Enter command or search..."));
    input_->setStyleSheet(QString("QLineEdit { background: %1; color: %2; border: 1px solid %3; "
                                  "padding: 4px 12px; font-size: " + fincept::ui::fonts::px(-1) + "px; } "
                                  "QLineEdit:focus { border-color: %4; }")
                              .arg(colors::BG_SURFACE(), colors::TEXT_PRIMARY(), colors::BORDER_MED(), colors::AMBER()));

    connect(input_, &QLineEdit::returnPressed, this, [this]() { emit search_submitted(input_->text()); });
    connect(input_, &QLineEdit::textChanged, this, &SearchBar::text_changed);

    hl->addWidget(input_);
}

QString SearchBar::text() const {
    return input_->text();
}
void SearchBar::clear() {
    input_->clear();
}

void SearchBar::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void SearchBar::retranslateUi() {
    if (input_)
        input_->setPlaceholderText(tr("Enter command or search..."));
}

} // namespace fincept::ui
