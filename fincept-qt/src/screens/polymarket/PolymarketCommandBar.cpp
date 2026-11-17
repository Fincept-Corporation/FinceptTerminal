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
    setFixedHeight(48);
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
    const QString accent_dim = accent_css_rgba(presentation_.accent, 0.18);
    const QString accent_border = accent_css_rgba(presentation_.accent, 0.50);

    const QString css =
        QString("#polyCommandBar {"
                       "  background: %1;"
                       "  border-bottom: 1px solid %2;"
                       "}"
                       // Exchange combo
                       "QComboBox#polyExchangeCombo {"
                       "  background: %3;"
                       "  color: %4;"
                       "  border: 1px solid %5;"
                       "  font-size: " + fincept::ui::fonts::px(-4) + "px;"
                       "  font-weight: 700;"
                       "  padding: 3px 8px;"
                       "  min-width: 110px;"
                       "}"
                       "QComboBox#polyExchangeCombo::drop-down { border: none; width: 18px; }"
                       "QComboBox#polyExchangeCombo QAbstractItemView {"
                       "  background: %3; color: %4; border: 1px solid %5;"
                       "  selection-background-color: %6; selection-color: %4;"
                       "}"
                       // Separator
                       "#polyCmdSep { background: %2; }"
                       // View tabs
                       "#polyViewBtn {"
                       "  background: transparent;"
                       "  color: %7;"
                       "  border: none;"
                       "  border-bottom: 2px solid transparent;"
                       "  font-size: " + fincept::ui::fonts::px(-4) + "px;"
                       "  font-weight: 700;"
                       "  padding: 0 14px;"
                       "  letter-spacing: 0.5px;"
                       "}"
                       "#polyViewBtn:hover { color: %4; border-bottom-color: %5; }"
                       "#polyViewBtn[active=\"true\"] { color: %8; border-bottom-color: %8; }"
                       // Category chips
                       "#polyCatBtn {"
                       "  background: transparent;"
                       "  color: %7;"
                       "  border: 1px solid %5;"
                       "  font-size: " + fincept::ui::fonts::px(-5) + "px;"
                       "  font-weight: 600;"
                       "  padding: 2px 10px;"
                       "  letter-spacing: 0.3px;"
                       "}"
                       "#polyCatBtn:hover { color: %4; border-color: %9; }"
                       "#polyCatBtn[active=\"true\"] { background: %6; color: %8; border-color: %8; }"
                       // Search
                       "#polySearchInput {"
                       "  background: %3;"
                       "  color: %4;"
                       "  border: 1px solid %5;"
                       "  padding: 4px 10px;"
                       "  font-size: " + fincept::ui::fonts::px(-4) + "px;"
                       "}"
                       "#polySearchInput:focus { border-color: %8; }"
                       // Sort
                       "QComboBox#polySortCombo {"
                       "  background: %3;"
                       "  color: %7;"
                       "  border: 1px solid %5;"
                       "  padding: 3px 6px;"
                       "  font-size: " + fincept::ui::fonts::px(-5) + "px;"
                       "  font-weight: 600;"
                       "}"
                       "QComboBox#polySortCombo::drop-down { border: none; width: 14px; }"
                       "QComboBox#polySortCombo QAbstractItemView {"
                       "  background: %3; color: %4; border: 1px solid %5;"
                       "}"
                       // Refresh
                       "#polyRefreshBtn {"
                       "  background: transparent;"
                       "  color: %7;"
                       "  border: 1px solid %5;"
                       "  padding: 3px 10px;"
                       "  font-size: " + fincept::ui::fonts::px(-5) + "px;"
                       "  font-weight: 700;"
                       "}"
                       "#polyRefreshBtn:hover { color: %4; border-color: %9; }"
                       "#polyRefreshBtn:disabled { color: %10; }"
                       // WS indicator
                       "#polyWsIndicator { font-size: " + fincept::ui::fonts::px(-5) + "px; font-weight: 700; padding: 2px 8px; }"
                       // Count label
                       "#polyCountLabel { color: %7; font-size: " + fincept::ui::fonts::px(-5) + "px; background: transparent; padding: 0 4px; }"
                       // Account chip
                       "#polyAccountChip {"
                       "  background: transparent;"
                       "  color: %7;"
                       "  border: 1px solid %5;"
                       "  font-size: " + fincept::ui::fonts::px(-5) + "px;"
                       "  font-weight: 700;"
                       "  padding: 3px 10px;"
                       "}"
                       "#polyAccountChip:hover { color: %4; border-color: %9; }")
            .arg(colors::BG_RAISED())      // %1
            .arg(colors::BORDER_DIM())     // %2
            .arg(colors::BG_BASE())        // %3
            .arg(colors::TEXT_PRIMARY())   // %4
            .arg(colors::BORDER_MED())     // %5
            .arg(accent_dim)               // %6
            .arg(colors::TEXT_SECONDARY()) // %7
            .arg(accent)                   // %8
            .arg(colors::BORDER_BRIGHT())  // %9
            .arg(colors::TEXT_DIM());      // %10

    // Only when the text changed: re-run on every presentation / appearance change,
    // and a re-set sheet re-polishes the whole bar.
    if (styleSheet() != css)
        setStyleSheet(css);
}

