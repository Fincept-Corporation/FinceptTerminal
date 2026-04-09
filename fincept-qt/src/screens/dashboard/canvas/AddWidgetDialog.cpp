#include "screens/dashboard/canvas/AddWidgetDialog.h"

#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QScrollArea>
#include <QSet>
#include <QTimer>
#include <QVBoxLayout>

namespace fincept::screens {

// ── Category accent colors ──────────────────────────────────────────────────

QString AddWidgetDialog::accent_for_category(const QString& category) {
    if (category == "Markets")
        return ui::colors::AMBER();
    if (category == "Research")
        return ui::colors::CYAN();
    if (category == "Portfolio")
        return ui::colors::POSITIVE();
    if (category == "Trading")
        return ui::colors::NEGATIVE();
    if (category == "Tools")
        return ui::colors::AMBER();
    return ui::colors::TEXT_SECONDARY();
}

// ── Per-widget icon glyphs ──────────────────────────────────────────────────

QString AddWidgetDialog::icon_for_widget(const QString& type_id) {
    // Simple UTF-8 glyphs — zero resource cost
    static const QMap<QString, QString> icons = {
        {"indices", QString(QChar(0x25B2))}, // ▲
        {"forex", "$"},
        {"crypto", QString(QChar(0x0E3F))},            // ฿
        {"commodities", QString(QChar(0x2666))},       // ♦
        {"sector_heatmap", QString(QChar(0x2593))},    // ▓
        {"top_movers", QString(QChar(0x21C5))},        // ⇅
        {"sentiment", QString(QChar(0x2665))},         // ♥
        {"news", QString(QChar(0x25A0))},              // ■
        {"stock_quote", QString(QChar(0x24C8))},       // Ⓢ
        {"screener", QString(QChar(0x2318))},          // ⌘
        {"econ_calendar", QString(QChar(0x25CB))},     // ○
        {"watchlist", QString(QChar(0x2606))},         // ☆
        {"performance", QString(QChar(0x2191))},       // ↑
        {"portfolio_summary", QString(QChar(0x25A3))}, // ▣
        {"risk_metrics", QString(QChar(0x26A0))},      // ⚠
        {"quick_trade", QString(QChar(0x26A1))},       // ⚡
        {"video_player", QString(QChar(0x25B6))},      // ▶
    };
    return icons.value(type_id, QString(QChar(0x25CF))); // ● default
}

// ── Constructor ─────────────────────────────────────────────────────────────

AddWidgetDialog::AddWidgetDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Add Widget");
    setFixedSize(620, 520);
    setStyleSheet(QString("QDialog { background: %1; }").arg(ui::colors::BG_BASE()));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 14, 16, 14);
    root->setSpacing(10);

    // ── Title row ──
    auto* title_row = new QHBoxLayout;
    auto* title = new QLabel("ADD WIDGET");
    title->setStyleSheet(
        QString("color: %1; font-size: 12px; font-weight: bold; letter-spacing: 1px;").arg(ui::colors::AMBER()));
    title_row->addWidget(title);
    title_row->addStretch();

    auto* subtitle = new QLabel(QString("%1 AVAILABLE").arg(WidgetRegistry::instance().all().size()));
    subtitle->setStyleSheet(QString("color: %1; font-size: 10px;").arg(ui::colors::TEXT_TERTIARY()));
    title_row->addWidget(subtitle);
    root->addLayout(title_row);

    // ── Search ──
    search_bar_ = new QLineEdit;
    search_bar_->setPlaceholderText("Search widgets...");
    search_bar_->setFixedHeight(30);
    search_bar_->setStyleSheet(
        QString("QLineEdit { background: %1; border: 1px solid %2; color: %3; "
                "padding: 4px 10px; font-size: 11px; border-radius: 2px; }"
                "QLineEdit:focus { border-color: %4; }")
            .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_MED(), ui::colors::TEXT_PRIMARY(), ui::colors::AMBER()));
    connect(search_bar_, &QLineEdit::textChanged, this, &AddWidgetDialog::filter_changed);
    root->addWidget(search_bar_);

    // ── Category bar (dynamically built from registry) ──
    build_category_bar(root);

    // ── Scrollable card grid ──
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(
        QString("QScrollArea { border: none; background: transparent; }"
                "QScrollBar:vertical { width: 5px; background: transparent; }"
                "QScrollBar::handle:vertical { background: %1; border-radius: 2px; min-height: 20px; }"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
                "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }")
            .arg(ui::colors::BORDER_BRIGHT));

    card_container_ = new QWidget(this);
    card_container_->setStyleSheet("background: transparent;");
    card_grid_ = new QGridLayout(card_container_);
    card_grid_->setContentsMargins(0, 0, 0, 0);
    card_grid_->setSpacing(6);
    scroll->setWidget(card_container_);
    root->addWidget(scroll, 1);

    // ── Bottom bar ──
    auto* bot = new QHBoxLayout;
    bot->setSpacing(8);
    bot->addStretch();

    auto* cancel_btn = new QPushButton("CANCEL");
    cancel_btn->setFixedSize(90, 30);
    cancel_btn->setCursor(Qt::PointingHandCursor);
    cancel_btn->setStyleSheet(
        QString("QPushButton { background: %1; border: 1px solid %2; color: %3; "
                "font-size: 10px; font-weight: bold; border-radius: 2px; }"
                "QPushButton:hover { border-color: %4; color: %4; }")
            .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_MED(), ui::colors::TEXT_PRIMARY(), ui::colors::AMBER()));
    connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);
    bot->addWidget(cancel_btn);

    add_btn_ = new QPushButton("ADD WIDGET");
    add_btn_->setFixedSize(110, 30);
    add_btn_->setCursor(Qt::PointingHandCursor);
    add_btn_->setEnabled(false);
    add_btn_->setStyleSheet(QString("QPushButton { background: %1; color: %4; border: none; "
                                    "font-size: 10px; font-weight: bold; border-radius: 2px; letter-spacing: 0.5px; }"
                                    "QPushButton:hover { background: %5; }"
                                    "QPushButton:disabled { background: %2; color: %3; }")
                                .arg(ui::colors::AMBER(), ui::colors::BG_RAISED(), ui::colors::TEXT_TERTIARY(),
                                     ui::colors::BG_BASE(), ui::colors::WARNING()));
    connect(add_btn_, &QPushButton::clicked, this, &AddWidgetDialog::confirm);
    bot->addWidget(add_btn_);
    root->addLayout(bot);

    // ── Initial population ──
    populate_cards();
}

