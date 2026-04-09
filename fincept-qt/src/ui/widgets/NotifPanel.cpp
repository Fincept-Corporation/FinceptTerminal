#include "ui/widgets/NotifPanel.h"

#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QVBoxLayout>

namespace fincept::ui {

using namespace fincept::notifications;

NotifPanel::NotifPanel(QWidget* parent) : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint) {
    setFixedSize(PANEL_W, PANEL_H);
    setStyleSheet(QString("QFrame { background: %1; border: 1px solid %2; border-radius: 4px; }")
                      .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_MED()));

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // ── Header ────────────────────────────────────────────────────────────────
    auto* header = new QWidget;
    header->setFixedHeight(36);
    header->setStyleSheet(
        QString("background: %1; border-bottom: 1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_MED()));

    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(12, 0, 8, 0);

    auto* title_lbl = new QLabel("NOTIFICATIONS");
    title_lbl->setStyleSheet(
        QString("color: %1; font-size: 11px; font-weight: bold; background: transparent;").arg(ui::colors::AMBER()));
    hl->addWidget(title_lbl);
    hl->addStretch();

    auto* mark_all_btn = new QPushButton("Mark all read");
    mark_all_btn->setFlat(true);
    mark_all_btn->setStyleSheet(
        QString("QPushButton { color: %1; background: transparent; border: none; font-size: 11px; }"
                "QPushButton:hover { color: %2; }")
            .arg(ui::colors::TEXT_SECONDARY(), ui::colors::AMBER()));
    connect(mark_all_btn, &QPushButton::clicked, this, [this]() {
        NotificationService::instance().mark_all_read();
        refresh();
    });
    hl->addWidget(mark_all_btn);
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
        auto* empty = new QLabel("No notifications");
        empty->setAlignment(Qt::AlignCenter);
        empty->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent; padding: 24px;")
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
                    return "#F59E0B";
                case NotifLevel::Alert:
                    return "#F97316";
                case NotifLevel::Critical:
                    return "#EF4444";
                default:
                    return "#06B6D4";
            }
        }();

        auto* row = new QWidget;
        row->setFixedHeight(52);
        row->setStyleSheet(QString("QWidget { background: %1; border-bottom: 1px solid %2; }"
                                   "QWidget:hover { background: %3; }")
                               .arg(rec.read ? ui::colors::BG_SURFACE() : ui::colors::BG_RAISED(),
                                    ui::colors::BORDER_DIM(), ui::colors::BG_HOVER()));

        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(12, 6, 12, 6);
        rl->setSpacing(8);

        auto* dot = new QLabel("●");
        dot->setFixedWidth(10);
        dot->setStyleSheet(QString("color: %1; font-size: 9px; background: transparent;").arg(dot_color));
        rl->addWidget(dot, 0, Qt::AlignTop);

        auto* text_col = new QWidget;
        text_col->setStyleSheet("background: transparent;");
        auto* tvl = new QVBoxLayout(text_col);
        tvl->setContentsMargins(0, 0, 0, 0);
        tvl->setSpacing(1);

        auto* title_lbl = new QLabel(rec.request.title.left(45));
        title_lbl->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: %2; background: transparent;")
                                     .arg(ui::colors::TEXT_PRIMARY(), rec.read ? "normal" : "bold"));
        tvl->addWidget(title_lbl);

        auto* msg_lbl = new QLabel(rec.request.message.left(60));
        msg_lbl->setStyleSheet(
            QString("color: %1; font-size: 11px; background: transparent;").arg(ui::colors::TEXT_SECONDARY()));
        tvl->addWidget(msg_lbl);
        rl->addWidget(text_col, 1);

        auto* time_lbl = new QLabel(rec.received_at.toString("hh:mm:ss"));
        time_lbl->setStyleSheet(
            QString("color: %1; font-size: 10px; background: transparent;").arg(ui::colors::TEXT_TERTIARY()));
        time_lbl->setAlignment(Qt::AlignTop | Qt::AlignRight);
        rl->addWidget(time_lbl);

        list_layout_->insertWidget(list_layout_->count() - 1, row);
    }
}

void NotifPanel::anchor_to(QWidget* ref_widget) {
    if (!ref_widget || !ref_widget->parentWidget())
        return;
    const QPoint global_pos = ref_widget->mapToGlobal(QPoint(0, 0));
    const int x = global_pos.x() + ref_widget->width() - PANEL_W;
    const int y = global_pos.y() - PANEL_H;
    move(x, y);
}

} // namespace fincept::ui
