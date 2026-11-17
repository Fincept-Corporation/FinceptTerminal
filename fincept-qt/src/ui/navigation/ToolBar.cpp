#include "ui/navigation/ToolBar.h"

#include "auth/AuthManager.h"
#include "services/agent_tasks/TaskRunService.h"
#include "services/llm/LlmService.h"
#include "services/billing/PlanAccess.h"
#include "ui/pushpins/PushpinBar.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/EnterprisePromo.h"
#include "ui/widgets/ThemeHelpers.h"
#include "ui/widgets/ThemeIcon.h"
#include "ui/widgets/MnemonicText.h"
#include "ui/widgets/PlanHint.h"

#include <QAction>
#include <QDateTime>
#include <QEvent>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QPushButton>
#include <QResizeEvent>
#include <QScreen>
#include <QStyle>
#include <QToolBar>
#include <QToolButton>

#include <algorithm>

namespace fincept::ui {

static QString menu_ss() {
    return QString("QMenuBar{background:transparent;color:%1;border:none;spacing:0;}"
                   "QMenuBar::item{background:transparent;padding:4px 10px;}"
                   "QMenuBar::item:selected{background:%2;color:%3;}")
        .arg(colors::TEXT_SECONDARY())
        .arg(colors::BG_RAISED())
        .arg(colors::TEXT_PRIMARY());
}

static QString popup_ss() {
    return QString("QMenu{background:%1;color:%2;border:1px solid %3;padding:4px 0;}"
                   "QMenu::item{padding:5px 24px 5px 12px;}"
                   "QMenu::item:selected{background:%4;}"
                   "QMenu::item:disabled{color:%5;}"
                   "QMenu::separator{background:%3;height:1px;margin:4px 8px;}")
        .arg(colors::BG_SURFACE())
        .arg(colors::TEXT_PRIMARY())
        .arg(colors::BORDER_MED())
        .arg(colors::BG_RAISED())
        .arg(colors::TEXT_TERTIARY());
}

ToolBar::ToolBar(QWidget* parent) : QWidget(parent) {
    // Heights / widths here are for the default font size; they grow with the
    // user's font (metrics::follow_*, and refresh_theme() for the max widths).
    metrics::follow_fixed_height(this, 32);
    // Fill the whole host toolbar row: apply_responsive_layout() decides what
    // fits in that width (a Preferred policy let QToolBar size this row to its
    // own size hint, which shrinks as items collapse).
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* hl = new QHBoxLayout(this);
    hl->setContentsMargins(4, 0, 6, 0);
    hl->setSpacing(0);

    menu_bar_ = new QMenuBar(this);
    // Keep the menu inline in the toolbar on macOS. Without this, Qt migrates
    // the QMenuBar into the native screen-top application menu, leaving
    // File/Navigate/View/Help missing from the toolbar row on Mac builds.
    menu_bar_->setNativeMenuBar(false);
    menu_bar_->setStyleSheet(menu_ss());
    rebuild_menus();
    hl->addWidget(menu_bar_);

    command_bar_ = new CommandBar(this);
    command_bar_->setMinimumWidth(80);
    command_bar_->setMaximumWidth(280);
    command_bar_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    connect(command_bar_, &CommandBar::navigate_to, this, &ToolBar::navigate_to);
    connect(command_bar_, &CommandBar::dock_command, this, &ToolBar::dock_command);
    connect(command_bar_, &CommandBar::toolbar_action, this, &ToolBar::action_triggered); // menu actions found by search
    hl->addWidget(command_bar_);

    auto sep = [&]() -> QLabel* {
        auto* s = new QLabel("|");
        s->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        hl->addWidget(s);
        separators_.append(s);
        return s;
    };

    auto mk = [](const QString& t) -> QLabel* {
        auto* l = new QLabel(t);
        l->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        return l;
    };

    sep();
    // FINCEPT / TERMINAL are brand marks — set raw, never translated.
    fincept_label_ = mk(QStringLiteral("FINCEPT "));
    hl->addWidget(fincept_label_);
    branding_label_ = mk(QStringLiteral("TERMINAL"));
    hl->addWidget(branding_label_);
    subtitle_label_ = mk({});
    hl->addWidget(subtitle_label_);
    live_gap_ = mk("  ");
    hl->addWidget(live_gap_);
    live_dot_ = mk(QString::fromUtf8("\xe2\x97\x8f")); // U+25CF — pure icon, no translation
    hl->addWidget(live_dot_);
    live_label_ = mk({});
    hl->addWidget(live_label_);

    // AI activity: is the model still working (and on what), or has it finished.
    ai_status_ = mk({});
    ai_status_->setVisible(false);
    hl->addSpacing(6);
    hl->addWidget(ai_status_);
    ai_pulse_timer_ = new QTimer(this);
    ai_pulse_timer_->setInterval(450);
    connect(ai_pulse_timer_, &QTimer::timeout, this, [this]() {
        ai_pulse_on_ = !ai_pulse_on_;
        update_ai_status();
    });
    ai_done_timer_ = new QTimer(this);
    ai_done_timer_->setSingleShot(true);
    ai_done_timer_->setInterval(6000);
    connect(ai_done_timer_, &QTimer::timeout, this, [this]() {
        ai_finished_ = false;
        update_ai_status();
    });
    {
        auto& llm = ai_chat::LlmService::instance();
        ai_running_ = llm.active_requests();
        ai_step_ = llm.activity_text();
        connect(&llm, &ai_chat::LlmService::activity_changed, this, [this](int running, const QString& what) {
            const bool was_running = ai_running_ > 0;
            ai_running_ = running;
            ai_step_ = what;
            if (running > 0) {
                ai_finished_ = false;
                ai_done_timer_->stop();
                if (!ai_pulse_timer_->isActive()) {
                    ai_pulse_on_ = true;
                    ai_pulse_timer_->start();
                }
            } else {
                ai_pulse_timer_->stop();
                if (was_running) {
                    ai_finished_ = true;
                    ai_done_timer_->start();
                }
            }
            update_ai_status();
        });
    }

    sep();

    // Inline pushpin chips — used to live in a separate toolbar row, now
    // embedded directly so symbol pins share the row with menus and clock.
    pushpin_bar_ = new PushpinBar(this);
    pushpin_bar_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    hl->addWidget(pushpin_bar_, 1);

    sep();

    clock_label_ = mk("");
    clock_label_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    hl->addWidget(clock_label_);

    hl->addStretch(0);
    hl->addSpacing(8); // keeps the clock's seconds from touching the user name

    user_label_ = mk("---");
    user_label_->setMaximumWidth(120);
    hl->addWidget(user_label_);
    sep();
    credits_label_ = mk("---");
    credits_label_->setMaximumWidth(100);
    hl->addWidget(credits_label_);
    sep();
    plan_btn_ = new QPushButton(QStringLiteral("---"));
    metrics::follow_fixed_height(plan_btn_, 20);
    plan_btn_->setCursor(Qt::PointingHandCursor);
    plan_btn_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(plan_btn_, &QPushButton::clicked, this, &ToolBar::plan_clicked);
    hl->addWidget(plan_btn_);
    // The plan badge and the balance explain the plan on hover: what is
    // included, what it saved — from the server's plan table (PlanAccess).
    connect(&fincept::billing::PlanAccess::instance(), &fincept::billing::PlanAccess::changed, this, [this]() {
        if (plan_btn_)
            plan_btn_->setToolTip(plan::badge_tooltip());
        if (credits_label_)
            credits_label_->setToolTip(plan::credits_tooltip());
    });
    hl->addSpacing(4);

    // Enterprise CTA. Deliberately added without its own separator: the
    // credits/chat visibility toggles in apply_responsive_layout() index into
    // separators_ by position, so inserting one here would shift them.
    upgrade_btn_ = new QPushButton;
    metrics::follow_fixed_height(upgrade_btn_, 20);
    upgrade_btn_->setCursor(Qt::PointingHandCursor);
    upgrade_btn_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(upgrade_btn_, &QPushButton::clicked, this, &ToolBar::upgrade_clicked);
    hl->addWidget(upgrade_btn_);
    sep();

    chat_mode_btn_ = new QPushButton;
    metrics::follow_fixed_height(chat_mode_btn_, 20);
    chat_mode_btn_->setCursor(Qt::PointingHandCursor);
    chat_mode_btn_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(chat_mode_btn_, &QPushButton::clicked, this, &ToolBar::chat_mode_toggled);
    hl->addWidget(chat_mode_btn_);
    // Agent run badges (no separator of their own: separators_ is indexed by position).
    chat_runs_badge_ = new QLabel(this);
    chat_attention_badge_ = new QLabel(this);
    for (QLabel* badge : {chat_runs_badge_, chat_attention_badge_}) {
        badge->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        badge->setVisible(false);
        hl->addSpacing(3);
        hl->addWidget(badge);
    }
    {
        auto& runs = agent_tasks::TaskRunService::instance();
        chat_active_runs_ = runs.active_count();
        chat_attention_runs_ = runs.attention_count();
        connect(&runs, &agent_tasks::TaskRunService::attention_changed, this, [this](int active, int attention) {
            chat_active_runs_ = active;
            chat_attention_runs_ = attention;
            update_chat_badge();
        });
    }
    sep();

    logout_btn_ = new QPushButton;
    logout_btn_->setProperty("fincept_agent_risk", "account"); // agents: signs out
    metrics::follow_fixed_height(logout_btn_, 20);
    logout_btn_->setCursor(Qt::PointingHandCursor);
    logout_btn_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(logout_btn_, &QPushButton::clicked, this, &ToolBar::logout_clicked);
    hl->addWidget(logout_btn_);

    // Overflow: shown only when apply_responsive_layout() had to collapse an
    // action (or the account info) out of the row; its menu lists those.
    overflow_btn_ = new QToolButton(this);
    overflow_btn_->setIcon(theme_icon(ThemeGlyph::Menu));
    overflow_btn_->setCursor(Qt::PointingHandCursor);
    overflow_btn_->setPopupMode(QToolButton::InstantPopup);
    overflow_btn_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    metrics::follow_fixed_height(overflow_btn_, 20);
    auto* overflow_menu = new QMenu(overflow_btn_);
    overflow_btn_->setMenu(overflow_menu);
    connect(overflow_menu, &QMenu::aboutToShow, this, [this, overflow_menu]() { fill_overflow_menu(overflow_menu); });
    overflow_btn_->setVisible(false);
    hl->addSpacing(4);
    hl->addWidget(overflow_btn_);

    retranslateUi();

    clock_timer_ = new QTimer(this);
    clock_timer_->setInterval(1000);
    connect(clock_timer_, &QTimer::timeout, this, &ToolBar::update_clock);
    // Started in showEvent() and stopped in hideEvent() (§P3).
    update_clock();

    connect(&auth::AuthManager::instance(), &auth::AuthManager::auth_state_changed, this,
            &ToolBar::refresh_user_display);
    // The credits chip follows the balance billed responses report (X-Credits-
    // Balance) instead of re-polling /user/profile for it.
    connect(&auth::AuthManager::instance(), &auth::AuthManager::credit_balance_changed, this,
            [this](double) { refresh_user_display(); });

    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this, [this](const ThemeTokens&) {
        refresh_theme();
        refresh_user_display(); // re-elide the user name for the new width / font
    });
    // Widths follow the font: re-fit the row once the change is fully applied
    // (apply_responsive_layout() does nothing while the handlers run).
    connect(&ThemeManager::instance(), &ThemeManager::appearance_settled, this,
            [this]() { apply_responsive_layout(width()); });

