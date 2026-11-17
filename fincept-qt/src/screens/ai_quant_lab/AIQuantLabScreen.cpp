// src/screens/ai_quant_lab/AIQuantLabScreen.cpp
#include "screens/ai_quant_lab/AIQuantLabScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/ai_quant_lab/QuantModulePanel.h"
#include "screens/common/CommandRows.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QCoreApplication>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QSet>

namespace fincept::screens {

using namespace fincept::services::quant;

// ── Constructor ───────────────────────────────────────────────────────────────

AIQuantLabScreen::AIQuantLabScreen(QWidget* parent) : QWidget(parent) {
    modules_ = all_quant_modules();
    build_ui();
    // Sheets and the sidebar widths are re-derived once a theme / font change has settled (sizes
    // must not be touched inside theme_changed).
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() { refresh_theme(); });
    LOG_DEBUG("AIQuantLab", "Screen constructed");
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

// ── UI construction ───────────────────────────────────────────────────────────

void AIQuantLabScreen::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(build_top_bar());

    // (called at bottom of build_ui — after all child widgets exist)

    auto* body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);

    body->addWidget(build_left_sidebar());

    // Content stack — panels are lazy-constructed via get_or_create_panel()
    content_stack_ = new QStackedWidget(this);
    body->addWidget(content_stack_, 1);

    body->addWidget(build_right_sidebar());

    auto* body_w = new QWidget(this);
    body_w->setLayout(body);
    root->addWidget(body_w, 1);

    root->addWidget(build_status_bar());

    retranslateUi();
    refresh_theme();
}

// ── Top bar ───────────────────────────────────────────────────────────────────
// Badge bar: compact scrollable row of module short-labels (like EconomicsScreen).
// Replaces the old overflowing QHBoxLayout of 24 buttons.

QWidget* AIQuantLabScreen::build_top_bar() {
    top_bar_ = new QWidget(this);
    top_bar_->setFixedHeight(40);

    auto* hl = new QHBoxLayout(top_bar_);
    hl->setContentsMargins(8, 0, 8, 0);
    hl->setSpacing(8);

    // Brand chip (text set in retranslateUi)
    brand_lbl_ = new QLabel(top_bar_);
    brand_lbl_->setObjectName("aqBrand");
    hl->addWidget(brand_lbl_);

    // Separator
    auto* div = new QWidget(top_bar_);
    div->setFixedSize(1, 20);
    div->setObjectName("aqTopDiv");
    hl->addWidget(div);

    // Scrollable badge bar
    badge_scroll_ = new QScrollArea(top_bar_);
    badge_scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    badge_scroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    badge_scroll_->setFrameShape(QFrame::NoFrame);
    badge_scroll_->setWidgetResizable(true);
    badge_scroll_->setFixedHeight(34);

    badge_bar_ = new QWidget(this);
    auto* bhl = new QHBoxLayout(badge_bar_);
    bhl->setContentsMargins(0, 3, 0, 3);
    bhl->setSpacing(3);

    for (int i = 0; i < modules_.size(); ++i) {
        const auto& mod = modules_[i];
        auto* btn = new QPushButton(mod.short_label, badge_bar_);
        btn->setToolTip(mod.label);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedHeight(26);
        // Short labels ("FACTOR", "BTEST") are meaningless to a screen reader —
        // announce the full module name instead.
        btn->setAccessibleName(mod.label);
        btn->setAccessibleDescription(mod.description);
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_module_selected(i); });
        bhl->addWidget(btn);
        badge_buttons_.append(btn);
    }
    bhl->addStretch();

    badge_scroll_->setWidget(badge_bar_);
    hl->addWidget(badge_scroll_, 1);

    // Module count chip (text set in retranslateUi)
    module_count_lbl_ = new QLabel(top_bar_);
    module_count_lbl_->setObjectName("aqModuleCount");
    hl->addWidget(module_count_lbl_);

    return top_bar_;
}

// ── Left sidebar ──────────────────────────────────────────────────────────────

