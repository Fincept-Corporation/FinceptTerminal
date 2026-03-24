#pragma once

#include "screens/node_editor/NodeEditorTypes.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QString>

namespace fincept::workflow {

/// Routing target for parameter values.
enum class ParamRoute { None, Header, Query, Body, Path };

/// Processed parameter with routing info.
struct ProcessedParam {
    QString key;
    QJsonValue value;
    ParamRoute route = ParamRoute::None;
};

/// HTTP request built from routed parameters.
struct RoutedRequest {
    QString url;
    QString method = "GET";
    QJsonObject headers;
    QJsonObject query;
    QJsonObject body;
};

/// Processes node parameters: extraction, type conversion, validation, HTTP routing.
class ParameterProcessor {
  public:
    /// Extract and convert a parameter value by type.
    static QJsonValue convert(const QJsonValue& raw, const QString& target_type);

    /// Validate a value against a parameter definition.
    static bool validate(const QJsonValue& value, const ParamDef& def, QString* error = nullptr);

    /// Validate all parameters for a node.
    static bool validate_all(const QJsonObject& params, const QVector<ParamDef>& defs, QStringList* errors = nullptr);

    /// Process parameters with expression resolution and validation.
    static QJsonObject process(const QJsonObject& raw_params, const QVector<ParamDef>& defs,
                               const QJsonObject& context = {});

    /// Build an HTTP request from parameters with routing annotations.
    static RoutedRequest build_request(const QJsonObject& params, const QVector<ParamDef>& defs,
                                       const QString& base_url);

    /// Extract a nested value using dot-path notation.
    static QJsonValue extract_path(const QJsonObject& obj, const QString& path);
};

} // namespace fincept::workflow
