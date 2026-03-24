// src/screens/ai_quant_lab/AIQuantLabScreen.cpp
#include "screens/ai_quant_lab/AIQuantLabScreen.h"

#include "core/logging/Logger.h"
#include "screens/ai_quant_lab/QuantModulePanel.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QScrollArea>

namespace fincept::screens {

using namespace fincept::services::quant;

AIQuantLabScreen::AIQuantLabScreen(QWidget* parent) : QWidget(parent) {
    modules_ = all_quant_modules();
    build_ui();
    LOG_INFO("AIQuantLab", "Screen constructed");
}

void AIQuantLabScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (first_show_) {
        first_show_ = false;
        on_module_selected(0);
    }
}

void AIQuantLabScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
}

void AIQuantLabScreen::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(build_top_bar());

    auto* body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);

    body->addWidget(build_left_sidebar());

    content_stack_ = new QStackedWidget(this);
    for (int i = 0; i < modules_.size(); ++i)
        content_stack_->addWidget(new QuantModulePanel(modules_[i], this));
    body->addWidget(content_stack_, 1);

    body->addWidget(build_right_sidebar());

    auto* body_w = new QWidget(this);
    body_w->setLayout(body);
    root->addWidget(body_w, 1);

    root->addWidget(build_status_bar());
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));
}

QWidget* AIQuantLabScreen::build_top_bar() {
    auto* bar = new QWidget(this);
    bar->setFixedHeight(44);
    bar->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM));

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(12);

    auto* brand = new QLabel("AI QUANT LAB", bar);
    brand->setStyleSheet(QString("color:#9D4EDD; font-size:%1px; font-weight:700; font-family:%2;"
                                 "padding:4px 12px; background:rgba(157,78,221,0.08);"
                                 "border:1px solid rgba(157,78,221,0.25); border-radius:2px;")
                             .arg(ui::fonts::TINY)
                             .arg(ui::fonts::DATA_FAMILY));
    hl->addWidget(brand);

    auto* div = new QWidget(bar);
    div->setFixedSize(1, 20);
    div->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_DIM));
    hl->addWidget(div);

    // Quick module buttons (show short labels)
    for (int i = 0; i < modules_.size(); ++i) {
        const auto& mod = modules_[i];
        auto* btn = new QPushButton(mod.short_label, bar);
        btn->setToolTip(mod.label);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            QString("QPushButton { color:%1; font-size:8px; font-family:%2;"
                    "padding:3px 6px; border:1px solid transparent; border-radius:2px;"
                    "background:transparent; }"
                    "QPushButton:hover { background:rgba(%3,0.08); color:%4; }")
                .arg(ui::colors::TEXT_TERTIARY)
                .arg(ui::fonts::DATA_FAMILY)
                .arg(QString("%1,%2,%3").arg(mod.color.red()).arg(mod.color.green()).arg(mod.color.blue()))
                .arg(mod.color.name()));
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_module_selected(i); });
        hl->addWidget(btn);
        quick_buttons_.append(btn);
    }

    hl->addStretch(1);

    auto* ready = new QLabel("18 MODULES", bar);
    ready->setStyleSheet(QString("color:%1; font-size:9px; font-family:%2; padding:3px 8px;"
                                 "background:rgba(22,163,74,0.08); border:1px solid rgba(22,163,74,0.25);"
                                 "border-radius:2px; font-weight:700;")
                             .arg(ui::colors::POSITIVE)
                             .arg(ui::fonts::DATA_FAMILY));
    hl->addWidget(ready);

    return bar;
}

