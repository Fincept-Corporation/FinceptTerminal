#include "screens/node_editor/properties/ParameterWidgets.h"

#include <QJsonDocument>

namespace {
static const char* kInputStyle = "background: #1e1e1e; color: #e5e5e5; border: 1px solid #2a2a2a;"
                                 "font-family: Consolas; font-size: 12px; padding: 4px 6px;";

static const char* kLabelStyle = "color: #808080; font-family: Consolas; font-size: 11px; font-weight: bold;";
} // namespace

namespace fincept::workflow {

QWidget* ParameterWidgetFactory::create(const ParamDef& param, const QJsonValue& current_value,
                                        std::function<void(const QString& key, QJsonValue value)> on_change,
                                        QWidget* parent) {
    auto* container = new QWidget(parent);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 4, 0, 4);
    layout->setSpacing(4);

    // Label
    auto* label = new QLabel(param.label + (param.required ? " *" : ""));
    label->setStyleSheet(kLabelStyle);
    layout->addWidget(label);

    QString key = param.key;

    if (param.type == "string" || param.type == "expression") {
        auto* edit = new QLineEdit;
        edit->setText(current_value.toString(param.default_value.toString()));
        edit->setPlaceholderText(param.placeholder);
        edit->setStyleSheet(
            QString("QLineEdit { %1 } QLineEdit:focus { border: 1px solid #d97706; }").arg(kInputStyle));
        layout->addWidget(edit);

        QObject::connect(edit, &QLineEdit::textChanged, container,
                         [key, on_change](const QString& text) { on_change(key, QJsonValue(text)); });
    } else if (param.type == "number") {
        auto* spin = new QDoubleSpinBox;
        spin->setDecimals(4);
        spin->setRange(-1e12, 1e12);
        spin->setValue(current_value.toDouble(param.default_value.toDouble()));
        spin->setStyleSheet(QString("QDoubleSpinBox { %1 }"
                                    "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {"
                                    "  background: #2a2a2a; border: 1px solid #2a2a2a; width: 14px;"
                                    "}")
                                .arg(kInputStyle));
        layout->addWidget(spin);

        QObject::connect(spin, &QDoubleSpinBox::valueChanged, container,
                         [key, on_change](double v) { on_change(key, QJsonValue(v)); });
    } else if (param.type == "boolean") {
        auto* check = new QCheckBox(param.label);
        check->setChecked(current_value.toBool(param.default_value.toBool()));
        check->setStyleSheet("QCheckBox { color: #e5e5e5; font-family: Consolas; font-size: 12px; }"
                             "QCheckBox::indicator {"
                             "  width: 14px; height: 14px; background: #1e1e1e; border: 1px solid #2a2a2a;"
                             "}"
                             "QCheckBox::indicator:checked { background: #d97706; }");
        layout->addWidget(check);

        // Remove the label since checkbox has its own
        label->hide();

        QObject::connect(check, &QCheckBox::toggled, container,
                         [key, on_change](bool checked) { on_change(key, QJsonValue(checked)); });
    } else if (param.type == "select") {
        auto* combo = new QComboBox;
        combo->addItems(param.options);
        combo->setCurrentText(current_value.toString(param.default_value.toString()));
        combo->setStyleSheet(QString("QComboBox { %1 }"
                                     "QComboBox::drop-down {"
                                     "  background: #2a2a2a; border: 1px solid #2a2a2a; width: 18px;"
                                     "}"
                                     "QComboBox QAbstractItemView {"
                                     "  background: #1e1e1e; color: #e5e5e5; border: 1px solid #2a2a2a;"
                                     "  selection-background-color: #d97706; font-family: Consolas;"
                                     "}")
                                 .arg(kInputStyle));
        layout->addWidget(combo);

        QObject::connect(combo, &QComboBox::currentTextChanged, container,
                         [key, on_change](const QString& text) { on_change(key, QJsonValue(text)); });
    } else if (param.type == "code" || param.type == "json") {
        auto* text_edit = new QPlainTextEdit;
        if (param.type == "json") {
            QJsonDocument doc;
            if (current_value.isObject())
                doc = QJsonDocument(current_value.toObject());
            else if (current_value.isArray())
                doc = QJsonDocument(current_value.toArray());
            text_edit->setPlainText(doc.isEmpty() ? param.default_value.toString("{}")
                                                  : QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
        } else {
            text_edit->setPlainText(current_value.toString(param.default_value.toString()));
        }
        text_edit->setPlaceholderText(param.placeholder);
        text_edit->setMinimumHeight(80);
        text_edit->setMaximumHeight(200);
        text_edit->setStyleSheet(QString("QPlainTextEdit { %1 }"
                                         "QPlainTextEdit:focus { border: 1px solid #d97706; }")
                                     .arg(kInputStyle));
        layout->addWidget(text_edit);

        QObject::connect(
            text_edit, &QPlainTextEdit::textChanged, container, [key, on_change, text_edit, param_type = param.type]() {
                QString text = text_edit->toPlainText();
                if (param_type == "json") {
                    QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8());
                    if (!doc.isNull())
                        on_change(key, doc.isObject() ? QJsonValue(doc.object()) : QJsonValue(doc.array()));
                } else {
                    on_change(key, QJsonValue(text));
                }
            });
    }

    return container;
}

} // namespace fincept::workflow
