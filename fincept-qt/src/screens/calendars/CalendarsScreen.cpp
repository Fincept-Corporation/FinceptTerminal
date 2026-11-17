// src/screens/calendars/CalendarsScreen.cpp
#include "screens/calendars/CalendarsScreen.h"

#include "core/agent/AgentRiskMarkers.h"
#include "core/keys/KeyConfigManager.h"
#include "core/session/ScreenStateManager.h"
#include "screens/calendars/CalendarPanel.h"
#include "screens/common/CommandRows.h"
#include "services/billing/PlanAccess.h"
#include "ui/theme/Theme.h"

#include <QCoreApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <iterator>

namespace fincept::screens {

namespace fd = services::fincept_data;

namespace calendars_screen_detail {

struct SubTab {
    fd::CalendarKind kind;
    const char* title;
};
const SubTab kSubTabs[] = {
    {fd::CalendarKind::Earnings, QT_TRANSLATE_NOOP("fincept::screens::CalendarsScreen", "Earnings")},
    {fd::CalendarKind::Ipos, QT_TRANSLATE_NOOP("fincept::screens::CalendarsScreen", "IPOs")},
    {fd::CalendarKind::Splits, QT_TRANSLATE_NOOP("fincept::screens::CalendarsScreen", "Splits")},
    {fd::CalendarKind::Economic, QT_TRANSLATE_NOOP("fincept::screens::CalendarsScreen", "Economic")},
};

} // namespace calendars_screen_detail

using namespace calendars_screen_detail;

CalendarsScreen::CalendarsScreen(QWidget* parent) : QWidget(parent) {
    build_ui();
    retranslate();
}

void CalendarsScreen::build_ui() {
    setStyleSheet(QString("background:%1; color:%2;").arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY()));
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // ── Title bar (same look as Equity Research) ─────────────────────────────
    auto* bar = new QWidget(this);
    bar->setMinimumHeight(48);
    bar->setStyleSheet(QString("* { background:%1; border-bottom:1px solid %2; }"
                               "QLabel#calTitle { color:%3; font-size:" + fincept::ui::fonts::px(0) + "px; font-weight:700; letter-spacing:2px; }"
                               "QLabel#calHint { color:%4; font-size:" + fincept::ui::fonts::px(-2) + "px; }")
                           .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::AMBER(),
                                ui::colors::TEXT_TERTIARY()));
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 8, 16, 8);
    title_label_ = new QLabel(bar);
    title_label_->setObjectName(QStringLiteral("calTitle"));
    hl->addWidget(title_label_);
    hl->addStretch(1);
    hint_label_ = new QLabel(bar);
    hint_label_->setObjectName(QStringLiteral("calHint"));
    hl->addWidget(hint_label_);
    vl->addWidget(bar);

    // ── Sub-tabs ─────────────────────────────────────────────────────────────
    tabs_ = new QTabWidget(this);
    tabs_->setDocumentMode(true);
    tabs_->tabBar()->setElideMode(Qt::ElideNone);
    tabs_->tabBar()->setExpanding(false);
    tabs_->setStyleSheet(QString(R"(
        QTabWidget::pane { border:0; background:%1; }
        QTabBar::tab {
            background:%2; color:%3; padding:8px 18px;
            border:0; border-bottom:2px solid transparent;
            font-size:)" + fincept::ui::fonts::px(-2) + R"(px; letter-spacing:1px;
        }
        QTabBar::tab:selected { color:%4; border-bottom:2px solid %4; }
        QTabBar::tab:hover    { color:%5; }
    )")
                             .arg(ui::colors::BG_BASE(), ui::colors::BG_SURFACE(), ui::colors::TEXT_SECONDARY(),
                                  ui::colors::AMBER(), ui::colors::TEXT_PRIMARY()));
    for (const auto& spec : kSubTabs) {
        auto* panel = new CalendarPanel(spec.kind, tabs_);
        panels_.append(panel);
        tabs_->addTab(panel, QString()); // titles set by retranslate()
    }
    connect(tabs_, &QTabWidget::currentChanged, this, [this](int) {
        ScreenStateManager::instance().notify_changed(this);
        activate_current();
    });
    vl->addWidget(tabs_, 1);

    // Agents: the frame's Refresh key (F5 by default) reloads the current tab — a billed
    // /data call unless the plan includes market data. The key is handled outside this
    // screen, so it is named here for the focused widget's ancestor walk.
    auto sync_refresh_key = [this]() {
        const QString key =
            KeyConfigManager::instance().key(KeyAction::Refresh).toString(QKeySequence::PortableText);
        if (key.isEmpty() || CalendarPanel::plan_includes_loads())
            setProperty(fincept::core::agent::kAgentRiskKeysProperty, QVariant());
        else
            setProperty(fincept::core::agent::kAgentRiskKeysProperty, key + QStringLiteral("=spend"));
    };
    sync_refresh_key();
    connect(&billing::PlanAccess::instance(), &billing::PlanAccess::changed, this, sync_refresh_key);
    connect(&KeyConfigManager::instance(), &KeyConfigManager::key_changed, this,
            [sync_refresh_key](KeyAction a, const QKeySequence&) {
                if (a == KeyAction::Refresh)
                    sync_refresh_key();
            });
}