QWidget* AIQuantLabScreen::build_left_sidebar() {
    auto* panel = new QWidget(this);
    panel->setFixedWidth(200);
    panel->setStyleSheet(
        QString("background:%1; border-right:1px solid %2;").arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* header = new QWidget(panel);
    header->setFixedHeight(36);
    header->setStyleSheet(QString("border-bottom:1px solid %1;").arg(ui::colors::BORDER_DIM));
    auto* hhl = new QHBoxLayout(header);
    hhl->setContentsMargins(12, 0, 12, 0);
    auto* title = new QLabel("MODULES", header);
    title->setStyleSheet(QString("color:#9D4EDD; font-size:9px; font-weight:700; font-family:%1; letter-spacing:1px;")
                             .arg(ui::fonts::DATA_FAMILY));
    hhl->addWidget(title);
    hhl->addStretch();
    vl->addWidget(header);

    auto* scroll = new QScrollArea(panel);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet("QScrollArea { border:none; background:transparent; }");

    auto* list = new QWidget(scroll);
    left_items_layout_ = new QVBoxLayout(list);
    left_items_layout_->setContentsMargins(0, 8, 0, 8);
    left_items_layout_->setSpacing(0);

    QString last_cat;
    for (int i = 0; i < modules_.size(); ++i) {
        const auto& mod = modules_[i];
        if (mod.category != last_cat) {
            last_cat = mod.category;
            QString cat_label = mod.category;
            cat_label.replace('_', '/');
            auto* cat = new QLabel(cat_label, list);
            cat->setStyleSheet(QString("color:%1; font-size:8px; font-weight:700; font-family:%2;"
                                       "padding:8px 12px 4px 12px; letter-spacing:1px;")
                                   .arg(ui::colors::TEXT_TERTIARY)
                                   .arg(ui::fonts::DATA_FAMILY));
            left_items_layout_->addWidget(cat);
        }

        auto* btn = new QPushButton(mod.label, list);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            QString("QPushButton { text-align:left; padding:6px 12px; border:none;"
                    "border-left:2px solid transparent; color:%1;"
                    "font-size:10px; font-family:%2; background:transparent; }"
                    "QPushButton:hover { background:rgba(%3,0.06); color:%4; }")
                .arg(ui::colors::TEXT_SECONDARY)
                .arg(ui::fonts::DATA_FAMILY)
                .arg(QString("%1,%2,%3").arg(mod.color.red()).arg(mod.color.green()).arg(mod.color.blue()))
                .arg(mod.color.name()));
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_module_selected(i); });
        left_items_layout_->addWidget(btn);
        module_buttons_.append(btn);
    }
    left_items_layout_->addStretch();
    scroll->setWidget(list);
    vl->addWidget(scroll, 1);

    return panel;
}

QWidget* AIQuantLabScreen::build_right_sidebar() {
    auto* panel = new QWidget(this);
    panel->setFixedWidth(220);
    panel->setStyleSheet(
        QString("background:%1; border-left:1px solid %2;").arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(12, 12, 12, 12);
    vl->setSpacing(10);

    auto* title = new QLabel("MODULE INFO", panel);
    title->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; font-family:%2; letter-spacing:1px;")
                             .arg(ui::colors::CYAN)
                             .arg(ui::fonts::DATA_FAMILY));
    vl->addWidget(title);

    right_title_ = new QLabel(panel);
    right_title_->setStyleSheet(
        QString("font-size:11px; font-weight:700; font-family:%1;").arg(ui::fonts::DATA_FAMILY));
    vl->addWidget(right_title_);

    right_category_ = new QLabel(panel);
    right_category_->setStyleSheet(QString("color:%1; font-size:9px; font-family:%2;")
                                       .arg(ui::colors::TEXT_SECONDARY)
                                       .arg(ui::fonts::DATA_FAMILY));
    vl->addWidget(right_category_);

    right_desc_ = new QLabel(panel);
    right_desc_->setWordWrap(true);
    right_desc_->setStyleSheet(QString("color:%1; font-size:9px; font-family:%2; line-height:1.5;")
                                   .arg(ui::colors::TEXT_PRIMARY)
                                   .arg(ui::fonts::DATA_FAMILY));
    vl->addWidget(right_desc_);

    right_script_ = new QLabel(panel);
    right_script_->setStyleSheet(QString("color:%1; font-size:8px; font-family:%2; padding:6px;"
                                         "background:%3; border:1px solid %4; border-radius:2px;")
                                     .arg(ui::colors::TEXT_TERTIARY)
                                     .arg(ui::fonts::DATA_FAMILY)
                                     .arg(ui::colors::BG_RAISED)
                                     .arg(ui::colors::BORDER_DIM));
    vl->addWidget(right_script_);

    // Quick stats
    auto* stats = new QWidget(panel);
    stats->setStyleSheet(QString("background:%1; border:1px solid %2; border-radius:2px; padding:10px;")
                             .arg(ui::colors::BG_RAISED)
                             .arg(ui::colors::BORDER_DIM));
    auto* svl = new QVBoxLayout(stats);
    svl->setContentsMargins(10, 10, 10, 10);
    svl->setSpacing(4);
    auto* st = new QLabel("PLATFORM STATS", stats);
    st->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; font-family:%2;")
                          .arg(ui::colors::CYAN)
                          .arg(ui::fonts::DATA_FAMILY));
    svl->addWidget(st);

    auto add_row = [&](const QString& l, const QString& v) {
        auto* r = new QWidget(stats);
        auto* rl = new QHBoxLayout(r);
        rl->setContentsMargins(0, 0, 0, 0);
        auto* lbl = new QLabel(l, r);
        lbl->setStyleSheet(QString("color:%1; font-size:9px; font-family:%2;")
                               .arg(ui::colors::TEXT_TERTIARY)
                               .arg(ui::fonts::DATA_FAMILY));
        auto* val = new QLabel(v, r);
        val->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; font-family:%2;")
                               .arg(ui::colors::TEXT_PRIMARY)
                               .arg(ui::fonts::DATA_FAMILY));
        rl->addWidget(lbl);
        rl->addStretch();
        rl->addWidget(val);
        svl->addWidget(r);
    };
    add_row("Modules", "18");
    add_row("ML Models", "30+");
    add_row("RL Algorithms", "5");
    add_row("Python Scripts", "25+");
    vl->addWidget(stats);

    vl->addStretch();
    return panel;
}