    refresh_theme();
    refresh_user_display();
}

void ToolBar::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(e);
}

void ToolBar::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    update_clock(); // catch up immediately after being hidden
    clock_timer_->start();
}

void ToolBar::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    clock_timer_->stop();
}

void ToolBar::retranslateUi() {
    if (subtitle_label_)
        subtitle_label_->setText(tr("  |  PROFESSIONAL RESEARCH DESK"));
    if (live_label_)
        live_label_->setText(tr(" LIVE"));
    if (plan_btn_)
        plan_btn_->setToolTip(plan::badge_tooltip());
    if (credits_label_)
        credits_label_->setToolTip(plan::credits_tooltip());
    if (upgrade_btn_) {
        upgrade_btn_->setText(tr("ENTERPRISE"));
        upgrade_btn_->setToolTip(tr("Fincept Terminal Enterprise — the private edition for firms"));
    }
    if (chat_mode_btn_) {
        // Keep the ⬡ glyph (U+2B21) as a visual icon; only the label after it
        // translates. Must use fromUtf8 to decode the UTF-8 bytes — wrapping
        // them in QStringLiteral widens each byte into its own UTF-16 unit and
        // renders as mojibake ("â¬¡").
        chat_mode_btn_->setText(QString::fromUtf8("\xe2\xac\xa1 ") + tr("CHAT"));
        chat_mode_btn_->setToolTip(tr("Switch to Chat Mode (F9)"));
        update_chat_badge(); // run counts in the tooltip / badges, in the new language
    }
    if (logout_btn_)
        logout_btn_->setText(tr("LOGOUT"));
    if (overflow_btn_) {
        overflow_btn_->setToolTip(tr("More — items that do not fit in the toolbar"));
        overflow_btn_->setAccessibleName(tr("More toolbar items"));
    }
    // Rebuild menus so the new translator applies to every QAction label.
    rebuild_menus();
    // Refresh user display so "FREE" / "---" placeholders pick up new locale.
    refresh_user_display();
}

