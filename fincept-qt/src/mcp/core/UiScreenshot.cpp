// UiScreenshot.cpp — ui_screenshot: a picture of a window, panel, dialog or
// element, secret fields blacked out, optionally with each control's ref drawn
// on it. Terminal only (uses the theme's colours and fonts).

#include "mcp/core/CoreToolsCommon.h"
#include "mcp/core/UiEngine.h"
#include "mcp/core/UiErrors.h"
#include "ui/theme/Theme.h"

#include <QBuffer>
#include <QColor>
#include <QFontMetrics>
#include <QJsonArray>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QTabBar>

namespace fincept::mcp::core::ui {

void ui_screenshot(const QJsonObject& args, Reply reply) {
    QString label, error;
    QWidget* scope = resolve_scope(args.value(QStringLiteral("target")).toString(), &label, &error);
    if (!scope) {
        reply(ToolResult::fail(error));
        return;
    }
    if (!scope->isVisible()) {
        reply(ui_fail(Err::Obscured, QStringLiteral("%1 is not visible (a background tab or hidden).").arg(label)));
        return;
    }
    bool png = args.value(QStringLiteral("format")).toString().toLower() == QLatin1String("png");
    const int max_width = clamp_int(args.value(QStringLiteral("max_width")), 1280, 320, 2560);
    const bool marks = args.value(QStringLiteral("marks")).toBool(false);

    if (is_secret(scope)) {
        reply(ui_fail(Err::Denied, QStringLiteral("%1 holds a secret (password / API key) and is never captured.").arg(label)));
        return;
    }
    QPixmap pm = scope->grab();
    if (pm.isNull()) {
        reply(ToolResult::fail(QStringLiteral("Could not capture %1.").arg(label)));
        return;
    }
    // Pixels must not leak what text never does: black out every visible secret
    // field (a revealed key, the MCP token with SHOW on) before anything else.
    int masked = 0;
    {
        QPainter p(&pm);
        for (QWidget* w : scope->findChildren<QWidget*>()) {
            if (!w->isVisible() || !is_secret(w))
                continue;
            if (QWidget* parent = w->parentWidget(); parent && parent != scope && is_secret(parent))
                continue; // the secret ancestor's box covers it
            const QRect rect(w->mapTo(scope, QPoint(0, 0)), w->size());
            if (!rect.intersects(scope->rect()))
                continue;
            p.fillRect(rect, QColor(::fincept::ui::colors::BG_RAISED()));
            p.setPen(QColor(::fincept::ui::colors::TEXT_SECONDARY()));
            p.drawText(rect, Qt::AlignCenter, QStringLiteral("[secret]"));
            ++masked;
        }
    }
    int marked = 0;
    if (marks) {
        WalkOptions opt;
        opt.detail = Detail::Brief;
        opt.max_nodes = 300;
        bool truncated = false;
        const QList<Node> nodes = collect_nodes(scope, opt, &truncated);
        QPainter p(&pm);
        const QFont f = ::fincept::ui::fonts::font(-3, QFont::Bold);
        p.setFont(f);
        const QFontMetrics fm(f);
        const QColor mark_fill(::fincept::ui::colors::AMBER());
        const QColor mark_text(::fincept::ui::colors::TEXT_ON_ACCENT());
        for (const Node& n : nodes) {
            if (n.ref.isEmpty() || !n.widget || !n.widget->isVisible())
                continue;
            QWidget* w = n.widget;
            if (w != scope && !scope->isAncestorOf(w))
                continue;
            QRect rect(w->mapTo(scope, QPoint(0, 0)), w->size());
            if (auto* bar = qobject_cast<QTabBar*>(w); bar && n.sub >= 0)
                rect = QRect(bar->mapTo(scope, bar->tabRect(n.sub).topLeft()), bar->tabRect(n.sub).size());
            if (!rect.intersects(scope->rect()))
                continue;
            p.setPen(QPen(mark_fill, 1));
            p.drawRect(rect.adjusted(0, 0, -1, -1));
            const QRect tag(rect.topLeft(), QSize(fm.horizontalAdvance(n.ref) + 6, fm.height() + 2));
            p.fillRect(tag, mark_fill);
            p.setPen(mark_text);
            p.drawText(tag, Qt::AlignCenter, n.ref);
            ++marked;
        }
    }
    if (pm.width() > max_width)
        pm = pm.scaledToWidth(max_width, Qt::SmoothTransformation);

    QByteArray bytes;
    {
        QBuffer buf(&bytes);
        buf.open(QIODevice::WriteOnly);
        if (!png && !pm.save(&buf, "JPG", 80)) {
            bytes.clear(); // no JPEG image plugin in this build — fall back to PNG
            buf.seek(0);
        }
        if (bytes.isEmpty()) {
            png = true;
            pm.save(&buf, "PNG");
        }
    }
    if (bytes.isEmpty()) {
        reply(ToolResult::fail(QStringLiteral("Could not encode the screenshot of %1.").arg(label)));
        return;
    }
    const QString mime = png ? QStringLiteral("image/png") : QStringLiteral("image/jpeg");

    ToolResult tr = ToolResult::ok(
        QStringLiteral("Screenshot of %1 — %2×%3 %4, %5 KB%6.")
            .arg(label)
            .arg(pm.width())
            .arg(pm.height())
            .arg(png ? QStringLiteral("PNG") : QStringLiteral("JPEG"))
            .arg(bytes.size() / 1024)
            .arg((marks ? QStringLiteral("; %1 controls labelled with their refs").arg(marked) : QString()) +
                 (masked > 0 ? QStringLiteral("; %1 secret field(s) blacked out").arg(masked) : QString())));
    tr.content.append(QJsonObject{{QStringLiteral("type"), QStringLiteral("image")},
                                  {QStringLiteral("data"), QString::fromLatin1(bytes.toBase64())},
                                  {QStringLiteral("mimeType"), mime}});
    reply(tr);
}

} // namespace fincept::mcp::core::ui
