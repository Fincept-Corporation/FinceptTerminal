#pragma once
// AiChatContextRail.h — the context rail of the AI Chat tab (right zone): what the model
// can see and use. Full width at >= 1500 px; an icon strip at 1100–1500 px and below, where
// a click on an icon asks the host to show the full rail as an overlay (peek_requested).
//
//   ▌CONTEXT · sent with your next message
//     LINKED  AAPL · equity                 [ATTACHED] / [ATTACH]
//     $  MSFT · mentioned                                   ×
//     ▤  Growth portfolio · reference                       ×
//     📄 notes.csv · 12.4 KB · ≈ 3.1k tok                    ×
//     [+ ADD ▾]   Symbol… · Portfolio ▸ · Watchlist ▸ · File…
//     SENT AS   [Context: AAPL (equity)] …  (the exact lines, monospace, dim)
//   ▌TOOLS                                   [ON] [NAV]
//     markets 12 · portfolio 6 · news 4 …   (expand: tool names)       MANAGE ›
//   ▌SESSION
//     MESSAGES 24 · TOKENS 18.2k (in 12.1k / out 6.1k this run) · COST $0.04 · LAST 4.1 s
//   ▌SAVED PROMPTS                                    [+ SAVE DRAFT]
//     Morning brief                                                   ×
//
// Nothing is attached silently: every item the next message carries is listed here (and as
// a chip in the composer), with the exact text it adds. Files are listed with their size
// from the file system; their contents are read only when the message is sent.
// Figures that the services do not report are not shown (no estimates dressed as data).

#include "screens/ai_chat/AiChatTypes.h"

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include <memory>

class QLabel;
class QMenu;
class QPushButton;
class QToolButton;
class QVBoxLayout;

namespace fincept::screens::aichat {

/// Tools of one category (McpService catalogue, read off the GUI thread by the host).
struct ToolGroup {
    QString category;   ///< "markets", "portfolio", "" (external) …
    QStringList tools;  ///< tool names
};

/// Session figures — only what the services report.
struct SessionStats {
    int messages = 0;
    qint64 tokens = 0;          ///< stored total (0 = none reported)
    qint64 prompt_tokens = -1;  ///< answers of this run only; -1 = not reported
    qint64 completion_tokens = -1;
    double cost_usd = -1.0;     ///< sum of reported costs; < 0 = none reported
    qint64 last_ms = -1;        ///< duration of the last answer (this run)
    QStringList models;         ///< distinct "provider / model" that answered
    QDateTime as_of;            ///< time of the last answer
};

class AicContextRail : public QWidget {
    Q_OBJECT
  public:
    enum class Mode { Full, Icons };

    explicit AicContextRail(QWidget* parent = nullptr);
    ~AicContextRail() override;

    void set_mode(Mode mode);
    Mode mode() const { return mode_; }
    /// Full: ~30 average characters; Icons: one glyph button column.
    int preferred_width(Mode mode) const;

    // ── Context (what the next message carries) ──────────────────────────────
    /// The symbol of the linked symbol group ("" = none) and whether it is attached.
    void set_linked_symbol(const QString& symbol, const QString& asset_class, bool attached);
    /// Mentions / portfolio / watchlist references and attached files.
    void set_context(const QVector<Attachment>& context, const QVector<Attachment>& files);
    /// The exact lines the context adds in front of the message (context_lines() + file headers).
    void set_wire_preview(const QString& text);

    // ── Tools ────────────────────────────────────────────────────────────────
    /// tools_on = the model may call tools in this chat (ToolPolicy::All / None);
    /// navigation_on = it may also switch tabs (All vs NoNavigation).
    void set_tool_policy(bool tools_on, bool navigation_on);
    void set_tool_groups(const QVector<ToolGroup>& groups, bool loading);

    // ── Session / prompts ───────────────────────────────────────────────────
    void set_stats(const SessionStats& stats);
    void set_saved_prompts(const QVector<PromptTemplate>& prompts);

  signals:
    /// ATTACH / DETACH on the linked symbol.
    void linked_attach_toggled(bool attached);
    /// × on a context item or file.
    void detach_requested(const fincept::screens::aichat::Attachment& item);
    /// + ADD ▾ entries — the host opens the picker (Symbol / File) or attaches directly.
    void add_symbol_requested();
    void add_file_requested();
    void add_reference_requested(const fincept::screens::aichat::Attachment& item); ///< portfolio / watchlist picked here
    void tool_policy_changed(bool tools_on, bool navigation_on);
    void manage_tools_requested(); ///< MANAGE › (MCP Servers tab: per-tool on / off)
    void prompt_activated(const fincept::screens::aichat::PromptTemplate& prompt); ///< fills the composer
    void prompt_delete_requested(int index);
    void save_draft_requested();
    /// Icons mode: an icon was clicked — show the full rail (section: 0 context, 1 tools,
    /// 2 session, 3 prompts).
    void peek_requested(int section);

  protected:
    void changeEvent(QEvent* e) override;

  private:
    Mode mode_ = Mode::Full;
    QWidget* full_ = nullptr;   ///< the four sections (scrollable)
    QWidget* icons_ = nullptr;  ///< icon strip
    void retranslate();

    // ── private (AiChatContextRail.cpp) ──────────────────────────────────────
    struct Parts; ///< state + widgets of the four sections
    std::unique_ptr<Parts> p_;

    void build_full();
    void build_icons();
    void rebuild_context();
    void rebuild_tools();
    void rebuild_prompts();
    void update_linked();
    void update_wire();
    void update_tool_toggles();
    void update_stats();
    void update_icons();
    void fit_metrics();
    void open_add_menu();
    void fill_reference_menu(QMenu* menu, bool portfolios);
};

} // namespace fincept::screens::aichat