void ToolBar::rebuild_menus() {
    if (!menu_bar_)
        return;
    menu_bar_->clear();
    menu_bar_->addMenu(build_file_menu());
    menu_bar_->addMenu(build_navigate_menu());
    menu_bar_->addMenu(build_view_menu());
    menu_bar_->addMenu(build_help_menu());
    // Reapply popup stylesheet after rebuild — refresh_theme normally does
    // this on theme change, but a pure language switch should also style
    // the freshly created submenus.
    for (auto* action : menu_bar_->actions())
        if (action->menu())
            action->menu()->setStyleSheet(popup_ss());
}

void ToolBar::refresh_theme() {
    setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;").arg(colors::BG_BASE()).arg(colors::BORDER_MED()));
    if (user_label_)
        user_label_->setMaximumWidth(metrics::width(120));
    if (credits_label_)
        credits_label_->setMaximumWidth(metrics::width(100));
    if (command_bar_)
        command_bar_->setMaximumWidth(metrics::width(280));
    if (menu_bar_) {
        menu_bar_->setStyleSheet(menu_ss());
        for (auto* action : menu_bar_->actions())
            if (action->menu())
                action->menu()->setStyleSheet(popup_ss());
    }
    for (auto* s : separators_)
        s->setStyleSheet(QString("color:%1;background:transparent;padding:0 3px;").arg(colors::TEXT_DIM()));
    auto lbl = [](QLabel* l, const QString& c, bool b = false) {
        if (l)
            l->setStyleSheet(QString("color:%1;%2background:transparent;").arg(c, b ? "font-weight:700;" : ""));
    };
    lbl(fincept_label_, colors::AMBER(), true);
    lbl(branding_label_, colors::TEXT_PRIMARY(), true);
    lbl(subtitle_label_, colors::TEXT_SECONDARY());
    lbl(live_dot_, colors::SUCCESS()); // connection state, not price direction
    lbl(live_label_, colors::SUCCESS(), true);
    lbl(clock_label_, colors::TEXT_PRIMARY());
    lbl(user_label_, colors::AMBER());
    lbl(credits_label_, colors::SUCCESS());
    // Plan badge. Free accounts get a subtle amber outline (tier="free", set in
    // refresh_user_display) — a quiet pointer to Pro, not a nag.
    if (plan_btn_)
        plan_btn_->setStyleSheet(QString("QPushButton{color:%1;background:transparent;border:1px solid transparent;"
                                         "padding:0 6px;font-weight:700;}"
                                         "QPushButton:hover{color:%2;}"
                                         "QPushButton[tier=\"free\"]{border-color:%3;}"
                                         "QPushButton[tier=\"free\"]:hover{border-color:%2;}")
                                     .arg(colors::TEXT_PRIMARY(), colors::AMBER(), colors::AMBER_DIM()));
    // Filled rather than outlined — the only solid button on the row, so the
    // Enterprise CTA reads as the primary action next to LOGOUT and CHAT. Its
    // text is the on-accent colour on both the accent and the dimmed hover fill.
    if (upgrade_btn_)
        upgrade_btn_->setStyleSheet(QString("QPushButton{background:%1;color:%2;border:1px solid %1;"
                                            "padding:0 8px;font-weight:700;}"
                                            "QPushButton:hover{background:%3;border-color:%1;color:%2;}")
                                        .arg(colors::AMBER(), colors::TEXT_ON_ACCENT(), colors::AMBER_DIM()));
    if (chat_mode_btn_)
        chat_mode_btn_->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                                              "padding:0 8px;font-weight:700;}"
                                              "QPushButton:hover{background:%2;color:%3;border-color:%1;}")
                                          .arg(colors::AMBER())
                                          .arg(colors::AMBER_DIM())
                                          .arg(colors::TEXT_ON_ACCENT()));
    if (logout_btn_)
        logout_btn_->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:1px solid %1;"
                                           "padding:0 8px;font-weight:700;}"
                                           "QPushButton:hover{background:%1;color:%2;border-color:%1;}")
                                       .arg(colors::DANGER())
                                       .arg(colors::TEXT_ON_ACCENT()));
    // Run state, not price direction: INFO = in progress, WARNING = needs the user.
    const QString badge_css = QStringLiteral("QLabel{color:%1;background:%2;border:1px solid %1;border-radius:2px;"
                                             "padding:0 4px;font-weight:700;}");
    if (chat_runs_badge_)
        chat_runs_badge_->setStyleSheet(badge_css.arg(colors::INFO(), colors::tint(QColor(colors::INFO()), 0.18)));
    if (chat_attention_badge_)
        chat_attention_badge_->setStyleSheet(
            badge_css.arg(colors::WARNING(), colors::tint(QColor(colors::WARNING()), 0.18)));
    if (overflow_btn_) {
        overflow_btn_->setStyleSheet(QString("QToolButton{background:transparent;border:1px solid %1;padding:0 4px;}"
                                             "QToolButton:hover{background:%2;border-color:%3;}"
                                             "QToolButton::menu-indicator{image:none;width:0;}")
                                         .arg(colors::BORDER_MED(), colors::BG_RAISED(), colors::BORDER_BRIGHT()));
        const int glyph = QFontMetrics(fonts::font(0)).height() * 3 / 4;
        overflow_btn_->setIconSize(QSize(glyph, glyph));
        if (auto* menu = overflow_btn_->menu())
            menu->setStyleSheet(popup_ss());
    }
    update_ai_status(); // the pill's colours and font follow the theme
    // Widths follow the font: re-fit the row.
    apply_responsive_layout(width());
}

