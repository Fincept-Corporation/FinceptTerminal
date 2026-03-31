#include "screens/node_editor/properties/ParameterWidgets.h"

#include "mcp/McpService.h"
#include "services/agents/AgentService.h"
#include "services/agents/AgentTypes.h"
#include "services/file_manager/FileManagerService.h"
#include "storage/repositories/LlmProfileRepository.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QPushButton>

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
    } else if (param.type == "file_managed") {
        // Allowed extensions come from placeholder, e.g. "xlsx,csv" or "json"
        QStringList allowed_exts;
        if (!param.placeholder.isEmpty())
            for (const QString& e : param.placeholder.split(','))
                allowed_exts << e.trimmed().toLower();

        // Build dropdown from FileManagerService — filter by allowed extensions
        auto* row = new QWidget;
        auto* rl  = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->setSpacing(4);

        auto* combo = new QComboBox;
        combo->setStyleSheet(QString("QComboBox { %1 }"
                                     "QComboBox::drop-down {"
                                     "  background: #2a2a2a; border: 1px solid #2a2a2a; width: 18px;"
                                     "}"
                                     "QComboBox QAbstractItemView {"
                                     "  background: #1e1e1e; color: #e5e5e5; border: 1px solid #2a2a2a;"
                                     "  selection-background-color: #d97706; font-family: Consolas;"
                                     "}")
                                 .arg(kInputStyle));

        // Build file filter string for QFileDialog e.g. "Spreadsheets (*.xlsx *.csv)"
        QString dialog_filter;
        if (!allowed_exts.isEmpty()) {
            QStringList stars;
            for (const QString& e : allowed_exts) stars << "*." + e;
            dialog_filter = allowed_exts.join("/").toUpper()
                            + " Files (" + stars.join(" ") + ");;All Files (*)";
        } else {
            dialog_filter = "All Files (*)";
        }

        // Populate combo from managed files; select_path forces selection after import
        auto populate = [combo, allowed_exts](const QString& select_path = {}) {
            QString prev = select_path.isEmpty()
                           ? combo->currentData().toString()
                           : select_path;
            combo->clear();
            combo->addItem("— select file —", QString());

            auto& svc = fincept::services::FileManagerService::instance();
            for (const auto& v : svc.all_files()) {
                QJsonObject f         = v.toObject();
                QString stored_name   = f.value("name").toString();
                QString original_name = f.value("originalName").toString();
                QString full_path     = svc.full_path(stored_name);
                QString ext           = QFileInfo(original_name).suffix().toLower();

                if (!allowed_exts.isEmpty() && !allowed_exts.contains(ext))
                    continue;

                combo->addItem(original_name + "  [" + ext.toUpper() + "]", full_path);

                if (!prev.isEmpty() && full_path == prev)
                    combo->setCurrentIndex(combo->count() - 1);
            }
        };
        populate();

        rl->addWidget(combo, 1);

        // Import button — opens file dialog, imports into FileManagerService, auto-selects
        auto* import_btn = new QPushButton("+ Import");
        import_btn->setFixedHeight(24);
        import_btn->setToolTip("Import a file from your PC into the File Manager");
        import_btn->setStyleSheet("QPushButton { background: #2a2a2a; color: #d97706;"
                                  " border: 1px solid #3a3a3a; font-family: Consolas;"
                                  " font-size: 10px; padding: 0 6px; }"
                                  "QPushButton:hover { background: #3a3a3a; }");
        rl->addWidget(import_btn);

        layout->addWidget(row);

        // Hint showing allowed extensions
        if (!allowed_exts.isEmpty()) {
            auto* hint = new QLabel("Accepted: " + allowed_exts.join(", ").toUpper()
                                    + "  •  Or pick an already-imported file above");
            hint->setStyleSheet("color: #525252; font-family: Consolas; font-size: 10px;");
            hint->setWordWrap(true);
            layout->addWidget(hint);
        }

        QObject::connect(combo, &QComboBox::currentIndexChanged, container,
                         [key, on_change, combo](int) {
                             on_change(key, QJsonValue(combo->currentData().toString()));
                         });

        QObject::connect(import_btn, &QPushButton::clicked, container,
                         [populate, dialog_filter, key, on_change, combo, import_btn]() {
                             QString path = QFileDialog::getOpenFileName(
                                 import_btn->window(),
                                 "Import File",
                                 QString(),
                                 dialog_filter);
                             if (path.isEmpty()) return;

                             QString file_id = fincept::services::FileManagerService::instance()
                                                   .import_file(path, "workflow_node");
                             if (file_id.isEmpty()) return;

                             // Find the full managed path for the newly imported file
                             auto f = fincept::services::FileManagerService::instance()
                                          .find_by_id(file_id);
                             QString full_path = fincept::services::FileManagerService::instance()
                                                     .full_path(f.name);

                             populate(full_path);
                             on_change(key, QJsonValue(full_path));
                         });
    } else if (param.type == "agent_select") {
        auto* row   = new QWidget;
        auto* rl    = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->setSpacing(4);

        auto* combo = new QComboBox;
        combo->setStyleSheet(QString("QComboBox { %1 }"
                                     "QComboBox::drop-down { background:#2a2a2a; border:1px solid #2a2a2a; width:18px; }"
                                     "QComboBox QAbstractItemView { background:#1e1e1e; color:#e5e5e5;"
                                     "  border:1px solid #2a2a2a; selection-background-color:#7c3aed;"
                                     "  font-family:Consolas; }").arg(kInputStyle));

        // Constrain combo so it never overflows the panel
        combo->setMaximumWidth(260);
        combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        auto populate_agents = [combo, current_value]() {
            QString saved = current_value.toString();
            if (combo->count() > 0) saved = combo->currentData().toString();
            combo->clear();
            combo->addItem("— select agent —", QString());

            const auto agents = fincept::services::AgentService::instance().cached_agents();
            for (const auto& a : agents) {
                QString label = a.name;
                if (!a.category.isEmpty()) label += "  [" + a.category + "]";
                combo->addItem(label, a.id);
                if (a.id == saved)
                    combo->setCurrentIndex(combo->count() - 1);
            }
        };

        // Helper: wire one-shot discovery → populate
        auto trigger_discovery = [populate_agents, combo]() {
            auto& svc = fincept::services::AgentService::instance();
            QObject::connect(&svc, &fincept::services::AgentService::agents_discovered,
                             combo, [populate_agents, combo](const QVector<fincept::services::AgentInfo>&,
                                                             const QVector<fincept::services::AgentCategory>&) {
                                 populate_agents();
                                 QObject::disconnect(&fincept::services::AgentService::instance(),
                                                     &fincept::services::AgentService::agents_discovered,
                                                     combo, nullptr);
                             });
            svc.discover_agents();
        };

        // Always auto-discover on widget creation — don't make user navigate elsewhere
        auto& svc = fincept::services::AgentService::instance();
        if (svc.cached_agent_count() > 0) {
            populate_agents();  // cache hot — fill immediately
        } else {
            combo->addItem("Loading agents…", QString());
            trigger_discovery();
        }

        rl->addWidget(combo, 1);

        auto* refresh_btn = new QPushButton("↻");
        refresh_btn->setFixedSize(24, 24);
        refresh_btn->setToolTip("Refresh agent list");
        refresh_btn->setStyleSheet("QPushButton { background:#2a2a2a; color:#7c3aed;"
                                   " border:1px solid #2a2a2a; font-size:13px; }"
                                   "QPushButton:hover { background:#3a3a3a; }");
        rl->addWidget(refresh_btn);
        layout->addWidget(row);

        QObject::connect(combo, &QComboBox::currentIndexChanged, container,
                         [key, on_change, combo](int) {
                             on_change(key, QJsonValue(combo->currentData().toString()));
                         });

        QObject::connect(refresh_btn, &QPushButton::clicked, container,
                         [trigger_discovery]() { trigger_discovery(); });

    } else if (param.type == "llm_select") {
        // Dropdown populated from LlmProfileRepository
        auto* combo = new QComboBox;
        combo->setMaximumWidth(260);
        combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        combo->setStyleSheet(QString("QComboBox { %1 }"
                                     "QComboBox::drop-down { background:#2a2a2a; border:1px solid #2a2a2a; width:18px; }"
                                     "QComboBox QAbstractItemView { background:#1e1e1e; color:#e5e5e5;"
                                     "  border:1px solid #2a2a2a; selection-background-color:#7c3aed;"
                                     "  font-family:Consolas; }").arg(kInputStyle));
        combo->addItem("— agent default —", QString());

        QString saved = current_value.toString();
        auto res = fincept::LlmProfileRepository::instance().list_profiles();
        if (res.is_ok()) {
            for (const auto& p : res.value()) {
                QString label = p.name + "  [" + p.provider + " / " + p.model_id + "]";
                combo->addItem(label, p.id);
                if (p.id == saved)
                    combo->setCurrentIndex(combo->count() - 1);
            }
        }
        layout->addWidget(combo);

        auto* hint = new QLabel("Leave blank to use the LLM assigned to the agent in Agent Config");
        hint->setStyleSheet("color:#525252; font-family:Consolas; font-size:10px;");
        hint->setWordWrap(true);
        layout->addWidget(hint);

        QObject::connect(combo, &QComboBox::currentIndexChanged, container,
                         [key, on_change, combo](int) {
                             on_change(key, QJsonValue(combo->currentData().toString()));
                         });
    } else if (param.type == "mcp_tool_select") {
        // Dropdown + refresh, populated from McpService::get_all_tools()
        // Displays: "Category / tool_name — description"
        // Value stored: bare tool_name (internal) or "serverId__toolName" (external)
        auto* row = new QWidget;
        auto* rl  = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->setSpacing(4);

        auto* combo = new QComboBox;
        combo->setMaximumWidth(260);
        combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        combo->setStyleSheet(QString("QComboBox { %1 }"
                                     "QComboBox::drop-down { background:#2a2a2a; border:1px solid #2a2a2a; width:18px; }"
                                     "QComboBox QAbstractItemView { background:#1e1e1e; color:#e5e5e5;"
                                     "  border:1px solid #2a2a2a; selection-background-color:#6366f1;"
                                     "  font-family:Consolas; }").arg(kInputStyle));

        auto populate_tools = [combo, current_value]() {
            QString saved = combo->count() > 0 ? combo->currentData().toString()
                                               : current_value.toString();
            combo->clear();
            combo->addItem("— select tool —", QString());

            auto tools = mcp::McpService::instance().get_all_tools();
            // Group by category for readability — use server_name as group header hint
            QString last_server;
            for (const auto& t : tools) {
                if (t.server_name != last_server) {
                    // Separator item (not selectable)
                    combo->addItem("── " + t.server_name + " ──", QString());
                    auto* model = qobject_cast<QStandardItemModel*>(combo->model());
                    if (model) {
                        auto* sep = model->item(combo->count() - 1);
                        sep->setFlags(sep->flags() & ~Qt::ItemIsEnabled);
                        sep->setData(QColor("#525252"), Qt::ForegroundRole);
                    }
                    last_server = t.server_name;
                }
                // Value: bare name for internal tools, "serverId__name" for external
                QString value = t.is_internal ? t.name : (t.server_id + "__" + t.name);
                QString label = t.name;
                if (!t.description.isEmpty())
                    label += "  — " + t.description.left(50);
                combo->addItem(label, value);
                if (value == saved)
                    combo->setCurrentIndex(combo->count() - 1);
            }
        };

        combo->addItem("Loading tools…", QString());
        // Populate async so we don't block the UI thread constructing the panel
        QMetaObject::invokeMethod(combo, [populate_tools]() { populate_tools(); }, Qt::QueuedConnection);

        rl->addWidget(combo, 1);

        auto* refresh_btn = new QPushButton("↻");
        refresh_btn->setFixedSize(24, 24);
        refresh_btn->setToolTip("Refresh tool list");
        refresh_btn->setStyleSheet("QPushButton { background:#2a2a2a; color:#6366f1;"
                                   " border:1px solid #2a2a2a; font-size:13px; }"
                                   "QPushButton:hover { background:#3a3a3a; }");
        rl->addWidget(refresh_btn);
        layout->addWidget(row);

        auto* hint = new QLabel("All Fincept internal tools. Input JSON flows in as arguments.");
        hint->setStyleSheet("color:#525252; font-family:Consolas; font-size:10px;");
        hint->setWordWrap(true);
        layout->addWidget(hint);

        QObject::connect(combo, &QComboBox::currentIndexChanged, container,
                         [key, on_change, combo](int) {
                             QString val = combo->currentData().toString();
                             if (!val.isEmpty())
                                 on_change(key, QJsonValue(val));
                         });

        QObject::connect(refresh_btn, &QPushButton::clicked, container,
                         [populate_tools]() { populate_tools(); });
    }

    return container;
}

} // namespace fincept::workflow
