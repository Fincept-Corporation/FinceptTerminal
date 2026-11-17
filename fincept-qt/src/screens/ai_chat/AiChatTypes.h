#pragma once
// AiChatTypes.h — the AI Chat tab's message model and its pure helpers.
//
// QtCore only, no widgets, no theme, no database: everything here is re-entrant and
// may run on a worker thread. The transcript renderer (AiChatTranscript) parses long
// answers with parse_markdown() on a worker thread when a session loads, and parses
// only the streaming tail on the GUI thread while an answer arrives.
//
//   Turn            one block of the transcript (YOU / the model / a notice)
//   ToolStep        one tool call of an answer (activity rows), persisted with the
//                   answer as an HTML-comment trailer (tool_trailer / strip_tool_trailer)
//   Attachment      what a user turn carried besides the typed text (files, context)
//   Block/ParsedDoc an answer split into blocks (paragraphs, headings, lists, quotes,
//                   code, tables, rules, math) with SANITISED inline HTML
//
// Inline HTML produced by the parser (Block::html, Block::items) is built from escaped
// text only: the model's own HTML is never passed through. It carries neutral markers
// that style::inline_html() (AiChatStyle.h, GUI thread) turns into themed styles:
//   <code>…</code>                      inline code
//   <a class="ln" href="https://…">     link (http / https / mailto only)
//   <a class="tk" href="fsym:AAPL">     recognised ticker (a chip that opens the symbol)
//   <span class="n">1,234.5</span>      a figure (tabular monospace)
//   <span class="up">+2.4%</span>       a signed move (direction colour + its sign)
//   <span class="dn">-1.1%</span>
//   <span class="m">x^2</span>         inline math (\( … \) or $$ … $$ on one line)
//   <b> <i> <s> <br>                    emphasis, strike, line break

#include <QDate>
#include <QDateTime>
#include <QJsonArray>
#include <QMetaType>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::screens::aichat {

// ── Tool activity ─────────────────────────────────────────────────────────────

struct ToolStep {
    enum class State { Running, Ok, Failed, Job };
    QString label;          ///< "get_quote · AAPL" (tool name, then the arguments summary)
    QString base;           ///< "get_quote" — consecutive steps of one base collapse into a row
    QString detail;         ///< failure reason / background job id / a short note
    State state = State::Running;
    qint64 started_ms = 0;  ///< UI clock, for steps that never get a completion record
    qint64 elapsed_ms = -1; ///< from the completion record; -1 = unknown
    bool parallel = false;  ///< elapsed covers a multi-call round
};

/// One activity row: consecutive steps of the same tool collapsed ("set_excel_cell ×45").
struct ToolRow {
    QString tool;           ///< "get_quote"
    QString args;           ///< "AAPL" (the first step's argument summary; empty for a run)
    int count = 1;
    int failed = 0;
    int running = 0;
    int jobs = 0;
    ToolStep::State state = ToolStep::State::Ok; ///< Running > Failed > Job > Ok
    bool timed = false;     ///< at least one step reported a duration
    qint64 ms = 0;          ///< summed (parallel rounds: the round's wall time once)
    bool parallel = false;
    QString detail;         ///< first failure reason, else the job id, else the first note
    QString job_id;
};

struct ToolSummary {
    int calls = 0;
    int failed = 0;
    int jobs = 0;
    int running = 0;
    bool timed = false;
    qint64 ms = 0; ///< wall time (parallel rounds counted once)
};

QVector<ToolRow> tool_rows(const QVector<ToolStep>& steps);
ToolSummary summarize_tools(const QVector<ToolStep>& steps);
QJsonArray tool_steps_to_json(const QVector<ToolStep>& steps);
QVector<ToolStep> tool_steps_from_json(const QJsonArray& steps);
/// "\n\n<!--fincept-tools:BASE64-->" (the newest 300 steps) or "" when there are none.
/// chat_messages has no tool column: the steps travel with the stored answer.
QString tool_trailer(const QVector<ToolStep>& steps);
/// The answer without its trailer; the steps it carried into `steps` (may be null).
QString strip_tool_trailer(const QString& content, QVector<ToolStep>* steps);
/// "840 ms", "1.2 s", "2m 05s"; "" for a negative value.
QString format_ms(qint64 ms);
/// "950", "12.4k", "1.2M" — compact token / count figure.
QString format_count(qint64 n);

// ── Attachments / the user envelope ───────────────────────────────────────────

