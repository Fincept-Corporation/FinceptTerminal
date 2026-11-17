#include "ui/widgets/NotifPanel.h"

#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QVBoxLayout>

namespace fincept::ui {

using namespace fincept::notifications;

NotifPanel::NotifPanel(QWidget* parent) : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint) {
    apply_size();
    setStyleSheet(QString("QFrame { background: %1; border: 1px solid %2; border-radius: 4px; }")
                      .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_MED()));

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // ── Header ────────────────────────────────────────────────────────────────
    auto* header = new QWidget;
    header->setMinimumHeight(36);
    header->setStyleSheet(
        QString("background: %1; border-bottom: 1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_MED()));

    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(12, 0, 8, 0);

    title_lbl_ = new QLabel(tr("NOTIFICATIONS"));
    title_lbl_->setStyleSheet(
        QString("color: %1; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: bold; background: transparent;").arg(ui::colors::AMBER()));
    hl->addWidget(title_lbl_);
    hl->addStretch();

    mark_all_btn_ = new QPushButton(tr("Mark all read"));
    mark_all_btn_->setFlat(true);
    mark_all_btn_->setStyleSheet(
        QString("QPushButton { color: %1; background: transparent; border: none; font-size: " + fincept::ui::fonts::px(-3) + "px; }"
                "QPushButton:hover { color: %2; }")
            .arg(ui::colors::TEXT_SECONDARY(), ui::colors::AMBER()));
    connect(mark_all_btn_, &QPushButton::clicked, this, [this]() {
        NotificationService::instance().mark_all_read();
        refresh();
    });
    hl->addWidget(mark_all_btn_);
    vl->addWidget(header);

    // ── Scroll area ───────────────────────────────────────────────────────────
    scroll_ = new QScrollArea;
    scroll_->setWidgetResizable(true);
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setStyleSheet(QString("QScrollArea { background: transparent; border: none; }"
                                   "QScrollBar:vertical { background: %1; width: 4px; }"
                                   "QScrollBar::handle:vertical { background: %2; }")
                               .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_MED()));

    list_container_ = new QWidget;
    list_container_->setStyleSheet("background: transparent;");
    list_layout_ = new QVBoxLayout(list_container_);
    list_layout_->setContentsMargins(0, 0, 0, 0);
    list_layout_->setSpacing(0);
    list_layout_->addStretch();

    scroll_->setWidget(list_container_);
    vl->addWidget(scroll_, 1);
}

void NotifPanel::showEvent(QShowEvent* e) {
    QFrame::showEvent(e);
    refresh();
}

void NotifPanel::refresh() {
    // Clear existing rows (keep the stretch at end)
    while (list_layout_->count() > 1) {
        auto* item = list_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    const auto& hist = NotificationService::instance().history();
    if (hist.isEmpty()) {
        auto* empty = new QLabel(tr("No notifications"));
        empty->setAlignment(Qt::AlignCenter);
        empty->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-2) + "px; background: transparent; padding: 24px;")
                                 .arg(ui::colors::TEXT_TERTIARY()));
        list_layout_->insertWidget(0, empty);
        return;
    }

    int count = 0;
    for (const auto& rec : hist) {
        if (++count > 50)
            break;

        const QString dot_color = [&]() -> QString {
            switch (rec.request.level) {
                case NotifLevel::Warning:
                    return ui::colors::WARNING();
                case NotifLevel::Alert:
                    return ui::colors::AMBER();
                case NotifLevel::Critical:
                    return ui::colors::DANGER();
                default:
                    return ui::colors::CYAN();
            }
        }();

        auto* row = new QWidget;
        row->setMinimumHeight(52); // grows with the user's font
        row->setStyleSheet(QString("QWidget { background: %1; border-bottom: 1px solid %2; }"
                                   "QWidget:hover { background: %3; }")
                               .arg(rec.read ? ui::colors::BG_SURFACE() : ui::colors::BG_RAISED(),
                                    ui::colors::BORDER_DIM(), ui::colors::BG_HOVER()));

        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(12, 6, 12, 6);
        rl->setSpacing(8);

        auto* dot = new QLabel("●");
        dot->setFixedWidth(ui::metrics::width(10));
        dot->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-5) + "px; background: transparent;").arg(dot_color));
        rl->addWidget(dot, 0, Qt::AlignTop);

        auto* text_col = new QWidget;
        text_col->setStyleSheet("background: transparent;");
        auto* tvl = new QVBoxLayout(text_col);
        tvl->setContentsMargins(0, 0, 0, 0);
        tvl->setSpacing(1);

        auto* title_lbl = new QLabel(rec.request.title.left(45));
        title_lbl->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: %2; background: transparent;")
                                     .arg(ui::colors::TEXT_PRIMARY(), rec.read ? "normal" : "bold"));
        tvl->addWidget(title_lbl);

        auto* msg_lbl = new QLabel(rec.request.message.left(60));
        msg_lbl->setStyleSheet(
            QString("color: %1; font-size: " + fincept::ui::fonts::px(-3) + "px; background: transparent;").arg(ui::colors::TEXT_SECONDARY()));
        tvl->addWidget(msg_lbl);
        rl->addWidget(text_col, 1);

        auto* time_lbl = new QLabel(rec.received_at.toString("hh:mm:ss"));
        time_lbl->setStyleSheet(
            QString("color: %1; font-size: " + fincept::ui::fonts::px(-4) + "px; background: transparent;").arg(ui::colors::TEXT_SECONDARY()));
        time_lbl->setAlignment(Qt::AlignTop | Qt::AlignRight);
        rl->addWidget(time_lbl);

        list_layout_->insertWidget(list_layout_->count() - 1, row);
    }
}

void NotifPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QFrame::changeEvent(event);
}

void NotifPanel::retranslateUi() {
    if (title_lbl_)
        title_lbl_->setText(tr("NOTIFICATIONS"));
    if (mark_all_btn_)
        mark_all_btn_->setText(tr("Mark all read"));
    // Rebuild the list so the empty-state label (and any future row labels)
    // pick up the new locale.
    refresh();
}

void NotifPanel::apply_size() {
    // Designed at the default font size; widened with the user's font.
    setFixedSize(ui::metrics::width(PANEL_W), ui::metrics::width(PANEL_H));
}

void NotifPanel::anchor_to(QWidget* ref_widget) {
    if (!ref_widget || !ref_widget->parentWidget())
        return;
    apply_size(); // the font may have changed since the last time it was shown
    const QPoint global_pos = ref_widget->mapToGlobal(QPoint(0, 0));
    const int x = global_pos.x() + ref_widget->width() - width();
    const int y = global_pos.y() - height();
    move(x, y);
}

} // namespace fincept::ui
