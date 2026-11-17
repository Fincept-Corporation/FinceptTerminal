#pragma once
#include "screens/common/ISectionLinked.h"

#include <QEvent>
#include <QHash>
#include <QLabel>
#include <QListWidget>
#include <QPointer>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

class QSplitter;
class QLineEdit;
class QPushButton;

namespace fincept::ui {
class EmptyState;
}

namespace fincept::screens {

/// Comprehensive in-app documentation browser for all terminal features.
/// Organized by category with skill-level guidance (Beginner → Pro).
class DocsScreen : public QWidget, public ISectionLinked {
    Q_OBJECT
  public:
    explicit DocsScreen(QWidget* parent = nullptr);

    /// ISectionLinked — a topic / page id ("welcome", "getting_started", "shortcuts", "fno", … — the
    /// sidebar's ids, build_content_pages()), also "page:<id>" / "topic:<id>": the sidebar click path
    /// (a topic search that hides it is cleared first). "" + focus = the topic search text.
    bool open_section(const QString& section, const QString& focus) override;

  protected:
    void changeEvent(QEvent* event) override;
    /// Search box keys: Enter opens the first match, Down moves into the topic
    /// list, Esc clears the search.
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    void build_sidebar();
    void build_content_pages();
    void navigate_to(const QString& section_id);
    /// Filter the sidebar tree by topic/category text (live, from the search box).
    void apply_search(const QString& text);
    /// The sidebar's "no topic matches" state over the tree (`show` false hides it).
    void update_search_empty(bool show, const QString& needle);
    /// Recompute the "N TOPICS | M CATEGORIES" chip from the actual tree.
    void update_topic_count();
    /// Show/hide the command-bar "OPEN SCREEN" button for the current topic.
    void update_open_button();
    /// Rebuild the keyboard-shortcuts page so it reflects the live key bindings
    /// (the user may have rebound keys in Settings since the page was built).
    void refresh_shortcuts_page();

    /// Re-apply tr() lookups. Static documentation content (sidebar tree + all
    /// pages) is rebuilt from scratch on QEvent::LanguageChange — caching the
    /// hundreds of labels that make up the docs would dominate the source.
    /// The current section is preserved across the rebuild by section_id.
    void retranslateUi();

    // ── Helpers ──────────────────────────────────────────────────────────────
    QWidget* make_page(const QString& title, const QString& subtitle,
                       const std::vector<std::pair<QString, QString>>& sections);
    QWidget* make_section_panel(const QString& icon, const QString& title, const QString& body,
                                const QString& accent_color);
    QWidget* make_skill_panel(const QString& beginner, const QString& intermediate, const QString& advanced,
                              const QString& pro);
    QWidget* make_tip_box(const QString& text, const QString& color);
    QLabel* make_heading(const QString& text);
    QLabel* make_body_label(const QString& text);
    QLabel* make_muted_label(const QString& text);

    // ── Content page builders ────────────────────────────────────────────────
    QWidget* page_welcome();
    QWidget* page_getting_started();
    QWidget* page_keyboard_shortcuts();

    // Core screens
    QWidget* page_dashboard();
    QWidget* page_markets();
    QWidget* page_news();
    QWidget* page_watchlist();
    QWidget* page_screener();

    // Trading
    QWidget* page_crypto_trading();
    QWidget* page_crypto_center();
    QWidget* page_equity_trading();
    QWidget* page_fno();
    QWidget* page_paper_trading();
    QWidget* page_algo_trading();
    QWidget* page_backtesting();

    // Research & Analytics
    QWidget* page_equity_research();
    QWidget* page_surface_analytics();
    QWidget* page_derivatives();
    QWidget* page_portfolio();
    QWidget* page_ma_analytics();

    // AI & Quantitative
    QWidget* page_ai_quant_lab();
    QWidget* page_quantlib();
    QWidget* page_ai_chat();
    QWidget* page_agent_config();
    QWidget* page_alpha_arena();
    QWidget* page_agent_workbench();

    // Data Sources
    QWidget* page_dbnomics();
    QWidget* page_economics();
    QWidget* page_akshare();
    QWidget* page_gov_data();
    QWidget* page_data_sources();
    QWidget* page_asia_markets();
    QWidget* page_trade_viz();

    // Geopolitics & Alt
    QWidget* page_geopolitics();
    QWidget* page_maritime();
    QWidget* page_polymarket();
    QWidget* page_alt_investments();
    QWidget* page_relationship_map();

    // Tools
    QWidget* page_report_builder();
    QWidget* page_node_editor();
    QWidget* page_code_editor();
    QWidget* page_excel();
    QWidget* page_notes();
    QWidget* page_mcp_servers();
    QWidget* page_data_mapping();
    QWidget* page_file_manager();

    // Account
    QWidget* page_settings();
    QWidget* page_profile();

    // ── Members ──────────────────────────────────────────────────────────────
    QSplitter* splitter_ = nullptr; // owns sidebar_ + pages_; reused on rebuild
    QTreeWidget* sidebar_ = nullptr;
    /// Over the sidebar's viewport while a search hides every topic; dies with the
    /// sidebar when a language change rebuilds it (hence QPointer).
    QPointer<ui::EmptyState> search_empty_;
    QStackedWidget* pages_ = nullptr;
    QLabel* breadcrumb_ = nullptr;
    // Command-bar widgets (cached for retranslateUi)
    QLabel* cmd_title_ = nullptr;
    QLabel* cmd_count_ = nullptr;
    QLineEdit* search_input_ = nullptr;
    QPushButton* open_btn_ = nullptr; ///< "OPEN SCREEN" — opens the screen the current topic documents
    QString current_topic_;           ///< section id of the page on top of pages_
    QMap<QString, int> page_index_; // section_id → stacked widget index
    /// Pages are built the first time they are shown (45 pages of hundreds of
    /// labels took seconds to build up front). Until then the stack holds an
    /// empty placeholder at the page's index.
    using PageBuilder = QWidget* (DocsScreen::*)();
    QHash<QString, PageBuilder> page_builders_; // section_id → builder (not built yet)
    QWidget* ensure_page(const QString& section_id);
    /// Lower-cased text of every page, so search finds topics by what they say
    /// ("XIRR", "VaR") and not only by their titles. Filled as pages are built;
    /// the first search builds the rest in the background, one page per
    /// event-loop turn, and re-applies itself as their text arrives.
    QHash<QString, QString> page_text_;
    void index_page_text();
    void index_page(const QString& section_id, QWidget* page);
    void index_next_page();
    bool indexing_ = false;
};

} // namespace fincept::screens
