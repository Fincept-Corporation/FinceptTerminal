#include "screens/fno/TemplatePickerPanel.h"

#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>
#include <QVBoxLayout>

namespace fincept::screens::fno {

using fincept::services::options::StrategyCategory;
using fincept::services::options::StrategyInstantiationOptions;
using fincept::services::options::StrategyTemplate;
using fincept::services::options::catalog;
using fincept::services::options::find;
using namespace fincept::ui;

namespace {

struct CatDef {
    StrategyCategory cat;
    const char* label;
};

constexpr CatDef kCats[5] = {
    {StrategyCategory::Bullish, "BULLISH"},
    {StrategyCategory::Bearish, "BEARISH"},
    {StrategyCategory::Neutral, "NEUTRAL"},
    {StrategyCategory::Volatility, "VOLATILE"},
    {StrategyCategory::Others, "OTHERS"},
};

}  // namespace

TemplatePickerPanel::TemplatePickerPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("fnoPickerPanel");
    setStyleSheet(QStringLiteral(
        "#fnoPickerPanel { background:%1; }"
        "#fnoPickerCatBtn { background:%2; color:%3; border:none; padding:5px 12px; "
        "                    font-size:9px; font-weight:700; letter-spacing:0.5px; }"
        "#fnoPickerCatBtn:hover { background:%4; color:%5; }"
        "#fnoPickerCatBtn[active=\"true\"] { background:%6; color:%2; }"
        "#fnoPickerList { background:%1; color:%5; border:1px solid %7; }"
        "#fnoPickerList::item { padding:6px 8px; border-bottom:1px solid %7; }"
        "#fnoPickerList::item:selected { background:%4; color:%5; }"
        "#fnoPickerList::item:hover { background:%4; }"
        "#fnoUseBtn { background:%6; color:%2; border:none; padding:6px 16px; "
        "             font-size:10px; font-weight:700; letter-spacing:0.5px; }"
        "#fnoUseBtn:hover { background:%5; color:%2; }"
        "#fnoUseBtn:disabled { background:%7; color:%3; }"
        "#fnoPickerLabel { color:%3; font-size:9px; font-weight:700; "
        "                  letter-spacing:0.4px; background:transparent; }"
        "QSpinBox { background:%2; color:%5; border:1px solid %7; padding:2px 4px; "
        "           font-size:11px; min-width:48px; }")
                      .arg(colors::BG_BASE(),     // %1 panel bg
                           colors::BG_RAISED(),   // %2 button bg
                           colors::TEXT_SECONDARY(), // %3 dim text
                           colors::BG_HOVER(),    // %4 hover
                           colors::TEXT_PRIMARY(), // %5 primary text
                           colors::AMBER(),       // %6 active
                           colors::BORDER_DIM())); // %7 border

    setup_ui();
    rebuild_list_for_category(active_category_);
}

void TemplatePickerPanel::setup_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    // ── Category strip ────────────────────────────────────────────────────
    auto* cat_row = new QWidget(this);
    auto* cat_lay = new QHBoxLayout(cat_row);
    cat_lay->setContentsMargins(0, 0, 0, 0);
    cat_lay->setSpacing(2);
    for (int i = 0; i < 5; ++i) {
        auto* btn = new QPushButton(QString::fromLatin1(kCats[i].label), cat_row);
        btn->setObjectName("fnoPickerCatBtn");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("active", kCats[i].cat == active_category_);
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_category_changed(i); });
        category_btns_.append(btn);
        cat_lay->addWidget(btn);
    }
    cat_lay->addStretch(1);
    root->addWidget(cat_row);

    // ── Template list ─────────────────────────────────────────────────────
    list_ = new QListWidget(this);
    list_->setObjectName("fnoPickerList");
    list_->setWordWrap(true);
    connect(list_, &QListWidget::itemSelectionChanged, this, &TemplatePickerPanel::on_template_selected);
    root->addWidget(list_, 1);

    // ── Width / Shift / Lots / USE ────────────────────────────────────────
    auto* ctrl_row = new QWidget(this);
    auto* ctrl_lay = new QHBoxLayout(ctrl_row);
    ctrl_lay->setContentsMargins(0, 0, 0, 0);
    ctrl_lay->setSpacing(8);

    auto add_kv = [&](const QString& label, QSpinBox*& spin, int min_v, int max_v, int default_v) {
        auto* l = new QLabel(label, ctrl_row);
        l->setObjectName("fnoPickerLabel");
        spin = new QSpinBox(ctrl_row);
        spin->setRange(min_v, max_v);
        spin->setValue(default_v);
        ctrl_lay->addWidget(l);
        ctrl_lay->addWidget(spin);
    };
    add_kv("WIDTH", width_spin_, 1, 10, 1);
    add_kv("SHIFT", shift_spin_, -10, 10, 0);
    add_kv("LOTS", lots_spin_, 1, 100, 1);

    ctrl_lay->addStretch(1);
    use_btn_ = new QPushButton("USE", ctrl_row);
    use_btn_->setObjectName("fnoUseBtn");
    use_btn_->setCursor(Qt::PointingHandCursor);
    use_btn_->setEnabled(false);
    connect(use_btn_, &QPushButton::clicked, this, &TemplatePickerPanel::on_use_clicked);
    ctrl_lay->addWidget(use_btn_);
    root->addWidget(ctrl_row);
}

void TemplatePickerPanel::on_category_changed(int index) {
    if (index < 0 || index >= 5)
        return;
    active_category_ = kCats[index].cat;
    for (int i = 0; i < category_btns_.size(); ++i) {
        QPushButton* b = category_btns_.at(i);
        b->setProperty("active", i == index);
        b->style()->unpolish(b);
        b->style()->polish(b);
    }
    rebuild_list_for_category(active_category_);
}

void TemplatePickerPanel::rebuild_list_for_category(StrategyCategory cat) {
    list_->clear();
    for (const auto& t : catalog()) {
        if (t.category != cat)
            continue;
        auto* item = new QListWidgetItem(t.name + "  —  " + t.outlook);
        item->setData(Qt::UserRole, t.id);
        item->setToolTip(t.description);
        list_->addItem(item);
    }
    selected_ = nullptr;
    use_btn_->setEnabled(false);
}

void TemplatePickerPanel::on_template_selected() {
    auto* item = list_->currentItem();
    if (!item) {
        selected_ = nullptr;
        use_btn_->setEnabled(false);
        return;
    }
    const QString id = item->data(Qt::UserRole).toString();
    // Fully qualify — `find` is a free function in fincept::services::options,
    // but inside a QWidget subclass the unqualified name resolves to
    // QWidget::find(WId) first.
    selected_ = fincept::services::options::find(id);
    if (selected_)
        apply_template_to_controls(*selected_);
    use_btn_->setEnabled(selected_ != nullptr);
}

void TemplatePickerPanel::apply_template_to_controls(const StrategyTemplate& tpl) {
    width_spin_->setValue(tpl.default_width > 0 ? tpl.default_width : 1);
    width_spin_->setEnabled(tpl.supports_width);
    shift_spin_->setValue(0);
    // lots_spin_ stays at user's last value
}

void TemplatePickerPanel::on_use_clicked() {
    if (!selected_)
        return;
    StrategyInstantiationOptions opts;
    opts.width = width_spin_->value();
    opts.shift = shift_spin_->value();
    opts.default_lots = lots_spin_->value();
    emit template_chosen(selected_->id, opts);
}

} // namespace fincept::screens::fno
