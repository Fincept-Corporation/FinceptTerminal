// src/ui/markdown/MarkdownRenderer.cpp
#include "ui/markdown/MarkdownRenderer.h"

#include <md4c-html.h>

#include <QRegularExpression>
#include <QString>
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
body  { color:#cdd6f4; font-family:'Segoe UI',Arial,sans-serif; font-size:13px; margin:0; padding:0; }
h1    { color:#cba6f7; font-size:18px; font-weight:700; margin:8px 0 4px; }
h2    { color:#cba6f7; font-size:16px; font-weight:700; margin:7px 0 3px; }
h3    { color:#b4befe; font-size:14px; font-weight:700; margin:6px 0 2px; }
h4,h5,h6 { color:#b4befe; font-size:13px; font-weight:600; margin:5px 0 2px; }
p     { margin:4px 0; line-height:1.55; }
a     { color:#89dceb; text-decoration:none; }
strong,b { color:#f9e2af; font-weight:700; }
em,i  { color:#f2cdcd; font-style:italic; }
code  { font-family:'Cascadia Code','Consolas','Courier New',monospace;
        font-size:12px; color:#a6e3a1;
        background:#1e1e2e; padding:1px 5px; border-radius:3px; }
pre   { background:#1e1e2e; border:1px solid #45475a;
        border-radius:5px; padding:10px 12px; margin:6px 0;
        font-family:'Cascadia Code','Consolas','Courier New',monospace;
        font-size:12px; color:#a6e3a1; white-space:pre-wrap; }
pre code { background:transparent; padding:0; border-radius:0; }
blockquote { border-left:3px solid #6c7086; margin:6px 0 6px 4px;
             padding:4px 10px; color:#9399b2; font-style:italic; }
ul,ol { margin:4px 0; padding-left:22px; }
li    { margin:2px 0; line-height:1.5; }
table { border-collapse:collapse; margin:6px 0; width:100%; }
th    { background:#313244; color:#cba6f7; font-weight:700;
        padding:5px 10px; border:1px solid #45475a; text-align:left; }
td    { padding:4px 10px; border:1px solid #45475a; color:#cdd6f4; }
tr:nth-child(even) td { background:#1e1e2e; }
hr    { border:none; border-top:1px solid #45475a; margin:8px 0; }
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

    int rc = md_html(input.c_str(),
                     static_cast<MD_SIZE>(input.size()),
                     md4c_callback,
                     &output,
                     flags,
                     /*render_flags=*/0);

    if (rc != 0) {
        // Parsing failed — return as plain preformatted text
        return QString("<pre>%1</pre>").arg(markdown.toHtmlEscaped());
    }

    return wrap_html(QString::fromUtf8(output.data(), static_cast<int>(output.size())));
}

} // namespace fincept::ui
