#pragma once
// ConnectionForm — the settings form of one connection, inline in the Data Sources detail pane
// (it replaces the old modal dialog). Built from the connection type's field list; validates
// shapes (full URLs, ports, JSON objects) and names the field that is wrong.

#include "screens/data_sources/DataSourceTypes.h"
#include "storage/repositories/DataSourceRepository.h"

#include <QHash>
#include <QJsonObject>
#include <QWidget>

class QCheckBox;
class QGridLayout;
class QLabel;
class QLineEdit;

namespace fincept::screens::datasources {

class ConnectionForm : public QWidget {
    Q_OBJECT
  public:
    explicit ConnectionForm(QWidget* parent = nullptr);

    /// Rebuild the fields for `connector` (nullptr = empty form).
    void set_connector(const ConnectorConfig* connector);
    const ConnectorConfig* connector() const { return connector_; }

    /// Fill from a saved connection (`duplicate`: "Copy of …" name, same settings).
    void load(const DataSource& ds, bool duplicate = false);
    /// Defaults of the type (a new connection).
    void load_defaults();

    /// Validated values; false with a plain message (the field gets focus).
    bool collect(QJsonObject* config, QString* name, bool* enabled, QString* tags, QString* error);
    /// The current values without validation (TEST of what is typed).
    QJsonObject current_config() const;
    QString current_name() const;

    bool is_dirty() const { return dirty_; }
    void mark_clean() { dirty_ = false; }

  signals:
    void edited();

  private:
    QWidget* add_row(const QString& label, QWidget* input, bool required);
    void set_field_value(const FieldDef& f, const QJsonValue& v);
    void touch();

    const ConnectorConfig* connector_ = nullptr;
    QWidget* body_ = nullptr;
    QGridLayout* grid_ = nullptr;
    int row_ = 0;
    QLineEdit* name_ = nullptr;
    QCheckBox* enabled_ = nullptr;
    QLineEdit* tags_ = nullptr;
    QLabel* secret_note_ = nullptr;
    QHash<QString, QWidget*> inputs_;
    bool dirty_ = false;
    bool loading_ = false;
};

} // namespace fincept::screens::datasources
