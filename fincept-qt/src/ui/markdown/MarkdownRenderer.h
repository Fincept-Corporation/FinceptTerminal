// src/ui/markdown/MarkdownRenderer.h
#pragma once
#include <QString>

namespace fincept::ui {

/// Converts a Markdown string to styled HTML suitable for display in QTextEdit/QTextBrowser.
/// Uses md4c (CommonMark-compliant) for parsing and wraps the output in a stylesheet
/// built from the active theme and font. The HTML holds the colours in effect when it
/// was rendered: viewers re-render on ThemeManager::theme_changed.
class MarkdownRenderer {
  public:
    /// Render markdown → complete HTML fragment (no <html>/<body> wrapper needed for QTextEdit).
    static QString render(const QString& markdown);

  private:
    MarkdownRenderer() = delete;
    static QString wrap_html(const QString& body_html);
};

} // namespace fincept::ui