struct Attachment {
    enum class Kind { File, Image, Symbol, Portfolio, Watchlist };
    Kind kind = Kind::File;
    QString name;          ///< file name, "AAPL", portfolio / watchlist name
    QString ref;           ///< absolute path, symbol, portfolio / watchlist id
    QString detail;        ///< asset class ("equity"), venue, …
    qint64 bytes = -1;     ///< file size from the file system (metadata, not contents); -1 = unknown
    bool truncated = false;///< the file was cut to kAttachMaxChars when sent
    bool linked = false;   ///< Symbol: arrived through symbol-group linking (not typed by the user)

    bool operator==(const Attachment& o) const {
        return kind == o.kind && name == o.name && ref == o.ref && detail == o.detail && bytes == o.bytes &&
               truncated == o.truncated && linked == o.linked;
    }
};

/// Characters of one attached text file that are sent (the rest is cut and the model is told).
inline constexpr qsizetype kAttachMaxChars = 32000;

/// One file read for sending (GUI thread reads it on Send — only files the user attached).
struct FilePayload {
    QString name;
    QString content;
    bool truncated = false;
};

/// The exact lines context attachments add in front of a message — what the context rail
/// shows under "WHAT WILL BE SENT". Symbols: "[Context: AAPL (equity)]" (the line the tab has
/// always sent for a linked symbol); portfolios / watchlists: a reference the model can look
/// up with its tools, never their contents. Files are not part of this (see build_user_wire).
QString context_lines(const QVector<Attachment>& context);
/// The stored / sent text of a user turn:
///   context lines + "\n\n"  (when any)
///   per file: "[Attached file: NAME]" (or "… — truncated to the first N characters]")
///             + "\n\n" + CONTENT + "\n\n---\n\n"
///   + what the user typed.
/// Built with single multi-argument QString::arg() calls (names and contents are outside text).
QString build_user_wire(const QVector<Attachment>& context, const QVector<FilePayload>& files, const QString& typed);

struct UserEnvelope {
    QString typed;                  ///< what the user typed (shown in the transcript)
    QVector<Attachment> chips;      ///< files (name, chars sent) and context symbols / refs
};
/// Inverse of build_user_wire for display: a stored user message → typed text + chips. Old
/// messages (single "[Attached file: …]" header, "[Context: …]" prefix) parse the same way.
UserEnvelope parse_user_wire(const QString& stored);

/// ≈ tokens of `text` (4 characters per token; CJK one per character). An estimate, shown as "≈".
int estimate_tokens(const QString& text);

// ── Turns ─────────────────────────────────────────────────────────────────────

enum class TurnRole { User, Assistant, Notice };
enum class TurnState {
    Done,       ///< finished answer / sent message
    Streaming,  ///< the answer is arriving
    Stopped,    ///< the user stopped it (partial text kept)
    Failed,     ///< the request failed (error = plain reason)
    Superseded, ///< regenerated: dimmed, never sent back to the model
    Empty       ///< success with no content
};
enum class NoticeKind {
    None,
    Info,       ///< plain system line ("Could not load this conversation …")
    Error,      ///< a failure with a plain reason (attachment unreadable, no session …)
    Credits,    ///< out of Fincept credits (VIEW PLANS · LLM SETTINGS · RETRY) + plan line
    SignIn,     ///< Fincept refused the sign-in (LLM SETTINGS · RETRY)
    Route,      ///< a harness was unavailable — answered by the next model (MODEL SETTINGS)
    RouteOffer, ///< "ask": ANSWER WITH <offer_label> · MODEL SETTINGS
    Setup       ///< no AI model configured (OPEN SETTINGS)
};

/// Hover / menu actions of a turn (AicTranscriptView::action_requested). Copy and Quote are
/// handled by the transcript itself and also reported.
enum class TurnAction {
    Copy,
    Regenerate,   ///< newest answer — spends
    Retry,        ///< newest failed / stopped / notice turn — spends
    EditResend,   ///< user turn: its text into the composer (nothing is sent)
    AddToReport,
    Quote,        ///< "> …" into the composer
    AnswerWith,   ///< RouteOffer: answer without the unavailable harness — spends
    ModelSettings,
    LlmSettings,
    ViewPlans,
    OpenSettings
};

