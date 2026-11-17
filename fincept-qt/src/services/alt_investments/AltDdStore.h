#pragma once
// AltDdStore — the saved due-diligence checklists of the Alternative Investments tab (table
// alt_dd_checklists, migration v064). Every read and write runs on a worker thread (the Database
// hands each thread its own connection); callbacks arrive on the GUI thread and are dropped when
// their context object is gone. Local only (not part of cloud sync).

#include <QJsonArray>
#include <QObject>
#include <QString>
#include <QVector>

#include <functional>

namespace fincept::services::alt {

struct DdChecklist {
    QString id;          ///< uuid
    QString name;        ///< the deal / fund it is for
    QString template_id; ///< AltDdTemplates.h id ("pe", "private_credit", ...)
    QJsonArray items;    ///< [{"id", "status": open|done|issue|na, "note"}]
    qint64 created_at = 0;
    qint64 updated_at = 0;
};

class AltDdStore {
  public:
    using ListDone = std::function<void(const QVector<DdChecklist>& lists, const QString& error)>;
    using Done = std::function<void(bool ok, const QString& error)>;

    /// Every checklist, most recently updated first.
    static void list(QObject* ctx, ListDone done);
    /// Insert or replace (by id; sets updated_at, and created_at when 0).
    static void save(const DdChecklist& checklist, QObject* ctx, Done done = {});
    static void remove(const QString& id, QObject* ctx, Done done = {});
};

} // namespace fincept::services::alt