// ── Build category tab bar dynamically ──────────────────────────────────────

void AddWidgetDialog::build_category_bar(QVBoxLayout* root) {
    auto* cat_row = new QHBoxLayout;
    cat_row->setSpacing(4);

    cat_group_ = new QButtonGroup(this);
    cat_group_->setExclusive(true);

    // Gather unique categories from registry
    QStringList categories;
    categories << "All";
    QSet<QString> seen;
    for (const auto& meta : WidgetRegistry::instance().all()) {
        if (!seen.contains(meta.category)) {
            seen.insert(meta.category);
            categories << meta.category;
        }
    }

    for (const auto& cat : categories) {
        auto* btn = new QPushButton(cat.toUpper());
        btn->setCheckable(true);
        btn->setFixedHeight(24);
        btn->setCursor(Qt::PointingHandCursor);

        QString accent = (cat == "All") ? ui::colors::AMBER() : accent_for_category(cat);
        btn->setStyleSheet(QString("QPushButton { background: %1; border: 1px solid %2; color: %3; "
                                   "padding: 2px 12px; font-size: 10px; font-weight: bold; "
                                   "letter-spacing: 0.5px; border-radius: 2px; }"
                                   "QPushButton:hover { border-color: %4; color: %4; }"
                                   "QPushButton:checked { background: %4; color: %5; border-color: %4; }")
                               .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_MED(), ui::colors::TEXT_PRIMARY(),
                                    accent, ui::colors::BG_BASE()));

        if (cat == "All")
            btn->setChecked(true);

        cat_group_->addButton(btn);
        cat_buttons_.append(btn);

        connect(btn, &QPushButton::clicked, this, [this, c = cat]() { category_clicked(c == "All" ? QString{} : c); });

        cat_row->addWidget(btn);
    }

    cat_row->addStretch();
    root->addLayout(cat_row);
}

// ── Slots ───────────────────────────────────────────────────────────────────

void AddWidgetDialog::filter_changed(const QString& text) {
    populate_cards(text, active_category_);
}

void AddWidgetDialog::category_clicked(const QString& category) {
    active_category_ = category;
    populate_cards(search_bar_->text(), category);
}

void AddWidgetDialog::card_clicked(const QString& type_id) {
    selected_id_ = type_id;
    add_btn_->setEnabled(true);

    // Re-highlight all cards
    for (int i = 0; i < card_grid_->count(); ++i) {
        auto* w = card_grid_->itemAt(i)->widget();
        if (!w)
            continue;

        QString wtype = w->property("wtype").toString();
        bool sel = (wtype == type_id);

        QString cat = w->property("wcat").toString();
        QString accent = accent_for_category(cat);

        if (sel) {
            w->setStyleSheet(QString("QWidget#widgetCard { background: %1; border: 1px solid %2; border-radius: 3px; }")
                                 .arg(ui::colors::BG_HOVER(), accent));
        } else {
            w->setStyleSheet(QString("QWidget#widgetCard { background: %1; border: 1px solid %2; border-radius: 3px; }"
                                     "QWidget#widgetCard:hover { background: %3; border-color: %4; }")
                                 .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::BG_HOVER(),
                                      ui::colors::BORDER_MED()));
        }
    }
}

