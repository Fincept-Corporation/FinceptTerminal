// src/screens/economics/panels/EconPanelBase.h
// Base class for all economics source panels.
// Provides: stat cards + empty/loading/error state + data table + CSV export.
// Subclasses call build_base_ui() from their constructor.
// For panels with a left selector sidebar, pass the right-side container
// to build_base_ui(container) instead.
#pragma once

#include "services/economics/EconomicsService.h"
#include "ui/widgets/PaginationBar.h"

#include <QDate>
#include <QEvent>
#include <QJsonArray>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QWidget>

namespace fincept::screens {

class EconPanelBase : public QWidget {
    Q_OBJECT
  public:
    explicit EconPanelBase(const QString& source_id, const QString& color, QWidget* parent = nullptr);

    /// Called by EconomicsScreen when user switches to this panel.
    virtual void activate() = 0;

    /// Panel-level state for persistence (text fields, selections).
    /// Default returns empty — subclasses override to save their inputs.
    virtual QVariantMap save_panel_state() const { return {}; }
    virtual void restore_panel_state(const QVariantMap& /*state*/) {}

  protected:
    // ── Subclass API ──────────────────────────────────────────────────────────
    /// Add source-specific controls into the toolbar layout.
    virtual void build_controls(QHBoxLayout* toolbar_layout) = 0;
    virtual void on_fetch() = 0;
    virtual void on_result(const QString& request_id, const services::EconomicsResult& result) = 0;

    // ── UI construction ───────────────────────────────────────────────────────
    /// Build toolbar + stat cards + table stack into the given container widget.
    /// Call this from the subclass constructor (pass 'this' or a right-side pane).
    void build_base_ui(QWidget* container);

    // ── State helpers ─────────────────────────────────────────────────────────
    // Empty msg → a translated default is used (see .cpp).
    void show_loading(const QString& msg = {});
    void show_error(const QString& msg);
    void show_empty(const QString& msg = {});
    void show_table();

    /// Populate the shared table + update stat cards.
    void display(const QJsonArray& rows, const QString& title = {});
    void export_csv();

    /// Per-provider accent color stylesheet (uses live theme tokens).
    QString panel_style() const;

    /// Called when theme changes — re-applies panel_style(). Override for extra widgets.
    virtual void refresh_panel_theme();

    // ── i18n ──────────────────────────────────────────────────────────────────
    void changeEvent(QEvent* event) override;
    /// Re-apply tr() lookups to the shared base widgets (FETCH/CSV buttons, stat
    /// card labels, current status message, record count). Subclasses that have
    /// their own translatable widgets override retranslateUi() and call this base
    /// implementation at the end.
    virtual void retranslateUi();

    // ── Shared utilities ──────────────────────────────────────────────────────
    /// Filter a QListWidget by hiding items that don't contain text (case-insensitive).
    static void filter_list(QListWidget* list, const QString& text);

    // ── Reusable token-based style helpers for sidebar panels ─────────────────
    static QString sidebar_style();      // left panel background + border
    static QString section_hdr_style();  // section header bar (e.g. "COUNTRY")
    static QString section_lbl_style();  // section header label text
    static QString search_input_style(); // search/filter QLineEdit
    static QString ctrl_label_style();   // toolbar control labels ("YEARS", "PRESET")
    QString list_style() const;          // QListWidget using source accent color
    static QString notice_style();       // warning/info notice label

    // ── Shared widgets (accessible to subclasses) ─────────────────────────────
    QLabel* stat_latest_ = nullptr;
    QLabel* stat_change_ = nullptr;
    QLabel* stat_min_ = nullptr;
    QLabel* stat_max_ = nullptr;
    QLabel* stat_avg_ = nullptr;
    QLabel* stat_count_ = nullptr;
    QPushButton* fetch_btn_ = nullptr;
    QPushButton* export_btn_ = nullptr;

    QString source_id_;
    QString color_;

  private:
    void update_stats(const QJsonArray& rows);
    void render_page();

    QWidget* container_ = nullptr;
    QWidget* cards_row_ = nullptr;
    QWidget* title_bar_ = nullptr;
    QStackedWidget* stack_ = nullptr; // 0=empty/status  1=table+pager
    QLabel* empty_lbl_ = nullptr;
    QTableWidget* table_ = nullptr;
    QLabel* title_lbl_ = nullptr;
    QLabel* row_count_ = nullptr;
    ui::PaginationBar* pager_ = nullptr;

    // Stat-card title labels (cached for retranslateUi)
    QLabel* stat_latest_lbl_ = nullptr;
    QLabel* stat_change_lbl_ = nullptr;
    QLabel* stat_min_lbl_ = nullptr;
    QLabel* stat_max_lbl_ = nullptr;
    QLabel* stat_avg_lbl_ = nullptr;
    QLabel* stat_count_lbl_ = nullptr;

    // Last status shown — so a language switch can re-render the message.
    enum class StatusKind { Empty, Loading, Error };
    StatusKind status_kind_ = StatusKind::Empty;
    QString status_msg_; // raw message last passed to show_empty/loading/error

    QJsonArray all_rows_;
    QStringList columns_;
};

} // namespace fincept::screens
