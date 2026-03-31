#pragma once

#include "screens/node_editor/NodeEditorTypes.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::workflow {

/// Creates the appropriate editor widget for a parameter definition.
class ParameterWidgetFactory {
  public:
    /// Create a labeled widget for the given parameter.
    /// The returned widget contains a label + editor.
    /// When the value changes, on_change is called.
    static QWidget* create(const ParamDef& param, const QJsonValue& current_value,
                           std::function<void(const QString& key, QJsonValue value)> on_change,
                           QWidget* parent = nullptr);
};

} // namespace fincept::workflow
