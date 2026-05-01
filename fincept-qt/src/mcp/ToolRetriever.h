#pragma once
// ToolRetriever.h — BM25 semantic-ish retrieval over the registered MCP tool catalog.
//
// Background — proven pattern from Anthropic's Tool Search (Nov 2025) and
// the broader "Tool RAG" literature (Red Hat, IBM, RAG-MCP arXiv 2025/26):
// once a tool catalog exceeds ~30-50 tools, sending all definitions to the
// LLM each turn collapses tool-pick accuracy. The fix is on-demand retrieval:
// expose ONE meta-tool ("tool.list") that semantically searches the catalog
// and returns the top-K most-relevant tools as references, then the LLM
// fetches the full schema for the one it wants and invokes it.
//
// This file implements the retrieval. BM25 (Okapi) over a per-tool document
// composed of `name + description + parameter names + parameter descriptions`.
// Pure C++, no embeddings, no Python, no model files. Anthropic's own server-
// side tool_search ships in two flavours — regex and BM25 — and BM25 is the
// one recommended for general natural-language queries.
//
// Index lifecycle: lazy. The retriever pulls tools from McpProvider::list_all_tools()
// and tracks `McpProvider::generation()` so external server starts/stops or
// runtime tool registrations trigger a rebuild on the next search.
//
// Thread safety: search() is callable from any thread (the MCP tool dispatch
// path runs on QtConcurrent worker threads). Internal mutex guards the index.

#include "mcp/McpTypes.h"

#include <QHash>
#include <QMutex>
#include <QString>
#include <QStringList>
#include <QVector>

#include <vector>

namespace fincept::mcp {

/// One ranked retrieval result.
struct ToolMatch {
    QString name;          // Bare tool name (no server_id prefix)
    QString category;
    QString description;   // Truncated to ~200 chars for prompt-friendliness
    bool is_destructive = false;
    double score = 0.0;    // BM25 score; higher = more relevant
};

class ToolRetriever {
  public:
    static ToolRetriever& instance();

    /// Search the tool catalog. Returns up to top_k most relevant tools.
    ///
    /// `query` — natural-language string. Tokenised, lowercased, stop-word
    ///           filtered, BM25-scored against per-tool documents.
    /// `top_k` — clamped to [1, 20]; defaults align with Anthropic's 3-5 band.
    /// `category_filter` — when non-empty, only tools in this category are scored.
    ///
    /// Disabled tools are excluded. Result is sorted by descending score.
    /// An empty query returns empty results (caller should fall back).
    std::vector<ToolMatch> search(const QString& query,
                                   int top_k = 5,
                                   const QString& category_filter = {});

    /// Force a rebuild on the next search. Mostly for tests; production code
    /// can rely on the generation-counter check inside search().
    void invalidate();

    ToolRetriever(const ToolRetriever&) = delete;
    ToolRetriever& operator=(const ToolRetriever&) = delete;

  private:
    ToolRetriever() = default;

    // ── Per-tool document (built once per index rebuild) ────────────────
    struct Doc {
        QString name;
        QString category;
        QString description;       // Original full description (returned to caller)
        bool is_destructive = false;

        // Tokenised fields. We weight name tokens 3x, parameter-name tokens 2x
        // (they're the strongest discriminator for tool selection), and
        // description tokens 1x. This is captured by repeating tokens at
        // index build time so BM25 TF naturally accounts for it.
        QStringList all_tokens;    // Concatenated weighted token stream
        int length = 0;            // |D| in BM25 formula
    };

    // ── Index state — guarded by mutex_ ────────────────────────────────
    mutable QMutex mutex_;
    quint64 indexed_generation_ = static_cast<quint64>(-1); // -1 = never built
    std::vector<Doc> docs_;
    QHash<QString, int> df_;       // term → document frequency
    double avg_doc_length_ = 0.0;

    // ── BM25 hyperparameters (Okapi defaults — battle-tested) ──────────
    static constexpr double kK1 = 1.5;   // term-frequency saturation
    static constexpr double kB = 0.75;   // length normalisation

    // Helpers (require mutex_ held).
    void rebuild_index_locked();
    static QStringList tokenise(const QString& text);
    static bool is_stop_word(const QString& token);
};

} // namespace fincept::mcp
