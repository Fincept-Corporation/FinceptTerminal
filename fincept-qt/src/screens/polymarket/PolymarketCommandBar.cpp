#include "screens/polymarket/PolymarketCommandBar.h"

#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

namespace fincept::screens::polymarket {

using namespace fincept::ui;

static const QStringList VIEW_NAMES = {"TRENDING", "MARKETS", "EVENTS", "SPORTS", "RESOLVED"};
static const QStringList SORT_KEYS = {"volume", "liquidity", "startDate"};

static const QString kBarStyle =
    QStringLiteral("#polyCommandBar { background: %1; border-bottom: 2px solid %2; }"
                   "#polyCmdTitle { color: %3; font-size: 12px; font-weight: 700; background: transparent; }"
                   "#polyViewBtn { background: transparent; color: %4; border: 1px solid %5; "
                   "  font-size: 9px; font-weight: 700; padding: 4px 12px; }"
                   "#polyViewBtn:hover { color: %3; }"
                   "#polyViewBtn[active=\"true\"] { background: %2; color: %6; border-color: %2; }"
                   "#polyCatBtn { background: transparent; color: %4; border: 1px solid %5; "
                   "  font-size: 8px; font-weight: 600; padding: 2px 8px; }"
                   "#polyCatBtn:hover { color: %3; }"
                   "#polyCatBtn[active=\"true\"] { background: rgba(217,119,6,0.15); color: %2; border-color: %2; }"
                   "#polySearchInput { background: %6; color: %3; border: 1px solid %5; "
                   "  padding: 4px 8px; font-size: 11px; }"
                   "#polySearchInput:focus { border-color: %7; }"
                   "#polyRefreshBtn { background: %8; color: %4; border: 1px solid %5; "
                   "  padding: 4px 10px; font-size: 9px; font-weight: 700; }"
                   "#polyRefreshBtn:hover { color: %3; }"
                   "#polyWsIndicator { font-size: 8px; font-weight: 700; padding: 2px 6px; }"
                   "#polyCountLabel { color: %4; font-size: 9px; background: transparent; }")
        .arg(colors::BG_RAISED())      // %1
        .arg(colors::AMBER())          // %2
        .arg(colors::TEXT_PRIMARY())   // %3
        .arg(colors::TEXT_SECONDARY()) // %4
        .arg(colors::BORDER_DIM())     // %5
        .arg(colors::BG_BASE())        // %6
        .arg(colors::BORDER_BRIGHT())  // %7
        .arg(colors::BG_SURFACE());    // %8

PolymarketCommandBar::PolymarketCommandBar(QWidget* parent) : QWidget(parent) {
    setObjectName("polyCommandBar");
    setStyleSheet(kBarStyle);
    setFixedHeight(72);
    build_ui();
}

void PolymarketCommandBar::build_ui() {
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(16, 6, 16, 6);
    vl->setSpacing(4);

    // Row 1: title + refresh + WS status
    auto* row1 = new QHBoxLayout;
    row1->setSpacing(8);
    auto* title = new QLabel("POLYMARKET PREDICTION MARKETS");
    title->setObjectName("polyCmdTitle");
    row1->addWidget(title);
    row1->addStretch(1);

    count_label_ = new QLabel;
    count_label_->setObjectName("polyCountLabel");
    row1->addWidget(count_label_);

    refresh_btn_ = new QPushButton("REFRESH");
    refresh_btn_->setObjectName("polyRefreshBtn");
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    connect(refresh_btn_, &QPushButton::clicked, this, &PolymarketCommandBar::refresh_clicked);
    row1->addWidget(refresh_btn_);

    ws_indicator_ = new QLabel("DISCONNECTED");
    ws_indicator_->setObjectName("polyWsIndicator");
    set_ws_status(false);
    row1->addWidget(ws_indicator_);
    vl->addLayout(row1);

    // Row 2: view pills + categories + search + sort
    auto* row2 = new QHBoxLayout;
    row2->setSpacing(4);

    for (int i = 0; i < VIEW_NAMES.size(); ++i) {
        auto* btn = new QPushButton(VIEW_NAMES[i]);
        btn->setObjectName("polyViewBtn");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("active", VIEW_NAMES[i] == active_view_);
        connect(btn, &QPushButton::clicked, this, [this, i]() { emit view_changed(VIEW_NAMES[i]); });
        row2->addWidget(btn);
        view_btns_.append(btn);
    }

    row2->addSpacing(8);

    category_container_ = new QWidget(this);
    auto* ccl = new QHBoxLayout(category_container_);
    ccl->setContentsMargins(0, 0, 0, 0);
    ccl->setSpacing(2);
    // Categories populated dynamically via set_categories()
    row2->addWidget(category_container_);

    row2->addSpacing(8);

    search_input_ = new QLineEdit;
    search_input_->setObjectName("polySearchInput");
    search_input_->setPlaceholderText("SEARCH...");
    search_input_->setFixedWidth(180);
    connect(search_input_, &QLineEdit::returnPressed, this,
            [this]() { emit search_submitted(search_input_->text().trimmed()); });
    row2->addWidget(search_input_);

    sort_combo_ = new QComboBox;
    sort_combo_->addItems({"VOLUME", "LIQUIDITY", "RECENT"});
    sort_combo_->setFixedWidth(90);
    connect(sort_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (idx >= 0 && idx < SORT_KEYS.size())
            emit sort_changed(SORT_KEYS[idx]);
    });
    row2->addWidget(sort_combo_);

