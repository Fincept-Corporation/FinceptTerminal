#pragma once

#include "core/result/Result.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QString>

#include <functional>

namespace fincept::services {

/// Thin testing client for the Data Mapping screen's "Test API" button.
///
/// This is a deliberate exception to the no-direct-HTTP rule for screens: the
/// Data Mapping screen lets the *user* type an arbitrary URL/method/body and
/// poke it to see the response shape. Wrapping that flow in a service keeps the
/// network call off the screen, preserves the screen-as-context lifetime model,
/// and centralises method dispatch.
///
/// Not a `Producer` — there is no topic, no cache, no schedule. One shot per
/// user click.
class DataMappingTestClient : public QObject {
    Q_OBJECT
  public:
    using Callback = std::function<void(Result<QJsonDocument>)>;

    enum class Method { Get, Post, Put, Delete };

    static DataMappingTestClient& instance();

    /// Issue a single test request. `context` scopes the callback's lifetime
    /// (Qt-style — when context is destroyed, the callback is dropped).
    void test_api(Method method, const QString& url, const QJsonObject& body,
                  const QObject* context, Callback callback);

  private:
    DataMappingTestClient() = default;
    Q_DISABLE_COPY(DataMappingTestClient)
};

} // namespace fincept::services
