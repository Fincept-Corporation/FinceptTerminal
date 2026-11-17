// src/ui/markdown/MarkdownRenderer.cpp
#include "ui/markdown/MarkdownRenderer.h"
#include "ui/theme/Theme.h"

#include <QRegularExpression>
#include <QString>

#include <md4c-html.h>
#include <string>

namespace fincept::ui {

// ── md4c callback ─────────────────────────────────────────────────────────────

static void md4c_callback(const MD_CHAR* text, MD_SIZE size, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(text, size);
}

// ── HTML stylesheet (active theme and font) ───────────────────────────────────

QString MarkdownRenderer::wrap_html(const QString& body) {
    // Qt's QTextEdit::setHtml() supports a limited CSS subset.
    // We embed a <style> block that overrides the defaults. It is built from
    // the active theme on every call; callers re-render on theme_changed.
    const QString text = colors::TEXT_PRIMARY;
    const QString muted = colors::TEXT_SECONDARY;
    const QString accent = colors::AMBER;
    const QString accent_dim = colors::AMBER_DIM;
    const QString link = colors::CYAN;
    const QString code = colors::CYAN;
    const QString code_bg = colors::BG_RAISED;
    const QString border = colors::BORDER_MED;
    const QString row_alt = colors::ROW_ALT;
    // The user's font as a CSS declaration (a single-quoted family list).
    const QString family = QStringLiteral("font-family:%1;").arg(QString(fonts::DATA_FAMILY));

    QString css;
    css.reserve(2048);
    css += QStringLiteral("\n<style>\n");
    css += "body  { color:" + text + "; " + family + " font-size:" + fonts::px(0) +
           "px; margin:0; padding:0; }\n";
    css += "h1    { color:" + accent + "; font-size:" + fonts::px(4) + "px; font-weight:700; margin:8px 0 4px; }\n";
    css += "h2    { color:" + accent + "; font-size:" + fonts::px(2) + "px; font-weight:700; margin:7px 0 3px; }\n";
    css += "h3    { color:" + accent + "; font-size:" + fonts::px(0) + "px; font-weight:700; margin:6px 0 2px; }\n";
    css += "h4,h5,h6 { color:" + accent_dim + "; font-size:" + fonts::px(-1) +
           "px; font-weight:600; margin:5px 0 2px; }\n";
    css += QStringLiteral("p     { margin:4px 0; line-height:1.55; }\n");
    css += "a     { color:" + link + "; text-decoration:none; }\n";
    css += "strong,b { color:" + accent + "; font-weight:700; }\n";
    css += "em,i  { color:" + muted + "; font-style:italic; }\n";
    css += "code  { " + family + " font-size:" + fonts::px(-1) + "px; color:" + code +
           "; background:" + code_bg + "; padding:1px 5px; }\n";
    css += "pre   { background:" + code_bg + "; border:1px solid " + border +
           "; padding:10px 12px; margin:6px 0; " + family + " font-size:" + fonts::px(-1) +
           "px; color:" + code + "; white-space:pre-wrap; }\n";
    css += QStringLiteral("pre code { background:transparent; padding:0; }\n");
    css += "blockquote { border-left:2px solid " + accent + "; margin:6px 0 6px 4px; padding:4px 10px; color:" +
           muted + "; font-style:italic; }\n";
    css += QStringLiteral("ul,ol { margin:4px 0; padding-left:22px; }\n");
    css += QStringLiteral("li    { margin:2px 0; line-height:1.5; }\n");
    css += QStringLiteral("table { border-collapse:collapse; margin:6px 0; width:100%; }\n");
    css += "th    { background:" + code_bg + "; color:" + accent + "; font-weight:700; padding:5px 10px; border:1px solid " +
           border + "; text-align:left; }\n";
    css += "td    { padding:4px 10px; border:1px solid " + border + "; color:" + text + "; }\n";
    css += "tr:nth-child(even) td { background:" + row_alt + "; }\n";
    css += "hr    { border:none; border-top:1px solid " + border + "; margin:8px 0; }\n";
    css += QStringLiteral("</style>\n");
    return css + body;
}

// ── Public API ────────────────────────────────────────────────────────────────

QString MarkdownRenderer::render(const QString& markdown) {
    if (markdown.isEmpty())
        return {};

    const std::string input = markdown.toStdString();
    std::string output;
    output.reserve(input.size() * 2);

    // MD_FLAG_TABLES      — GFM-style tables
    // MD_FLAG_STRIKETHROUGH — ~~strike~~
    // MD_FLAG_TASKLISTS   — - [x] checkboxes
    // MD_FLAG_LATEXMATHSPANS — skip (not needed)
    unsigned flags = MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH | MD_FLAG_TASKLISTS;

    int rc = md_html(input.c_str(), static_cast<MD_SIZE>(input.size()), md4c_callback, &output, flags,
                     /*render_flags=*/0);

    if (rc != 0) {
        // Parsing failed — return as plain preformatted text
        return QString("<pre>%1</pre>").arg(markdown.toHtmlEscaped());
    }

    return wrap_html(QString::fromUtf8(output.data(), static_cast<int>(output.size())));
}

} // namespace fincept::ui
