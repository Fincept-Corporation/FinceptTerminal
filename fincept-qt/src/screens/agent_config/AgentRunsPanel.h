#pragma once
// AgentRunsPanel.h — RUNS section of the agents screen: the ONE run list of every
// engine (model/view — no widget per row):
//   HARNESS  coding-agent runs (CodingAgentService: Claude Code, Codex, BETA ones)
//   AGENTIC  local agentic tasks (AgentService; only while Agentic Mode is on)
//   CHAT     Chat Mode runs, cloud or local (TaskRunService; read-only here)
// with STATUS / ENGINE / TITLE / MODEL / STARTED / DURATION / TOKENS / COST /
// ORIGIN columns, a status, an engine and a text filter, and engine chips. Rows
// are keyed "<engine>:<id>" (AgentRunListLogic.h). Selecting a row shows it in
// the screen's inspector; double-click / Enter / OPEN does the same and moves the
// focus there. Never blank: no runs → what to do and NEW RUN; nothing matching →
// CLEAR FILTERS. Moved from the Agent Workbench's RUNS tab (harness only) and
// widened to all engines; the AGENTIC tab's own task list is gone.

#include "screens/agent_config/AgentRunListLogic.h"

#include <QAbstractTableModel>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMetaObject>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QStringList>
#include <QStyledItemDelegate>
#include <QVector>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTableView;
class QTimer;

namespace fincept::screens {

class AgentRunsTableModel : public QAbstractTableModel {
    Q_OBJECT
  public:
    enum Column {
        ColStatus = 0,
        ColEngine,
        ColTitle,
        ColModel,
        ColStarted,
        ColDuration,
        ColTokens,
        ColCost,
        ColSchedule,
        ColCount
    };
    static constexpr int kSortRole = Qt::UserRole + 1;
    static constexpr int kRunKeyRole = Qt::UserRole + 2;
    static constexpr int kRowIndexRole = Qt::UserRole + 3;

    explicit AgentRunsTableModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    /// Rebuild every row from the services (harness + chat) and the agentic cache.
    void reload();
    /// Re-read one harness / chat run (insert, update, or drop when it is gone).
    void upsert_harness(const QString& id);
    void upsert_chat(const QString& id);
    /// An AgentService list_tasks() answer. `replace`: it was the unfiltered list this
    /// model asked for (tasks missing from it are dropped); otherwise rows are merged.
    void set_agentic(const QJsonArray& tasks, bool replace);
    /// Agentic rows shown at all (Agentic Mode).
    void set_agentic_enabled(bool enabled);
    /// The stored agent_tasks row of an agentic run (empty when unknown).
    QJsonObject agentic_json(const QString& task_id) const { return agentic_.value(task_id); }
    const agent_runs::RunRow* row_by_key(const QString& key) const;
    QStringList harness_ids_in_rows() const;
    int count_with_status(const QString& status) const;
    int count_active() const;
    /// Repaint the DURATION column of running rows (1 Hz while visible).
    bool refresh_active_durations();
    void retranslate_headers();

  private:
    agent_runs::RunRow harness_row(const QString& id, bool* ok) const;
    agent_runs::RunRow chat_row(const QString& id, bool* ok) const;
    agent_runs::RunRow agentic_row(const QJsonObject& task) const;
    void upsert(const agent_runs::RunRow& row);
    void remove_key(const QString& key);
    void reindex();

    QVector<agent_runs::RunRow> rows_;
    QHash<QString, int> index_; // key -> row
    QHash<QString, QJsonObject> agentic_; // task id -> agent_tasks row
    bool agentic_enabled_ = true;
};

class AgentRunsFilterProxy : public QSortFilterProxyModel {
    Q_OBJECT
  public:
    explicit AgentRunsFilterProxy(QObject* parent = nullptr);
    void set_filter(const agent_runs::RunFilter& filter);
    bool filtering() const { return filter_.active(); }

  protected:
    bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override;

  private:
    agent_runs::RunFilter filter_;
};

/// Paints the STATUS and ENGINE columns as bordered chips coloured by state / engine.
class AgentRunStatusDelegate : public QStyledItemDelegate {
    Q_OBJECT
  public:
    explicit AgentRunStatusDelegate(QObject* parent = nullptr);
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

class AgentRunsPanel : public QWidget {
    Q_OBJECT
  public:
    explicit AgentRunsPanel(QWidget* parent = nullptr);

    /// Select (and scroll to) a run by key; false when it is not listed (yet).
    bool select_run(const QString& key);
    QString selected_key() const;
    /// Show only one engine ("harness", "agentic", "chat", "harness:<id>"); "" = all.
    void set_engine_filter(const QString& engine);
    void set_text_filter(const QString& text);
    /// Agentic Mode switched (rows and the engine filter entry follow it).
    void set_agentic_enabled(bool enabled);
    /// Ask AgentService for the unfiltered agentic task list (a local Python call,
    /// no model quota); rate-limited.
    void request_agentic_refresh();
    AgentRunsTableModel* runs_model() const { return model_; }

  signals:
    /// The current row changed (the inspector follows it).
    void run_selected(const QString& key);
    /// Double-click / Enter / OPEN: show it and move the focus to the inspector.
    void open_run_requested(const QString& key);
    /// The empty state's NEW RUN.
    void new_run_requested();
    /// The agentic toolbar: "schedule" (SCHEDULE A TASK…), "schedules", "libraries".
    void agentic_action_requested(const QString& action);
    /// Counts changed (the screen's status bar).
    void counts_changed();

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    void retranslate();
    void subscribe();
    void unsubscribe();
    void apply_filters();
    void update_count();
    void rebuild_engine_filter();
    void fit_rows();
    QStringList selected_keys() const;
    void on_open();
    void on_delete();
    void on_clear_finished();
    void flush_dirty();
    void on_tick();

    QComboBox* status_combo_ = nullptr;
    QComboBox* engine_combo_ = nullptr;
    QLineEdit* search_edit_ = nullptr;
    QLabel* count_lbl_ = nullptr;
    QPushButton* open_btn_ = nullptr;
    QPushButton* delete_btn_ = nullptr;
    QPushButton* clear_btn_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QPushButton* schedule_task_btn_ = nullptr;
    QPushButton* task_schedules_btn_ = nullptr;
    QPushButton* libraries_btn_ = nullptr;
    QLabel* hint_lbl_ = nullptr;
    QStackedWidget* stack_ = nullptr; // table | empty state
    QLabel* empty_title_ = nullptr;
    QLabel* empty_text_ = nullptr;
    QPushButton* empty_btn_ = nullptr;
    QTableView* table_ = nullptr;
    AgentRunsTableModel* model_ = nullptr;
    AgentRunsFilterProxy* proxy_ = nullptr;
    QTimer* dirty_timer_ = nullptr;
    QTimer* tick_timer_ = nullptr;
    QTimer* agentic_timer_ = nullptr; // rate limit of list_tasks()
    QSet<QString> dirty_harness_;
    QSet<QString> dirty_chat_;
    QList<QMetaObject::Connection> conns_;
    QString pending_select_;
    bool agentic_enabled_ = true;
    bool agentic_requested_ = false; // the next tasks_listed answers our unfiltered request
    bool agentic_stale_ = true;
};

} // namespace fincept::screens