void ToolBar::update_ai_status() {
    if (!ai_status_)
        return;
    const bool running = ai_running_ > 0;
    if (!running && !ai_finished_) {
        if (ai_status_->isVisible()) {
            ai_status_->setVisible(false);
            apply_responsive_layout(width());
        }
        return;
    }
    QString text;
    QString tip;
    if (running) {
        QString step = ai_step_;
        if (step.size() > 44)
            step = step.left(43) + QChar(0x2026);
        // U+25CF / U+25CB alternate: the dot blinks while the pill stays filled.
        text = (ai_pulse_on_ ? QString(QChar(0x25CF)) : QString(QChar(0x25CB))) + QLatin1Char(' ') +
               (step.isEmpty() ? tr("AI WORKING") : tr("AI WORKING") + QStringLiteral("  ·  ") + step);
        tip = step.isEmpty() ? tr("The AI is working on your request.")
                             : tr("The AI is working on your request: %1").arg(ai_step_);
    } else {
        text = QString(QChar(0x2713)) + QLatin1Char(' ') + tr("AI FINISHED");
        tip = tr("The AI has finished.");
    }
    const QString fill = running ? colors::AMBER() : colors::SUCCESS();
    // Restyled only when the state or the theme changed — not on every blink of the dot.
    const QString css = QString("color:%1;background:%2;font-weight:800;font-size:%3px;letter-spacing:0.5px;"
                                "padding:1px 9px;border-radius:2px;")
                            .arg(colors::BG_BASE(), fill)
                            .arg(fonts::px(11));
    if (ai_status_->styleSheet() != css)
        ai_status_->setStyleSheet(css);
    const bool resize = !ai_status_->isVisible() || ai_status_->text().size() != text.size();
    ai_status_->setText(text);
    ai_status_->setToolTip(tip);
    ai_status_->setAccessibleName(tip);
    ai_status_->setVisible(true);
    if (resize)
        apply_responsive_layout(width());
}

void ToolBar::update_chat_badge() {
    if (!chat_mode_btn_ || !chat_runs_badge_ || !chat_attention_badge_)
        return;
    const QString running = tr("%n agent run(s) in progress", nullptr, chat_active_runs_);
    const QString waiting = tr("%n agent run(s) waiting for your answer or approval", nullptr, chat_attention_runs_);
    chat_runs_badge_->setText(QString::number(chat_active_runs_));
    chat_runs_badge_->setToolTip(running);
    chat_runs_badge_->setAccessibleName(running);
    chat_attention_badge_->setText(QStringLiteral("!") + QString::number(chat_attention_runs_));
    chat_attention_badge_->setToolTip(waiting);
    chat_attention_badge_->setAccessibleName(waiting);
    QStringList counts;
    if (chat_active_runs_ > 0)
        counts << running;
    if (chat_attention_runs_ > 0)
        counts << waiting;
    const QString base = tr("Switch to Chat Mode (F9)");
    chat_mode_btn_->setToolTip(counts.isEmpty() ? base : base + QLatin1Char('\n') + counts.join(QLatin1Char('\n')));
    chat_mode_btn_->setAccessibleName(counts.isEmpty() ? tr("Chat Mode")
                                                       : tr("Chat Mode: %1").arg(counts.join(QStringLiteral(", "))));
    // Badge visibility (needs room next to CHAT) and widths: re-fit the row.
    apply_responsive_layout(width());
}

void ToolBar::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    apply_responsive_layout(e->size().width());
}

