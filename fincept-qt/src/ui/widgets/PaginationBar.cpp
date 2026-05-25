#include "ui/widgets/PaginationBar.h"
#include "ui/theme/Theme.h"

#include <algorithm>

namespace fincept::ui {

static QString nav_btn_style() {
    return QString(
        "QPushButton { background:%1; color:%2; border:1px solid %3;"
        "  padding:2px 8px; font-size:%4px; font-weight:700; font-family:%5; }"
        "QPushButton:hover { background:%6; color:%7; }"
        "QPushButton:disabled { color:%8; background:%9; border-color:%10; }")
        .arg(colors::BG_RAISED(), colors::TEXT_SECONDARY(), colors::BORDER_DIM())
        .arg(fonts::TINY)
        .arg(fonts::DATA_FAMILY)
        .arg(colors::BG_HOVER(), colors::TEXT_PRIMARY())
        .arg(colors::TEXT_DIM(), colors::BG_SURFACE(), colors::BORDER_DIM());
}

PaginationBar::PaginationBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(28);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 0, 8, 0);
    layout->setSpacing(4);

    status_label_ = new QLabel;
    status_label_->setStyleSheet(
        QString("color:%1; font-size:%2px; font-family:%3; background:transparent;")
            .arg(colors::TEXT_TERTIARY())
            .arg(fonts::TINY)
            .arg(fonts::DATA_FAMILY));
    layout->addWidget(status_label_);

    layout->addStretch();

    btn_first_ = new QPushButton(QStringLiteral("<<"));
    btn_first_->setFixedSize(28, 20);
    btn_first_->setStyleSheet(nav_btn_style());
    connect(btn_first_, &QPushButton::clicked, this, [this]() {
        current_page_ = 1;
        update_controls();
        emit page_changed(offset(), page_size_);
    });
    layout->addWidget(btn_first_);

    btn_prev_ = new QPushButton(QStringLiteral("<"));
    btn_prev_->setFixedSize(22, 20);
    btn_prev_->setStyleSheet(nav_btn_style());
    connect(btn_prev_, &QPushButton::clicked, this, [this]() {
        if (current_page_ > 1) --current_page_;
        update_controls();
        emit page_changed(offset(), page_size_);
    });
    layout->addWidget(btn_prev_);

    page_label_ = new QLabel;
    page_label_->setAlignment(Qt::AlignCenter);
    page_label_->setMinimumWidth(80);
    page_label_->setStyleSheet(
        QString("color:%1; font-size:%2px; font-weight:700; font-family:%3; background:transparent;")
            .arg(colors::TEXT_PRIMARY())
            .arg(fonts::TINY)
            .arg(fonts::DATA_FAMILY));
    layout->addWidget(page_label_);

    btn_next_ = new QPushButton(QStringLiteral(">"));
    btn_next_->setFixedSize(22, 20);
    btn_next_->setStyleSheet(nav_btn_style());
    connect(btn_next_, &QPushButton::clicked, this, [this]() {
        if (current_page_ < total_pages()) ++current_page_;
        update_controls();
        emit page_changed(offset(), page_size_);
    });
    layout->addWidget(btn_next_);

    btn_last_ = new QPushButton(QStringLiteral(">>"));
    btn_last_->setFixedSize(28, 20);
    btn_last_->setStyleSheet(nav_btn_style());
    connect(btn_last_, &QPushButton::clicked, this, [this]() {
        current_page_ = total_pages();
        update_controls();
        emit page_changed(offset(), page_size_);
    });
    layout->addWidget(btn_last_);

    size_combo_ = new QComboBox;
    size_combo_->addItems({"25", "50", "100", "250"});
    size_combo_->setCurrentIndex(1);
    size_combo_->setFixedWidth(55);
    size_combo_->setStyleSheet(
        QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                "  font-size:%4px; font-family:%5; padding:1px 4px; }"
                "QComboBox::drop-down { border:none; }")
            .arg(colors::BG_SURFACE(), colors::TEXT_PRIMARY(), colors::BORDER_DIM())
            .arg(fonts::TINY)
            .arg(fonts::DATA_FAMILY));
    connect(size_combo_, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        page_size_ = text.toInt();
        current_page_ = 1;
        update_controls();
        emit page_changed(offset(), page_size_);
    });
    layout->addWidget(size_combo_);

    auto* rows_lbl = new QLabel(QStringLiteral("rows"));
    rows_lbl->setStyleSheet(
        QString("color:%1; font-size:%2px; font-family:%3; background:transparent;")
            .arg(colors::TEXT_DIM())
            .arg(fonts::TINY)
            .arg(fonts::DATA_FAMILY));
    layout->addWidget(rows_lbl);

    update_controls();
}

void PaginationBar::set_total(int total_rows) {
    total_rows_ = total_rows;
    current_page_ = 1;
    update_controls();
}

int PaginationBar::total_pages() const {
    if (total_rows_ <= 0 || page_size_ <= 0) return 1;
    return (total_rows_ + page_size_ - 1) / page_size_;
}

void PaginationBar::update_controls() {
    const int tp = total_pages();
    current_page_ = std::clamp(current_page_, 1, tp);

    const int start = offset() + 1;
    const int end = std::min(offset() + page_size_, total_rows_);

    status_label_->setText(
        total_rows_ > 0 ? QString("%1-%2 of %3").arg(start).arg(end).arg(total_rows_)
                        : QStringLiteral("0 rows"));
    page_label_->setText(QString("Page %1 / %2").arg(current_page_).arg(tp));

    btn_first_->setEnabled(current_page_ > 1);
    btn_prev_->setEnabled(current_page_ > 1);
    btn_next_->setEnabled(current_page_ < tp);
    btn_last_->setEnabled(current_page_ < tp);

    setVisible(total_rows_ > page_size_);
}

} // namespace fincept::ui
