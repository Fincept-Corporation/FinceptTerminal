// src/screens/geopolitics/HDXDataPanel.h
#pragma once
#include "services/geopolitics/GeopoliticsTypes.h"

#include <QComboBox>
#include <QEvent>
#include <QHash>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSet>
#include <QStringList>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

class QStackedWidget;

namespace fincept::ui {
class AutoLoadPane;
class EmptyState;
} // namespace fincept::ui

namespace fincept::screens {

/// HDX Humanitarian Data Exchange panel — search, browse, and explore datasets.
class HDXDataPanel : public QWidget {
    Q_OBJECT
  public:
    explicit HDXDataPanel(QWidget* parent = nullptr);

    /// Jump to the Explorer view and search HDX for `country` (used by the
    /// Relations tab). Safe to call before the panel has ever been shown.
    void explore_country(const QString& country);
    /// Same, for a topic keyword ("displacement", "food security", ...).
    void explore_topic(const QString& topic);
    /// Same, for one HDX organisation's datasets (`org_slug`, e.g. "unhcr").
    void explore_organization(const QString& org_slug);

  protected:
    void changeEvent(QEvent* event) override;

  private slots:
    void on_hdx_results(const QString& context, QVector<fincept::services::geo::HDXDataset> datasets);
    void on_hdx_total(const QString& context, int total);
    void on_error(const QString& context, const QString& message);
    void on_view_changed(int index);

  private:
    void build_ui();
    void connect_service();
    void populate_table(const QVector<fincept::services::geo::HDXDataset>& datasets);
    /// Highlight the active view button and show/hide the explorer bar.
    void set_active_view(int index);
    void retranslateUi();
    /// Result context the active view shows ("conflicts", "humanitarian",
    /// explorer_ctx_, "search").
    QString current_context() const;

    // ── The shared AutoLoadPane (Policy::AutoRun: HDX is free public data) ──
    /// Pane view of the active view: "conflicts", "humanitarian",
    /// "explorer/<ctx>/<query>", "search/<query>" — each remembered on its own.
    QString view_key() const;
    /// Switch the pane to the active view (default Explorer / Datasets query
    /// first); it shows the remembered result or runs the search on first show.
    void show_view();
    /// An explicit search (SEARCH, Enter, a Relations drill-down): run now.
    void search_now();
    /// Pane start: issue the active view's search with its row limit.
    void start_search(quint64 id);
    /// Pane render: {"datasets": [...], "total": n, "limit": n}.
    void render_payload(const QJsonValue& payload);

    // View tab buttons
    QVector<QPushButton*> view_buttons_;
    int active_view_ = 0;

    // Search
    QLineEdit* search_edit_ = nullptr;

    // Datasets table inside the pane (content: table | no-match state)
    fincept::ui::AutoLoadPane* pane_ = nullptr;
    QStackedWidget* content_stack_ = nullptr;
    QTableWidget* datasets_table_ = nullptr;
    fincept::ui::EmptyState* message_state_ = nullptr; // no matches
    /// Result context -> the pane run waiting for it.
    QHash<QString, quint64> pending_;
    /// Result context -> searches still out (an answer that is not the last one is dropped).
    QHash<QString, int> outstanding_;
    // Explorer filter bar (shown only on Explorer tab)
    QWidget* explorer_bar_ = nullptr;
    QComboBox* country_combo_ = nullptr;
    QComboBox* topic_combo_ = nullptr;

    // Stats badge ("20 of 1,768 datasets") + LOAD MORE (re-runs with a larger limit)
    QLabel* dataset_count_ = nullptr;
    QPushButton* load_more_btn_ = nullptr;
    QHash<QString, int> totals_; // context -> total matches reported by HDX
    QHash<QString, int> limits_; // context -> rows asked for in the last run
    // What the Explorer / Datasets views last searched for (LOAD MORE re-runs it).
    QString explorer_ctx_ = QStringLiteral("country"); // "country" | "topic" | "organization"
    QString explorer_query_;
    QString search_query_;

    // Static text widgets (cached for retranslateUi)
    QLabel* title_lbl_ = nullptr;
    QLabel* country_lbl_ = nullptr;
    QLabel* topic_lbl_ = nullptr;
    QPushButton* explore_btn_ = nullptr;
    /// HDX source + licence attribution footer (required by HDX terms).
    QLabel* attribution_lbl_ = nullptr;
    // Fixed English view-tab labels, re-applied in retranslateUi (upper-cased).
    QStringList view_labels_;
};

} // namespace fincept::screens
