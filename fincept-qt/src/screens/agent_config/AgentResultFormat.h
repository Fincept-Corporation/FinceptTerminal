#pragma once
// AgentResultFormat.h — one way to show an agent answer in the agent_config
// panels (Agents, Teams, Workflows, Create Agent test): every answer goes
// through ui::MarkdownRenderer (the same renderer as the agent chat and the
// planner), the markdown source is kept on the view so a theme change or a
// state restore re-renders it instead of degrading to plain text, and run
// durations read in human units.

#include "ui/markdown/MarkdownRenderer.h"

#include <QString>
#include <QTextEdit>
#include <QVariant>

namespace fincept::screens::agent_result {

/// Property on a result QTextEdit holding the markdown it shows.
inline constexpr const char* kMarkdownProp = "agent_result_md";

/// "850 ms", "18.3 s", "2m 05s".
inline QString duration(qint64 ms) {
    if (ms < 0)
        return {};
    if (ms < 1000)
        return QStringLiteral("%1 ms").arg(ms);
    if (ms < 60000)
        return QStringLiteral("%1 s").arg(ms / 1000.0, 0, 'f', 1);
    return QStringLiteral("%1m %2s").arg(ms / 60000).arg((ms % 60000) / 1000, 2, 10, QLatin1Char('0'));
}

/// Render `md` into `view` and remember it.
inline void show_markdown(QTextEdit* view, const QString& md) {
    if (!view)
        return;
    view->setProperty(kMarkdownProp, md);
    view->setHtml(fincept::ui::MarkdownRenderer::render(md));
}

/// Plain text (errors, streamed tokens): forget any markdown source.
inline void show_plain(QTextEdit* view, const QString& text) {
    if (!view)
        return;
    view->setProperty(kMarkdownProp, QVariant());
    view->setPlainText(text);
}

/// What to persist for the view: its markdown source, else its plain text.
inline QString saved_text(const QTextEdit* view) {
    if (!view)
        return {};
    const QVariant md = view->property(kMarkdownProp);
    return md.isValid() ? md.toString() : view->toPlainText();
}

/// Re-render after a theme change (the HTML carries the theme's colours).
inline void rerender(QTextEdit* view) {
    if (!view)
        return;
    const QVariant md = view->property(kMarkdownProp);
    if (md.isValid())
        view->setHtml(fincept::ui::MarkdownRenderer::render(md.toString()));
}

} // namespace fincept::screens::agent_result