void ToolBar::apply_responsive_layout(int w) {
    // Fit the row from what each item really needs at the current font
    // (size hints come from font metrics + the item's stylesheet padding), not
    // from fixed window-width thresholds — those clipped the row at 24 px even
    // on a 1600 px window and at 14 px on a 1280 px one. Items give way in
    // priority order; collapsed actions stay reachable from the overflow menu.
    auto* hl = qobject_cast<QHBoxLayout*>(layout());
    if (in_responsive_layout_ || !hl || !overflow_btn_)
        return;
    // Not while the theme_changed handlers run: showing an item here (the
    // subtitle at a smaller font, the overflow button at a larger one) makes
    // Qt activate every layout up to the window and style the whole
    // application widget by widget — 1.5 s of a 3.5 s (24 -> 14 px) and of a
    // 4.1 s (14 -> 9 px) freeze, measured. The appearance_settled() connection
    // re-fits the row once the change is in.
    if (ThemeManager::instance().notifying())
        return;
    // The room is the host toolbar's row, not this widget's current width: if
    // the host ever sized this row to its (shrinking) size hint, deciding from
    // our own width would collapse one more group on every resize.
    if (auto* host = qobject_cast<QToolBar*>(parentWidget())) {
        QStyle* st = host->style();
        const int inset = 2 * (st->pixelMetric(QStyle::PM_ToolBarFrameWidth, nullptr, host) +
                               st->pixelMetric(QStyle::PM_ToolBarItemMargin, nullptr, host));
        w = std::max(w, host->contentsRect().width() - inset);
    }
    if (w <= 0)
        return;
    in_responsive_layout_ = true;

    auto sep_at = [this](int i) -> QWidget* { return i < separators_.size() ? separators_[i] : nullptr; };
    // Separators (by position): 0 command bar|brand, 1 LIVE|pins, 2 pins|clock,
    // 3 user|credits, 4 credits|plan, 5 ENTERPRISE|CHAT, 6 CHAT|LOGOUT. Each
    // group takes the separator that would otherwise be left dangling.
    struct Group {
        QList<QWidget*> widgets;
        bool actionable; // collapsing it shows the overflow button
    };
    const QList<Group> groups = {
        {{subtitle_label_}, false},
        // Status items, but ones the user reads: collapsed, they move into the
        // overflow menu like the actions do.
        {{clock_label_, sep_at(2)}, true},
        {{live_gap_, live_dot_, live_label_}, true},
        {{credits_label_, sep_at(4)}, true},
        {{user_label_, sep_at(3)}, true},
        {{upgrade_btn_}, true},
        {{fincept_label_, branding_label_, sep_at(1)}, false},
        {{chat_mode_btn_, chat_runs_badge_, chat_attention_badge_, sep_at(5)}, true},
        {{plan_btn_}, true},
        {{logout_btn_, sep_at(6)}, true},
    };

    auto wanted = [this](QWidget* wd) {
        if (wd == chat_runs_badge_)
            return chat_active_runs_ > 0;
        if (wd == chat_attention_badge_)
            return chat_attention_runs_ > 0;
        return wd != nullptr;
    };
    auto width_of = [&](QWidget* wd) -> int {
        if (!wd || !wanted(wd))
            return 0;
        wd->ensurePolished(); // stylesheet padding is part of the hint
        return std::min(wd->sizeHint().width(), wd->maximumWidth());
    };

    // Width with every group shown. Flexible items count at their floor: the
    // pins strip scrolls, the command bar keeps room to type, the menu bar
    // folds into its own » extension only once everything else is gone.
    const QMargins m = hl->contentsMargins();
    int full = m.left() + m.right();
    for (int i = 0; i < hl->count(); ++i) {
        QLayoutItem* item = hl->itemAt(i);
        if (QWidget* wd = item->widget()) {
            if (wd == overflow_btn_)
                continue; // added below when needed
            if (wd == pushpin_bar_)
                full += pushpin_bar_->minimumSizeHint().width();
            else if (wd == command_bar_)
                full += std::min(command_bar_->maximumWidth(), metrics::width(160));
            else
                full += width_of(wd);
        } else if (QSpacerItem* sp = item->spacerItem()) {
            full += sp->sizeHint().width();
        }
    }
    const int overflow_w = width_of(overflow_btn_);

    int hidden = 0;
    bool overflow_needed = false;
    int total = full;
    while (hidden < groups.size() && total + (overflow_needed ? overflow_w : 0) > w) {
        for (QWidget* wd : groups[hidden].widgets)
            total -= width_of(wd);
        overflow_needed = overflow_needed || groups[hidden].actionable;
        ++hidden;
    }

    for (int g = 0; g < groups.size(); ++g) {
        for (QWidget* wd : groups[g].widgets) {
            if (!wd)
                continue;
            const bool show = g >= hidden && wanted(wd);
            if (wd->isHidden() == show)
                wd->setVisible(show);
        }
    }
    if (overflow_btn_->isHidden() == overflow_needed)
        overflow_btn_->setVisible(overflow_needed);
    in_responsive_layout_ = false;
}

