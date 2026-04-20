#include "screens/polymarket/PolymarketCommandBar.h"

#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace fincept::screens::polymarket {

using namespace fincept::ui;

static const QStringList SORT_KEYS = {"volume", "liquidity", "startDate"};

PolymarketCommandBar::PolymarketCommandBar(QWidget* parent) : QWidget(parent) {
    setObjectName("polyCommandBar");
    setFixedHeight(72);
    build_ui();
    apply_accent();
    rebuild_view_pills();
}

static QString accent_css_rgb(const QColor& c) {
    return QStringLiteral("rgba(%1,%2,%3,1.0)").arg(c.red()).arg(c.green()).arg(c.blue());
}
static QString accent_css_rgba(const QColor& c, double a) {
    return QStringLiteral("rgba(%1,%2,%3,%4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(a);
}

void PolymarketCommandBar::apply_accent() {
    const QString accent = accent_css_rgb(presentation_.accent);
    const QString accent_bg = accent_css_rgba(presentation_.accent, 0.15);

    // The bar uses object-name-scoped styling so the accent can swap live
    // on exchange change without a full stylesheet rebuild further down the
    // tree. Any descendant that also needs the accent (tab underline in the
    // detail panel, outcome row left-border, etc.) picks it up via its own
    // set_presentation() call.
    const QString css =
        QStringLiteral("#polyCommandBar { background: %1; border-bottom: 2px solid %2; }"
                       "#polyCmdTitle { color: %3; font-size: 12px; font-weight: 700; background: transparent; }"
                       "#polyViewBtn { background: transparent; color: %4; border: 1px solid %5; "
                       "  font-size: 9px; font-weight: 700; padding: 4px 12px; }"
                       "#polyViewBtn:hover { color: %3; }"
                       "#polyViewBtn[active=\"true\"] { background: %2; color: %6; border-color: %2; }"
                       "#polyCatBtn { background: transparent; color: %4; border: 1px solid %5; "
                       "  font-size: 8px; font-weight: 600; padding: 2px 8px; }"
                       "#polyCatBtn:hover { color: %3; }"
                       "#polyCatBtn[active=\"true\"] { background: %7; color: %2; border-color: %2; }"
                       "#polySearchInput { background: %6; color: %3; border: 1px solid %5; "
                       "  padding: 4px 8px; font-size: 11px; }"
                       "#polySearchInput:focus { border-color: %8; }"
                       "#polyRefreshBtn { background: %9; color: %4; border: 1px solid %5; "
                       "  padding: 4px 10px; font-size: 9px; font-weight: 700; }"
                       "#polyRefreshBtn:hover { color: %3; }"
                       "#polyWsIndicator { font-size: 8px; font-weight: 700; padding: 2px 6px; }"
                       "#polyCountLabel { color: %4; font-size: 9px; background: transparent; }")
            .arg(colors::BG_RAISED())      // %1
            .arg(accent)                   // %2
            .arg(colors::TEXT_PRIMARY())   // %3
            .arg(colors::TEXT_SECONDARY()) // %4
            .arg(colors::BORDER_DIM())     // %5
            .arg(colors::BG_BASE())        // %6
            .arg(accent_bg)                // %7
            .arg(colors::BORDER_BRIGHT())  // %8
            .arg(colors::BG_SURFACE());    // %9
    setStyleSheet(css);
}

void PolymarketCommandBar::build_ui() {
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(16, 6, 16, 6);
    vl->setSpacing(4);

    // Row 1: title + count + refresh + WS status
    auto* row1 = new QHBoxLayout;
    row1->setSpacing(8);
    title_label_ = new QLabel(tr("PREDICTION MARKETS"));
    title_label_->setObjectName("polyCmdTitle");
    row1->addWidget(title_label_);
    row1->addStretch(1);

    count_label_ = new QLabel;
    count_label_->setObjectName("polyCountLabel");
    row1->addWidget(count_label_);

    refresh_btn_ = new QPushButton(tr("REFRESH"));
    refresh_btn_->setObjectName("polyRefreshBtn");
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    connect(refresh_btn_, &QPushButton::clicked, this, &PolymarketCommandBar::refresh_clicked);
    row1->addWidget(refresh_btn_);

    ws_indicator_ = new QLabel(tr("DISCONNECTED"));
    ws_indicator_->setObjectName("polyWsIndicator");
    set_ws_status(false);
    row1->addWidget(ws_indicator_);
    vl->addLayout(row1);

    // Row 2: exchange dropdown + view pills + categories + search + sort
    auto* row2 = new QHBoxLayout;
    row2->setSpacing(4);

    exchange_combo_ = new QComboBox;
    exchange_combo_->setFixedWidth(130);
    exchange_combo_->setToolTip(tr("Select prediction-market exchange"));
    connect(exchange_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (idx < 0) return;
        const QString id = exchange_combo_->itemData(idx).toString();
        if (!id.isEmpty()) emit exchange_changed(id);
    });
    row2->addWidget(exchange_combo_);

    account_chip_ = new QPushButton(tr("⚪ No account"), this);
    account_chip_->setObjectName("polyAccountChip");
    account_chip_->setCursor(Qt::PointingHandCursor);
    account_chip_->setToolTip(tr("Connect a Polymarket or Kalshi account to place orders"));
    account_chip_->setStyleSheet(QString("#polyAccountChip { background: transparent; color: %1; "
                                         "border: 1px solid %2; font-size: 9px; font-weight: 700; "
                                         "padding: 4px 10px; } #polyAccountChip:hover { color: %3; }")
                                     .arg(colors::TEXT_DIM())
                                     .arg(colors::BORDER_DIM())
                                     .arg(colors::TEXT_PRIMARY()));
    connect(account_chip_, &QPushButton::clicked, this, &PolymarketCommandBar::account_clicked);
    row2->addWidget(account_chip_);

    row2->addSpacing(8);

    // View pills — rebuilt dynamically from presentation.view_names.
    view_pills_container_ = new QWidget(this);
    auto* pl = new QHBoxLayout(view_pills_container_);
    pl->setContentsMargins(0, 0, 0, 0);
    pl->setSpacing(4);
    row2->addWidget(view_pills_container_);

    row2->addSpacing(8);

    category_container_ = new QWidget(this);
    auto* ccl = new QHBoxLayout(category_container_);
    ccl->setContentsMargins(0, 0, 0, 0);
    ccl->setSpacing(2);
    // Populated by rebuild_categories() per presentation mode.
    row2->addWidget(category_container_);

    row2->addSpacing(8);

    search_input_ = new QLineEdit;
    search_input_->setObjectName("polySearchInput");
    search_input_->setPlaceholderText(tr("SEARCH..."));
    search_input_->setFixedWidth(180);
    connect(search_input_, &QLineEdit::returnPressed, this,
            [this]() { emit search_submitted(search_input_->text().trimmed()); });
    row2->addWidget(search_input_);

    sort_combo_ = new QComboBox;
    sort_combo_->addItems({tr("VOLUME"), tr("LIQUIDITY"), tr("RECENT")});
    sort_combo_->setFixedWidth(90);
    connect(sort_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (idx >= 0 && idx < SORT_KEYS.size()) emit sort_changed(SORT_KEYS[idx]);
    });
    row2->addWidget(sort_combo_);

    row2->addStretch(1);
    vl->addLayout(row2);
}