QWidget* AIQuantLabScreen::build_left_sidebar() {
    left_panel_ = new QWidget(this);
    left_panel_->setFixedWidth(200);

    auto* vl = new QVBoxLayout(left_panel_);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Header
    auto* header = new QWidget(left_panel_);
    header->setObjectName("aqSidebarHeader");
    header->setFixedHeight(36);
    auto* hhl = new QHBoxLayout(header);
    hhl->setContentsMargins(12, 0, 12, 0);
    sidebar_title_ = new QLabel(header);
    sidebar_title_->setObjectName("aqSidebarTitle");
    hhl->addWidget(sidebar_title_);
    hhl->addStretch();
    vl->addWidget(header);

    // Scrollable module list
    auto* scroll = new QScrollArea(left_panel_);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea { border:none; background:transparent; }"
                          "QScrollBar:vertical { width:4px; }"
                          "QScrollBar::handle:vertical { border-radius:2px; }");

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
            cat->setObjectName("aqCatLabel");
            left_items_layout_->addWidget(cat);
        }

        auto* btn = new QPushButton(mod.label, list);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip(mod.description);
        btn->setAccessibleName(mod.label);
        btn->setAccessibleDescription(mod.description);
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_module_selected(i); });
        left_items_layout_->addWidget(btn);
        module_buttons_.append(btn);
    }
    left_items_layout_->addStretch();
    scroll->setWidget(list);
    vl->addWidget(scroll, 1);

    left_panel_->setAccessibleName(tr("Quant module list"));
    return left_panel_;
}

// ── Right sidebar ─────────────────────────────────────────────────────────────

QWidget* AIQuantLabScreen::build_right_sidebar() {
    right_panel_ = new QWidget(this);
    right_panel_->setFixedWidth(220);

    auto* vl = new QVBoxLayout(right_panel_);
    vl->setContentsMargins(12, 12, 12, 12);
    vl->setSpacing(10);

    info_title_ = new QLabel(right_panel_);
    info_title_->setObjectName("aqInfoTitle");
    vl->addWidget(info_title_);

    right_title_ = new QLabel(right_panel_);
    right_title_->setObjectName("aqRightTitle");
    vl->addWidget(right_title_);

    right_category_ = new QLabel(right_panel_);
    right_category_->setObjectName("aqRightCategory");
    vl->addWidget(right_category_);

    right_desc_ = new QLabel(right_panel_);
    right_desc_->setObjectName("aqRightDesc");
    right_desc_->setWordWrap(true);
    vl->addWidget(right_desc_);

    right_script_ = new QLabel(right_panel_);
    right_script_->setObjectName("aqRightScript");
    vl->addWidget(right_script_);

    // Platform stats card
    stats_card_ = new QWidget(right_panel_);
    stats_card_->setObjectName("aqStatsCard");
    auto* svl = new QVBoxLayout(stats_card_);
    svl->setContentsMargins(10, 10, 10, 10);
    svl->setSpacing(4);

    stats_title_ = new QLabel(stats_card_);
    stats_title_->setObjectName("aqStatsTitle");
    svl->addWidget(stats_title_);

    // Stat labels stored as members so retranslateUi can reset them. The
    // values ("30+", "5", "25+") are constants displayed verbatim.
    auto add_stat = [&](QLabel*& label_out, const QString& value) {
        auto* row = new QWidget(stats_card_);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        label_out = new QLabel(row);
        label_out->setObjectName("aqStatLabel");
        auto* val = new QLabel(value, row);
        val->setObjectName("aqStatValue");
        rl->addWidget(label_out);
        rl->addStretch();
        rl->addWidget(val);
        svl->addWidget(row);
    };
    // "Python Scripts" was a hard-coded "25+". Derive it from the module table
    // so the card can't drift away from what the terminal can actually run.
    QSet<QString> distinct_scripts;
    for (const auto& m : modules_)
        distinct_scripts.insert(m.script);

    add_stat(stat_modules_lbl_, QString::number(modules_.size()));
    add_stat(stat_ml_lbl_, "30+");
    add_stat(stat_rl_lbl_, "5"); // PPO, DQN, A2C, SAC, TD3 — see qlib_rl.py
    add_stat(stat_py_lbl_, QString::number(distinct_scripts.size()));
    vl->addWidget(stats_card_);

    vl->addStretch();
    return right_panel_;
}

