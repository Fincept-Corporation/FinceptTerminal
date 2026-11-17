// ReportBuilderService.cpp — see header for design rationale.

#include "services/report_builder/ReportBuilderService.h"

#include "core/config/AppPaths.h"
#include "core/config/AppSettings.h"
#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "core/report/ReportVariables.h"
#include "services/report_builder/ReportBuilderTemplates.h"
#include "services/report_builder/ReportLibrary.h"
#include "services/report_builder/ReportScheduler.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QUndoCommand>

#include <utility>

namespace fincept::services {

static constexpr const char* RB_SVC_TAG = "ReportBuilderService";
static constexpr int kAutosaveIntervalMs = 60'000;
static constexpr int kLibraryDelayMs = 4'000; // library copy this long after edits stop
static constexpr int kLlmWindowMs = 400;
static constexpr int kRecentMax = 10;
static constexpr const char* kRecentKey = "report_builder/recent";
static constexpr const char* kCurrentFileKey = "report_builder/current_file";
static constexpr const char* kLibraryIdKey = "report_builder/library_id";
static constexpr const char* kFileLibraryKey = "report_builder/file_library_ids";
static constexpr const char* kDefaultCompanyKey = "report_builder/default_company";
static constexpr const char* kDefaultAuthorKey = "report_builder/default_author";
static constexpr int kSchedulerStartDelayMs = 20'000; // let the terminal settle before schedules run

namespace rep = report;

namespace {

QSettings rbs_settings() {
    return app_settings::open();
}

// The library row a .fincept file was imported as, so opening the same file again
// continues that row instead of adding another one.
qint64 rbs_library_id_for_file(const QString& path) {
    if (path.isEmpty())
        return 0;
    const QVariantMap m = rbs_settings().value(kFileLibraryKey).toMap();
    return m.value(QFileInfo(path).absoluteFilePath()).toLongLong();
}

void rbs_remember_file_library_id(const QString& path, qint64 id) {
    if (path.isEmpty() || id <= 0)
        return;
    QSettings s = rbs_settings();
    QVariantMap m = s.value(kFileLibraryKey).toMap();
    m.insert(QFileInfo(path).absoluteFilePath(), id);
    // Keep the map small: drop entries whose file is gone.
    if (m.size() > 200) {
        for (auto it = m.begin(); it != m.end();) {
            if (!QFileInfo::exists(it.key()))
                it = m.erase(it);
            else
                ++it;
        }
    }
    s.setValue(kFileLibraryKey, m);
}

} // namespace

// ── Undo commands ────────────────────────────────────────────────────────────
//
// Owned by the service (formerly owned by the screen). They mutate the doc
// directly without re-entering the public mutators (which would push another
// command onto the stack — infinite loop). The service exposes private direct-
// mutators via friend access.

namespace {

class AddCmd : public QUndoCommand {
  public:
    AddCmd(ReportBuilderService* svc, rep::ReportComponent comp, int at, QUndoCommand* parent = nullptr)
        : QUndoCommand("Add " + comp.type, parent), svc_(svc), comp_(std::move(comp)), at_(at) {}
    void redo() override;
    void undo() override;

  private:
    ReportBuilderService* svc_;
    rep::ReportComponent comp_;
    int at_;
};

class RemoveCmd : public QUndoCommand {
  public:
    RemoveCmd(ReportBuilderService* svc, int index) : QUndoCommand("Remove component"), svc_(svc), index_(index) {}
    void redo() override;
    void undo() override;
    void set_saved(rep::ReportComponent c) { saved_ = std::move(c); }

  private:
    ReportBuilderService* svc_;
    rep::ReportComponent saved_;
    int index_;
};

class UpdateCmd : public QUndoCommand {
  public:
    UpdateCmd(ReportBuilderService* svc, int id, QString old_content, QMap<QString, QString> old_config,
              QString new_content, QMap<QString, QString> new_config, const QString& text = {},
              bool mergeable = true)
        : QUndoCommand(text.isEmpty() ? QStringLiteral("Edit component") : text),
          svc_(svc),
          id_(id),
          mergeable_(mergeable),
          old_content_(std::move(old_content)),
          new_content_(std::move(new_content)),
          old_config_(std::move(old_config)),
          new_config_(std::move(new_config)) {}
    void redo() override;
    void undo() override;
    // -1 = never merged (a data refresh stays its own undo step).
    int id() const override { return mergeable_ ? 1001 : -1; }
    bool mergeWith(const QUndoCommand* other) override;

  private:
    ReportBuilderService* svc_;
    int id_;
    bool mergeable_;
    QString old_content_, new_content_;
    QMap<QString, QString> old_config_, new_config_;
};

class MoveCmd : public QUndoCommand {
  public:
    MoveCmd(ReportBuilderService* svc, int id, int from, int to)
        : QUndoCommand("Move component"), svc_(svc), id_(id), from_(from), to_(to) {}
    void redo() override;
    void undo() override;

  private:
    ReportBuilderService* svc_;
    int id_;
    int from_, to_;
};

class MetadataCmd : public QUndoCommand {
  public:
    MetadataCmd(ReportBuilderService* svc, rep::ReportMetadata old_m, rep::ReportMetadata new_m)
        : QUndoCommand("Edit metadata"), svc_(svc), old_(std::move(old_m)), new_(std::move(new_m)) {}
    void redo() override;
    void undo() override;

