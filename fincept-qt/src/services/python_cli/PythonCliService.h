#pragma once
// PythonCliService — generic one-shot wrapper for user-triggered Python CLI
// scripts that return a JSON document on stdout. Exists so screens can
// satisfy D1 (no direct PythonRunner calls) without inventing a domain
// service for each one-off analytical feature.
//
// Use this for **ONE-SHOT** user actions only (a button click that runs
// `<script> <args...>` and displays the result). Anything streaming, cached
// across screens, or refresh-on-cadence belongs on DataHub via a Producer.
// See §D of CLAUDE.md for the exact rule.

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

namespace fincept::services::python_cli {

struct CliResult {
    bool success = false;
    QJsonObject data;  // parsed top-level JSON object
    QString error;
};

using CliCallback = std::function<void(const CliResult&)>;

/// Thin async wrapper around PythonRunner for screens that just need to
/// "run a script and parse its JSON reply."
class PythonCliService : public QObject {
    Q_OBJECT
  public:
    static PythonCliService& instance();

    /// Runs `<script> <args...>`. If stdout contains a JSON object it is
    /// delivered in `CliResult::data`. `success = false` on process failure,
    /// empty output, or a top-level `{"success": false, "error": ...}`
    /// payload. Callback fires on the main thread.
    void run(const QString& script, const QStringList& args, CliCallback cb);

  private:
    PythonCliService() = default;
};

} // namespace fincept::services::python_cli
