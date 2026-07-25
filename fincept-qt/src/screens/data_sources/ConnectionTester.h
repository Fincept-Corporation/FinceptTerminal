#pragma once
// ConnectionTester.h — async TCP / URL probe for a saved data-source
// connection. Walks the connector's saved config to derive a probe URL
// (provider-specific, see provider_probe_url) or falls back to host+port
// fields, then runs a TCP connect on a worker thread.
//
// The result dialog is shown on the UI thread inside the tester. The
// callback is invoked on the UI thread (queued connection) so the screen
// can update its own caches/cells.

#include <QJsonObject>
#include <QPointer>
#include <QString>
#include <QWidget>

#include <functional>

namespace fincept::screens::datasources {

/// Result-callback signature: (connection_id, ok, human_readable_message).
using TestResultCallback = std::function<void(const QString& conn_id, bool ok, const QString& msg)>;

/// Test connectivity for the saved connection identified by `conn_id`.
///
/// Behaviour:
///   - If the connector is non-testable, shows an info dialog and returns.
///   - If no probe URL/host is derivable, shows a "no testable endpoint" dialog.
///   - Otherwise runs the probe on a worker thread, then on the UI thread
///     fires `on_result` and shows a result dialog.
///
/// `parent` is used as the parent for all dialogs and as the QPointer guard
/// for the async lambdas.
void test_connection(QWidget* parent, const QString& conn_id, const TestResultCallback& on_result);

/// Provider-specific probe URL synthesis. Exposed for the background poll
/// timer in DataSourcesScreen, which derives host/port without showing a
/// dialog. Returns {} when no HTTP probe is applicable.
///
/// SECURITY: the returned URL frequently embeds the connection's API key or
/// token (that is how most REST health checks authenticate). Never show it in
/// the UI or write it to the log without passing it through redact_url().
QString provider_probe_url(const QString& provider_id, const QJsonObject& cfg);

/// Mask credential material in a URL so it is safe to display or log.
/// Strips any userinfo component and replaces the value of every query
/// parameter whose key looks like a secret (key / token / secret / password /
/// auth / sig / credential) with "***". Non-URL input is returned unchanged.
QString redact_url(const QString& url);

} // namespace fincept::screens::datasources
