// LoggingSection.cpp — global level / JSON mode / log path / per-tag overrides.

#include "screens/settings/LoggingSection.h"

#include "core/config/AppConfig.h"
#include "core/config/AppPaths.h"
#include "core/logging/Logger.h"
#include "screens/settings/SettingsRowHelpers.h"
#include "screens/settings/SettingsStyles.h"
#include "ui/theme/Theme.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFrame>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVBoxLayout>

namespace fincept::screens {

LoggingSection::LoggingSection(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void LoggingSection::build_ui() {
    using namespace settings_styles;
    using namespace settings_helpers;

    // Full level range — matches LogLevel enum in core/logging/Logger.h.
    static const QStringList LEVEL_NAMES = {"Trace", "Debug", "Info", "Warn", "Error", "Fatal"};

    auto level_to_idx = [](const QString& s) -> int {
        int i = LEVEL_NAMES.indexOf(s);
        return i >= 0 ? i : LEVEL_NAMES.indexOf("Info");
    };

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* page = new QWidget(this);
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(24, 24, 24, 24);
    vl->setSpacing(16);

    auto* t = new QLabel("LOGGING");
    t->setStyleSheet(section_title_ss());
    vl->addWidget(t);
    vl->addWidget(make_sep());

    // ── Global level ────────────────────────────────────────────────────────
    auto* global_lbl = new QLabel("Global Log Level");
    global_lbl->setStyleSheet(sub_title_ss());
    vl->addWidget(global_lbl);

    auto* global_desc = new QLabel("Minimum level for all tags unless overridden.");
    global_desc->setStyleSheet(label_ss());
    global_desc->setWordWrap(true);
    vl->addWidget(global_desc);

    log_global_level_ = new QComboBox;
    log_global_level_->addItems(LEVEL_NAMES);
    log_global_level_->setStyleSheet(combo_ss());
    log_global_level_->setFixedWidth(160);

    const QString saved_global = AppConfig::instance().get("log/global_level", "Info").toString();
    log_global_level_->setCurrentIndex(level_to_idx(saved_global));

    vl->addWidget(log_global_level_);
    vl->addWidget(make_sep());

    // ── Output format ───────────────────────────────────────────────────────
    auto* fmt_title = new QLabel("Output Format");
    fmt_title->setStyleSheet(sub_title_ss());
    vl->addWidget(fmt_title);

    auto* fmt_desc = new QLabel(
        "Plain text is human-readable; JSON emits one structured object per line (easier to parse with tooling).");
    fmt_desc->setStyleSheet(label_ss());
    fmt_desc->setWordWrap(true);
    vl->addWidget(fmt_desc);

    log_json_mode_ = new QCheckBox("Emit structured JSON lines");
    log_json_mode_->setChecked(AppConfig::instance().get("log/json_mode", false).toBool());
    vl->addWidget(log_json_mode_);
    vl->addWidget(make_sep());

    // ── Log file location ───────────────────────────────────────────────────
    auto* path_title = new QLabel("Log File");
    path_title->setStyleSheet(sub_title_ss());
    vl->addWidget(path_title);

    const QString log_path = AppPaths::logs() + "/fincept.log";
    log_path_label_ = new QLabel(QDir::toNativeSeparators(log_path));
    log_path_label_->setStyleSheet(label_ss());
    log_path_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    log_path_label_->setWordWrap(true);
    vl->addWidget(log_path_label_);

    auto* path_row = new QWidget(this);
    auto* path_rl = new QHBoxLayout(path_row);
    path_rl->setContentsMargins(0, 0, 0, 0);
    path_rl->setSpacing(8);

    auto* open_folder_btn = new QPushButton("Open Log Folder");
    open_folder_btn->setStyleSheet(btn_secondary_ss());
    open_folder_btn->setFixedHeight(30);
    open_folder_btn->setFixedWidth(160);
    connect(open_folder_btn, &QPushButton::clicked, this,
            []() { QDesktopServices::openUrl(QUrl::fromLocalFile(AppPaths::logs())); });

    auto* copy_path_btn = new QPushButton("Copy Path");
    copy_path_btn->setStyleSheet(btn_secondary_ss());
    copy_path_btn->setFixedHeight(30);
    copy_path_btn->setFixedWidth(120);
    connect(copy_path_btn, &QPushButton::clicked, this, [this]() {
        if (log_path_label_)
            QApplication::clipboard()->setText(log_path_label_->text());
    });

    path_rl->addWidget(open_folder_btn);
    path_rl->addWidget(copy_path_btn);
    path_rl->addStretch();
    vl->addWidget(path_row);
    vl->addWidget(make_sep());

    // ── Per-tag overrides ───────────────────────────────────────────────────
    auto* tag_title = new QLabel("Per-Tag Overrides");
    tag_title->setStyleSheet(sub_title_ss());
    vl->addWidget(tag_title);

    auto* tag_desc = new QLabel("Override the log level for a specific tag (e.g. ExchangeService, AgentService).");
    tag_desc->setStyleSheet(label_ss());
    tag_desc->setWordWrap(true);
    vl->addWidget(tag_desc);

    log_tag_list_   = new QWidget(this);
    log_tag_layout_ = new QVBoxLayout(log_tag_list_);
    log_tag_layout_->setContentsMargins(0, 0, 0, 0);
    log_tag_layout_->setSpacing(6);

    auto& cfg = AppConfig::instance();
    const int count = cfg.get("log/tag_count", 0).toInt();
    auto add_tag_row = [this, &level_to_idx](const QString& tag, const QString& level) {
        using namespace settings_styles;
        auto* row = new QWidget(this);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->setSpacing(8);

        auto* tag_edit = new QLineEdit(tag);
        tag_edit->setPlaceholderText("Tag name");
        tag_edit->setFixedWidth(220);
        tag_edit->setStyleSheet(input_ss());

        auto* lvl_combo = new QComboBox;
        lvl_combo->addItems(LEVEL_NAMES);
        lvl_combo->setStyleSheet(combo_ss());
        lvl_combo->setFixedWidth(120);
        lvl_combo->setCurrentIndex(level_to_idx(level));

        auto* del_btn = new QPushButton("Remove");
        del_btn->setStyleSheet(btn_danger_ss());
        del_btn->setFixedHeight(28);
        connect(del_btn, &QPushButton::clicked, this, [row]() { row->deleteLater(); });

        rl->addWidget(tag_edit);
        rl->addWidget(lvl_combo);
        rl->addWidget(del_btn);
        rl->addStretch();
        log_tag_layout_->addWidget(row);
    };

    for (int i = 0; i < count; ++i) {
        const QString tag   = cfg.get(QString("log/tag_%1_name").arg(i)).toString();
        const QString level = cfg.get(QString("log/tag_%1_level").arg(i)).toString();
        if (!tag.isEmpty()) add_tag_row(tag, level);
    }

    vl->addWidget(log_tag_list_);

    auto* add_btn = new QPushButton("+ Add Tag Override");
    add_btn->setStyleSheet(btn_secondary_ss());
    add_btn->setFixedHeight(30);
    add_btn->setFixedWidth(180);
    connect(add_btn, &QPushButton::clicked, this, [add_tag_row]() mutable { add_tag_row({}, "Info"); });
    vl->addWidget(add_btn);
    vl->addWidget(make_sep());

    // ── Save ────────────────────────────────────────────────────────────────
    auto* save_btn = new QPushButton("Apply & Save");
    save_btn->setStyleSheet(btn_primary_ss());
    save_btn->setFixedHeight(34);
    save_btn->setFixedWidth(140);

    connect(save_btn, &QPushButton::clicked, this, [this]() {
        auto& cfg = AppConfig::instance();
        auto& log = Logger::instance();

        const QString gl = log_global_level_->currentText();
        cfg.set("log/global_level", gl);
        const QHash<QString, LogLevel> lvl_map = {{"Trace", LogLevel::Trace}, {"Debug", LogLevel::Debug},
                                                  {"Info",  LogLevel::Info},  {"Warn",  LogLevel::Warn},
                                                  {"Error", LogLevel::Error}, {"Fatal", LogLevel::Fatal}};
        log.set_level(lvl_map.value(gl, LogLevel::Info));

        const bool json_on = log_json_mode_ && log_json_mode_->isChecked();
        cfg.set("log/json_mode", json_on);
        log.set_json_mode(json_on);

        log.clear_all_tag_levels();
        const auto rows = log_tag_list_->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
        int saved = 0;
        for (auto* row : rows) {
            auto* tag_edit  = row->findChild<QLineEdit*>();
            auto* lvl_combo = row->findChild<QComboBox*>();
            if (!tag_edit || !lvl_combo) continue;
            const QString tag   = tag_edit->text().trimmed();
            const QString level = lvl_combo->currentText();
            if (tag.isEmpty()) continue;
            cfg.set(QString("log/tag_%1_name").arg(saved), tag);
            cfg.set(QString("log/tag_%1_level").arg(saved), level);
            log.set_tag_level(tag, lvl_map.value(level, LogLevel::Info));
            ++saved;
        }
        cfg.set("log/tag_count", saved);

        LOG_INFO("Settings", QString("Logging config saved: global=%1, tags=%2").arg(gl).arg(saved));
    });

    vl->addWidget(save_btn);
    vl->addStretch();
    scroll->setWidget(page);
    root->addWidget(scroll);
}

} // namespace fincept::screens