  private:
    ReportBuilderService* svc_;
    rep::ReportMetadata old_, new_;
};

class ThemeCmd : public QUndoCommand {
  public:
    ThemeCmd(ReportBuilderService* svc, rep::ReportTheme old_t, rep::ReportTheme new_t)
        : QUndoCommand("Change theme"), svc_(svc), old_(std::move(old_t)), new_(std::move(new_t)) {}
    void redo() override;
    void undo() override;

  private:
    ReportBuilderService* svc_;
    rep::ReportTheme old_, new_;
};

} // namespace

// Direct (no-undo) mutators — used by the undo commands. They take the mutex,
// modify doc_, drop the mutex, then emit signals. Signals fire on the owning
// thread of the service (main thread).

namespace detail {

class ServiceFriend {
  public:
    static void direct_insert(ReportBuilderService* svc, rep::ReportComponent c, int at);
    static rep::ReportComponent direct_remove(ReportBuilderService* svc, int index);
    static void direct_update_by_id(ReportBuilderService* svc, int id, const QString& content,
                                    const QMap<QString, QString>& config);
    static void direct_move_by_id(ReportBuilderService* svc, int id, int new_index);
    static void direct_set_metadata(ReportBuilderService* svc, rep::ReportMetadata m);
    static void direct_set_theme(ReportBuilderService* svc, rep::ReportTheme t);

