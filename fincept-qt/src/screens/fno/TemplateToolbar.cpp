#include "screens/fno/TemplateToolbar.h"

#include "services/options/StrategyTemplates.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace fincept::screens::fno {

using fincept::services::options::StrategyCategory;
using fincept::services::options::StrategyInstantiationOptions;
using fincept::services::options::catalog;
using fincept::services::options::strategy_category_str;
using namespace fincept::ui;

TemplateToolbar::TemplateToolbar(QWidget* parent) : QWidget(parent) {
    setObjectName("fnoTemplateToolbar");
    setFixedHeight(28);

    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(8, 0, 8, 0);
    lay->setSpacing(6);

    template_label_ = new QLabel(tr("TEMPLATE"), this);
    template_label_->setObjectName("fnoToolbarLabel");
    lay->addWidget(template_label_);

    template_combo_ = new QComboBox(this);
    template_combo_->setObjectName("fnoToolbarCombo");
    template_combo_->setMinimumWidth(200);

    StrategyCategory prev_cat = StrategyCategory(-1);
    for (const auto& tpl : catalog()) {
        if (tpl.category != prev_cat) {
            if (prev_cat != StrategyCategory(-1))
                template_combo_->insertSeparator(template_combo_->count());
            prev_cat = tpl.category;
        }
        const QString text = QString("%1: %2")
                                 .arg(strategy_category_str(tpl.category), tpl.name);
        template_combo_->addItem(text, tpl.id);
    }
    lay->addWidget(template_combo_);

    width_label_ = new QLabel(tr("W:"), this);
    width_label_->setObjectName("fnoToolbarLabel");
    lay->addWidget(width_label_);
    width_spin_ = new QSpinBox(this);
    width_spin_->setObjectName("fnoToolbarSpin");
    width_spin_->setRange(1, 10);
    width_spin_->setValue(1);
    width_spin_->setFixedWidth(50);
    lay->addWidget(width_spin_);

    shift_label_ = new QLabel(tr("S:"), this);
    shift_label_->setObjectName("fnoToolbarLabel");
    lay->addWidget(shift_label_);
    shift_spin_ = new QSpinBox(this);
    shift_spin_->setObjectName("fnoToolbarSpin");
    shift_spin_->setRange(-20, 20);
    shift_spin_->setValue(0);
    shift_spin_->setFixedWidth(50);
    lay->addWidget(shift_spin_);

    lots_label_ = new QLabel(tr("L:"), this);
    lots_label_->setObjectName("fnoToolbarLabel");
    lay->addWidget(lots_label_);
    lots_spin_ = new QSpinBox(this);
    lots_spin_->setObjectName("fnoToolbarSpin");
    lots_spin_->setRange(1, 100);
    lots_spin_->setValue(1);
    lots_spin_->setFixedWidth(50);
    lay->addWidget(lots_spin_);

    lay->addStretch(1);

    add_btn_ = new QPushButton(tr("+ ADD LEG"), this);
    add_btn_->setObjectName("fnoToolbarAccentBtn");
    add_btn_->setCursor(Qt::PointingHandCursor);
    connect(add_btn_, &QPushButton::clicked, this, &TemplateToolbar::add_leg_requested);
    lay->addWidget(add_btn_);

    use_btn_ = new QPushButton(tr("USE"), this);
    use_btn_->setObjectName("fnoToolbarAccentBtn");
    use_btn_->setCursor(Qt::PointingHandCursor);
    connect(use_btn_, &QPushButton::clicked, this, &TemplateToolbar::on_use_clicked);
    lay->addWidget(use_btn_);

    setStyleSheet(QString(
        "#fnoTemplateToolbar { background:%1; border-bottom:1px solid %2; }"
        "#fnoToolbarLabel { color:%3; font-size:9px; font-weight:700; "
        "  letter-spacing:0.6px; background:transparent; }"
        "#fnoToolbarCombo { background:%4; color:%5; border:1px solid %2; "
        "  padding:1px 4px; font-size:11px; min-width:200px; }"
        "#fnoToolbarCombo QAbstractItemView { background:%4; color:%5; "
        "  border:1px solid %2; selection-background-color:%6; }"
        "#fnoToolbarSpin { background:%4; color:%5; border:1px solid %2; "
        "  padding:1px 4px; font-size:11px; }"
        "#fnoToolbarAccentBtn { background:%1; color:%7; border:1px solid %2; "
        "  padding:2px 10px; font-size:10px; font-weight:700; }"
        "#fnoToolbarAccentBtn:hover { background:%6; color:%5; }")
                      .arg(colors::BG_RAISED(),      // %1
                           colors::BORDER_DIM(),      // %2
                           colors::TEXT_SECONDARY(),   // %3
                           colors::BG_BASE(),          // %4
                           colors::TEXT_PRIMARY(),     // %5
                           colors::BG_HOVER(),         // %6
                           colors::AMBER()));          // %7
}

void TemplateToolbar::on_use_clicked() {
    const QString template_id = template_combo_->currentData(Qt::UserRole).toString();
    if (template_id.isEmpty())
        return;
    StrategyInstantiationOptions opts;
    opts.width = width_spin_->value();
    opts.shift = shift_spin_->value();
    opts.default_lots = lots_spin_->value();
    emit template_chosen(template_id, opts);
}

void TemplateToolbar::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void TemplateToolbar::retranslateUi() {
    if (template_label_) template_label_->setText(tr("TEMPLATE"));
    if (width_label_)    width_label_->setText(tr("W:"));
    if (shift_label_)    shift_label_->setText(tr("S:"));
    if (lots_label_)     lots_label_->setText(tr("L:"));
    if (add_btn_)        add_btn_->setText(tr("+ ADD LEG"));
    if (use_btn_)        use_btn_->setText(tr("USE"));
}

} // namespace fincept::screens::fno