// ── Status bar ────────────────────────────────────────────────────────────────

QWidget* AIQuantLabScreen::build_status_bar() {
    status_bar_ = new QWidget(this);
    status_bar_->setMinimumHeight(24);

    auto* hl = new QHBoxLayout(status_bar_);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(16);

    engine_lbl_ = new QLabel(status_bar_);
    engine_lbl_->setObjectName("aqStatusLabel");
    // Engine value is a fixed product/tech identifier — kept as-is.
    status_engine_val_ = new QLabel("QLIB + GS QUANT + PYTHON", status_bar_);
    status_engine_val_->setObjectName("aqStatusValue");
    hl->addWidget(engine_lbl_);
    hl->addWidget(status_engine_val_);
    hl->addStretch();

    status_ready_lbl_ = new QLabel(status_bar_);
    status_ready_lbl_->setObjectName("aqStatusReady");
    hl->addWidget(status_ready_lbl_);

    return status_bar_;
}

// ── Theme refresh ─────────────────────────────────────────────────────────────

void AIQuantLabScreen::refresh_theme() {
    using namespace ui::colors;
    using namespace ui::fonts;

    // Root
    setStyleSheet(QString("background:%1;").arg(BG_BASE()));

    // Sidebar widths were chosen for the default font size; scale them with it
    // so module names are not clipped at a larger one.
    left_panel_->setFixedWidth(200 * font_px() / ui::ThemeManager::kDefaultFontPx);
    right_panel_->setFixedWidth(220 * font_px() / ui::ThemeManager::kDefaultFontPx);

    // Top bar
    top_bar_->setStyleSheet(QString("background:%1; border-bottom:1px solid %2;").arg(BG_RAISED(), BORDER_MED()));

    // Brand chip
    // findChild is safe here — it's a named child of top_bar_
    if (auto* brand = top_bar_->findChild<QLabel*>("aqBrand"))
        brand->setStyleSheet(QString("color:%1; font-weight:700; font-size:%2px;"
                                     "padding:3px 10px; background:%3;"
                                     "border:1px solid %4; border-radius:2px;")
                                 .arg(INFO(), px(-2), alpha(INFO, 0.10), alpha(INFO, 0.28)));

    if (auto* div = top_bar_->findChild<QWidget*>("aqTopDiv"))
        div->setStyleSheet(QString("background:%1;").arg(BORDER_DIM()));

    // Badge scroll area + bar
    badge_scroll_->setStyleSheet(QString("QScrollArea { background:%1; border:none; }"
                                         "QScrollBar:horizontal { height:0px; }")
                                     .arg(BG_RAISED()));
    badge_bar_->setStyleSheet(QString("background:%1;").arg(BG_RAISED()));

    if (auto* lbl = top_bar_->findChild<QLabel*>("aqModuleCount"))
        lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700;"
                                   "padding:3px 8px; background:%3;"
                                   "border:1px solid %4; border-radius:2px;")
                               .arg(POSITIVE(), px(-2), alpha(POSITIVE, 0.08), alpha(POSITIVE, 0.25)));

    // Left sidebar
    left_panel_->setStyleSheet(QString("background:%1; border-right:1px solid %2;").arg(BG_SURFACE(), BORDER_MED()));

    if (auto* hdr = left_panel_->findChild<QWidget*>("aqSidebarHeader"))
        hdr->setStyleSheet(QString("background:%1; border-bottom:1px solid %2;").arg(BG_SURFACE(), BORDER_MED()));

    sidebar_title_->setStyleSheet(
        QString("color:%1; font-weight:700; font-size:%2px; letter-spacing:1px;").arg(CYAN(), px(-2)));

    // Category labels
    for (auto* lbl : left_panel_->findChildren<QLabel*>("aqCatLabel"))
        lbl->setStyleSheet(QString("color:%1; font-weight:700; font-size:%2px;"
                                   "padding:8px 12px 4px 12px; letter-spacing:1px; background:transparent;")
                               .arg(TEXT_TERTIARY(), px(-2)));

    // Right sidebar
    right_panel_->setStyleSheet(QString("background:%1; border-left:1px solid %2;").arg(BG_SURFACE(), BORDER_MED()));

    if (auto* t = right_panel_->findChild<QLabel*>("aqInfoTitle"))
        t->setStyleSheet(QString("color:%1; font-weight:700; font-size:%2px; letter-spacing:1px;").arg(CYAN(), px(-2)));

    right_category_->setStyleSheet(
        QString("color:%1; font-size:%2px; background:transparent;").arg(TEXT_SECONDARY(), px(-2)));

    right_desc_->setStyleSheet(QString("color:%1; font-size:%2px; background:transparent;").arg(TEXT_PRIMARY(), px(-2)));

    right_script_->setStyleSheet(QString("color:%1; font-size:%2px; padding:6px;"
                                         "background:%3; border:1px solid %4; border-radius:2px;")
                                     .arg(TEXT_SECONDARY(), px(-2), BG_RAISED(), BORDER_DIM()));

    stats_card_->setStyleSheet(
        QString("background:%1; border:1px solid %2; border-radius:3px;").arg(BG_RAISED(), BORDER_DIM()));

    stats_title_->setStyleSheet(
        QString("color:%1; font-weight:700; font-size:%2px; letter-spacing:0.5px;").arg(CYAN(), px(-2)));

    for (auto* lbl : stats_card_->findChildren<QLabel*>("aqStatLabel"))
        lbl->setStyleSheet(QString("color:%1; font-size:%2px; background:transparent;").arg(TEXT_SECONDARY(), px(-2)));

    for (auto* val : stats_card_->findChildren<QLabel*>("aqStatValue"))
        val->setStyleSheet(
            QString("color:%1; font-weight:700; font-size:%2px; background:transparent;").arg(TEXT_PRIMARY(), px(-2)));

    // Status bar
    status_bar_->setStyleSheet(QString("background:%1; border-top:1px solid %2;").arg(BG_RAISED(), BORDER_MED()));

    for (auto* lbl : status_bar_->findChildren<QLabel*>("aqStatusLabel"))
        lbl->setStyleSheet(QString("color:%1; font-size:%2px; background:transparent;").arg(TEXT_SECONDARY(), px(-2)));

    status_engine_val_->setStyleSheet(
        QString("color:%1; font-weight:700; font-size:%2px; background:transparent;").arg(POSITIVE(), px(-2)));

    status_ready_lbl_->setStyleSheet(
        QString("color:%1; font-weight:700; font-size:%2px; background:transparent;").arg(POSITIVE(), px(-2)));

    // Re-apply selection styles so active module stays highlighted after theme switch
    update_sidebar_selection(/*all=*/true);
}