    // Read helpers used by the undo commands. They hold the service mutex
    // briefly and return a snapshot — undo command bodies must not access
    // svc_->mutex_ / svc_->doc_ directly (those members are private).
    static int locked_index_of(ReportBuilderService* svc, int id);
    static rep::ReportComponent locked_get_at(ReportBuilderService* svc, int index);
};

void ServiceFriend::direct_insert(ReportBuilderService* svc, rep::ReportComponent c, int at) {
    int index;
    {
        QMutexLocker lock(&svc->mutex_);
        if (at < 0 || at > svc->doc_.components.size())
            at = svc->doc_.components.size();
        if (c.id <= 0)
            c.id = svc->doc_.allocate_id();
        else if (c.id >= svc->doc_.next_id)
            svc->doc_.next_id = c.id + 1;
        svc->doc_.components.insert(at, c);
        index = at;
    }
    emit svc->component_added(c.id, index);
    emit svc->document_changed();
}

rep::ReportComponent ServiceFriend::direct_remove(ReportBuilderService* svc, int index) {
    rep::ReportComponent removed;
    int id = -1;
    {
        QMutexLocker lock(&svc->mutex_);
        if (index < 0 || index >= svc->doc_.components.size())
            return removed;
        removed = svc->doc_.components.takeAt(index);
        id = removed.id;
    }
    if (id != -1) {
        emit svc->component_removed(id, index);
        emit svc->document_changed();
    }
    return removed;
}

void ServiceFriend::direct_update_by_id(ReportBuilderService* svc, int id, const QString& content,
                                        const QMap<QString, QString>& config) {
    bool found = false;
    {
        QMutexLocker lock(&svc->mutex_);
        int idx = svc->doc_.index_of(id);
        if (idx >= 0) {
            svc->doc_.components[idx].content = content;
            svc->doc_.components[idx].config = config;
            found = true;
        }
    }
    if (found) {
        emit svc->component_updated(id);
        emit svc->document_changed();
    }
}

void ServiceFriend::direct_move_by_id(ReportBuilderService* svc, int id, int new_index) {
    int from = -1, to = -1;
    {
        QMutexLocker lock(&svc->mutex_);
        from = svc->doc_.index_of(id);
        if (from < 0)
            return;
        to = new_index;
        if (to < 0)
            to = 0;
        if (to >= svc->doc_.components.size())
            to = svc->doc_.components.size() - 1;
        if (from == to)
            return;
        rep::ReportComponent c = svc->doc_.components.takeAt(from);
        svc->doc_.components.insert(to, std::move(c));
    }
    emit svc->component_moved(id, from, to);
    emit svc->document_changed();
}

void ServiceFriend::direct_set_metadata(ReportBuilderService* svc, rep::ReportMetadata m) {
    {
        QMutexLocker lock(&svc->mutex_);
        svc->doc_.metadata = std::move(m);
    }
    emit svc->metadata_changed();
    emit svc->document_changed();
}

void ServiceFriend::direct_set_theme(ReportBuilderService* svc, rep::ReportTheme t) {
    {
        QMutexLocker lock(&svc->mutex_);
        svc->doc_.theme = std::move(t);
    }
    emit svc->theme_changed();
    emit svc->document_changed();
}

int ServiceFriend::locked_index_of(ReportBuilderService* svc, int id) {
    QMutexLocker lock(&svc->mutex_);
    return svc->doc_.index_of(id);
}

rep::ReportComponent ServiceFriend::locked_get_at(ReportBuilderService* svc, int index) {
    QMutexLocker lock(&svc->mutex_);
    if (index < 0 || index >= svc->doc_.components.size())
        return rep::ReportComponent{};
    return svc->doc_.components[index];
}

} // namespace detail

// ── Undo command bodies ──────────────────────────────────────────────────────

void AddCmd::redo() {
    detail::ServiceFriend::direct_insert(svc_, comp_, at_);
}
void AddCmd::undo() {
    // Find by id rather than index — concurrent mutations may have shifted
    // positions since the original add. (Within a macro this is unlikely
    // but the bookkeeping cost is trivial.)
    int idx = detail::ServiceFriend::locked_index_of(svc_, comp_.id);
    if (idx >= 0)
        detail::ServiceFriend::direct_remove(svc_, idx);
}

void RemoveCmd::redo() {
    if (saved_.id == 0) {
        // First execution: capture before removing so undo can restore.
        saved_ = detail::ServiceFriend::locked_get_at(svc_, index_);
    }
    detail::ServiceFriend::direct_remove(svc_, index_);
}
void RemoveCmd::undo() {
    if (saved_.id != 0)
        detail::ServiceFriend::direct_insert(svc_, saved_, index_);
}

void UpdateCmd::redo() {
    detail::ServiceFriend::direct_update_by_id(svc_, id_, new_content_, new_config_);
}
void UpdateCmd::undo() {
    detail::ServiceFriend::direct_update_by_id(svc_, id_, old_content_, old_config_);
}
bool UpdateCmd::mergeWith(const QUndoCommand* other) {
    if (!mergeable_ || other->id() != id())
        return false;
    auto* o = static_cast<const UpdateCmd*>(other);
    if (o->id_ != id_)
        return false;
    new_content_ = o->new_content_;
    new_config_ = o->new_config_;
    return true;
}

void MoveCmd::redo() {
    detail::ServiceFriend::direct_move_by_id(svc_, id_, to_);
}
void MoveCmd::undo() {
    detail::ServiceFriend::direct_move_by_id(svc_, id_, from_);
}

void MetadataCmd::redo() {
    detail::ServiceFriend::direct_set_metadata(svc_, new_);
}
void MetadataCmd::undo() {
    detail::ServiceFriend::direct_set_metadata(svc_, old_);
}

void ThemeCmd::redo() {
    detail::ServiceFriend::direct_set_theme(svc_, new_);
}
void ThemeCmd::undo() {
    detail::ServiceFriend::direct_set_theme(svc_, old_);
}

// ── Service ──────────────────────────────────────────────────────────────────

ReportBuilderService& ReportBuilderService::instance() {
    static ReportBuilderService s;
    return s;
}

ReportBuilderService::ReportBuilderService() {
    doc_.metadata.date = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    apply_identity_defaults(doc_.metadata);
    undo_stack_ = new QUndoStack(this);

    // Autosave is driven by edits, not by the clock: the first change after a save
    // arms a one-shot timer, and nothing runs while the report is clean. (The service
    // is constructed at startup, and a free-running 60 s timer re-serialised the
    // document every minute even if Report Builder was never opened.)
    autosave_timer_ = new QTimer(this);
    autosave_timer_->setSingleShot(true);
    autosave_timer_->setInterval(kAutosaveIntervalMs);
    connect(autosave_timer_, &QTimer::timeout, this, &ReportBuilderService::trigger_autosave);

    // The library copy follows a few seconds after edits stop (restarted by every edit),
    // so a burst of typing is one write — and one cloud push when sync is on.
    library_timer_ = new QTimer(this);
    library_timer_->setSingleShot(true);
    library_timer_->setInterval(kLibraryDelayMs);
    connect(library_timer_, &QTimer::timeout, this, &ReportBuilderService::save_to_library);

    connect(this, &ReportBuilderService::document_changed, this, [this]() {
        autosave_dirty_ = true;
        if (!autosave_timer_->isActive())
            autosave_timer_->start();
        library_dirty_ = true;
        library_timer_->start();
        if (!current_file_.isEmpty())
            file_dirty_ = true;
    });
    // Don't lose the last < 60 s of edits on a normal quit.
    if (auto* app = QCoreApplication::instance())
        connect(app, &QCoreApplication::aboutToQuit, this, [this]() {
            if (autosave_dirty_)
                trigger_autosave();
            if (library_dirty_ || library_timer_->isActive())
                save_to_library_blocking();
        });

    llm_window_timer_ = new QTimer(this);
    llm_window_timer_->setSingleShot(true);
    llm_window_timer_->setInterval(kLlmWindowMs);
    connect(llm_window_timer_, &QTimer::timeout, this, [this]() {
        // Close the macro we opened in note_llm_mutation().
        if (macro_depth_ > 0) {
            macro_depth_ = 0;
            undo_stack_->endMacro();
            LOG_DEBUG(RB_SVC_TAG, "LLM macro window closed");
        }
    });

    // In the profile's data folder, not %TEMP%: a fixed temp name was shared by
    // every profile and by the private build's uninstaller sweep (%TEMP%\fincept_*).
    autosave_path_ = AppPaths::data() + "/report_autosave.fincept";
    const QString legacy_autosave =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/fincept_report_autosave.fincept";

    // Restore current_file_ / the library row from settings so the title bar and the
    // library show the right report on relaunch. (The doc itself comes from autosave.)
    QSettings s = rbs_settings();
    current_file_ = s.value(kCurrentFileKey).toString();
    library_id_ = s.value(kLibraryIdKey).toLongLong();

    // Try to restore from autosave on construction. Failures are silent —
    // an empty doc is the correct fallback. Until the first autosave in the new
    // place, the one earlier versions left in the temp folder is read.
    QFile f(QFile::exists(autosave_path_) ? autosave_path_ : legacy_autosave);
    if (f.exists() && f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString json = QString::fromUtf8(f.readAll());
        f.close();
        auto r = rep::ReportDocument::from_json(json);
        if (r.is_ok()) {
            doc_ = r.value();
            LOG_INFO(RB_SVC_TAG, "Restored from autosave: " + autosave_path_);
        }
    }
    chains_[library_generation_].id = library_id_;
    chains_[library_generation_].file = current_file_;
    chains_[library_generation_].expected = doc_.revision;

    // The open report's row was deleted (library page, after its undo grace period).
    connect(&ReportLibrary::instance(), &ReportLibrary::report_removed, this,
            [this](qint64 id) { detach_library(id); });

    // Scheduled regeneration runs while the terminal is open (in-process only).
    if (QCoreApplication::instance())
        QTimer::singleShot(kSchedulerStartDelayMs, this, []() { ReportScheduler::instance().start(); });
}

// ── Identity defaults ───────────────────────────────────────────────────────

QString ReportBuilderService::default_company() {
    return rbs_settings().value(kDefaultCompanyKey).toString();
}

QString ReportBuilderService::default_author() {
    const QString a = rbs_settings().value(kDefaultAuthorKey).toString();
    return a.isEmpty() ? QStringLiteral("Analyst") : a;
}

void ReportBuilderService::set_identity_defaults(const QString& author, const QString& company) {
    QSettings s = rbs_settings();
    s.setValue(kDefaultAuthorKey, author.trimmed());
    s.setValue(kDefaultCompanyKey, company.trimmed());
}

void ReportBuilderService::apply_identity_defaults(rep::ReportMetadata& meta) {
    if (meta.company.isEmpty())
        meta.company = default_company();
    if (meta.author.isEmpty() || meta.author == QLatin1String("Analyst"))
        meta.author = default_author();
}

// ── Stale saves ─────────────────────────────────────────────────────────────

void ReportBuilderService::resolve_library_conflict(ConflictChoice choice) {
    if (conflict_.id <= 0)
        return;
    const LibraryConflict c = conflict_;
    conflict_ = LibraryConflict{};
    WriteChain& chain = chains_[library_generation_];
    chain.conflict = false;
    switch (choice) {
        case ConflictChoice::KeepMine:
            LOG_INFO(RB_SVC_TAG, logf("Report conflict resolved", {{"report", c.id}, {"choice", "keep_mine"}}));
            chain.expected = c.theirs.revision;
            chain.force_next = true;
            library_dirty_ = true;
            save_to_library();
            break;
        case ConflictChoice::TakeTheirs:
            LOG_INFO(RB_SVC_TAG, logf("Report conflict resolved", {{"report", c.id}, {"choice", "take_theirs"}}));
            open_document(c.theirs, c.id, current_file());
            break;
        case ConflictChoice::KeepBoth: {
            LOG_INFO(RB_SVC_TAG, logf("Report conflict resolved", {{"report", c.id}, {"choice", "keep_both"}}));
            // The stored version stays in its row; the open one becomes a new report.
            {
                QMutexLocker lock(&mutex_);
                library_id_ = 0;
                doc_.revision = rep::ReportRevision{};
            }
            chain.id = 0;
            chain.expected = rep::ReportRevision{};
            persist_library_id();
            emit library_id_changed(0);
            rep::ReportMetadata meta = metadata();
            meta.title = tr("%1 (my copy)").arg(meta.title);
            set_metadata(meta); // one undo step; also queues the library write (a new row)
            break;
        }
    }
}

// ── Read API ────────────────────────────────────────────────────────────────

QVector<rep::ReportComponent> ReportBuilderService::components() const {
    QMutexLocker lock(&mutex_);
    return doc_.components;
}
rep::ReportMetadata ReportBuilderService::metadata() const {
    QMutexLocker lock(&mutex_);
    return doc_.metadata;
}
rep::ReportTheme ReportBuilderService::theme() const {
    QMutexLocker lock(&mutex_);
    return doc_.theme;
}
int ReportBuilderService::next_id() const {
    QMutexLocker lock(&mutex_);
    return doc_.next_id;
}
int ReportBuilderService::index_of(int id) const {
    QMutexLocker lock(&mutex_);
    return doc_.index_of(id);
}
bool ReportBuilderService::component(int id, rep::ReportComponent* out) const {
    QMutexLocker lock(&mutex_);
    const int idx = doc_.index_of(id);
    if (idx < 0)
        return false;
    if (out)
        *out = doc_.components[idx];
    return true;
}
QString ReportBuilderService::current_file() const {
    QMutexLocker lock(&mutex_);
    return current_file_;
}
QStringList ReportBuilderService::recent_files() const {
    if (!recent_loaded_)
        load_recent();
    return recent_cache_;
}
qint64 ReportBuilderService::library_id() const {
    QMutexLocker lock(&mutex_);
    return library_id_;
}
rep::ReportDocument ReportBuilderService::snapshot() const {
    QMutexLocker lock(&mutex_);
    return doc_;
}
QVector<int> ReportBuilderService::unseen_ids() const {
    QMutexLocker lock(&mutex_);
    QVector<int> out;
    for (int id : doc_.unseen_ids)
        if (doc_.index_of(id) >= 0)
            out.append(id);
    return out;
}
QStringList ReportBuilderService::unseen_sources() const {
    QMutexLocker lock(&mutex_);
    return doc_.unseen_sources;
}

// ── Write API ────────────────────────────────────────────────────────────────

int ReportBuilderService::add_component(const rep::ReportComponent& comp, int insert_at) {
    rep::ReportComponent c = comp;
    if (c.id <= 0) {
        QMutexLocker lock(&mutex_);
        c.id = doc_.allocate_id();
    }
    int at = insert_at;
    {
        QMutexLocker lock(&mutex_);
        if (at < 0 || at > doc_.components.size())
            at = doc_.components.size();
    }
    LOG_DEBUG(RB_SVC_TAG, QString("add_component: type=%1 id=%2 at=%3").arg(c.type).arg(c.id).arg(at));
    push_undo(new AddCmd(this, c, at));
    return c.id;
}

QVector<int> ReportBuilderService::append_components(const QVector<rep::ReportComponent>& blocks,
                                                     const QString& source_label, int insert_at, bool mark_unseen) {
    QVector<int> ids;
    if (blocks.isEmpty())
        return ids;
    int at = insert_at;
    {
        QMutexLocker lock(&mutex_);
        if (at < 0 || at > doc_.components.size())
            at = doc_.components.size();
    }
    // One undo step for the whole send ("Add from Derivatives"), even inside an open macro.
    begin_macro(source_label.isEmpty() ? tr("Add blocks") : tr("Add from %1").arg(source_label));
    for (const rep::ReportComponent& b : blocks) {
        rep::ReportComponent c = b;
        {
            QMutexLocker lock(&mutex_);
            c.id = doc_.allocate_id();
        }
        push_undo(new AddCmd(this, c, at++));
        ids.append(c.id);
    }
    end_macro();
    if (mark_unseen) {
        {
            QMutexLocker lock(&mutex_);
            doc_.unseen_ids += ids;
            if (!source_label.isEmpty() && !doc_.unseen_sources.contains(source_label))
                doc_.unseen_sources.append(source_label);
        }
        emit inbox_changed();
    }
    return ids;
}

bool ReportBuilderService::update_component(int id, const QString& content, const QMap<QString, QString>& config) {
    QString old_content;
    QMap<QString, QString> old_config;
    {
        QMutexLocker lock(&mutex_);
        int idx = doc_.index_of(id);
        if (idx < 0)
            return false;
        old_content = doc_.components[idx].content;
        old_config = doc_.components[idx].config;
    }
    if (old_content == content && old_config == config)
        return true; // nothing changed: no undo step, no re-render
    push_undo(new UpdateCmd(this, id, old_content, old_config, content, config));
    return true;
}

bool ReportBuilderService::patch_component(int id, const QString* content_or_null,
                                           const QMap<QString, QString>& partial_config, const QString& undo_text,
                                           bool mergeable, bool empty_removes) {
    QString old_content, new_content;
    QMap<QString, QString> old_config, new_config;
    {
        QMutexLocker lock(&mutex_);
        int idx = doc_.index_of(id);
        if (idx < 0)
            return false;
        old_content = doc_.components[idx].content;
        old_config = doc_.components[idx].config;
        new_content = content_or_null ? *content_or_null : old_content;
        new_config = old_config;
        for (auto it = partial_config.cbegin(); it != partial_config.cend(); ++it) {
            if (empty_removes && it.value().isEmpty())
                new_config.remove(it.key());
            else
                new_config[it.key()] = it.value();
        }
    }
    if (new_content == old_content && new_config == old_config)
        return true;
    push_undo(new UpdateCmd(this, id, old_content, old_config, new_content, new_config, undo_text, mergeable));
    return true;
}

bool ReportBuilderService::remove_component(int id) {
    int idx;
    {
        QMutexLocker lock(&mutex_);
        idx = doc_.index_of(id);
    }
    if (idx < 0)
        return false;
    push_undo(new RemoveCmd(this, idx));
    return true;
}

bool ReportBuilderService::move_component(int id, int new_index) {
    int from;
    {
        QMutexLocker lock(&mutex_);
        from = doc_.index_of(id);
        if (from < 0)
            return false;
        if (new_index < 0)
            new_index = 0;
        if (new_index >= doc_.components.size())
            new_index = doc_.components.size() - 1;
        if (from == new_index)
            return true;
    }
    push_undo(new MoveCmd(this, id, from, new_index));
    return true;
}

void ReportBuilderService::set_metadata(const rep::ReportMetadata& meta) {
    rep::ReportMetadata old;
    {
        QMutexLocker lock(&mutex_);
        old = doc_.metadata;
    }
    push_undo(new MetadataCmd(this, old, meta));
}

void ReportBuilderService::set_theme(const rep::ReportTheme& th) {
    rep::ReportTheme old;
    {
        QMutexLocker lock(&mutex_);
        old = doc_.theme;
    }
    push_undo(new ThemeCmd(this, old, th));
}

void ReportBuilderService::clear_document() {
    // "New report": the report that was open stays in the library; a blank one opens.
    rep::ReportDocument fresh;
    fresh.metadata.date = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    apply_identity_defaults(fresh.metadata);
    fresh.theme = theme(); // the look the user picked carries over
    open_document(fresh, 0, QString());
    // trigger_autosave() deliberately never writes an EMPTY document (so a stray timer tick
    // can't clobber a good recovery copy) — which meant "New Report" left the previous
    // report's autosave behind, and the next launch brought it back from the dead.
    QFile::remove(autosave_path_);
    autosave_hash_ = 0;
    autosave_dirty_ = false;
    autosave_timer_->stop();
    emit document_cleared();
}

void ReportBuilderService::replace_document(const rep::ReportDocument& doc) {
    {
        QMutexLocker lock(&mutex_);
        doc_ = doc;
    }
    undo_stack_->clear();
    emit document_changed();
    // document_changed() only tells a view to refresh its structure list — the canvas is
    // redrawn by the per-component signals, which a wholesale replacement never emits.
    // Views already know how to rebuild everything on "document_loaded"; an empty path =
    // not from a file.
    emit document_loaded(QString());
}

void ReportBuilderService::open_document(const rep::ReportDocument& doc, qint64 library_id, const QString& file_path) {
    // The outgoing report's last state goes to its own library row first (its chain).
    if (library_dirty_ || library_timer_->isActive())
        save_to_library();
    library_timer_->stop();
    // Any open LLM / append macro belongs to the outgoing document.
    if (macro_depth_ > 0) {
        macro_depth_ = 0;
        undo_stack_->endMacro();
        llm_window_timer_->stop();
    }

    const QString json = doc.to_json();
    {
        QMutexLocker lock(&mutex_);
        doc_ = doc;
        current_file_ = file_path;
        library_id_ = library_id;
    }
    ++library_generation_;
    WriteChain& chain = chains_[library_generation_];
    chain.id = library_id;
    chain.file = file_path;
    chain.expected = doc.revision;
    conflict_ = LibraryConflict{};
    detached_from_ = 0;
    // The content hash leaves the revision out (the library stamps it on every write).
    library_hash_ = library_id > 0 ? qHash(doc.to_json(false)) : 0;
    library_dirty_ = false;
    file_dirty_ = false;
    undo_stack_->clear();
    persist_current_file();
    persist_library_id();
    if (!file_path.isEmpty()) {
        if (!recent_loaded_)
            load_recent();
        recent_cache_.removeAll(file_path);
        recent_cache_.prepend(file_path);
        if (recent_cache_.size() > kRecentMax)
            recent_cache_ = recent_cache_.mid(0, kRecentMax);
        save_recent();
    }
    if (library_id > 0)
        ReportLibrary::instance().note_opened(library_id);
    write_autosave(json);
    autosave_dirty_ = false;
    autosave_timer_->stop();

    emit current_file_changed(file_path);
    emit library_id_changed(library_id);
    emit document_loaded(file_path);
    emit inbox_changed();
    // A report that is not in the library yet (template, file, blank with content) is
    // added at once so it is listed and synced.
    if (library_id <= 0) {
        QMutexLocker lock(&mutex_);
        if (worth_saving_locked()) {
            lock.unlock();
            library_dirty_ = true;
            library_timer_->start(0);
        }
    }
}

void ReportBuilderService::open_file_document(const rep::ReportDocument& doc, const QString& path) {
    open_document(doc, rbs_library_id_for_file(path), path);
}

Result<void> ReportBuilderService::fork_to_file(const QString& path) {
    const rep::ReportDocument doc = snapshot();
    open_document(doc, 0, path);
    return save_to(path);
}

void ReportBuilderService::detach_library(qint64 deleted_id) {
    if (deleted_id <= 0 || library_id() != deleted_id)
        return;
    {
        QMutexLocker lock(&mutex_);
        library_id_ = 0;
    }
    chains_[library_generation_].id = 0;
    detached_from_ = deleted_id;
    library_hash_ = 0;
    library_timer_->stop();
    library_dirty_ = false;
    persist_library_id();
    emit library_id_changed(0);
}

void ReportBuilderService::reattach_library(qint64 id) {
    if (id <= 0 || detached_from_ != id || library_id() != 0)
        return;
    set_library_id(id);
    detached_from_ = 0;
}

void ReportBuilderService::mark_inbox_seen() {
    {
        QMutexLocker lock(&mutex_);
        if (doc_.unseen_ids.isEmpty() && doc_.unseen_sources.isEmpty())
            return;
        doc_.unseen_ids.clear();
        doc_.unseen_sources.clear();
    }
    emit inbox_changed();
    // Not an undoable edit, but the stored copies must forget the notice too.
    library_dirty_ = true;
    library_timer_->start();
    autosave_dirty_ = true;
    if (!autosave_timer_->isActive())
        autosave_timer_->start();
}

void ReportBuilderService::apply_template(const QString& name, const QVariantMap& params) {
    LOG_DEBUG(RB_SVC_TAG, "apply_template: " + name);
    // A template starts a NEW report; the one that was open stays in the library.
    rep::ReportDocument doc = report_templates::build(name, theme(), params);
    apply_identity_defaults(doc.metadata);
    open_document(doc, 0, QString());
}

// ── Undo helpers ────────────────────────────────────────────────────────────

QUndoStack* ReportBuilderService::undo_stack() {
    return undo_stack_;
}

void ReportBuilderService::begin_macro(const QString& description) {
    if (macro_depth_++ == 0)
        undo_stack_->beginMacro(description);
}

void ReportBuilderService::end_macro() {
    if (macro_depth_ == 0)
        return;
    if (--macro_depth_ == 0)
        undo_stack_->endMacro();
}

void ReportBuilderService::note_llm_mutation() {
    if (macro_depth_ == 0) {
        macro_depth_ = 1;
        undo_stack_->beginMacro("AI: report changes");
        LOG_DEBUG(RB_SVC_TAG, "LLM macro window opened (400ms)");
    }
    // (Re)start the window timer — extend as long as mutations keep arriving.
    llm_window_timer_->start();
}

void ReportBuilderService::push_undo(QUndoCommand* cmd) {
    undo_stack_->push(cmd);
}

// ── File ops ────────────────────────────────────────────────────────────────

Result<void> ReportBuilderService::save_to(const QString& path) {
    QString json;
    {
        QMutexLocker lock(&mutex_);
        json = doc_.to_json();
    }
    // QSaveFile writes to a sibling temp file and renames it over the target on commit(), so a
    // full disk, a crash or a permissions error mid-write leaves the user's existing report
    // untouched.
    QSaveFile f(path);
    f.setDirectWriteFallback(true); // read-only directory but writable file: still allow the save
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return Result<void>::err("Could not open file for writing: " + f.errorString().toStdString());
    const QByteArray bytes = json.toUtf8();
    if (f.write(bytes) != bytes.size()) {
        const std::string why = f.errorString().toStdString();
        f.cancelWriting();
        return Result<void>::err("Could not write the report: " + why);
    }
    if (!f.commit())
        return Result<void>::err("Could not finish writing the file: " + f.errorString().toStdString());
    set_current_file(path);
    chains_[library_generation_].file = path;
    rbs_remember_file_library_id(path, library_id());
    file_dirty_ = false;
    // The recovery copy must not lag behind what the user just saved.
    write_autosave(json);
    // ...and neither must the library.
    save_to_library();
    return Result<void>::ok();
}

Result<void> ReportBuilderService::load_from(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return Result<void>::err("Could not open file: " + f.errorString().toStdString());
    QString json = QString::fromUtf8(f.readAll());
    f.close();
    auto r = rep::ReportDocument::from_json(json);
    if (r.is_err())
        return Result<void>::err("Parse error: " + r.error());
    // A file opened again continues the library row it was imported as.
    open_document(r.value(), rbs_library_id_for_file(path), path);
    return Result<void>::ok();
}

void ReportBuilderService::set_current_file(const QString& path) {
    {
        QMutexLocker lock(&mutex_);
        current_file_ = path;
    }
    persist_current_file();
    if (!path.isEmpty()) {
        if (!recent_loaded_)
            load_recent();
        recent_cache_.removeAll(path);
        recent_cache_.prepend(path);
        if (recent_cache_.size() > kRecentMax)
            recent_cache_ = recent_cache_.mid(0, kRecentMax);
        save_recent();
    }
    emit current_file_changed(path);
}

QStringList ReportBuilderService::prune_recent_files() {
    if (!recent_loaded_)
        load_recent();
    QStringList kept;
    for (const QString& p : recent_cache_)
        if (QFileInfo::exists(p))
            kept << p;
    if (kept != recent_cache_) {
        recent_cache_ = kept;
        save_recent();
    }
    return recent_cache_;
}

void ReportBuilderService::persist_current_file() {
    QSettings s = rbs_settings();
    s.setValue(kCurrentFileKey, current_file_);
}

void ReportBuilderService::persist_library_id() {
    QSettings s = rbs_settings();
    s.setValue(kLibraryIdKey, library_id_);
}

QString ReportBuilderService::autosave_path() const {
    return autosave_path_;
}

void ReportBuilderService::trigger_autosave() {
    autosave_timer_->stop();
    autosave_dirty_ = false;
    QString json;
    {
        QMutexLocker lock(&mutex_);
        // Skip writing an empty doc — avoids clobbering a useful autosave
        // with nothing if the user just bumped the timer with no content.
        if (doc_.components.isEmpty() && doc_.metadata.title == "Untitled Report")
            return;
        json = doc_.to_json();
    }
    write_autosave(json);
}

void ReportBuilderService::write_autosave(const QString& json) {
    // Skip when byte-identical to what is already on disk. Written via QSaveFile so a
    // crash mid-write can't leave a truncated autosave that fails to parse and silently
    // loses the whole report on next launch.
    const std::size_t digest = qHash(json);
    if (digest == autosave_hash_)
        return;
    QSaveFile f(autosave_path_);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(json.toUtf8());
        if (f.commit())
            autosave_hash_ = digest;
        else
            LOG_WARN(RB_SVC_TAG, "Autosave could not be written: " + f.errorString());
    }
}

