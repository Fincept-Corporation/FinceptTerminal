// src/screens/gov_data/GovDataProviderPanel.h
// Reusable panel for CKAN-style government data portals.
// Supports hierarchical browsing: Publishers/Orgs → Datasets → Resources + Search
#pragma once

#include "screens/gov_data/GovDataShape.h"
#include "screens/gov_data/GovTableModel.h"
#include "services/gov_data/GovDataService.h"
#include "ui/widgets/EmptyState.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableView>
#include <QTableWidget>
#include <QWidget>

#include <atomic>
#include <functional>
#include <memory>

namespace fincept::ui {
class AutoLoadPane;
class InputMemory;
class LoadingSkeleton;
} // namespace fincept::ui

namespace fincept::screens {

struct GovProviderOptions {
    QString watermark_text; // if non-empty, show a bottom status bar with this text
    /// Multi-portal scripts (universal_ckan_api.py): command that lists the portals the script serves
    /// ({data: [{code, name, default}]}). When set, a portal selector bar is shown and the selected
    /// portal code is passed as the first argument of every other command.
    QString portals_command;
    /// The script's "recently updated datasets" command ("recent-datasets", "recent"; arg: limit).
    /// What the DATASETS tab shows before a publisher was opened. Empty = the script has none.
    QString recent_command;
    /// Open on the recently updated datasets instead of the publishers list.
    bool land_on_recent = false;
    /// How failure texts name the source ("Canada Open Gov"); empty = "The portal".
    QString source_label;
};

class GovDataProviderPanel : public QWidget {
    Q_OBJECT
  public:
    explicit GovDataProviderPanel(const QString& script, const QString& provider_color,
                                  const QString& org_label = "Publishers", const GovProviderOptions& options = {},
                                  QWidget* parent = nullptr);

  public slots:
    /// Called by the screen when the panel is first opened. The pane loads the
    /// landing view on show by itself; this only makes sure a view is selected.
    void load_initial_data();

  protected:
    void changeEvent(QEvent* event) override;

  private slots:
    void on_result(const QString& request_id, const services::GovDataResult& result);
    void on_org_clicked(int row);
    void on_dataset_clicked(int row);
    void on_search();
    void on_back();
    void on_export_csv();

  private:
    // ── Views ────────────────────────────────────────────────────────────────
    // One ui::AutoLoadPane hosts every page; each list is a view of it, keyed
    // "[<portal>|]<kind>[:<arg>]" — kinds: portals, orgs, recent, org:<id>,
    // search:<query>, res:<dataset id>. A view remembers its last answer ("as of"),
    // so BACK and tab switches show it at once.
    enum class Kind { Portals, Orgs, Recent, Org, Search, Resources };
    struct ViewRef {
        Kind kind = Kind::Orgs;
        QString arg;
    };

    void build_ui();
    QWidget* build_toolbar();
    void build_pane(QWidget* parent);
    void retranslateUi();

    QString view_key(Kind kind, const QString& arg = {}) const;
    ViewRef parse_view(const QString& key) const;
    /// Switch the pane to `key` (age, skeleton and caption for its kind first).
    /// `user` = the user asked for it: a view with nothing remembered runs now.
    void go(const QString& key, bool user);
    void go_datasets_tab();
    void start_run(quint64 run);
    /// The payload of a list view from a script answer. Pure (no member, widget or theme access):
    /// runs on the shaping thread.
    static QJsonObject payload_for(Kind kind, const QJsonObject& data);
    void render(const QJsonValue& payload);
    void show_no_data(const ViewRef& v);

    void populate_orgs(const QJsonArray& data);
    void populate_datasets(const QJsonArray& data, int total_count);
    void populate_resources(const QJsonArray& data);
    void update_toolbar_state();
    void update_breadcrumb();

    // ── Remembered navigation (InputMemory values "portal", "view", "list", "labels") ──
    /// Reads the remembered portal, view, dataset list and their labels; returns the view to open
    /// (empty = the landing view). For a multi-portal script the view opens once the portal list is in.
    QString restore_navigation();
    /// Writes the view on screen, the dataset list and their labels.
    void remember_navigation();
    /// `key` is a view this panel can open (for the selected portal).
    bool valid_view(const QString& key) const;