// ── Lazy panel construction ───────────────────────────────────────────────────

QuantModulePanel* AIQuantLabScreen::get_or_create_panel(int index) {
    if (panels_.contains(index))
        return panels_[index];

    // Built under its final parent: an unparented panel was polished once alone and again, whole,
    // when the stack adopted it.
    auto* panel = new QuantModulePanel(modules_[index], content_stack_);
    panels_[index] = panel;
    content_stack_->addWidget(panel);
    LOG_DEBUG("AIQuantLab", "Created panel for: " + modules_[index].id);
    return panel;
}

// ── Module selection ──────────────────────────────────────────────────────────

void AIQuantLabScreen::on_module_selected(int index) {
    if (index < 0 || index >= modules_.size())
        return;
    active_index_ = index;

    QuantModulePanel* panel = get_or_create_panel(index);
    content_stack_->setCurrentWidget(panel);

    update_sidebar_selection();
    update_right_panel();
    ScreenStateManager::instance().notify_changed(this);
}

void AIQuantLabScreen::update_sidebar_selection(bool all) {
    using namespace ui::colors;

    // A module click restyles the two buttons whose state changed (it used to restyle all 46 —
    // 46 stylesheet parses and re-polishes per click); a theme change restyles every one.
    const int previous = styled_index_;
    styled_index_ = active_index_;
    auto needs_style = [&](int i) { return all || i == active_index_ || i == previous; };

    // Module colours are category colours, not tokens: text and borders use them made
    // legible on the active theme, washes are tints of them into the background.
    for (int i = 0; i < module_buttons_.size(); ++i) {
        if (!needs_style(i))
            continue;
        const auto& mod = modules_[i];
        const bool active = (i == active_index_);
        const QString fg = legible(mod.color);
        const QString wash = tint(mod.color, 0.08);

        module_buttons_[i]->setStyleSheet(QString("QPushButton { text-align:left; padding:6px 12px; border:none;"
                                                  "border-left:2px solid %1; color:%2; font-size:" + fincept::ui::fonts::px(-4) + "px;"
                                                  "font-weight:%3; background:%4; }"
                                                  "QPushButton:hover { background:%5; color:%6; }")
                                              .arg(active ? fg : "transparent")
                                              .arg(active ? fg : QString(TEXT_SECONDARY()))
                                              .arg(active ? "700" : "400")
                                              .arg(active ? wash : "transparent")
                                              .arg(wash, fg));
    }

    for (int i = 0; i < badge_buttons_.size(); ++i) {
        if (!needs_style(i))
            continue;
        const auto& mod = modules_[i];
        const bool active = (i == active_index_);
        const QString fg = legible(mod.color);

        badge_buttons_[i]->setStyleSheet(
            QString("QPushButton { color:%1; font-size:" + fincept::ui::fonts::px(-5) + "px; font-weight:%2;"
                    "padding:0 8px; border:1px solid %3; border-radius:2px; background:%4; }"
                    "QPushButton:hover { color:%5; border-color:%6;"
                    "background:%7; }")
                .arg(active ? fg : QString(TEXT_SECONDARY()))
                .arg(active ? "700" : "400")
                .arg(active ? tint(mod.color, 0.35) : "transparent")
                .arg(active ? tint(mod.color, 0.12) : "transparent")
                .arg(fg, tint(mod.color, 0.40), tint(mod.color, 0.08)));
    }
}

