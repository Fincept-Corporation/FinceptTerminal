// ReportBuilderService.cpp — see header for design rationale.

#include "services/report_builder/ReportBuilderService.h"

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QUndoCommand>

#include <utility>

namespace fincept::services {

static constexpr const char* RB_SVC_TAG = "ReportBuilderService";
static constexpr int kAutosaveIntervalMs = 60'000;
static constexpr int kLlmWindowMs = 400;
static constexpr int kRecentMax = 10;
static constexpr const char* kRecentKey = "report_builder/recent";
static constexpr const char* kCurrentFileKey = "report_builder/current_file";

namespace rep = report;

// ── Undo commands ────────────────────────────────────────────────────────────
//
// Owned by the service (formerly owned by the screen). They mutate the doc
// directly without re-entering the public mutators (which would push another
// command onto the stack — infinite loop). The service exposes private direct-
// mutators via friend access.

namespace {

class AddCmd : public QUndoCommand {
  public:
    AddCmd(ReportBuilderService* svc, rep::ReportComponent comp, int at)
        : QUndoCommand("Add " + comp.type), svc_(svc), comp_(std::move(comp)), at_(at) {}
    void redo() override;
    void undo() override;

  private:
    ReportBuilderService* svc_;
    rep::ReportComponent comp_;
    int at_;
};

class RemoveCmd : public QUndoCommand {
  public:
    RemoveCmd(ReportBuilderService* svc, int index)
        : QUndoCommand("Remove component"), svc_(svc), index_(index) {}
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
              QString new_content, QMap<QString, QString> new_config)
        : QUndoCommand("Edit component"),
          svc_(svc),
          id_(id),
          old_content_(std::move(old_content)),
          new_content_(std::move(new_content)),
          old_config_(std::move(old_config)),
          new_config_(std::move(new_config)) {}
    void redo() override;
    void undo() override;
    int id() const override { return 1001; }
    bool mergeWith(const QUndoCommand* other) override;

  private:
    ReportBuilderService* svc_;
    int id_;
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
        : QUndoCommand("Edit metadata"),
          svc_(svc),
          old_(std::move(old_m)),
          new_(std::move(new_m)) {}
    void redo() override;
    void undo() override;