void PolymarketCommandBar::build_ui() {
    auto* hl = new QHBoxLayout(this);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(0);

    // ── Exchange selector ─────────────────────────────────────────────────
    exchange_combo_ = new QComboBox;
    exchange_combo_->setObjectName("polyExchangeCombo");
    exchange_combo_->setToolTip(tr("Switch prediction market exchange"));
    exchange_combo_->setMinimumHeight(26);
    connect(exchange_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (idx < 0)
            return;
        const QString id = exchange_combo_->itemData(idx).toString();
        if (!id.isEmpty())
            emit exchange_changed(id);
    });
    hl->addWidget(exchange_combo_);

    hl->addSpacing(8);

    // ── Account chip ──────────────────────────────────────────────────────
    account_chip_ = new QPushButton(tr("CONNECT"), this);
    account_chip_->setObjectName("polyAccountChip");
    account_chip_->setMinimumHeight(26);
    account_chip_->setCursor(Qt::PointingHandCursor);
    account_chip_->setToolTip(tr("Connect a trading account"));
    connect(account_chip_, &QPushButton::clicked, this, [this]() {
        if (!account_read_error_.isEmpty())
            emit account_retry_clicked();
        else
            emit account_clicked();
    });
    hl->addWidget(account_chip_);

    // ── Left separator ────────────────────────────────────────────────────
    auto* sep1 = new QWidget;
    sep1->setObjectName("polyCmdSep");
    sep1->setFixedSize(1, 28);
    hl->addSpacing(10);
    hl->addWidget(sep1);
    hl->addSpacing(2);

    // ── View tabs (fills available width) ────────────────────────────────
    view_pills_container_ = new QWidget(this);
    view_pills_container_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    auto* pl = new QHBoxLayout(view_pills_container_);
    pl->setContentsMargins(0, 0, 0, 0);
    pl->setSpacing(0);
    hl->addWidget(view_pills_container_);

    // ── Right separator ───────────────────────────────────────────────────
    auto* sep2 = new QWidget;
    sep2->setObjectName("polyCmdSep");
    sep2->setFixedSize(1, 28);
    hl->addSpacing(2);
    hl->addWidget(sep2);
    hl->addSpacing(8);

    // ── Category row ──────────────────────────────────────────────────────
    category_container_ = new QWidget(this);
    // Ignored, not Expanding: the chip row takes whatever width is left and never widens the
    // bar. With a dozen chips its minimum width pushed the whole screen past a 1600 px window
    // (sideways scroll bar, search box and sort control off-screen). fit_category_chips() hides
    // the chips that do not fit.
    category_container_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    category_container_->installEventFilter(this);
    auto* ccl = new QHBoxLayout(category_container_);
    ccl->setContentsMargins(0, 0, 0, 0);
    ccl->setSpacing(4);
    hl->addWidget(category_container_, 1);

    hl->addSpacing(8);

    // ── Search ────────────────────────────────────────────────────────────
    search_input_ = new QLineEdit;
    search_input_->setObjectName("polySearchInput");
    search_input_->setPlaceholderText(tr("Search markets..."));
    search_input_->setFixedWidth(170);
    search_input_->setMinimumHeight(26);
    connect(search_input_, &QLineEdit::returnPressed, this,
            [this]() { emit search_submitted(search_input_->text().trimmed()); });
    hl->addWidget(search_input_);

    hl->addSpacing(6);

    // ── Sort ──────────────────────────────────────────────────────────────
    sort_combo_ = new QComboBox;
    sort_combo_->setObjectName("polySortCombo");
    sort_combo_->addItems({tr("VOLUME"), tr("LIQUIDITY"), tr("DATE")});
    sort_combo_->setMinimumSize(88, 26);
    connect(sort_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (idx >= 0 && idx < SORT_KEYS.size())
            emit sort_changed(SORT_KEYS[idx]);
    });
    hl->addWidget(sort_combo_);

    hl->addSpacing(6);

    // ── Count ─────────────────────────────────────────────────────────────
    count_label_ = new QLabel;
    count_label_->setObjectName("polyCountLabel");
    hl->addWidget(count_label_);

    hl->addSpacing(6);

    // ── Refresh ───────────────────────────────────────────────────────────
    refresh_btn_ = new QPushButton(tr("↻"));
    refresh_btn_->setObjectName("polyRefreshBtn");
    refresh_btn_->setMinimumSize(28, 26);
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    refresh_btn_->setToolTip(tr("Refresh"));
    connect(refresh_btn_, &QPushButton::clicked, this, &PolymarketCommandBar::refresh_clicked);
    hl->addWidget(refresh_btn_);

    hl->addSpacing(8);

    // ── WS indicator ──────────────────────────────────────────────────────
    ws_indicator_ = new QLabel;
    ws_indicator_->setObjectName("polyWsIndicator");
    ws_indicator_->setMinimumWidth(52);
    set_ws_status(false);
    hl->addWidget(ws_indicator_);
}

