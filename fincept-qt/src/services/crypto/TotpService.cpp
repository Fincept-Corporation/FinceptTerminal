#include "services/crypto/TotpService.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace fincept::services::crypto {

TotpService& TotpService::instance() {
    static TotpService s;
    return s;
}

void TotpService::generate(const QString& secret, TotpCallback cb) {
    if (secret.isEmpty()) {
        if (cb) {
            TotpResult r;
            r.error = "empty secret";
            cb(r);
        }
        return;
    }

    // Relative to PythonRunner::scripts_dir() — run() prepends it itself.
    // (This used to pass the absolute scripts_dir() + "/exchange/totp_gen.py",
    // which run() then prefixed a second time, so every call failed the
    // QFileInfo::exists() check with "Script not found".)
    const QString script = QStringLiteral("exchange/totp_gen.py");

    // ── SECURITY: the seed goes on stdin, never argv ─────────────────────────
    // `secret` is the broker's PERMANENT base32 TOTP seed — not a one-time
    // code. In argv it is readable by any process running as the same user
    // (Win32_Process.CommandLine via WMI on Windows with no elevation;
    // /proc/<pid>/cmdline on Linux) and it lands in crash dumps and EDR
    // telemetry. Stealing it is a lasting bypass of the broker's second factor,
    // and this fires on every authenticated broker login.
    //
    // PythonRunner::run() now takes `stdin_data`: it writes the bytes to the
    // child and closes the write channel, which is exactly what totp_gen.py's
    // `--stdin` path waits for (it reads one line and must see EOF). The seed
    // never appears in the command line and never touches disk.
    fincept::python::PythonRunner::instance().run(
        script, {QStringLiteral("--stdin")}, [cb = std::move(cb)](const fincept::python::PythonResult& result) {
            TotpResult out;
            if (!result.success) {
                out.error = result.output.isEmpty() ? QStringLiteral("totp_gen failed") : result.output;
                if (cb)
                    cb(out);
                return;
            }
            const auto doc = QJsonDocument::fromJson(result.output.toUtf8());
            if (!doc.isObject()) {
                out.error = "malformed totp_gen output";
                if (cb)
                    cb(out);
                return;
            }
            const auto obj = doc.object();
            if (!obj.contains("code")) {
                out.error = "totp_gen output missing 'code'";
                if (cb)
                    cb(out);
                return;
            }
            out.success = true;
            out.code = obj.value("code").toString();
            out.valid_for = obj.value("valid_for").toInt();
            if (cb)
                cb(out);
        },
        /*on_line=*/{}, /*stdin_data=*/secret.toUtf8() + '\n');
}

} // namespace fincept::services::crypto
