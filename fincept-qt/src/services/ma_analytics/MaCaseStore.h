#pragma once
// MaCaseStore — named M&A cases (MA-8): the inputs of one analysis plus the result they produced, saved so a case
// can be reopened and two cases compared. Table `ma_cases` (shared M&A / Alt Investments migration):
//   id TEXT PK, name, module (ModuleInfo::key), context (analysis id), inputs_json, result_json, notes,
//   created_at / updated_at (epoch ms).
// Every SQL statement runs on a worker thread (QtConcurrent; Database hands each thread its own connection); the
// callback runs on the GUI thread, and not at all when `ctx` is gone. Local only: not part of cloud sync.

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVector>

#include <functional>

namespace fincept::services::ma {

struct MaCase {
    QString id; ///< empty on a new case (save() assigns one)
    QString name;
    QString module;
    QString context;
    QJsonObject inputs;
    QJsonObject result;
    QString notes;
    qint64 created_at = 0;
    qint64 updated_at = 0;
};

class MaCaseStore : public QObject {
    Q_OBJECT
  public:
    static MaCaseStore& instance();

    using ListDone = std::function<void(bool ok, QVector<MaCase> cases, QString error)>;
    using SaveDone = std::function<void(bool ok, MaCase saved, QString error)>;
    using RemoveDone = std::function<void(bool ok, QString error)>;

    /// Cases of `module` (every module when empty), newest first.
    void list(const QString& module, QObject* ctx, ListDone done);
    /// Insert (empty id) or replace a case; a case with the same module, context and name is overwritten.
    void save(const MaCase& c, QObject* ctx, SaveDone done);
    void remove(const QString& id, QObject* ctx, RemoveDone done);

    /// Plain-language text for a storage failure ("no such table" = the database was not updated yet).
    static QString plain_error(const QString& raw);

  private:
    explicit MaCaseStore(QObject* parent = nullptr);
    Q_DISABLE_COPY(MaCaseStore)
};

} // namespace fincept::services::ma