struct Turn {
    quint64 id = 0;                  ///< assigned by the transcript (append / set_turns)
    TurnRole role = TurnRole::User;
    TurnState state = TurnState::Done;
    NoticeKind notice = NoticeKind::None;
    QString text;                    ///< user: what was typed; assistant / notice: Markdown
    QString thinking;                ///< chain-of-thought (assistant), shown collapsed
    QVector<ToolStep> tools;         ///< tool activity (assistant)
    QVector<Attachment> attachments; ///< user: chips (files, context)
    QString provider;                ///< who answered ("openai", "harness:claude", "fincept")
    QString model;
    QString route_label;             ///< "Claude Code · sonnet · high" when the service gave one
    QDateTime when;                  ///< local time; invalid = unknown
    int tokens = 0;                  ///< total tokens as reported (0 = not reported)
    int prompt_tokens = -1;          ///< this run only (-1 = not known)
    int completion_tokens = -1;
    qint64 duration_ms = -1;         ///< request → final answer (this run only)
    double cost_usd = -1.0;          ///< reported by a harness; < 0 = not reported
    QString billing_note;            ///< "0 credits · billed to your Anthropic API key"
    QString error;                   ///< plain reason (Failed, Error notice)
    QString offer_label;             ///< RouteOffer: the model that would answer
    QString skip_harness;            ///< RouteOffer: the harness to leave out
};

/// "YOU", "Claude Code · sonnet", "OpenAI / gpt-4o", "Fincept LLM" — the header name of a turn.
QString turn_speaker(const Turn& t);
/// Plain Markdown of one turn (COPY, ADD TO REPORT, QUOTE, conversation export).
QString turn_markdown(const Turn& t);

// ── Markdown → blocks ─────────────────────────────────────────────────────────

enum class BlockKind { Paragraph, Heading, List, Quote, Code, Table, Rule, Math };

struct TableData {
    QStringList header;            ///< plain cell text (inline Markdown stripped)
    QVector<QStringList> rows;     ///< plain cell text, padded to header.size()
    QVector<int> align;            ///< per column: 0 left, 1 centre, 2 right (numeric columns: right)
    QVector<bool> numeric;         ///< per column: most non-empty cells are figures
    int label_col = -1;            ///< first non-numeric column (chart x labels)
    int time_col = -1;             ///< a date / period column (chart x axis), -1 = none
    bool chartable = false;        ///< >= 3 rows, a label / time column and >= 1 numeric column
};

/// A small numeric series found in a list ("2021: 12.3", "Q1 — 4.5%"): a one-click local chart.
struct NumericSeries {
    QStringList labels;
    QVector<double> values;
    QString unit; ///< "%", "$", "" …
    bool valid() const { return values.size() >= 3 && labels.size() == values.size(); }
};

struct Block {
    BlockKind kind = BlockKind::Paragraph;
    int level = 0;            ///< Heading: 1..6
    QString html;             ///< Paragraph / Heading / Quote: sanitised inline HTML (markers above)
    QStringList items;        ///< List: one sanitised inline HTML string per item
    QVector<int> item_levels; ///< List: nesting depth per item (0 = top)
    bool ordered = false;     ///< List: numbered
    int start = 1;            ///< List: first number
    QString lang;             ///< Code: language tag ("python", "sql", "" …)
    QString code;             ///< Code / Math: raw text (never HTML)
    TableData table;          ///< Table
    NumericSeries series;     ///< List: a chartable series, when found
    QString source;           ///< the block's Markdown (COPY, QUOTE)
    bool closed = true;       ///< false: still streaming (open fence, the text's last block)
    int src_begin = 0;        ///< offsets of the block in the parsed text
    int src_end = 0;
};

/// A numbered source under an answer (every distinct http/https link, in order of appearance).
struct SourceLink {
    int number = 0;
    QString url;
    QString title; ///< the link text ("" for a bare URL)
    QString host;  ///< "reuters.com"
};

struct ParseOptions {
    /// Bare words recognised as tickers in addition to "$AAPL", "AAPL.NS", "NASDAQ:AAPL",
    /// "(NYSE: IBM)": symbols the conversation already established (the linked symbol,
    /// mentions, tool arguments). Upper case.
    QSet<QString> known_symbols;
    bool tickers = true;  ///< false: no ticker chips (Quick Chat keeps them too; tests)
    bool figures = true;  ///< wrap figures in <span class="n">
};

struct ParsedDoc {
    QVector<Block> blocks;
    QVector<SourceLink> sources;
    QStringList tickers; ///< distinct recognised tickers, in order
};