void ToolBar::fill_overflow_menu(QMenu* menu) {
    menu->clear();
    menu->setStyleSheet(popup_ss());
    // Status the row had no room for: the date and time (as of opening the
    // menu) and the connection state.
    if (clock_label_ && clock_label_->isHidden()) {
        update_clock();
        auto* a = menu->addAction(clock_label_->text());
        a->setEnabled(false);
    }
    if (live_label_ && live_label_->isHidden()) {
        auto* a = menu->addAction(live_dot_->text() + live_label_->text());
        a->setEnabled(false);
    }
    const auto& s = auth::AuthManager::instance().session();
    if (s.authenticated) {
        if (user_label_ && user_label_->isHidden()) {
            auto* a = menu->addAction(literal_ampersands(user_label_->toolTip()));
            a->setEnabled(false);
        }
        if (credits_label_ && credits_label_->isHidden()) {
            auto* a = menu->addAction(literal_ampersands(credits_label_->text()));
            a->setEnabled(false);
        }
    }
    auto add_separator = [menu]() {
        if (!menu->isEmpty() && !menu->actions().constLast()->isSeparator())
            menu->addSeparator();
    };
    add_separator();
    if (plan_btn_ && plan_btn_->isHidden())
        menu->addAction(tr("Plan: %1 — Plans && Pricing…").arg(literal_ampersands(plan_btn_->text())), this,
                        [this]() { emit plan_clicked(); });
    if (upgrade_btn_ && upgrade_btn_->isHidden())
        menu->addAction(tr("Fincept Terminal Enterprise…"), this, [this]() { emit upgrade_clicked(); });
    if (chat_mode_btn_ && chat_mode_btn_->isHidden()) {
        QString label = tr("Chat Mode\tF9");
        if (chat_attention_runs_ > 0)
            label = tr("Chat Mode — %n run(s) need you\tF9", nullptr, chat_attention_runs_);
        else if (chat_active_runs_ > 0)
            label = tr("Chat Mode — %n run(s) in progress\tF9", nullptr, chat_active_runs_);
        menu->addAction(label, this, [this]() { emit chat_mode_toggled(); });
    }
    if (logout_btn_ && logout_btn_->isHidden()) {
        add_separator();
        menu->addAction(tr("Logout"), this, [this]() { emit logout_clicked(); });
        menu->actions().constLast()->setProperty("fincept_agent_risk", "account"); // agents: signs out
    }
}

void ToolBar::update_clock() {
    auto dt = QDateTime::currentDateTime();
    clock_label_->setText(dt.toString("dd MMM yy").toUpper() + " " + dt.toString("HH:mm:ss"));
}

void ToolBar::refresh_user_display() {
    const auto& s = auth::AuthManager::instance().session();
    auto set_tier = [this](const QString& tier) {
        if (plan_btn_->property("tier").toString() == tier)
            return;
        plan_btn_->setProperty("tier", tier);
        plan_btn_->style()->unpolish(plan_btn_);
        plan_btn_->style()->polish(plan_btn_);
    };
    if (!s.authenticated) {
        user_label_->setText(QStringLiteral("---"));
        user_label_->setToolTip(QString());
        credits_label_->setText(QStringLiteral("---"));
        plan_btn_->setText(QStringLiteral("---"));
        set_tier(QString());
        apply_responsive_layout(width());
        return;
    }

    QString name = s.user_info.username.isEmpty() ? s.user_info.email : s.user_info.username;
    QFontMetrics fm(user_label_->font());
    user_label_->setText(fm.elidedText(name, Qt::ElideRight, user_label_->maximumWidth() - 4));
    user_label_->setToolTip(name);

    // Fractional balances are real (an AI call can cost 0.35 credits) — the
    // int cast showed 0.35 as "0 CR".
    credits_label_->setText(tr("%1 CR").arg(auth::format_credits(s.user_info.credit_balance)));
    credits_label_->setStyleSheet(
        QString("color:%1;background:transparent;")
            .arg(s.user_info.credit_balance > 0 ? colors::SUCCESS.get() : colors::DANGER.get()));

    // "FREE" / "PRO"; legacy paid tiers keep their own name.
    plan_btn_->setText(s.plan_label());
    set_tier(s.has_paid_plan() ? QStringLiteral("paid") : QStringLiteral("free"));
    apply_responsive_layout(width()); // the texts' widths changed
}

QMenu* ToolBar::build_file_menu() {
    auto* m = new QMenu(tr("File"), this);
    m->setStyleSheet(popup_ss());
    m->addAction(tr("New Window"), this, [this]() { emit action_triggered("new_window"); });

    // Rebuilt on popup so plug/unplug is reflected. Emits "move_to_monitor:<name>" — names are stable, indices aren't.
    auto* monitors = m->addMenu(tr("Move to Monitor"));
    monitors->setStyleSheet(popup_ss());
    connect(monitors, &QMenu::aboutToShow, this, [this, monitors]() {
        monitors->clear();
        const auto screens = QGuiApplication::screens();
        if (screens.size() <= 1) {
            auto* only = monitors->addAction(tr("(single monitor)"));
            only->setEnabled(false);
            return;
        }
        int idx = 1;
        for (QScreen* s : screens) {
            const QString name = s->name();
            const QSize size = s->size();
            // Resolution format (W×H) is locale-neutral.
            const QString label = QString("%1. %2  (%3×%4)").arg(idx++).arg(name).arg(size.width()).arg(size.height());
            monitors->addAction(label, this,
                                [this, name]() { emit action_triggered(QString("move_to_monitor:%1").arg(name)); });
        }
    });

    m->addAction(tr("Close Window"), this, [this]() { emit action_triggered("close_window"); });
    m->addAction(tr("Close All Windows"), this, [this]() { emit action_triggered("close_all_windows"); });

    m->addSeparator();
    m->addAction(tr("New Layout"), this, [this]() { emit action_triggered("layout_new"); });
    m->addAction(tr("Open Layout…"), this, [this]() { emit action_triggered("layout_open"); });
    m->addAction(tr("Save Layout"), this, [this]() { emit action_triggered("layout_save"); });
    m->addAction(tr("Save Layout As…"), this, [this]() { emit action_triggered("layout_save_as"); });
    m->addSeparator();
    m->addAction(tr("Import Layout"), this, [this]() { emit action_triggered("import_data"); });
    m->addAction(tr("Export Layout"), this, [this]() { emit action_triggered("export_data"); });
    m->addSeparator();
    m->addAction(tr("File Manager"), this, [this]() { emit navigate_to("file_manager"); });
    m->addSeparator();
    // Refreshes the focused panel (same as View ▸ Refresh Screen) — it was labelled "Refresh All".
    m->addAction(tr("Refresh Panel"), this, [this]() { emit action_triggered("refresh"); });
    return m;
}

