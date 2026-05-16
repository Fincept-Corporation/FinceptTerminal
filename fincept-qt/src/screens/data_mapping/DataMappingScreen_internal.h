// src/screens/data_mapping/DataMappingScreen_internal.h
//
// Shared schema/template data structures + accessors for the
// DataMappingScreen partial-class split. Not for use outside this folder.

#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace fincept::screens::data_mapping_internal {

struct SchemaField {
    QString name;
    QString type;
    bool required;
    QString description;
};

struct SchemaInfo {
    QString name;
    QString description;
    QList<SchemaField> fields;
};

struct FieldMapping {
    QString target;
    QString expression;
    QString transform;
    QString default_val;
};

struct MappingTemplate {
    QString id;
    QString name;
    QString description;
    QString broker;
    QString schema;
    QStringList tags;
    bool verified;
    QString base_url;
    QString endpoint;
    QString method;
    QString auth_type;
    QString headers;
    QString body;
    QString parser;
    QList<FieldMapping> field_mappings;
};

/// Static catalog of supported wire schemas (OHLCV, QUOTE, TICK, ...).
const QList<SchemaInfo>& schemas();

/// Static catalog of pre-built broker/API mapping templates.
const QList<MappingTemplate>& templates();

} // namespace fincept::screens::data_mapping_internal