// ── Presentation ────────────────────────────────────────────────────────────

void PolymarketCommandBar::set_presentation(const ExchangePresentation& p) {
    const bool accent_changed = p.accent != presentation_.accent;
    const bool views_changed = p.view_names != presentation_.view_names;
    const bool category_mode_changed = p.category_mode != presentation_.category_mode;

    presentation_ = p;
    title_label_->setText(tr("%1 · PREDICTION MARKETS").arg(p.display_name.toUpper()));

    if (accent_changed) apply_accent();
    if (views_changed || view_btns_.isEmpty()) {
        if (!presentation_.default_view.isEmpty())
            active_view_ = presentation_.default_view;
        rebuild_view_pills();
    }
    if (category_mode_changed) rebuild_categories();
}

void PolymarketCommandBar::rebuild_view_pills() {
    auto* layout = qobject_cast<QHBoxLayout*>(view_pills_container_->layout());
    if (!layout) return;

    while (layout->count() > 0) {
        auto* item = layout->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    view_btns_.clear();

    for (const QString& name : presentation_.view_names) {
        auto* btn = new QPushButton(name);
        btn->setObjectName("polyViewBtn");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("active", name == active_view_);
        connect(btn, &QPushButton::clicked, this, [this, name]() { emit view_changed(name); });
        layout->addWidget(btn);
        view_btns_.append(btn);
    }
}

// ── Categories ──────────────────────────────────────────────────────────────

void PolymarketCommandBar::set_categories(const QStringList& tags) {
    current_tags_ = tags;
    rebuild_categories();
}

void PolymarketCommandBar::rebuild_categories() {
    auto* layout = qobject_cast<QHBoxLayout*>(category_container_->layout());
    if (!layout) return;

    while (layout->count() > 0) {
        auto* item = layout->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    category_combo_ = nullptr;

    if (presentation_.category_mode == ExchangePresentation::CategoryMode::ComboBox) {
        // Kalshi-style: hundreds of series tickers — a dropdown with a
        // type-ahead is the only layout that scales.
        category_combo_ = new QComboBox;
        category_combo_->setEditable(true);
        category_combo_->setInsertPolicy(QComboBox::NoInsert);
        category_combo_->setFixedWidth(200);
        category_combo_->setStyleSheet(
            QString("QComboBox { background: %1; color: %2; border: 1px solid %3; "
                    "padding: 3px 8px; font-size: 10px; }"
                    "QComboBox QAbstractItemView { background: %1; color: %2; selection-background-color: %4; }")
                .arg(colors::BG_BASE(), colors::TEXT_PRIMARY(), colors::BORDER_DIM(),
                     QString("rgba(%1,%2,%3,0.2)")
                         .arg(presentation_.accent.red())
                         .arg(presentation_.accent.green())
                         .arg(presentation_.accent.blue())));
        category_combo_->addItem(tr("ALL"), QStringLiteral("ALL"));
        for (const QString& t : current_tags_) category_combo_->addItem(t, t);
        // Restore selection.
        const int idx = qMax(0, category_combo_->findData(active_category_));
        {
            QSignalBlocker b(category_combo_);
            category_combo_->setCurrentIndex(idx);
        }
        connect(category_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int i) {
            if (!category_combo_ || i < 0) return;
            const QString slug = category_combo_->itemData(i).toString();
            if (!slug.isEmpty()) emit category_changed(slug);
        });
        layout->addWidget(category_combo_);
        return;
    }

    // Chip row (Polymarket default).
    auto* all_btn = new QPushButton(tr("ALL"));
    all_btn->setObjectName("polyCatBtn");
    all_btn->setCursor(Qt::PointingHandCursor);
    all_btn->setProperty("active", active_category_ == "ALL");
    connect(all_btn, &QPushButton::clicked, this, [this]() { emit category_changed("ALL"); });
    layout->addWidget(all_btn);

    const int cap = qMin(current_tags_.size(), qMax(0, presentation_.category_visible_cap));
    for (int i = 0; i < cap; ++i) {
        const QString& slug = current_tags_[i];
        auto* btn = new QPushButton(slug.toUpper());
        btn->setObjectName("polyCatBtn");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("active", active_category_ == slug);
        connect(btn, &QPushButton::clicked, this, [this, slug]() { emit category_changed(slug); });
        layout->addWidget(btn);
    }
}

// ── Active-state tracking ───────────────────────────────────────────────────

void PolymarketCommandBar::set_active_view(const QString& view) {
    active_view_ = view;
    for (auto* btn : view_btns_) {
        btn->setProperty("active", btn->text() == view);
        btn->style()->unpolish(btn);
        btn->style()->polish(btn);
    }
}

void PolymarketCommandBar::set_active_category(const QString& cat) {
    active_category_ = cat;
    if (category_combo_) {
        const int idx = qMax(0, category_combo_->findData(cat));
        QSignalBlocker b(category_combo_);
        category_combo_->setCurrentIndex(idx);
        return;
    }
    auto* layout = category_container_->layout();
    if (!layout) return;
    for (int i = 0; i < layout->count(); ++i) {
        auto* w = qobject_cast<QPushButton*>(layout->itemAt(i)->widget());
        if (!w) continue;
        const bool is_active = (cat == "ALL" && i == 0) || (cat != "ALL" && w->text() == cat.toUpper());
        w->setProperty("active", is_active);
        w->style()->unpolish(w);
        w->style()->polish(w);
    }
}

void PolymarketCommandBar::set_loading(bool loading) {
    refresh_btn_->setEnabled(!loading);
    refresh_btn_->setText(loading ? tr("LOADING...") : tr("REFRESH"));
}

void PolymarketCommandBar::set_ws_status(bool connected) {
    if (connected) {
        ws_indicator_->setText(tr("LIVE"));
        ws_indicator_->setStyleSheet(QString("color: %1; background: rgba(22,163,74,0.2); "
                                             "font-size: 8px; font-weight: 700; padding: 2px 6px;")
                                         .arg(colors::POSITIVE()));
    } else {
        ws_indicator_->setText(tr("OFFLINE"));
        ws_indicator_->setStyleSheet(QString("color: %1; background: transparent; "
                                             "font-size: 8px; font-weight: 700; padding: 2px 6px;")
                                         .arg(colors::TEXT_DIM()));
    }
}

void PolymarketCommandBar::set_market_count(int count) {
    count_label_->setText(QString::number(count) + " markets");
}

void PolymarketCommandBar::set_account_status(bool connected, const QString& label) {
    if (!account_chip_) return;
    if (connected) {
        account_chip_->setText(QString("🟢 %1").arg(label.isEmpty() ? tr("Connected") : label));
        account_chip_->setStyleSheet(QString("#polyAccountChip { background: rgba(22,163,74,0.12); "
                                             "color: %1; border: 1px solid %1; font-size: 9px; "
                                             "font-weight: 700; padding: 4px 10px; }")
                                         .arg(colors::POSITIVE()));
    } else {
        account_chip_->setText(tr("⚪ No account"));
        account_chip_->setStyleSheet(QString("#polyAccountChip { background: transparent; "
                                             "color: %1; border: 1px solid %2; font-size: 9px; "
                                             "font-weight: 700; padding: 4px 10px; } "
                                             "#polyAccountChip:hover { color: %3; }")
                                         .arg(colors::TEXT_DIM())
                                         .arg(colors::BORDER_DIM())
                                         .arg(colors::TEXT_PRIMARY()));
    }
}

void PolymarketCommandBar::set_exchanges(const QStringList& ids, const QStringList& labels,
                                         const QString& active_id) {
    if (!exchange_combo_) return;
    const QSignalBlocker block(exchange_combo_);
    exchange_combo_->clear();
    const int n = qMin(ids.size(), labels.size());
    int active_idx = -1;
    for (int i = 0; i < n; ++i) {
        exchange_combo_->addItem(labels[i], ids[i]);
        if (ids[i] == active_id) active_idx = i;
    }
    if (active_idx >= 0) exchange_combo_->setCurrentIndex(active_idx);
}

} // namespace fincept::screens::polymarket