    row2->addStretch(1);
    vl->addLayout(row2);
}

void PolymarketCommandBar::set_categories(const QVector<services::polymarket::Tag>& tags) {
    auto* layout = category_container_->layout();
    while (layout->count() > 0) {
        auto* item = layout->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    // "ALL" button
    auto* all_btn = new QPushButton("ALL");
    all_btn->setObjectName("polyCatBtn");
    all_btn->setCursor(Qt::PointingHandCursor);
    all_btn->setProperty("active", active_category_ == "ALL");
    connect(all_btn, &QPushButton::clicked, this, [this]() { emit category_changed("ALL"); });
    layout->addWidget(all_btn);

    for (const auto& tag : tags) {
        auto* btn = new QPushButton(tag.label.toUpper());
        btn->setObjectName("polyCatBtn");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("active", active_category_ == tag.slug);
        connect(btn, &QPushButton::clicked, this, [this, slug = tag.slug]() { emit category_changed(slug); });
        layout->addWidget(btn);
    }
}

void PolymarketCommandBar::set_active_view(const QString& view) {
    active_view_ = view;
    for (int i = 0; i < view_btns_.size(); ++i) {
        view_btns_[i]->setProperty("active", VIEW_NAMES[i] == view);
        view_btns_[i]->style()->unpolish(view_btns_[i]);
        view_btns_[i]->style()->polish(view_btns_[i]);
    }
}

void PolymarketCommandBar::set_active_category(const QString& cat) {
    active_category_ = cat;
    auto* layout = category_container_->layout();
    for (int i = 0; i < layout->count(); ++i) {
        auto* w = qobject_cast<QPushButton*>(layout->itemAt(i)->widget());
        if (!w)
            continue;
        bool is_active = (cat == "ALL" && i == 0) || (cat != "ALL" && w->text() == cat.toUpper());
        w->setProperty("active", is_active);
        w->style()->unpolish(w);
        w->style()->polish(w);
    }
}

void PolymarketCommandBar::set_loading(bool loading) {
    refresh_btn_->setEnabled(!loading);
    refresh_btn_->setText(loading ? "LOADING..." : "REFRESH");
}

void PolymarketCommandBar::set_ws_status(bool connected) {
    if (connected) {
        ws_indicator_->setText("LIVE");
        ws_indicator_->setStyleSheet(QString("color: %1; background: rgba(22,163,74,0.2); "
                                             "font-size: 8px; font-weight: 700; padding: 2px 6px;")
                                         .arg(colors::POSITIVE()));
    } else {
        ws_indicator_->setText("OFFLINE");
        ws_indicator_->setStyleSheet(QString("color: %1; background: transparent; "
                                             "font-size: 8px; font-weight: 700; padding: 2px 6px;")
                                         .arg(colors::TEXT_DIM()));
    }
}

void PolymarketCommandBar::set_market_count(int count) {
    count_label_->setText(QString::number(count) + " markets");
}

} // namespace fincept::screens::polymarket
