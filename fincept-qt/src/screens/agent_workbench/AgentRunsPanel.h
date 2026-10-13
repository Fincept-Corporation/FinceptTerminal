#pragma once
// AgentRunsPanel.h — RUNS tab of the Agent Workbench: sortable, filterable run
// history (model/view — no widget per row). Double-click opens a run's
// transcript in the RUN tab.

#include <QAbstractTableModel>
#include <QList>
#include <QMetaObject>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QStringList>
#include <QStyledItemDelegate>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableView;
class QTimer;

namespace fincept::screens {

class AgentRunsTableModel : public QAbstractTableModel {
    Q_OBJECT
  public:
    enum Column { ColStatus = 0, ColTitle, ColAgent, ColStarted, ColDuration, ColCost, ColSchedule, ColCount };
    static constexpr int kSortRole = Qt::UserRole + 1;
    static constexpr int kRunIdRole = Qt::UserRole + 2;
    static constexpr int kStatusIdRole = Qt::UserRole + 3;
    static constexpr int kAgentIdRole = Qt::UserRole + 4;
    static constexpr int kSearchRole = Qt::UserRole + 5;

    explicit AgentRunsTableModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void reload();
    void add_run(const QString& run_id);
    void refresh_runs(const QSet<QString>& ids);
    /// Repaint the DURATION column of active runs (1 Hz while visible).
    bool refresh_active_durations();
    void retranslate_headers();

  private:
    QStringList ids_;
};

class AgentRunsFilterProxy : public QSortFilterProxyModel {
    Q_OBJECT
  public:
    explicit AgentRunsFilterProxy(QObject* parent = nullptr);
    /// status: "" = all, "active" = queued/running, or a status id; agent: "" | claude | codex.
    void set_filters(const QString& status, const QString& agent, const QString& text);

  protected:
    bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override;

  private:
    QString status_;
    QString agent_;
    QString text_;
};

/// Paints the STATUS column as a bordered pill coloured by state.
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

  signals:
    void open_run_requested(const QString& run_id);

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;

  private:
    void retranslate();
    void subscribe();
    void unsubscribe();
    void apply_filters();
    void update_count();
    QStringList selected_ids() const;
    void on_open();
    void on_delete();
    void on_clear_finished();
    void flush_dirty();
    void on_tick();

    QComboBox* status_combo_ = nullptr;
    QComboBox* agent_combo_ = nullptr;
    QLineEdit* search_edit_ = nullptr;
    QLabel* count_lbl_ = nullptr;
    QPushButton* open_btn_ = nullptr;
    QPushButton* delete_btn_ = nullptr;
    QPushButton* clear_btn_ = nullptr;
    QLabel* hint_lbl_ = nullptr;
    QTableView* table_ = nullptr;
    AgentRunsTableModel* model_ = nullptr;
    AgentRunsFilterProxy* proxy_ = nullptr;
    QTimer* dirty_timer_ = nullptr;
    QTimer* tick_timer_ = nullptr;
    QSet<QString> dirty_;
    QList<QMetaObject::Connection> conns_;
};

} // namespace fincept::screens