/// Parse `markdown` into blocks. Re-entrant; any thread. The last block is marked
/// closed = false when `streaming` (it may still grow).
ParsedDoc parse_markdown(const QString& markdown, const ParseOptions& options = {}, bool streaming = false);
/// One line of inline Markdown → sanitised inline HTML (the markers above). Re-entrant.
QString inline_markdown_html(const QString& text, const ParseOptions& options = {}, QStringList* tickers = nullptr,
                             QVector<SourceLink>* sources = nullptr);
/// Inline Markdown → plain text (table cells, previews, snippets).
QString strip_inline_markdown(const QString& text);
/// A figure in a table cell or a list ("1,234.5", "-3.2%", "$12.4B", "(45.0)", "1.2x") → value.
bool parse_figure(const QString& text, double* value);
/// A period / date label ("2024", "2024-03-31", "Q1 2024", "FY23", "Mar 2024", "H1 2025").
bool looks_like_period(const QString& text);
/// RFC 4180 CSV of a table (header first).
QString table_to_csv(const TableData& table);
/// Only http / https / mailto links may be opened (externally); everything else is refused.
bool is_safe_link(const QString& url);
/// Upper-case ticker shape: "AAPL", "BRK.B", "RELIANCE.NS", "^GSPC", "BTC-USD", "EURUSD=X".
bool looks_like_ticker(const QString& word);

// ── Sessions ──────────────────────────────────────────────────────────────────

/// One row of the sessions rail / the start page's recent list (display data only).
struct SessionSummary {
    QString id;
    QString title;       ///< display title (never empty)
    QString snippet;     ///< last message, one line ("" = not known yet: only sessions opened this run)
    QString provider;    ///< as stored with the session
    QString model;
    QDateTime updated;   ///< local time of the last change
    int messages = 0;
    bool pinned = false;
    bool running = false; ///< a request of this session is in flight
    bool unread = false;  ///< an answer arrived while the tab was not visible
};

/// chat_sessions / chat_messages timestamps (SQLite datetime('now'), UTC) → local time.
QDateTime db_time_local(const QString& stamp);
enum class DateGroup { Pinned, Today, Yesterday, ThisWeek, Older };
DateGroup date_group(const QDateTime& when, const QDate& today);
/// "TODAY", "YESTERDAY", "THIS WEEK", "OLDER", "PINNED" (translated, upper case).
QString date_group_title(DateGroup g);
/// "14:32" today, "Tue" this week, "Mar 4" this year, "2025-03-04" older.
QString short_when(const QDateTime& when, const QDate& today);
/// First line of a message, inline Markdown and envelopes stripped, cut to `max_chars`.
QString snippet_of(const QString& role, const QString& stored_content, int max_chars = 90);

// ── Prompts ───────────────────────────────────────────────────────────────────

/// A prompt the composer can insert: a built-in research template or one the user saved.
struct PromptTemplate {
    QString id;      ///< "tpl.filings", "saved.<n>"
    QString group;   ///< "Research", "Markets", "Portfolio", "Writing", "Saved"
    QString title;   ///< "Summarise filings"
    QString body;    ///< the text that FILLS the composer ({SYMBOL} is replaced when a symbol is known)
    bool saved = false;
};
/// Built-in research templates ("/" menu): summarise filings, compare peers, explain a move,
/// portfolio risk review, draft a note, … Translated.
QVector<PromptTemplate> builtin_templates();

/// An example card of the start page: fills the composer, never sends.
struct ExampleCard {
    QString group;   ///< "Markets", "Company research", "Portfolio", "Macro", "Code / data"
    QString title;   ///< short label on the card
    QString prompt;  ///< text put into the composer
};
QVector<ExampleCard> example_cards();

/// Saved prompts (SettingsRepository "ai_chat.saved_prompts", a JSON array of {title, body}).
/// GUI thread only (the settings connection lives there).
QVector<PromptTemplate> load_saved_prompts();
bool store_saved_prompts(const QVector<PromptTemplate>& prompts);

/// A one-cell Jupyter notebook (nbformat 4) holding `code` — OPEN IN CODE TAB.
QByteArray notebook_json(const QString& code);

} // namespace fincept::screens::aichat

Q_DECLARE_METATYPE(fincept::screens::aichat::TurnAction)
Q_DECLARE_METATYPE(fincept::screens::aichat::Attachment)
Q_DECLARE_METATYPE(fincept::screens::aichat::PromptTemplate)
Q_DECLARE_METATYPE(fincept::screens::aichat::SessionSummary)
