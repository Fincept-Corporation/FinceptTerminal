// src/ui/markdown/MarkdownRenderer.h
#pragma once
#include <QString>

namespace fincept::ui {

/// Converts a Markdown string to styled HTML suitable for display in QTextEdit/QTextBrowser.
/// Uses md4c (CommonMark-compliant) for parsing and wraps the output in a dark-theme
/// stylesheet matching the Obsidian design system.
class MarkdownRenderer {
  public:
    /// Render markdown → complete HTML fragment (no <html>/<body> wrapper needed for QTextEdit).
    static QString render(const QString& markdown);

  private:
    MarkdownRenderer() = delete;
    static QString wrap_html(const QString& body_html);
};

} // namespace fincept::ui