void AddWidgetDialog::confirm() {
    if (selected_id_.isEmpty())
        return;
    emit widget_selected(selected_id_);
    accept();
}

// ── Card grid population ────────────────────────────────────────────────────

void AddWidgetDialog::populate_cards(const QString& filter, const QString& category) {
    // Clear existing cards
    QLayoutItem* item;
    while ((item = card_grid_->takeAt(0)) != nullptr) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    selected_id_.clear();
    if (add_btn_)
        add_btn_->setEnabled(false);

    const auto metas =
        category.isEmpty() ? WidgetRegistry::instance().all() : WidgetRegistry::instance().by_category(category);

    int col = 0;
    int row = 0;

    for (const auto& meta : metas) {
        // Filter by search text
        if (!filter.isEmpty() && !meta.display_name.contains(filter, Qt::CaseInsensitive) &&
            !meta.description.contains(filter, Qt::CaseInsensitive) &&
            !meta.category.contains(filter, Qt::CaseInsensitive))
            continue;

        QString accent = accent_for_category(meta.category);

        // ── Card container ──
        auto* card = new QWidget(this);
        card->setObjectName("widgetCard");
        card->setProperty("wtype", meta.type_id);
        card->setProperty("wcat", meta.category);
        card->setCursor(Qt::PointingHandCursor);
        card->setFixedHeight(72);
        card->setStyleSheet(QString("QWidget#widgetCard { background: %1; border: 1px solid %2; border-radius: 3px; }"
                                    "QWidget#widgetCard:hover { background: %3; border-color: %4; }")
                                .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::BG_HOVER(),
                                     ui::colors::BORDER_MED()));

        auto* cl = new QHBoxLayout(card);
        cl->setContentsMargins(10, 8, 10, 8);
        cl->setSpacing(10);

        // ── Left: icon + accent stripe ──
        auto* icon_frame = new QWidget(this);
        icon_frame->setFixedSize(36, 36);
        icon_frame->setStyleSheet(
            QString("background: %1; border: 1px solid %2; border-radius: 3px;").arg(ui::colors::BG_BASE(), accent));

        auto* icon_lbl = new QLabel(icon_for_widget(meta.type_id), icon_frame);
        icon_lbl->setAlignment(Qt::AlignCenter);
        icon_lbl->setGeometry(0, 0, 36, 36);
        icon_lbl->setStyleSheet(
            QString("color: %1; font-size: 14px; background: transparent; border: none;").arg(accent));

        cl->addWidget(icon_frame);

        // ── Center: name + description ──
        auto* text_col = new QVBoxLayout;
        text_col->setContentsMargins(0, 0, 0, 0);
        text_col->setSpacing(2);

        auto* name_lbl = new QLabel(meta.display_name);
        name_lbl->setStyleSheet(
            QString("color: %1; font-size: 11px; font-weight: bold; background: transparent; border: none;")
                .arg(ui::colors::TEXT_PRIMARY()));

        auto* desc_lbl = new QLabel(meta.description);
        desc_lbl->setWordWrap(true);
        desc_lbl->setStyleSheet(QString("color: %1; font-size: 9px; background: transparent; border: none;")
                                    .arg(ui::colors::TEXT_SECONDARY()));

        text_col->addWidget(name_lbl);
        text_col->addWidget(desc_lbl);
        text_col->addStretch();

        cl->addLayout(text_col, 1);

        // ── Right: category badge ──
        auto* badge = new QLabel(meta.category.toUpper());
        badge->setAlignment(Qt::AlignCenter);
        badge->setFixedHeight(16);
        badge->setStyleSheet(QString("color: %1; font-size: 8px; font-weight: bold; letter-spacing: 0.5px; "
                                     "background: %2; border: 1px solid %1; border-radius: 2px; "
                                     "padding: 1px 6px;")
                                 .arg(accent, ui::colors::BG_BASE()));

        cl->addWidget(badge, 0, Qt::AlignTop);

        // ── Click handler via transparent overlay button ──
        auto* click_btn = new QPushButton(card);
        click_btn->setStyleSheet("QPushButton { background: transparent; border: none; }");
        click_btn->setCursor(Qt::PointingHandCursor);

        // Connect click — capture type_id by value
        const QString tid = meta.type_id;
        connect(click_btn, &QPushButton::clicked, this, [this, tid]() { card_clicked(tid); });

        // Place button on grid
        card_grid_->addWidget(card, row, col);

        // When the card is shown, resize the overlay button to fill it
        connect(click_btn, &QPushButton::destroyed, this, []() {}); // prevent warning
        QTimer::singleShot(0, card, [click_btn, card]() {
            click_btn->setGeometry(card->rect());
            click_btn->raise();
        });

        if (++col >= 2) {
            col = 0;
            ++row;
        }
    }

    // Add stretch at the bottom
    card_grid_->setRowStretch(row + (col > 0 ? 1 : 0), 1);
}

} // namespace fincept::screens
