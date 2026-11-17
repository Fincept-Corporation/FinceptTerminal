#pragma once
// RunTemplateLibrary.h — the task templates a user can start from (WP6): the
// built-ins of RunTemplates.h plus the user's own (agent_run_templates, v063),
// and the last step before a run starts from a form or a schedule:
// prepare_request() expands relative periods for today, fills the template's
// placeholders again and appends what the cloud agent cannot read itself — the
// holdings of a terminal portfolio, the symbols of a terminal watchlist.
//
// GUI-thread singleton; database reads / writes on a private worker thread;
// callbacks on the GUI thread, dropped when `ctx` died.

#include "services/agent_tasks/RunTemplates.h"

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>
#include <memory>
#include <optional>

namespace fincept::agent_tasks {

class RunTemplateLibrary : public QObject {
    Q_OBJECT
  public:
    static RunTemplateLibrary& instance();
    ~RunTemplateLibrary() override;

    /// Read the user's templates (once; later calls do nothing).
    void ensure_loaded();
    bool is_loaded() const;

    /// Built-ins first (display order), then the user's (by title).
    QVector<RunTemplate> templates() const;
    QVector<RunTemplate> user_templates() const;
    std::optional<RunTemplate> find(const QString& id) const;

    using SaveCallback = std::function<void(bool ok, const QString& template_id, const QString& error)>;
    using ResultCallback = std::function<void(bool ok, const QString& error)>;
    /// Create (empty id, or a built-in's id: saved as a new "user:" template) or update a user template.
    void save_user_template(RunTemplate t, QObject* ctx, SaveCallback cb);
    void delete_user_template(const QString& id, QObject* ctx, ResultCallback cb);

    struct Prepared {
        bool ok = false;
        RunRequest request;
        QString error;      ///< why it cannot start (no holdings, watchlist gone …)
        QStringList notes;  ///< what was added ("12 holdings of Growth were added")
    };
    using PrepareCallback = std::function<void(const Prepared& prepared)>;
    /// Make `request` ready to start: relative dates of its context expanded for
    /// today; with `refill_template_id` naming a template its prompt and title are
    /// filled from it again, otherwise the prompt's own placeholders are filled; a
    /// portfolio's holdings / a watchlist's symbols appended (read off the GUI thread).
    void prepare_request(const RunRequest& request, const QString& refill_template_id, QObject* ctx,
                         PrepareCallback cb);

  signals:
    void templates_changed();

  private:
    RunTemplateLibrary();
    Q_DISABLE_COPY(RunTemplateLibrary)

    struct Impl;
    std::unique_ptr<Impl> d_;
};

} // namespace fincept::agent_tasks