// ── Presentation ────────────────────────────────────────────────────────────

void PolymarketCommandBar::set_presentation(const ExchangePresentation& p) {
    const bool accent_changed = p.accent != presentation_.accent;
    const bool views_changed = p.view_names != presentation_.view_names;
    const bool category_mode_changed = p.category_mode != presentation_.category_mode;

    presentation_ = p;

    if (accent_changed)
        apply_accent();
    if (views_changed || view_btns_.isEmpty()) {
        if (!presentation_.default_view.isEmpty())
            active_view_ = presentation_.default_view;
        rebuild_view_pills();
    }
    // The series combo's selection tint is built from the accent.
    if (category_mode_changed || (accent_changed && category_combo_))
        rebuild_categories();
}

void PolymarketCommandBar::rebuild_view_pills() {
    auto* layout = qobject_cast<QHBoxLayout*>(view_pills_container_->layout());
    if (!layout)
        return;

    while (layout->count() > 0) {
        auto* item = layout->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    view_btns_.clear();

    for (const QString& name : presentation_.view_names) {
        auto* btn = new QPushButton(name);
        btn->setObjectName("polyViewBtn");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedHeight(48); // full bar height for flush underline
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
    if (!layout)
        return;

    while (layout->count() > 0) {
        auto* item = layout->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    category_combo_ = nullptr;

    if (presentation_.category_mode == ExchangePresentation::CategoryMode::ComboBox) {
        category_combo_ = new QComboBox;
        category_combo_->setEditable(true);
        category_combo_->setInsertPolicy(QComboBox::NoInsert);
        category_combo_->setFixedWidth(190);
        category_combo_->setMinimumHeight(26);
        category_combo_->setStyleSheet(
            QString("QComboBox { background: %1; color: %2; border: 1px solid %3; "
                    "padding: 3px 8px; font-size: " + fincept::ui::fonts::px(-5) + "px; font-weight: 600; }"
                    "QComboBox::drop-down { border: none; width: 14px; }"
                    "QComboBox QAbstractItemView { background: %1; color: %2; border: 1px solid %3; "
                    "selection-background-color: rgba(%4,%5,%6,0.2); selection-color: %2; }")
                .arg(colors::BG_BASE(), colors::TEXT_PRIMARY(), colors::BORDER_MED())
                .arg(presentation_.accent.red())
                .arg(presentation_.accent.green())
                .arg(presentation_.accent.blue()));
        category_combo_->addItem(tr("ALL SERIES"), QStringLiteral("ALL"));
        for (const QString& t : current_tags_)
            category_combo_->addItem(t, t);
        const int idx = qMax(0, category_combo_->findData(active_category_));
        {
            QSignalBlocker b(category_combo_);
            category_combo_->setCurrentIndex(idx);
        }
        connect(category_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int i) {
            if (!category_combo_ || i < 0)
                return;
            const QString slug = category_combo_->itemData(i).toString();
            if (!slug.isEmpty())
                emit category_changed(slug);
        });
        layout->addWidget(category_combo_);
        return;
    }

    // Chip row (Polymarket default) — compact pills
    auto* all_btn = new QPushButton(tr("ALL"));
    all_btn->setObjectName("polyCatBtn");
    all_btn->setMinimumHeight(22);
    all_btn->setCursor(Qt::PointingHandCursor);
    all_btn->setProperty("active", active_category_ == "ALL");
    connect(all_btn, &QPushButton::clicked, this, [this]() { emit category_changed("ALL"); });
    layout->addWidget(all_btn);

    const int cap = qMin(current_tags_.size(), qMax(0, presentation_.category_visible_cap));
    for (int i = 0; i < cap; ++i) {
        const QString& slug = current_tags_[i];
        auto* btn = new QPushButton(slug.toUpper());
        btn->setObjectName("polyCatBtn");
        btn->setMinimumHeight(22);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("active", active_category_ == slug);
        connect(btn, &QPushButton::clicked, this, [this, slug]() { emit category_changed(slug); });
        layout->addWidget(btn);
    }
    layout->addStretch(1);
    fit_category_chips();
}

void PolymarketCommandBar::fit_category_chips() {
    if (fitting_chips_ || !category_container_ || category_combo_)
        return;
    auto* layout = qobject_cast<QHBoxLayout*>(category_container_->layout());
    if (!layout)
        return;
    fitting_chips_ = true;
    const int avail = category_container_->width();
    int x = 0;
    for (int i = 0; i < layout->count(); ++i) {
        QWidget* chip = layout->itemAt(i)->widget();
        if (!chip)
            continue;
        x += chip->sizeHint().width();
        // The first chip (ALL) always stays, so the row is never empty.
        const bool fits = i == 0 || x <= avail;
        if (chip->isHidden() == fits)
            chip->setVisible(fits);
        x += layout->spacing();
    }
    fitting_chips_ = false;
}

bool PolymarketCommandBar::eventFilter(QObject* watched, QEvent* event) {
    if (watched == category_container_ && event->type() == QEvent::Resize)
        fit_category_chips();
    return QWidget::eventFilter(watched, event);
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
    if (!layout)
        return;
    for (int i = 0; i < layout->count(); ++i) {
        auto* w = qobject_cast<QPushButton*>(layout->itemAt(i)->widget());
        if (!w)
            continue;
        const bool is_active = (cat == "ALL" && i == 0) || (cat != "ALL" && w->text() == cat.toUpper());
        w->setProperty("active", is_active);
        w->style()->unpolish(w);
        w->style()->polish(w);
    }
}

void PolymarketCommandBar::set_loading(bool loading) {
    refresh_btn_->setEnabled(!loading);
    refresh_btn_->setText(loading ? tr("…") : tr("↻"));
}

void PolymarketCommandBar::set_ws_status(bool connected) {
    ws_connected_ = connected;
    // Say what "OFF" means. Kalshi's streaming client is a stub (it needs signed WS
    // headers that aren't wired yet) and the Fincept demo adapter has no feed at all,
    // so for those the indicator can never turn on — that read as a broken connection.
    if (connected) {
        ws_indicator_->setToolTip(tr("Live WebSocket feed connected"));
    } else if (presentation_.exchange_id == QStringLiteral("kalshi") ||
               presentation_.exchange_id == QStringLiteral("fincept")) {
        ws_indicator_->setToolTip(tr("Live streaming is not available for %1 in this build — prices, books and trades "
                                     "update when you refresh or re-select a market.")
                                      .arg(presentation_.display_name));
    } else {
        ws_indicator_->setToolTip(tr("WebSocket disconnected — it reconnects when a market is selected."));
    }
    if (connected) {
        ws_indicator_->setText(tr("● LIVE"));
        ws_indicator_->setStyleSheet(
            QString("color: %1; background: %2; font-size: " + fincept::ui::fonts::px(-5) + "px; "
                    "font-weight: 700; padding: 2px 8px; border: 1px solid %3;")
                .arg(colors::SUCCESS(), colors::alpha(colors::SUCCESS, 0.15), colors::alpha(colors::SUCCESS, 0.40)));
    } else {
        ws_indicator_->setText(tr("○ OFF"));
        ws_indicator_->setStyleSheet(QString("color: %1; background: transparent; font-size: " + fincept::ui::fonts::px(-5) + "px; "
                                             "font-weight: 700; padding: 2px 8px; border: 1px solid %2;")
                                         .arg(colors::TEXT_TERTIARY(), colors::BORDER_DIM()));
    }
}

void PolymarketCommandBar::set_market_count(int count) {
    count_label_->setText(QString::number(count));
}

void PolymarketCommandBar::set_account_status(bool connected, const QString& label) {
    if (!account_chip_)
        return;
    account_connected_ = connected;
    account_label_ = label;
    if (!account_read_error_.isEmpty()) {
        // Unreadable is neither "connected" nor "no account": say so, click retries.
        account_chip_->setText(tr("⚠ UNREADABLE · RETRY"));
        account_chip_->setToolTip(account_read_error_ + QLatin1Char('\n') +
                                  tr("Nothing was changed. Click to read the stored account again."));
        account_chip_->setStyleSheet(QString("#polyAccountChip { background: %2; color: %1; "
                                             "border: 1px solid %3; font-size: %5px; "
                                             "font-weight: 700; padding: 3px 10px; }"
                                             "#polyAccountChip:hover { background: %4; }")
                                         .arg(colors::WARNING(), colors::alpha(colors::WARNING, 0.12),
                                              colors::alpha(colors::WARNING, 0.40), colors::alpha(colors::WARNING, 0.22))
                                         .arg(fincept::ui::fonts::px(-5)));
        return;
    }
    account_chip_->setToolTip(connected ? tr("Trading account connected — click to manage it")
                                        : tr("Connect a trading account"));
    if (connected) {
        account_chip_->setText(QString("✓ %1").arg(label.isEmpty() ? tr("Account") : label));
        account_chip_->setStyleSheet(QString("#polyAccountChip { background: %2; color: %1; "
                                             "border: 1px solid %3; font-size: " + fincept::ui::fonts::px(-5) + "px; "
                                             "font-weight: 700; padding: 3px 10px; }"
                                             "#polyAccountChip:hover { background: %4; }")
                                         .arg(colors::SUCCESS(), colors::alpha(colors::SUCCESS, 0.12),
                                              colors::alpha(colors::SUCCESS, 0.40), colors::alpha(colors::SUCCESS, 0.22)));
    } else {
        account_chip_->setText(tr("CONNECT"));
        account_chip_->setStyleSheet(
            QString("#polyAccountChip { background: transparent; color: %1; "
                    "border: 1px solid %2; font-size: " + fincept::ui::fonts::px(-5) + "px; font-weight: 700; padding: 3px 10px; }"
                    "#polyAccountChip:hover { color: %3; border-color: %4; }")
                .arg(colors::TEXT_SECONDARY(), colors::BORDER_MED(), colors::TEXT_PRIMARY(), colors::BORDER_BRIGHT()));
    }
}

void PolymarketCommandBar::set_account_unreadable(const QString& reason) {
    if (reason == account_read_error_)
        return;
    account_read_error_ = reason;
    set_account_status(account_connected_, account_label_);
}

void PolymarketCommandBar::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void PolymarketCommandBar::retranslateUi() {
    if (exchange_combo_)
        exchange_combo_->setToolTip(tr("Switch prediction market exchange"));
    if (search_input_)
        search_input_->setPlaceholderText(tr("Search markets..."));
    if (refresh_btn_)
        refresh_btn_->setToolTip(tr("Refresh"));

    // Sort combo display strings (logical sort key is index-mapped via SORT_KEYS).
    if (sort_combo_) {
        const QSignalBlocker block(sort_combo_);
        const int idx = sort_combo_->currentIndex();
        const QStringList labels = {tr("VOLUME"), tr("LIQUIDITY"), tr("DATE")};
        for (int i = 0; i < sort_combo_->count() && i < labels.size(); ++i)
            sort_combo_->setItemText(i, labels[i]);
        sort_combo_->setCurrentIndex(idx);
    }

    // State-bearing widgets — re-derive from stored state so the new language
    // is reflected. View pills + category chips carry exchange code values and
    // are rebuilt from the presentation, not retranslated.
    set_ws_status(ws_connected_);
    set_account_status(account_connected_, account_label_);
}

void PolymarketCommandBar::set_exchanges(const QStringList& ids, const QStringList& labels, const QString& active_id) {
    if (!exchange_combo_)
        return;
    const QSignalBlocker block(exchange_combo_);
    exchange_combo_->clear();
    const int n = qMin(ids.size(), labels.size());
    int active_idx = -1;
    for (int i = 0; i < n; ++i) {
        exchange_combo_->addItem(labels[i], ids[i]);
        if (ids[i] == active_id)
            active_idx = i;
    }
    if (active_idx >= 0)
        exchange_combo_->setCurrentIndex(active_idx);
}

} // namespace fincept::screens::polymarket