void ReportBuilderService::load_recent() const {
    recent_cache_ = rbs_settings().value(kRecentKey).toStringList();
    recent_loaded_ = true;
}

void ReportBuilderService::save_recent() const {
    QSettings s = rbs_settings();
    s.setValue(kRecentKey, recent_cache_);
}

// ── Library ─────────────────────────────────────────────────────────────────

bool ReportBuilderService::worth_saving_locked() const {
    // An existing row is always kept current; a new one is only created for a report
    // with something in it (not for every blank "New Report").
    return library_id_ > 0 || !doc_.components.isEmpty() || doc_.metadata.title != QLatin1String("Untitled Report");
}

void ReportBuilderService::set_library_id(qint64 id) {
    {
        QMutexLocker lock(&mutex_);
        if (library_id_ == id)
            return;
        library_id_ = id;
    }
    chains_[library_generation_].id = id;
    persist_library_id();
    emit library_id_changed(id);
}

void ReportBuilderService::save_to_library() {
    library_timer_->stop();
    library_dirty_ = false;
    if (chains_[library_generation_].conflict)
        return; // paused until the user decides (the recovery copy keeps the edits)
    QString json, title;
    {
        QMutexLocker lock(&mutex_);
        if (!worth_saving_locked())
            return;
        json = doc_.to_json(/*with_revision=*/false);
        title = rep::vars::display_title(doc_.metadata);
    }
    if (library_id() > 0 && qHash(json) == library_hash_) {
        emit library_saved(library_id()); // unchanged since the last write (e.g. undone back)
        return;
    }
    request_library_write(library_generation_, title, json);
}