void AIQuantLabScreen::update_right_panel() {
    const auto& mod = modules_[active_index_];
    right_title_->setText(mod.label.toUpper());
    right_title_->setStyleSheet(
        QString("color:%1; font-weight:700; font-size:" + fincept::ui::fonts::px(-3) + "px; background:transparent;")
            .arg(ui::colors::legible(mod.color)));
    right_category_->setText(tr("%1 module").arg(QString(mod.category).replace('_', '/')));
    right_desc_->setText(mod.description);
    right_script_->setText(tr("Script: %1").arg(mod.script));
}

// ── Re-translation ───────────────────────────────────────────────────────────
// Static chrome only: top-bar brand + module count, sidebar headers, info
// panel title, stats card, status bar. Module labels + categories are
// data-derived (see services::quant::QuantModule), kept verbatim.

void AIQuantLabScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void AIQuantLabScreen::retranslateUi() {
    if (brand_lbl_)
        brand_lbl_->setText(tr("AI QUANT LAB"));
    if (module_count_lbl_)
        module_count_lbl_->setText(tr("%1 MODULES").arg(modules_.size()));
    if (sidebar_title_)
        sidebar_title_->setText(tr("MODULES"));
    if (info_title_)
        info_title_->setText(tr("MODULE INFO"));
    if (stats_title_)
        stats_title_->setText(tr("PLATFORM STATS"));
    if (stat_modules_lbl_)
        stat_modules_lbl_->setText(tr("Modules"));
    if (stat_ml_lbl_)
        stat_ml_lbl_->setText(tr("ML Models"));
    if (stat_rl_lbl_)
        stat_rl_lbl_->setText(tr("RL Algorithms"));
    if (stat_py_lbl_)
        stat_py_lbl_->setText(tr("Python Scripts"));
    if (engine_lbl_)
        engine_lbl_->setText(tr("ENGINE:"));
    if (status_ready_lbl_)
        status_ready_lbl_->setText(tr("READY"));

    // Re-render the active module's info panel so the "{category} module" /
    // "Script: {path}" sub-labels pick up the new language.
    if (!modules_.isEmpty())
        update_right_panel();
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap AIQuantLabScreen::save_state() const {
    return {{"module_index", active_index_}};
}

void AIQuantLabScreen::restore_state(const QVariantMap& state) {
    const int idx = state.value("module_index", 0).toInt();
    if (idx >= 0 && idx < modules_.size()) {
        // The first showEvent() selects module 0; without this it ran AFTER the restore
        // and replaced the saved module with the first one on every launch.
        first_show_ = false;
        on_module_selected(idx);
    }
}

bool AIQuantLabScreen::open_section(const QString& section, const QString& focus) {
    Q_UNUSED(focus); // the module forms take no focus from a link
    QString key = section.trimmed();
    if (key.isEmpty())
        return true;
    QString rest;
    if (command_rows::split_kind(key, "module", &rest))
        key = rest;
    QString view;
    const qsizetype slash = key.indexOf(QLatin1Char('/'));
    if (slash >= 0) {
        view = key.mid(slash + 1).trimmed();
        key = key.left(slash).trimmed();
    }
    int index = -1;
    for (int i = 0; i < modules_.size() && index < 0; ++i)
        if (modules_[i].id == key)
            index = i;
    for (int i = 0; i < modules_.size() && index < 0; ++i)
        if (modules_[i].id.compare(key, Qt::CaseInsensitive) == 0)
            index = i;
    if (index < 0)
        return false;
    // A view link checks the view first (the panel is built lazily, as a click would; it is not shown),
    // so an unknown view leaves the module on screen as it was.
    if (!view.isEmpty() && !get_or_create_panel(index)->open_view_tab(view, /*select=*/false))
        return false;
    first_show_ = false; // the first showEvent() must not replace this module with the first one
    if (index != active_index_ || content_stack_->currentWidget() != panels_.value(index))
        on_module_selected(index); // the module button's click path
    if (!view.isEmpty())
        panels_.value(index)->open_view_tab(view, /*select=*/true);
    return true;
}

// ── Search box rows (core/command/CommandRegistry.h; screens/common/CommandRows.h) ──
namespace {

struct AqlLinkRow {
    const char* section;
    const char* title;
    const char* keywords;
};

// One row per module (ids as all_quant_modules(); the seed's "ai_quant_lab.<id>" rows are replaced).
const AqlLinkRow kAqlLinkModules[] = {
    {"factor_discovery", QT_TRANSLATE_NOOP("fincept::screens::AIQuantLabScreen", "Factor discovery"),
     "alpha factors, factor mining, qlib, factor library, alpha158"},
    {"model_library", QT_TRANSLATE_NOOP("fincept::screens::AIQuantLabScreen", "Model library"),
     "ml models, machine learning, lightgbm, xgboost, lstm, transformer, train model"},
    {"backtesting", QT_TRANSLATE_NOOP("fincept::screens::AIQuantLabScreen", "Quant Lab backtesting"),
     "backtest, qlib backtest, portfolio backtest, strategy test"},
    {"live_signals", QT_TRANSLATE_NOOP("fincept::screens::AIQuantLabScreen", "Live signals"),
     "trading signals, predictions, model signals, feature importance"},
    {"deep_agent", QT_TRANSLATE_NOOP("fincept::screens::AIQuantLabScreen", "Deep research agent"),
     "deep agent, rd-agent, ai research agent, llm agent, autonomous research"},
    {"rl_trading", QT_TRANSLATE_NOOP("fincept::screens::AIQuantLabScreen", "Reinforcement learning trading"),
     "rl trading, reinforcement learning, ppo, dqn, a2c, sac, td3, trading agent"},
    {"online_learning", QT_TRANSLATE_NOOP("fincept::screens::AIQuantLabScreen", "Online learning"),
     "incremental learning, concept drift, streaming model"},
    {"meta_learning", QT_TRANSLATE_NOOP("fincept::screens::AIQuantLabScreen", "Meta learning"),
     "few-shot learning, automl, model selection, ensemble, hyperparameter tuning"},
    {"hft", QT_TRANSLATE_NOOP("fincept::screens::AIQuantLabScreen", "High-frequency trading"),
     "hft, order book, market making, microstructure, slippage, latency"},
    {"rolling_retraining", QT_TRANSLATE_NOOP("fincept::screens::AIQuantLabScreen", "Rolling retraining"),
     "retrain, rolling window, walk forward, model refresh"},
    {"advanced_models", QT_TRANSLATE_NOOP("fincept::screens::AIQuantLabScreen", "Deep learning models"),
     "advanced models, lstm, gru, transformer, pytorch, neural network, deep learning"},
    {"feature_engineering", QT_TRANSLATE_NOOP("fincept::screens::AIQuantLabScreen", "Feature engineering"),
     "features, technical indicators, feature selection, expression engine"},
    {"portfolio_opt", QT_TRANSLATE_NOOP("fincept::screens::AIQuantLabScreen", "Portfolio optimization"),
     "optimization, black-litterman, hrp, min variance, max sharpe, efficient frontier"},
    {"factor_evaluation", QT_TRANSLATE_NOOP("fincept::screens::AIQuantLabScreen", "Factor evaluation"),
     "ic, information coefficient, factor returns, factor scoring, combine factors"},
    {"strategy_builder", QT_TRANSLATE_NOOP("fincept::screens::AIQuantLabScreen", "Quant strategy builder"),
     "strategy, topk dropout, enhanced indexing, risk parity, mean variance"},
    {"data_processors", QT_TRANSLATE_NOOP("fincept::screens::AIQuantLabScreen", "Data processors"),
     "normalize, normalization, zscore, winsorize, cs rank, fillna, preprocessing, pipeline"},
    {"quant_reporting", QT_TRANSLATE_NOOP("fincept::screens::AIQuantLabScreen", "Quant reports"),
     "reporting, ic report, cumulative returns, risk report, model performance"},
    {"cfa_quant", QT_TRANSLATE_NOOP("fincept::screens::AIQuantLabScreen", "CFA quant methods"),
     "cfa, trend analysis, arima, pca, bootstrap, ml prediction"},
    {"gs_quant", QT_TRANSLATE_NOOP("fincept::screens::AIQuantLabScreen", "GS Quant risk"),
     "gs quant, goldman sachs, var, greeks, component var, stress test, risk metrics"},
    {"statsmodels", QT_TRANSLATE_NOOP("fincept::screens::AIQuantLabScreen", "Statsmodels"),
     "statistics, ols, regression, arima, stationarity, adf, granger, cointegration, acf, pacf"},
    {"functime", QT_TRANSLATE_NOOP("fincept::screens::AIQuantLabScreen", "Functime forecasting"),
     "functime, forecasting, anomaly detection, seasonality, prediction intervals"},
    {"fortitudo", QT_TRANSLATE_NOOP("fincept::screens::AIQuantLabScreen", "Fortitudo portfolio tools"),
     "fortitudo, entropy pooling, cvar, mean variance, efficient frontier, exponential decay"},
    {"gluonts", QT_TRANSLATE_NOOP("fincept::screens::AIQuantLabScreen", "GluonTS forecasting"),
     "gluonts, probabilistic forecast, quantile forecast, distribution fit, forecast evaluation"},
};

// A few module tab views a beginner looks for by name ("module:<id>/<view key>", the panel's ViewSpec
// keys). Each opens like a click on the tab; views that train / call an LLM never run by themselves.
const AqlLinkRow kAqlLinkViews[] = {
    {"module:statsmodels/ols", QT_TRANSLATE_NOOP("fincept::screens::AIQuantLabScreen", "OLS regression"),
     "regression, linear regression, ols, beta, statsmodels"},
    {"module:statsmodels/arima", QT_TRANSLATE_NOOP("fincept::screens::AIQuantLabScreen", "ARIMA forecast"),
     "arima, time series model, forecast, statsmodels"},
    {"module:statsmodels/stationarity", QT_TRANSLATE_NOOP("fincept::screens::AIQuantLabScreen", "Stationarity test"),
     "adf, kpss, unit root, stationarity, statsmodels"},
    {"module:statsmodels/granger", QT_TRANSLATE_NOOP("fincept::screens::AIQuantLabScreen", "Granger causality"),
     "granger, causality, lead lag, statsmodels"},
    {"module:gs_quant/var", QT_TRANSLATE_NOOP("fincept::screens::AIQuantLabScreen", "Value at risk"),
     "var, cvar, expected shortfall, value at risk, tail risk"},
    {"module:gs_quant/greeks", QT_TRANSLATE_NOOP("fincept::screens::AIQuantLabScreen", "Option Greeks calculator"),
     "greeks, delta, gamma, vega, theta, black scholes, option pricing"},
    {"module:gs_quant/stress_test", QT_TRANSLATE_NOOP("fincept::screens::AIQuantLabScreen", "Crisis stress test"),
     "stress test, crisis scenarios, 2008, covid, scenario analysis"},
    {"module:fortitudo/efficient_frontier", QT_TRANSLATE_NOOP("fincept::screens::AIQuantLabScreen", "Efficient frontier"),
     "efficient frontier, mean variance, markowitz, portfolio"},
    {"module:fortitudo/cvar_optimize", QT_TRANSLATE_NOOP("fincept::screens::AIQuantLabScreen", "CVaR optimization"),
     "cvar, tail risk, portfolio optimization, conditional value at risk"},
    {"module:portfolio_opt/black_litterman", QT_TRANSLATE_NOOP("fincept::screens::AIQuantLabScreen", "Black-Litterman"),
     "black litterman, views, prior, portfolio optimization"},
    {"module:functime/anomalies", QT_TRANSLATE_NOOP("fincept::screens::AIQuantLabScreen", "Anomaly detection"),
     "anomalies, outliers, anomaly detection, functime"},
    {"module:functime/seasonality", QT_TRANSLATE_NOOP("fincept::screens::AIQuantLabScreen", "Seasonality decomposition"),
     "seasonality, stl, trend, decomposition, functime"},
    {"module:gluonts/probabilistic", QT_TRANSLATE_NOOP("fincept::screens::AIQuantLabScreen", "Probabilistic forecast"),
     "probabilistic forecast, forecast paths, uncertainty, gluonts"},
    {"module:feature_engineering/indicators", QT_TRANSLATE_NOOP("fincept::screens::AIQuantLabScreen", "Technical indicator lab"),
     "indicators, rsi, macd, bollinger, moving average, technical indicators"},
    {"module:hft/orderbook", QT_TRANSLATE_NOOP("fincept::screens::AIQuantLabScreen", "Order book analytics"),
     "order book, depth, bid ask, spread, hft"},
};

QList<command::CommandItem> aql_command_rows() {
    namespace rows = command_rows;
    const QString where = AIQuantLabScreen::tr("AI Quant Lab");
    QList<command::CommandItem> out;
    auto add = [&](const AqlLinkRow& r) {
        QStringList keywords;
        for (const QString& w : QString::fromLatin1(r.keywords).split(QLatin1Char(','), Qt::SkipEmptyParts))
            keywords.append(w.trimmed());
        keywords << QStringLiteral("quant lab");
        out.append(rows::section(QStringLiteral("ai_quant_lab"), QString::fromLatin1(r.section),
                                 QCoreApplication::translate("fincept::screens::AIQuantLabScreen", r.title), keywords,
                                 where));
    };
    for (const AqlLinkRow& r : kAqlLinkModules)
        add(r);
    for (const AqlLinkRow& r : kAqlLinkViews)
        add(r);
    return out;
}

void aql_install_commands() {
    command_rows::install(QStringLiteral("ai_quant_lab"), &aql_command_rows);
}

} // namespace

Q_COREAPP_STARTUP_FUNCTION(aql_install_commands)

} // namespace fincept::screens
