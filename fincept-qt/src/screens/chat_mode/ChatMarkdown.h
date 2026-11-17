#pragma once
// ChatMarkdown.h — model output (Markdown) -> transcript blocks, safely (2026-11-17).
//
// md4c parses (CommonMark + GitHub tables / strikethrough / task lists; raw HTML is
// switched OFF, so "<script>" / "<img src=…>" arrive as text and are escaped). The
// output is our own: every text run is HTML-escaped and only these tags are written:
// b i s u code a br p ul ol li blockquote pre h1-h6 table tr th td span. Links are
// kept only for http / https / mailto (link_allowed) — anything else stays text;
// images are never loaded (an allowed image URL becomes a "[image: alt]" link). $AAPL
// cashtags and the run's context symbols become fincept-symbol: links (ticker chips).
//
// Top-level blocks are separate (paragraph, heading, list, quote, code, table, rule)
// so the transcript can virtualise per block and stream incrementally: StreamCursor
// finds where the streamed text is made of complete blocks (the last blank line
// outside a code fence); only the open tail after it is parsed again on the next
// update. Qt Core + md4c only: safe on worker threads (history loads parse there).

#include <QSet>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVector>

namespace fincept::chat_mode::md {

enum class BlockKind { Paragraph, Heading, List, Quote, Code, Table, Rule };

struct Block {
    BlockKind kind = BlockKind::Paragraph;
    int level = 0;             ///< Heading: 1..6
    QString html;              ///< Paragraph / Heading / List / Quote: safe rich text (inner HTML)
    QString plain;             ///< plain text (copy / quote / estimates); Code: the code itself
    QString lang;              ///< Code: the fence's language tag ("python"), may be empty
    QStringList header;        ///< Table: column titles (plain)
    QVector<QStringList> rows; ///< Table: body cells (plain), each padded to header.size()
    QVector<int> align;        ///< Table: 0 default, 1 left, 2 centre, 3 right
    QVector<bool> numeric;     ///< Table: the column holds numbers (right-aligned, tabular)
    bool operator==(const Block& o) const {
        return kind == o.kind && level == o.level && html == o.html && plain == o.plain && lang == o.lang &&
               header == o.header && rows == o.rows;
    }
    bool operator!=(const Block& o) const { return !(*this == o); }
};

struct Link {
    QString url;  ///< allowed scheme only
    QString text; ///< the link text (plain), may be empty
};

struct Document {
    QVector<Block> blocks;
    QVector<Link> links; ///< allowed links in order of first appearance, unique by URL
};

struct ParseOptions {
    QSet<QString> symbols; ///< bare words linked as ticker chips (exact, case-sensitive)
    bool cashtags = true;  ///< "$AAPL" becomes a ticker chip
};

/// URL scheme of ticker chips: "fincept-symbol:AAPL".
inline constexpr const char* kSymbolScheme = "fincept-symbol";

/// http / https / mailto with a host (mailto: a path) — the only links opened.
bool link_allowed(const QUrl& url);

/// Parse `markdown` into blocks. Never throws; malformed input becomes paragraphs.
Document parse(const QString& markdown, const ParseOptions& options = {});

/// Escape plain text for the rich-text widgets (& < > " and line breaks as <br>).
QString escape_html(const QString& plain, bool keep_line_breaks = true);

/// Where streamed Markdown is made of complete blocks. Feed it the whole text so far
/// (append-only); advance() scans only the new complete lines and returns the end of
/// the stable prefix: text.left(stable_end()) will not change any more, so its blocks
/// can be parsed once; the rest (the open tail) is parsed again on each update.
class StreamCursor {
  public:
    int advance(const QString& text);
    int stable_end() const { return stable_; }
    void reset() { *this = StreamCursor(); }

  private:
    int scanned_ = 0;  ///< first character not scanned yet (always a line start)
    int stable_ = 0;
    bool fence_ = false;
    QChar fence_char_;
    int fence_len_ = 0;
};

} // namespace fincept::chat_mode::md