void ReportBuilderService::save_to_library_blocking() {
    library_timer_->stop();
    library_dirty_ = false;
    // A write still in flight may be creating this report's row: let it finish and
    // deliver its id, or the synchronous save below would add a second row.
    auto chain = chains_.find(library_generation_);
    if (chain != chains_.end() && chain->in_flight) {
        ReportLibrary::instance().wait_for_idle(3000);
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
    QString json, title;
    qint64 id;
    {
        QMutexLocker lock(&mutex_);
        if (!worth_saving_locked())
            return;
        json = doc_.to_json(/*with_revision=*/false);
        title = rep::vars::display_title(doc_.metadata);
        id = library_id_;
    }
    WriteChain& wc = chains_[library_generation_];
    if (id > 0 && qHash(json) == library_hash_ && !wc.conflict)
        return;
    // Quitting with an unresolved conflict (or meeting one now): nothing can be asked any
    // more, so the open version is kept as a separate report instead of overwriting.
    auto& lib = ReportLibrary::instance();
    LibrarySaveOutcome out;
    if (!wc.conflict)
        out = lib.save_checked_blocking(id, title, json, wc.expected, wc.force_next);
    if (wc.conflict || out.conflict) {
        const QString copy_title = tr("%1 (unsynced copy)").arg(title);
        const qint64 copy = lib.save_blocking(0, copy_title, json);
        LOG_WARN(RB_SVC_TAG, logf("Report changed elsewhere; open version kept as a copy on quit",
                                  {{"report", id}, {"copy", copy}}));
        return;
    }
    if (out.ok) {
        library_hash_ = qHash(json);
        wc.expected = out.revision;
        {
            QMutexLocker lock(&mutex_);
            doc_.revision = out.revision;
        }
        set_library_id(out.id);
        rbs_remember_file_library_id(current_file(), out.id);
    }
}

void ReportBuilderService::request_library_write(quint64 generation, const QString& title, const QString& json) {
    WriteChain& chain = chains_[generation];
    chain.has_next = true;
    chain.next_title = title;
    chain.next_json = json;
    if (!chain.in_flight)
        start_library_write(generation);
}

void ReportBuilderService::start_library_write(quint64 generation) {
    auto it = chains_.find(generation);
    if (it == chains_.end() || !it->has_next)
        return;
    if (it->conflict) {
        it->has_next = false;
        return;
    }
    it->in_flight = true;
    it->has_next = false;
    const QString title = it->next_title;
    const QString json = it->next_json;
    it->next_json.clear();
    const bool force = it->force_next;
    it->force_next = false;
    const std::size_t hash = qHash(json);
    ReportLibrary::instance().save_checked(
        it->id, title, json, it->expected, force, this, [this, generation, hash](const LibrarySaveOutcome& out) {
            auto c = chains_.find(generation);
            if (c == chains_.end())
                return;
            c->in_flight = false;
            if (out.conflict) {
                c->conflict = true;
                c->has_next = false;
                if (generation == library_generation_) {
                    conflict_.id = out.id;
                    conflict_.theirs = out.theirs;
                    conflict_.theirs_title = out.theirs_title;
                    conflict_.theirs_updated = out.theirs_updated;
                    emit library_conflict_detected();
                } else {
                    chains_.erase(c); // an older report: its last edits stay in the recovery copy
                }
                return;
            }
            if (out.ok) {
                c->id = out.id;
                c->expected = out.revision;
                rbs_remember_file_library_id(c->file, out.id);
                if (generation == library_generation_) {
                    library_hash_ = hash;
                    {
                        QMutexLocker lock(&mutex_);
                        doc_.revision = out.revision;
                    }
                    set_library_id(out.id);
                    ReportLibrary::instance().note_opened(out.id);
                    emit library_saved(out.id);
                }
            } else {
                LOG_WARN(RB_SVC_TAG, "Library save failed: " + out.error);
                if (generation == library_generation_)
                    emit library_save_failed(out.error);
            }
            if (c->has_next)
                start_library_write(generation);
            else if (generation != library_generation_)
                chains_.erase(c); // an older report, fully written
        });
}

// ── Open requests ───────────────────────────────────────────────────────────

void ReportBuilderService::request_open(const OpenRequest& request) {
    // Only the newest request matters: the user asked for that report last.
    open_requests_.clear();
    open_requests_.append(request);
    emit open_requested();
}

bool ReportBuilderService::take_open_request(OpenRequest* out) {
    if (open_requests_.isEmpty())
        return false;
    if (out)
        *out = open_requests_.takeLast();
    open_requests_.clear();
    return true;
}

} // namespace fincept::services