    // ── Multi-portal support (options_.portals_command) ─────────────────────
    bool multi_portal() const { return !options_.portals_command.isEmpty(); }
    bool has_recent() const { return !options_.recent_command.isEmpty(); }
    void populate_portals(const QJsonArray& portals);
    void on_portal_changed(int index);
    QString landing_view() const;

    /// org_label_ is a CKAN-style key passed in by the parent ("Publishers" or
    /// "Organizations"). These helpers return the user-visible, translated form
    /// in the casing the call site needs.
    QString org_label_translated() const;       // "Publishers"
    QString org_label_translated_upper() const; // "PUBLISHERS"
    QString org_label_translated_lower() const; // "publishers"
    QString all_orgs_breadcrumb() const;        // "All Publishers"
    QString list_label(const QString& key) const;

    QString script_;
    QString color_;
    QString org_label_;
    GovProviderOptions options_;
    QString prefix_; // request-id prefix of this panel's runs

    // Toolbar
    QPushButton* back_btn_ = nullptr;
    QPushButton* orgs_btn_ = nullptr;
    QPushButton* datasets_btn_ = nullptr;
    QLineEdit* search_input_ = nullptr;
    QPushButton* fetch_btn_ = nullptr;
    QPushButton* export_btn_ = nullptr;

    // Optional portal selector bar (CKAN universal panel only)
    QLabel* portal_label_ = nullptr;
    QComboBox* portal_combo_ = nullptr;
    QString portal_;            // code of the selected portal ("" until the list has loaded)
    bool portals_loaded_ = false;

    // Breadcrumb — fixed segments, re-labelled on navigation (never rebuilt)
    QPushButton* crumb_root_ = nullptr;
    QLabel* crumb_sep1_ = nullptr;
    QPushButton* crumb_list_ = nullptr;
    QLabel* crumb_sep2_ = nullptr;
    QLabel* crumb_leaf_ = nullptr;
    QLabel* as_of_label_ = nullptr;
    QLabel* row_count_label_ = nullptr;

    // Content: the pane; inside it a stack of the three tables and the no-data page
    ui::AutoLoadPane* pane_ = nullptr;
    QStackedWidget* content_stack_ = nullptr;
    QTableView* orgs_table_ = nullptr;
    QTableView* datasets_table_ = nullptr;
    QTableView* resources_table_ = nullptr;
    GovTableModel* orgs_model_ = nullptr;
    GovTableModel* datasets_model_ = nullptr;
    GovTableModel* resources_model_ = nullptr;
    ui::EmptyState* no_data_page_ = nullptr;
    ui::LoadingSkeleton* opening_page_ = nullptr; // between the portal list and the portal's first view
    ui::InputMemory* memory_ = nullptr;

