#pragma once
// AgentRunFilesView.h — the FILES tab of a run: every file the agent created,
// changed or removed, one row per path, from the run's FileChange events
// (Codex file_change items; Claude Edit / Write calls; ACP edit tool calls —
// each adapter's parser maps its harness's own form). Double-click opens the
// file. Model/view; no file-system access on the GUI thread (paths are shown
// as reported, relative ones resolved against the run's working folder).

#include "services/coding_agents/CodingAgentTypes.h"

#include <QAbstractTableModel>
#include <QHash>
#include <QVector>
#include <QWidget>

class QLabel;
class QStackedWidget;
class QTableView;

namespace fincept::screens {

class AgentRunFilesModel : public QAbstractTableModel {
    Q_OBJECT
  public:
    enum Column { ColChange = 0, ColPath, ColTimes, ColWhen, ColCount };
    static constexpr int kPathRole = Qt::UserRole + 1;

    explicit AgentRunFilesModel(QObject* parent = nullptr);
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void reset_events(const QVector<services::coding_agents::AgentEvent>& events);
    void add_events(const QVector<services::coding_agents::AgentEvent>& events);
    void clear_rows();
    void retranslate_headers();

  private:
    struct Row {
        QString path;
        QString kind;
        int times = 0;
        qint64 last_ms = 0;
    };
    void take(const services::coding_agents::AgentEvent& ev, bool notify);

    QVector<Row> rows_;
    QHash<QString, int> by_path_;
};

class AgentRunFilesView : public QWidget {
    Q_OBJECT
  public:
    explicit AgentRunFilesView(QWidget* parent = nullptr);

    /// The run whose files are shown (its working folder resolves relative paths).
    void set_run(const QString& run_id, const QString& workdir, bool read_only_run);
    void reset_events(const QVector<services::coding_agents::AgentEvent>& events);
    void add_events(const QVector<services::coding_agents::AgentEvent>& events);
    int file_count() const;

  signals:
    void file_count_changed(int count);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void retranslate();
    void refresh_state();
    void open_row(const QModelIndex& index);

    QStackedWidget* stack_ = nullptr;
    QLabel* empty_lbl_ = nullptr;
    QTableView* table_ = nullptr;
    AgentRunFilesModel* model_ = nullptr;
    QString workdir_;
    bool read_only_run_ = true;
    int last_count_ = -1;
};

} // namespace fincept::screens
