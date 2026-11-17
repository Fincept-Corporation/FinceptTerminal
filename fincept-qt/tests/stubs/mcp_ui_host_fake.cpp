// mcp_ui_host_fake.cpp — see mcp_ui_host_fake.h.

#include "mcp_ui_host_fake.h"

#include "mcp/core/CoreApproval.h"
#include "mcp/core/UiHost.h"

#include <QApplication>
#include <QCoreApplication>
#include <QTimer>
#include <QVariant>
#include <QWidget>

namespace fincept::mcp {

namespace test_host {

namespace {
ToolApprovalVerdict g_verdict = ToolApprovalVerdict::Denied;
bool g_answer = true;
bool g_locked = false;
} // namespace

void set_verdict(ToolApprovalVerdict verdict, bool answer) {
    g_verdict = verdict;
    g_answer = answer;
}

QList<ToolApprovalRequest>& approvals() {
    static QList<ToolApprovalRequest> list;
    return list;
}

void set_locked(bool locked) {
    g_locked = locked;
}

} // namespace test_host

namespace core {

void ask_user_approval(const ToolApprovalRequest& request, std::function<void(ToolApprovalVerdict)> done, int) {
    test_host::approvals().append(request);
    if (!test_host::g_answer || !done)
        return;
    const ToolApprovalVerdict v = test_host::g_verdict;
    // Answered later on the GUI thread, as the real dialog's verdict is.
    QTimer::singleShot(20, QCoreApplication::instance(), [done, v]() { done(v); });
}

QString approval_refusal_text(ToolApprovalVerdict verdict, const QString& what) {
    switch (verdict) {
        case ToolApprovalVerdict::Approved:
            return {};
        case ToolApprovalVerdict::Denied:
            return QStringLiteral("The user declined: %1.").arg(what);
        case ToolApprovalVerdict::TimedOut:
            return QStringLiteral("Nobody approved \"%1\" in time.").arg(what);
        case ToolApprovalVerdict::Unavailable:
            return QStringLiteral("\"%1\" needs the user's approval, but nobody can be asked.").arg(what);
    }
    return {};
}

namespace ui::host {

QList<QWidget*> frames() {
    QList<QWidget*> out;
    for (QWidget* w : QApplication::topLevelWidgets()) {
        if (w->property("fincept_test_frame").toBool())
            out << w;
    }
    return out;
}

bool is_frame(const QObject* o) {
    return o && o->isWidgetType() && o->property("fincept_test_frame").toBool();
}

QWidget* panel_area(QWidget*) {
    return nullptr;
}

int window_id(QWidget* frame) {
    return frames().indexOf(frame) + 1;
}

bool frame_locked(QWidget*) {
    return test_host::g_locked;
}

QList<QWidget*> panels(QWidget* frame) {
    QList<QWidget*> out;
    if (!frame)
        return out;
    for (QWidget* w : frame->findChildren<QWidget*>()) {
        if (w->property("fincept_test_panel").isValid() && !w->property("fincept_test_closed").toBool())
            out << w;
    }
    return out;
}

bool is_panel(const QObject* o) {
    return o && o->isWidgetType() && o->property("fincept_test_panel").isValid();
}

bool panel_closed(QWidget* panel) {
    return !panel || panel->property("fincept_test_closed").toBool();
}

QString panel_id(QWidget* panel) {
    return panel ? panel->property("fincept_test_panel").toString() : QString();
}

QString panel_title(QWidget* panel) {
    return panel ? panel->windowTitle() : QString();
}

QWidget* panel_content(QWidget* panel) {
    return panel;
}

QWidget* panel_screen(QWidget* panel) {
    return panel;
}

QWidget* current_panel(QWidget* frame) {
    for (QWidget* p : panels(frame)) {
        if (p->isVisible())
            return p;
    }
    return nullptr;
}

void focus_panel(QWidget* panel) {
    if (panel)
        panel->setFocus(Qt::OtherFocusReason);
}

void close_panel(QWidget* panel) {
    if (!panel)
        return;
    panel->setProperty("fincept_test_closed", true);
    panel->hide();
}

QString title_for_id(const QString& screen_id) {
    return screen_id;
}

SymbolInfo panel_symbol(QWidget*) {
    return {};
}

TerminalStatus terminal_status() {
    TerminalStatus st;
    st.version = QStringLiteral("test");
    st.signed_in = true;
    st.plan = QStringLiteral("free");
    st.locked = test_host::g_locked;
    return st;
}

bool terminal_locked() {
    return test_host::g_locked;
}

int python_pending() {
    return 0;
}

bool watch_symbol_box(QWidget*, int, std::function<void(const QString&)>) {
    return false;
}

} // namespace ui::host

} // namespace core

} // namespace fincept::mcp