    // Navigation state
    QString list_view_;                  // last dataset list shown (recent / org / search)
    QString restore_view_;               // multi-portal: remembered view, opened once the portal list is in
    QString restore_list_;               // multi-portal: remembered dataset list, kept once the portal list is in
    QHash<QString, QString> labels_;     // view key -> what the breadcrumb calls it
    QHash<quint64, std::shared_ptr<std::atomic<bool>>> tokens_;
};

// ── Shared utilities — available to all gov_data panels ─────────────────────

/// Selection, row height (follows the font size), alternating rows of a gov table (a QTableWidget too).
void configure_table(QTableView* table);
/// Writes the table on screen as CSV (headers + every row; a cell's GovTableModel::ExportRole when it has one).
void export_table_to_csv(QAbstractItemView* table, const QString& default_name, QWidget* parent);
/// Re-applies `color` to the items of one table column that carry a foreground brush. Item brushes
/// are not reached by stylesheets, so panels call this when the appearance has settled.
void recolor_column(QTableWidget* table, int column, const QColor& color);

// Shared panel stylesheet builder — set ONCE on the panel, before its children exist.
// Pass the provider accent color (hex string as it reads on the dark theme, e.g. "#2563EB"); it is
// drawn through ui::colors::legible() / tint(), which the central re-style pass re-derives on a theme
// or font change, so no theme_changed handler is needed.
// Optional extra_qss is appended verbatim — use it for panel-specific widgets
// (QSpinBox, QComboBox, QTextBrowser, etc.) that are not in the shared set.
QString make_gov_panel_style(const QString& provider_color, const QString& extra_qss = {});

// The pure JSON helpers (request ids, field reading, compact rows) are in GovDataShape.h (included
// above), in this same namespace.
namespace gov {

/// "12.3 KB" — a file size from bytes ("—" when unknown).
QString file_size(double bytes);

/// A result pane the Gov Data way (table skeleton, timeout, "as of" label, a source label in
/// failure texts). Build it in its final parent; the host adds content, start, cancel, render.
ui::AutoLoadPane* make_pane(QWidget* parent, const QString& cache_key, QLabel* status_label);

/// A small "as of 14:02" label for a breadcrumb / toolbar (object name govAsOf).
QLabel* make_as_of_label(QWidget* parent);

/// Reports a failed request on `pane`: missing key -> the needs-setup page naming it (OPEN SETTINGS >
/// CREDENTIALS); rate limited -> when to try again; unreachable / timed out -> a plain sentence;
/// anything else -> the script's first line. Every page offers RETRY (or the settings link).
void fail_pane(ui::AutoLoadPane* pane, quint64 run, const services::GovDataResult& result, const QString& source);

/// Cancel tokens of a panel's runs (raised when the pane cancels a run: view left, superseded).
std::shared_ptr<std::atomic<bool>> start_token(QHash<quint64, std::shared_ptr<std::atomic<bool>>>& tokens,
                                               quint64 run);
void cancel_token(QHash<quint64, std::shared_ptr<std::atomic<bool>>>& tokens, quint64 run);

/// Fills a table in one pass: updates off, rows reset to `rows`; restored when it goes out of scope.
class TableFill {
  public:
    TableFill(QTableWidget* table, int rows);
    ~TableFill();
    TableFill(const TableFill&) = delete;
    TableFill& operator=(const TableFill&) = delete;

  private:
    QTableWidget* table_;
};

/// The no-data page of a panel's content stack.
ui::EmptyState* make_no_data_page(QWidget* parent);
void show_no_data(QStackedWidget* stack, ui::EmptyState* page, const ui::EmptyState::Spec& spec);

// ── Tables as models (no item per cell, cells formatted when shown, ink from the live theme) ──

/// A table model whose Accent ink is `provider_color` made legible on the active theme and whose
/// Secondary ink is the theme's secondary text; repaints its visible cells once a new appearance
/// has settled.
GovTableModel* make_model(const QString& provider_color, QObject* parent);
/// A configured table view (configure_table) on `model`, built in its final parent. Set the model's
/// columns first: the view's column sizes are set on the model's columns.
QTableView* make_table_view(QWidget* parent, GovTableModel* model);
/// Shows `rows` in `model` and scrolls `view` to the top.
void show_rows(QTableView* view, GovTableModel* model, const QJsonArray& rows);

/// "a, b, c +2" — the first three tags of a compact dataset row (its "tags" array).
QString tags_text(const QJsonObject& row);
/// Tooltip of a dataset title: the English title ("title_en") when the source published another
/// language, then the description.
QString dataset_tooltip(const QJsonObject& row);

/// Runs `shape` (pure: values in, JSON out — never a widget, `this`, the theme or tr()) on the panels'
/// shaping thread and hands its result to `done` on the GUI thread, unless `context` is gone by then.
/// One thread: results come back in the order they were asked for. `done` checks the run is still
/// current (AutoLoadPane::is_current) before delivering.
void shape_async(QObject* context, std::function<QJsonObject()> shape, std::function<void(const QJsonObject&)> done);

} // namespace gov

} // namespace fincept::screens