void CalendarsScreen::retranslate() {
    title_label_->setText(tr("CALENDARS"));
    hint_label_->setText(tr("Double-click a symbol to open it in Equity Research"));
    for (int i = 0; i < tabs_->count() && i < static_cast<int>(std::size(kSubTabs)); ++i)
        tabs_->setTabText(i, tr(kSubTabs[i].title));
}

void CalendarsScreen::activate_current() {
    if (!isVisible())
        return;
    if (auto* panel = qobject_cast<CalendarPanel*>(tabs_->currentWidget()))
        panel->activate();
}

void CalendarsScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    // One turn later: a saved session restored right after the first show selects
    // its sub-tab first, so only that one loads.
    QTimer::singleShot(0, this, &CalendarsScreen::activate_current);
}

void CalendarsScreen::refresh() {
    if (auto* panel = qobject_cast<CalendarPanel*>(tabs_->currentWidget()))
        panel->refresh();
}

void CalendarsScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(event);
}

QVariantMap CalendarsScreen::save_state() const {
    QVariantMap state;
    if (auto* panel = qobject_cast<CalendarPanel*>(tabs_->currentWidget()))
        state.insert(QStringLiteral("tab"), panel->key());
    for (const auto* panel : panels_) {
        const QVariantMap s = panel->save_state();
        if (!s.isEmpty())
            state.insert(panel->key(), s);
    }
    return state;
}

void CalendarsScreen::restore_state(const QVariantMap& state) {
    for (auto* panel : panels_)
        panel->restore_state(state.value(panel->key()).toMap());
    const QString key = state.value(QStringLiteral("tab")).toString();
    for (int i = 0; i < panels_.size(); ++i) {
        if (panels_[i]->key() == key) {
            const QSignalBlocker block(tabs_);
            tabs_->setCurrentIndex(i);
            break;
        }
    }
    activate_current();
}

bool CalendarsScreen::open_section(const QString& section, const QString& focus) {
    QString key = section.trimmed().toLower();
    if (key.isEmpty())
        return true; // "" + focus: nothing in a calendar takes a focus; the view stays as it is
    if (key.startsWith(QLatin1String("tab:")))
        key = key.mid(4);
    Q_UNUSED(focus);
    for (int i = 0; i < panels_.size(); ++i) {
        if (panels_[i]->key() == key) {
            tabs_->setCurrentIndex(i); // the tab's own switch path (currentChanged: state + activate)
            return true;
        }
    }
    return false;
}

// ── Search box rows (core/command/CommandRegistry.h; screens/common/CommandRows.h) ──
namespace {

QList<command::CommandItem> cal_command_rows() {
    namespace rows = command_rows;
    const QString where = CalendarsScreen::tr("Calendars");
    return {
        rows::section(QStringLiteral("calendars"), QStringLiteral("earnings"), CalendarsScreen::tr("Earnings calendar"),
                      {QStringLiteral("earnings"), QStringLiteral("eps"), QStringLiteral("results"),
                       QStringLiteral("reporting dates"), QStringLiteral("quarterly results")},
                      where),
        rows::section(QStringLiteral("calendars"), QStringLiteral("ipos"), CalendarsScreen::tr("IPO calendar"),
                      {QStringLiteral("ipo"), QStringLiteral("listings"), QStringLiteral("new issues"),
                       QStringLiteral("initial public offering")},
                      where),
        rows::section(QStringLiteral("calendars"), QStringLiteral("splits"), CalendarsScreen::tr("Splits calendar"),
                      {QStringLiteral("splits"), QStringLiteral("stock split"), QStringLiteral("reverse split")}, where),
        rows::section(QStringLiteral("calendars"), QStringLiteral("economic"), CalendarsScreen::tr("Economic calendar"),
                      {QStringLiteral("economic events"), QStringLiteral("macro events"), QStringLiteral("releases"),
                       QStringLiteral("cpi"), QStringLiteral("nfp"), QStringLiteral("fomc")},
                      where),
    };
}

void cal_install_commands() {
    command_rows::install(QStringLiteral("calendars"), &cal_command_rows);
}

} // namespace

Q_COREAPP_STARTUP_FUNCTION(cal_install_commands)

} // namespace fincept::screens