  private:
    ReportBuilderService* svc_;
    rep::ReportMetadata old_, new_;
};

class ThemeCmd : public QUndoCommand {
  public:
    ThemeCmd(ReportBuilderService* svc, rep::ReportTheme old_t, rep::ReportTheme new_t)
        : QUndoCommand("Change theme"),
          svc_(svc),
          old_(std::move(old_t)),
          new_(std::move(new_t)) {}
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

void AddCmd::redo() { detail::ServiceFriend::direct_insert(svc_, comp_, at_); }
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
    if (other->id() != id())
        return false;
    auto* o = static_cast<const UpdateCmd*>(other);
    if (o->id_ != id_)
        return false;
    new_content_ = o->new_content_;
    new_config_ = o->new_config_;
    return true;
}

void MoveCmd::redo() { detail::ServiceFriend::direct_move_by_id(svc_, id_, to_); }
void MoveCmd::undo() { detail::ServiceFriend::direct_move_by_id(svc_, id_, from_); }

void MetadataCmd::redo() { detail::ServiceFriend::direct_set_metadata(svc_, new_); }
void MetadataCmd::undo() { detail::ServiceFriend::direct_set_metadata(svc_, old_); }

void ThemeCmd::redo() { detail::ServiceFriend::direct_set_theme(svc_, new_); }
void ThemeCmd::undo() { detail::ServiceFriend::direct_set_theme(svc_, old_); }

// ── Service ──────────────────────────────────────────────────────────────────

ReportBuilderService& ReportBuilderService::instance() {
    static ReportBuilderService s;
    return s;
}

ReportBuilderService::ReportBuilderService() {
    doc_.metadata.date = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    undo_stack_ = new QUndoStack(this);

    autosave_timer_ = new QTimer(this);
    autosave_timer_->setInterval(kAutosaveIntervalMs);
    connect(autosave_timer_, &QTimer::timeout, this, &ReportBuilderService::trigger_autosave);
    autosave_timer_->start();

    llm_window_timer_ = new QTimer(this);
    llm_window_timer_->setSingleShot(true);
    llm_window_timer_->setInterval(kLlmWindowMs);
    connect(llm_window_timer_, &QTimer::timeout, this, [this]() {
        // Close the macro we opened in note_llm_mutation().
        if (macro_depth_ > 0) {
            macro_depth_ = 0;
            undo_stack_->endMacro();
            LOG_INFO(RB_SVC_TAG, "LLM macro window closed");
        }
    });

    autosave_path_ =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/fincept_report_autosave.fincept";

    // Restore current_file_ from settings so the title bar shows the right
    // path on relaunch. (The doc itself comes from autosave below.)
    QSettings s("Fincept", "FinceptTerminal");
    current_file_ = s.value(kCurrentFileKey).toString();

    // Try to restore from autosave on construction. Failures are silent —
    // an empty doc is the correct fallback.
    QFile f(autosave_path_);
    if (f.exists() && f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString json = QString::fromUtf8(f.readAll());
        f.close();
        auto r = rep::ReportDocument::from_json(json);
        if (r.is_ok()) {
            doc_ = r.value();
            LOG_INFO(RB_SVC_TAG, "Restored from autosave: " + autosave_path_);
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
QString ReportBuilderService::current_file() const {
    QMutexLocker lock(&mutex_);
    return current_file_;
}
QStringList ReportBuilderService::recent_files() const {
    if (!recent_loaded_)
        load_recent();
    return recent_cache_;
}
rep::ReportDocument ReportBuilderService::snapshot() const {
    QMutexLocker lock(&mutex_);
    return doc_;
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
    LOG_INFO(RB_SVC_TAG, QString("add_component: type=%1 id=%2 at=%3").arg(c.type).arg(c.id).arg(at));
    push_undo(new AddCmd(this, c, at));
    return c.id;
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
    push_undo(new UpdateCmd(this, id, old_content, old_config, content, config));
    return true;
}

bool ReportBuilderService::patch_component(int id, const QString* content_or_null,
                                            const QMap<QString, QString>& partial_config) {
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
        for (auto it = partial_config.cbegin(); it != partial_config.cend(); ++it)
            new_config[it.key()] = it.value();
    }
    push_undo(new UpdateCmd(this, id, old_content, old_config, new_content, new_config));
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
    begin_macro("Clear report");
    auto comps = components();
    for (auto it = comps.crbegin(); it != comps.crend(); ++it)
        remove_component(it->id);
    set_metadata(rep::ReportMetadata{});
    end_macro();
    {
        QMutexLocker lock(&mutex_);
        doc_.metadata.date = QDateTime::currentDateTime().toString("yyyy-MM-dd");
        current_file_.clear();
    }
    persist_current_file();
    emit document_cleared();
    emit current_file_changed({});
}

void ReportBuilderService::replace_document(const rep::ReportDocument& doc) {
    {
        QMutexLocker lock(&mutex_);
        doc_ = doc;
    }
    undo_stack_->clear();
    emit document_changed();
}

// ── Undo helpers ────────────────────────────────────────────────────────────

QUndoStack* ReportBuilderService::undo_stack() { return undo_stack_; }

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
        LOG_INFO(RB_SVC_TAG, "LLM macro window opened (400ms)");
    }
    // (Re)start the window timer — extend as long as mutations keep arriving.
    llm_window_timer_->start();
}

void ReportBuilderService::push_undo(QUndoCommand* cmd) { undo_stack_->push(cmd); }

// ── File ops ────────────────────────────────────────────────────────────────

Result<void> ReportBuilderService::save_to(const QString& path) {
    QString json;
    {
        QMutexLocker lock(&mutex_);
        json = doc_.to_json();
    }
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return Result<void>::err("Could not open file for writing: " + f.errorString().toStdString());
    f.write(json.toUtf8());
    f.close();
    set_current_file(path);
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
    {
        QMutexLocker lock(&mutex_);
        doc_ = r.value();
    }
    undo_stack_->clear();
    set_current_file(path);
    emit document_loaded(path);
    emit document_changed();
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

void ReportBuilderService::persist_current_file() {
    QSettings s("Fincept", "FinceptTerminal");
    s.setValue(kCurrentFileKey, current_file_);
}

QString ReportBuilderService::autosave_path() const { return autosave_path_; }

void ReportBuilderService::trigger_autosave() {
    QString json;
    {
        QMutexLocker lock(&mutex_);
        // Skip writing an empty doc — avoids clobbering a useful autosave
        // with nothing if the user just bumped the timer with no content.
        if (doc_.components.isEmpty() && doc_.metadata.title == "Untitled Report")
            return;
        json = doc_.to_json();
    }
    QFile f(autosave_path_);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text))
        f.write(json.toUtf8());
}

void ReportBuilderService::load_recent() const {
    QSettings s("Fincept", "FinceptTerminal");
    recent_cache_ = s.value(kRecentKey).toStringList();
    recent_loaded_ = true;
}

void ReportBuilderService::save_recent() const {
    QSettings s("Fincept", "FinceptTerminal");
    s.setValue(kRecentKey, recent_cache_);
}

// ── apply_template ──────────────────────────────────────────────────────────
//
// Templates are large literal data — keeping the table of templates here
// (out of the screen) means the LLM and manual UI both build the same
// content. The template body lives in ReportBuilderTemplates.cpp so this
// file stays focused.

extern void apply_template_to_service(ReportBuilderService* svc, const QString& name);

void ReportBuilderService::apply_template(const QString& name) {
    LOG_INFO(RB_SVC_TAG, "apply_template: " + name);
    apply_template_to_service(this, name);
}

} // namespace fincept::services