QMenu* ToolBar::build_navigate_menu() {
    auto* m = new QMenu(tr("Navigate"), this);
    m->setStyleSheet(QString("QMenu{background:%1;color:%2;border:1px solid %3;padding:2px 0;}"
                             "QMenu::item{padding:3px 20px 3px 10px;}"
                             "QMenu::item:selected{background:%4;}"
                             "QMenu::item:disabled{color:%5;font-weight:700;padding:6px 10px 2px 10px;}"
                             "QMenu::separator{background:%3;height:1px;margin:2px 6px;}"
                             "QMenu::scroller{background:%1;height:14px;}"
                             "QMenu::scroller:hover{background:%4;}")
                         .arg(colors::BG_SURFACE())
                         .arg(colors::TEXT_PRIMARY())
                         .arg(colors::BORDER_MED())
                         .arg(colors::BG_RAISED())
                         .arg(colors::AMBER()));

    auto add_sub = [&](const QString& title) -> QMenu* {
        // Menus read '&' as a mnemonic ("Markets & Data" showed as "Markets _Data").
        auto* sub = m->addMenu(literal_ampersands(title));
        sub->setStyleSheet(m->styleSheet());
        return sub;
    };

    auto nav = [this](QMenu* menu, const QString& label, const QString& id) {
        menu->addAction(literal_ampersands(label), this, [this, id]() { emit navigate_to(id); });
    };

    auto* mkt = add_sub(tr("Markets & Data"));
    nav(mkt, tr("Economics"), "economics");
    nav(mkt, tr("GOVT Data"), "gov_data");
    nav(mkt, tr("DBnomics"), "dbnomics");
    nav(mkt, tr("AKShare Data"), "akshare");
    nav(mkt, tr("Asia Markets"), "asia_markets");
    nav(mkt, tr("Relationship Map"), "relationship_map");

    auto* trd = add_sub(tr("Trading & Portfolio"));
    nav(trd, tr("Equity Trading"), "equity_trading");
    nav(trd, tr("Alpha Arena"), "alpha_arena");
    nav(trd, tr("Prediction Markets"), "polymarket");
    nav(trd, tr("Derivatives"), "derivatives");
    nav(trd, tr("F&&O"), "fno");
    nav(trd, tr("Watchlist"), "watchlist");

    auto* crypto = add_sub(tr("Crypto"));
    nav(crypto, tr("Crypto Center"), "crypto_center");

    auto* res = add_sub(tr("Research & Intelligence"));
    nav(res, tr("Equity Research"), "equity_research");
    nav(res, tr("Screener"), "screener"); // registered, but was in no browse surface
    nav(res, tr("Calendars"), "calendars");
    nav(res, tr("M&A Analytics"), "ma_analytics");
    nav(res, tr("Alt. Investments"), "alt_investments");
    nav(res, tr("Geopolitics"), "geopolitics");
    nav(res, tr("Maritime"), "maritime");
    nav(res, tr("Surface Analytics"), "surface_analytics");

    auto* tools = add_sub(tr("Tools"));
    nav(tools, tr("Agent Config"), "agent_config"); // includes the former Agent Workbench (runs)
    nav(tools, tr("MCP Servers"), "mcp_servers");
    nav(tools, tr("Data Mapping"), "data_mapping");
    nav(tools, tr("Data Sources"), "data_sources");
    nav(tools, tr("Report Builder"), "report_builder");
    nav(tools, tr("Excel"), "excel");
    nav(tools, tr("Trade Viz"), "trade_viz");
    nav(tools, tr("File Manager"), "file_manager");
    nav(tools, tr("Notes"), "notes");

    m->addSeparator();

    nav(m, tr("Docs"), "docs");
    nav(m, tr("Support"), "support");
    nav(m, tr("About"), "about");

    return m;
}

