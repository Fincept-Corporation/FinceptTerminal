// src/ui/markdown/MarkdownRenderer.cpp
#include "ui/markdown/MarkdownRenderer.h"

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

// ── HTML stylesheet (Obsidian dark theme) ─────────────────────────────────────

QString MarkdownRenderer::wrap_html(const QString& body) {
    // Qt's QTextEdit::setHtml() supports a limited CSS subset.
    // We embed a <style> block that overrides the defaults.
    static const QLatin1String kStyle(R"(
<style>
body  { color:#e5e5e5; font-family:'Consolas','Courier New',monospace; font-size:14px; margin:0; padding:0; }
h1    { color:#d97706; font-size:18px; font-weight:700; margin:8px 0 4px; }
h2    { color:#d97706; font-size:16px; font-weight:700; margin:7px 0 3px; }
h3    { color:#d97706; font-size:14px; font-weight:700; margin:6px 0 2px; }
h4,h5,h6 { color:#b45309; font-size:13px; font-weight:600; margin:5px 0 2px; }
p     { margin:4px 0; line-height:1.55; }
a     { color:#d97706; text-decoration:none; }
strong,b { color:#f59e0b; font-weight:700; }
em,i  { color:#808080; font-style:italic; }
code  { font-family:'Consolas','Courier New',monospace;
        font-size:13px; color:#16a34a;
        background:#111111; padding:1px 5px; }
pre   { background:#111111; border:1px solid #1a1a1a;
        padding:10px 12px; margin:6px 0;
        font-family:'Consolas','Courier New',monospace;
        font-size:13px; color:#16a34a; white-space:pre-wrap; }
pre code { background:transparent; padding:0; }
blockquote { border-left:2px solid #d97706; margin:6px 0 6px 4px;
             padding:4px 10px; color:#808080; font-style:italic; }
ul,ol { margin:4px 0; padding-left:22px; }
li    { margin:2px 0; line-height:1.5; }
table { border-collapse:collapse; margin:6px 0; width:100%; }
th    { background:#111111; color:#d97706; font-weight:700;
        padding:5px 10px; border:1px solid #1a1a1a; text-align:left; }
td    { padding:4px 10px; border:1px solid #1a1a1a; color:#e5e5e5; }
tr:nth-child(even) td { background:#0d0d0d; }
hr    { border:none; border-top:1px solid #1a1a1a; margin:8px 0; }
</style>
)");
    return kStyle + body;
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