QWidget* AIQuantLabScreen::build_status_bar() {
    auto* bar = new QWidget(this);
    bar->setFixedHeight(24);
    bar->setStyleSheet(
        QString("background:%1; border-top:1px solid %2;").arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM));
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(16);
    auto s =
        QString("color:%1; font-size:8px; font-family:%2;").arg(ui::colors::TEXT_TERTIARY).arg(ui::fonts::DATA_FAMILY);
    auto* l1 = new QLabel("ENGINE:", bar);
    l1->setStyleSheet(s);
    auto* v1 = new QLabel("QLIB + GS QUANT + PYTHON", bar);
    v1->setStyleSheet(QString("color:%1; font-size:8px; font-weight:700; font-family:%2;")
                          .arg(ui::colors::POSITIVE)
                          .arg(ui::fonts::DATA_FAMILY));
    hl->addWidget(l1);
    hl->addWidget(v1);
    hl->addStretch();
    auto* v2 = new QLabel("READY", bar);
    v2->setStyleSheet(QString("color:%1; font-size:8px; font-weight:700; font-family:%2;")
                          .arg(ui::colors::POSITIVE)
                          .arg(ui::fonts::DATA_FAMILY));
    hl->addWidget(v2);
    return bar;
}

void AIQuantLabScreen::on_module_selected(int index) {
    if (index < 0 || index >= modules_.size())
        return;
    active_index_ = index;
    content_stack_->setCurrentIndex(index);
    update_sidebar_selection();
    update_right_panel();
}

void AIQuantLabScreen::update_sidebar_selection() {
    for (int i = 0; i < module_buttons_.size(); ++i) {
        const auto& mod = modules_[i];
        bool active = (i == active_index_);
        module_buttons_[i]->setStyleSheet(
            QString("QPushButton { text-align:left; padding:6px 12px; border:none;"
                    "border-left:2px solid %1; color:%2;"
                    "font-size:10px; font-family:%3; font-weight:%4; background:%5; }"
                    "QPushButton:hover { background:rgba(%6,0.1); color:%7; }")
                .arg(active ? mod.color.name() : "transparent")
                .arg(active ? mod.color.name() : ui::colors::TEXT_SECONDARY)
                .arg(ui::fonts::DATA_FAMILY)
                .arg(active ? "700" : "400")
                .arg(active ? QString("rgba(%1,%2,%3,0.08)")
                                  .arg(mod.color.red())
                                  .arg(mod.color.green())
                                  .arg(mod.color.blue())
                            : "transparent")
                .arg(QString("%1,%2,%3").arg(mod.color.red()).arg(mod.color.green()).arg(mod.color.blue()))
                .arg(mod.color.name()));
    }
    for (int i = 0; i < quick_buttons_.size(); ++i) {
        const auto& mod = modules_[i];
        bool active = (i == active_index_);
        quick_buttons_[i]->setStyleSheet(QString("QPushButton { color:%1; font-size:8px; font-family:%2;"
                                                 "padding:3px 6px; border:1px solid %3; border-radius:2px;"
                                                 "background:%4; font-weight:%5; }")
                                             .arg(active ? mod.color.name() : ui::colors::TEXT_TERTIARY)
                                             .arg(ui::fonts::DATA_FAMILY)
                                             .arg(active ? QString("rgba(%1,%2,%3,0.3)")
                                                               .arg(mod.color.red())
                                                               .arg(mod.color.green())
                                                               .arg(mod.color.blue())
                                                         : "transparent")
                                             .arg(active ? QString("rgba(%1,%2,%3,0.12)")
                                                               .arg(mod.color.red())
                                                               .arg(mod.color.green())
                                                               .arg(mod.color.blue())
                                                         : "transparent")
                                             .arg(active ? "700" : "400"));
    }
}

void AIQuantLabScreen::update_right_panel() {
    const auto& mod = modules_[active_index_];
    right_title_->setText(mod.label.toUpper());
    right_title_->setStyleSheet(QString("color:%1; font-size:11px; font-weight:700; font-family:%2;")
                                    .arg(mod.color.name())
                                    .arg(ui::fonts::DATA_FAMILY));
    right_category_->setText(QString(mod.category).replace('_', '/') + " module");
    right_desc_->setText(mod.description);
    right_script_->setText("Script: " + mod.script);
}

} // namespace fincept::screens