QMenu* ToolBar::build_view_menu() {
    auto* m = new QMenu(tr("View"), this);
    m->setStyleSheet(popup_ss());
    m->addAction(tr("Search…\tCtrl+K"), this, [this]() {
        if (command_bar_)
            command_bar_->activate();
    });
    m->addAction(tr("Component Browser"), this, [this]() {
        if (command_bar_)
            command_bar_->open_component_browser();
    });
    m->addSeparator();
    m->addAction(tr("Fullscreen\tF11"), this, [this]() { emit action_triggered("fullscreen"); });
    m->addSeparator();
    m->addAction(tr("Focus Mode\tF10"), this, [this]() { emit action_triggered("focus_mode"); });
    // Not checkable — state lives on WindowFrame::always_on_top_; a checkable QAction would drift on focus changes.
    m->addAction(tr("Always on Top\tCtrl+Shift+T"), this, [this]() { emit action_triggered("always_on_top"); });
    m->addSeparator();

    auto* panels = m->addMenu(tr("Float Panel"));
    panels->setStyleSheet(popup_ss());
    panels->addAction(tr("Dashboard"), this, [this]() { emit action_triggered("panel_dashboard"); });
    panels->addAction(tr("Watchlist"), this, [this]() { emit action_triggered("panel_watchlist"); });
    panels->addAction(tr("News Feed"), this, [this]() { emit action_triggered("panel_news"); });
    panels->addAction(tr("Portfolio"), this, [this]() { emit action_triggered("panel_portfolio"); });
    panels->addAction(tr("Markets"), this, [this]() { emit action_triggered("panel_markets"); });
    panels->addSeparator();
    panels->addAction(tr("Crypto Trading"), this, [this]() { emit action_triggered("panel_crypto"); });
    panels->addAction(tr("Equity Trading"), this, [this]() { emit action_triggered("panel_equity"); });
    panels->addAction(tr("Algo Trading"), this, [this]() { emit action_triggered("panel_algo"); });
    panels->addSeparator();
    panels->addAction(tr("Equity Research"), this, [this]() { emit action_triggered("panel_research"); });
    panels->addAction(tr("Economics"), this, [this]() { emit action_triggered("panel_economics"); });
    panels->addAction(tr("Geopolitics"), this, [this]() { emit action_triggered("panel_geopolitics"); });
    panels->addAction(tr("AI Chat"), this, [this]() { emit action_triggered("panel_ai_chat"); });
    m->addSeparator();

    auto* persp = m->addMenu(tr("Quick Switch"));
    persp->setStyleSheet(popup_ss());
    // Saved as a layout (File ▸ Open Layout lists it). The ADS "perspective" this used to
    // store could never be opened again — nothing lists or loads perspectives.
    persp->addAction(tr("Save Workspace…"), this, [this]() { emit action_triggered("layout_save_as"); });
    persp->addSeparator();

    auto* qs_trading = persp->addMenu(tr("Trading"));
    qs_trading->setStyleSheet(popup_ss());
    qs_trading->addAction(tr("Crypto Trading"), this, [this]() { emit action_triggered("perspective_trading"); });
    qs_trading->addAction(tr("Equity Trading"), this, [this]() { emit action_triggered("perspective_equity"); });
    qs_trading->addAction(tr("Algo Trading"), this, [this]() { emit action_triggered("perspective_algo"); });

    auto* qs_research = persp->addMenu(tr("Research"));
    qs_research->setStyleSheet(popup_ss());
    qs_research->addAction(tr("Equity Research"), this, [this]() { emit action_triggered("perspective_research"); });
    qs_research->addAction(tr("Derivatives"), this, [this]() { emit action_triggered("perspective_derivatives"); });
    qs_research->addAction(tr("F&&O"), this, [this]() { emit action_triggered("perspective_fno"); });
    qs_research->addAction(tr("M&&A Analytics"), this, [this]() { emit action_triggered("perspective_ma"); });

    persp->addAction(tr("Portfolio View"), this, [this]() { emit action_triggered("perspective_portfolio"); });
    persp->addAction(tr("Markets View"), this, [this]() { emit action_triggered("perspective_markets"); });
    persp->addAction(tr("News View"), this, [this]() { emit action_triggered("perspective_news"); });

    auto* qs_econ = persp->addMenu(tr("Economics && Data"));
    qs_econ->setStyleSheet(popup_ss());
    qs_econ->addAction(tr("Economics"), this, [this]() { emit action_triggered("perspective_economics"); });
    qs_econ->addAction(tr("Data Sources"), this, [this]() { emit action_triggered("perspective_data"); });

    persp->addAction(tr("Geopolitics View"), this, [this]() { emit action_triggered("perspective_geopolitics"); });

    auto* qs_ai = persp->addMenu(tr("AI && Quant"));
    qs_ai->setStyleSheet(popup_ss());
    qs_ai->addAction(tr("Quant Lab"), this, [this]() { emit action_triggered("perspective_quant"); });
    qs_ai->addAction(tr("AI Chat"), this, [this]() { emit action_triggered("perspective_ai"); });

    persp->addAction(tr("Tools View"), this, [this]() { emit action_triggered("perspective_tools"); });
    m->addSeparator();

    m->addAction(tr("Refresh Screen\tF5"), this, [this]() { emit action_triggered("refresh"); });
    m->addAction(tr("Take Screenshot\tCtrl+P"), this, [this]() { emit action_triggered("screenshot"); });
    return m;
}

QMenu* ToolBar::build_help_menu() {
    auto* m = new QMenu(tr("Help"), this);
    m->setStyleSheet(popup_ss());
    m->addAction(tr("Help Center"), this, [this]() { emit navigate_to("help"); });
    m->addAction(tr("Documentation"), this, [this]() { emit navigate_to("docs"); });
    m->addAction(tr("Report a Problem"), this, [this]() { emit navigate_to("support"); });
    m->addAction(tr("Check for Updates"), this, [this]() { emit action_triggered("check_updates"); });
    m->addAction(tr("About Fincept"), this, [this]() { emit navigate_to("about"); });
    m->addSeparator();
    m->addAction(tr("Terms of Service"), this, [this]() { emit navigate_to("terms"); });
    m->addAction(tr("Privacy Policy"), this, [this]() { emit navigate_to("privacy"); });
    m->addAction(tr("Trademarks"), this, [this]() { emit navigate_to("trademarks"); });
    m->addSeparator();
    // Plans & Pricing carries the Enterprise card (site + demo). The Enterprise
    // dialog stays on the toolbar ENTERPRISE button and the command palette;
    // Compare Editions in that dialog and on About; Contact in the Help Center.
    m->addAction(tr("Plans && Pricing…"), this, [this]() { emit plan_clicked(); });
    // No other menu offers Logout (the toolbar LOGOUT button is not a menu).
    m->addSeparator();
    m->addAction(tr("Logout"), this, [this]() { emit action_triggered("logout"); });
    m->actions().constLast()->setProperty("fincept_agent_risk", "account"); // agents: signs out
    return m;
}

} // namespace fincept::ui
