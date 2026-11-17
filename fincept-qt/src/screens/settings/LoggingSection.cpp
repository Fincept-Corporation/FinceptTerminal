// LoggingSection.cpp — global level / JSON mode / log path / per-tag overrides,
// the environment override in force, and "Copy diagnostics (redacted)".

#include "screens/settings/LoggingSection.h"

#include "core/config/AppConfig.h"
#include "core/config/AppPaths.h"
#include "core/config/EditionIdentity.h"
#include "core/logging/LogSettings.h"
#include "core/logging/Logger.h"
#include "services/harness/HarnessRegistry.h"
#include "storage/sqlite/Database.h"
#include "storage/sqlite/migrations/MigrationRunner.h"
#include "screens/settings/SettingsRowHelpers.h"
#include "screens/settings/SettingsStyles.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/MnemonicText.h"

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
#include <QSysInfo>
#include <QTimer>
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

    section_title_ = new QLabel(tr("LOGGING"));
    section_title_->setStyleSheet(section_title_ss());
    vl->addWidget(section_title_);
    vl->addWidget(make_sep());

    // ── Global level ────────────────────────────────────────────────────────
    global_lbl_ = new QLabel(tr("Global Log Level"));
    global_lbl_->setStyleSheet(sub_title_ss());
    vl->addWidget(global_lbl_);

    global_desc_ = new QLabel(tr("Minimum level for all tags unless overridden."));
    global_desc_->setStyleSheet(label_ss());
    global_desc_->setWordWrap(true);
    vl->addWidget(global_desc_);

    log_global_level_ = new QComboBox;
    log_global_level_->addItems(LEVEL_NAMES);
    log_global_level_->setStyleSheet(combo_ss());
    log_global_level_->setFixedWidth(160);

    log_global_level_->setCurrentIndex(level_to_idx(Logger::level_name(saved_log_level())));

    vl->addWidget(log_global_level_);

    // An environment variable (FINCEPT_LOG_LEVEL / FINCEPT_LOG_TAGS) wins over
    // what is saved here; say so instead of letting a saved level look ignored.
    override_lbl_ = new QLabel(this);
    override_lbl_->setStyleSheet(QStringLiteral("color:%1; font-size:%2px; background:transparent;")
                                     .arg(ui::colors::WARNING(), ui::fonts::px(-2)));
    override_lbl_->setWordWrap(true);
    override_lbl_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    vl->addWidget(override_lbl_);
    vl->addWidget(make_sep());

    // ── Output format ───────────────────────────────────────────────────────
    fmt_title_ = new QLabel(tr("Output Format"));
    fmt_title_->setStyleSheet(sub_title_ss());
    vl->addWidget(fmt_title_);

    fmt_desc_ = new QLabel(
        tr("Plain text is human-readable; JSON emits one structured object per line (easier to parse with tooling)."));
    fmt_desc_->setStyleSheet(label_ss());
    fmt_desc_->setWordWrap(true);
    vl->addWidget(fmt_desc_);

    log_json_mode_ = new QCheckBox(tr("Emit structured JSON lines"));
    log_json_mode_->setStyleSheet(check_ss()); // was the only unstyled control on this page
    log_json_mode_->setChecked(saved_log_json_mode());
    vl->addWidget(log_json_mode_);
    vl->addWidget(make_sep());

    // ── Log file location ───────────────────────────────────────────────────
    path_title_ = new QLabel(tr("Log File"));
    path_title_->setStyleSheet(sub_title_ss());
    vl->addWidget(path_title_);

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

    open_folder_btn_ = new QPushButton(tr("Open Log Folder"));
    open_folder_btn_->setStyleSheet(btn_secondary_ss());
    open_folder_btn_->setMinimumSize(160, 30);
    open_folder_btn_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(open_folder_btn_, &QPushButton::clicked, this,
            []() { QDesktopServices::openUrl(QUrl::fromLocalFile(AppPaths::logs())); });

    copy_path_btn_ = new QPushButton(tr("Copy Path"));
    copy_path_btn_->setStyleSheet(btn_secondary_ss());
    copy_path_btn_->setMinimumSize(120, 30);
    copy_path_btn_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(copy_path_btn_, &QPushButton::clicked, this, [this]() {
        if (log_path_label_)
            QApplication::clipboard()->setText(log_path_label_->text());
    });

    path_rl->addWidget(open_folder_btn_);
    path_rl->addWidget(copy_path_btn_);
    path_rl->addStretch();
    vl->addWidget(path_row);
    vl->addWidget(make_sep());

    // ── Per-tag overrides ───────────────────────────────────────────────────
    tag_title_ = new QLabel(tr("Per-Tag Overrides"));
    tag_title_->setStyleSheet(sub_title_ss());
    vl->addWidget(tag_title_);

    tag_desc_ = new QLabel(tr("Override the log level for a specific tag (e.g. ExchangeService, AgentService)."));
    tag_desc_->setStyleSheet(label_ss());
    tag_desc_->setWordWrap(true);
    vl->addWidget(tag_desc_);

    log_tag_list_ = new QWidget(this);
    log_tag_layout_ = new QVBoxLayout(log_tag_list_);
    log_tag_layout_->setContentsMargins(0, 0, 0, 0);
    log_tag_layout_->setSpacing(6);

    // level_to_idx is captured BY VALUE: add_tag_row is copied into the
    // "+ Add Tag Override" click handler, which runs long after build_ui() has
    // returned. Capturing by reference left that copy holding a dangling
    // reference to a destroyed stack lambda — undefined behaviour on every
    // click of the Add button.
    auto add_tag_row = [this, level_to_idx](const QString& tag, const QString& level) {
        using namespace settings_styles;
        auto* row = new QWidget(this);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->setSpacing(8);

        auto* tag_edit = new QLineEdit(tag);
        tag_edit->setPlaceholderText(tr("Tag name"));
        tag_edit->setFixedWidth(220);
        tag_edit->setStyleSheet(input_ss());

        auto* lvl_combo = new QComboBox;
        lvl_combo->addItems(LEVEL_NAMES);
        lvl_combo->setStyleSheet(combo_ss());
        lvl_combo->setFixedWidth(120);
        lvl_combo->setCurrentIndex(level_to_idx(level));

        auto* del_btn = new QPushButton(tr("Remove"));
        del_btn->setStyleSheet(btn_danger_ss());
        del_btn->setMinimumHeight(28);
        connect(del_btn, &QPushButton::clicked, this, [row]() { row->deleteLater(); });

        rl->addWidget(tag_edit);
        rl->addWidget(lvl_combo);
        rl->addWidget(del_btn);
        rl->addStretch();
        log_tag_layout_->addWidget(row);
    };

    {
        const QHash<QString, LogLevel> saved_tags = saved_log_tag_levels();
        QStringList names = saved_tags.keys();
        names.sort(Qt::CaseInsensitive);
        for (const QString& tag : std::as_const(names))
            add_tag_row(tag, Logger::level_name(saved_tags.value(tag)));
    }

    vl->addWidget(log_tag_list_);

    add_btn_ = new QPushButton(tr("+ Add Tag Override"));
    add_btn_->setStyleSheet(btn_secondary_ss());
    add_btn_->setMinimumSize(180, 30);
    add_btn_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(add_btn_, &QPushButton::clicked, this, [add_tag_row]() mutable { add_tag_row({}, "Info"); });
    vl->addWidget(add_btn_);
    vl->addWidget(make_sep());

    // ── Save ────────────────────────────────────────────────────────────────
    save_btn_ = new QPushButton(fincept::ui::literal_ampersands(tr("Apply & Save")));
    save_btn_->setStyleSheet(btn_primary_ss());
    save_btn_->setMinimumSize(140, 34);
    save_btn_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    connect(save_btn_, &QPushButton::clicked, this, [this]() {
        const QString gl = log_global_level_->currentText();
        const LogLevel global = Logger::parse_level(gl).value_or(LogLevel::Info);
        const bool json_on = log_json_mode_ && log_json_mode_->isChecked();

        QHash<QString, LogLevel> tags;
        const auto rows = log_tag_list_->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
        for (auto* row : rows) {
            auto* tag_edit = row->findChild<QLineEdit*>();
            auto* lvl_combo = row->findChild<QComboBox*>();
            if (!tag_edit || !lvl_combo)
                continue;
            const QString tag = tag_edit->text().trimmed();
            if (tag.isEmpty())
                continue;
            tags.insert(tag, Logger::parse_level(lvl_combo->currentText()).value_or(LogLevel::Info));
        }
        // Saves the log/* keys and applies them (an environment override still wins).
        save_log_settings(global, tags, json_on);
        const int saved = static_cast<int>(tags.size());

        // Saving previously produced no on-screen acknowledgement at all.
        save_btn_->setText(tr("Saved ✓"));
        QTimer::singleShot(2000, save_btn_, [this]() {
            if (save_btn_)
                save_btn_->setText(fincept::ui::literal_ampersands(tr("Apply & Save")));
        });

        LOG_INFO("Settings", logf("Logging settings saved", {{"global", Logger::level_name(global)},
                                                              {"tags", saved},
                                                              {"json", json_on}}));
    });

    vl->addWidget(save_btn_);
    vl->addWidget(make_sep());

    // ── Diagnostics ─────────────────────────────────────────────────────────
    diag_title_ = new QLabel(tr("Diagnostics"));
    diag_title_->setStyleSheet(sub_title_ss());
    vl->addWidget(diag_title_);

    diag_desc_ = new QLabel(tr("Copies a summary of this installation and the last 200 warnings and errors, "
                               "with file paths, addresses, keys and tokens removed, for a support request."));
    diag_desc_->setStyleSheet(label_ss());
    diag_desc_->setWordWrap(true);
    vl->addWidget(diag_desc_);

    copy_diag_btn_ = new QPushButton(tr("Copy Diagnostics (Redacted)"));
    copy_diag_btn_->setStyleSheet(btn_secondary_ss());
    copy_diag_btn_->setMinimumSize(220, 30);
    copy_diag_btn_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(copy_diag_btn_, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(diagnostics_text());
        copy_diag_btn_->setText(tr("Copied ✓"));
        QTimer::singleShot(2000, copy_diag_btn_, [this]() {
            if (copy_diag_btn_)
                copy_diag_btn_->setText(tr("Copy Diagnostics (Redacted)"));
        });
        LOG_INFO("Settings", "Diagnostics copied to the clipboard");
    });
    vl->addWidget(copy_diag_btn_);

    retranslateUi(); // fills the override notice
    vl->addStretch();
    scroll->setWidget(page);
    root->addWidget(scroll);
}

void LoggingSection::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void LoggingSection::retranslateUi() {
    if (section_title_)
        section_title_->setText(tr("LOGGING"));
    if (global_lbl_)
        global_lbl_->setText(tr("Global Log Level"));
    if (global_desc_)
        global_desc_->setText(tr("Minimum level for all tags unless overridden."));
    if (fmt_title_)
        fmt_title_->setText(tr("Output Format"));
    if (fmt_desc_)
        fmt_desc_->setText(tr(
            "Plain text is human-readable; JSON emits one structured object per line (easier to parse with tooling)."));
    if (log_json_mode_)
        log_json_mode_->setText(tr("Emit structured JSON lines"));
    if (path_title_)
        path_title_->setText(tr("Log File"));
    if (open_folder_btn_)
        open_folder_btn_->setText(tr("Open Log Folder"));
    if (copy_path_btn_)
        copy_path_btn_->setText(tr("Copy Path"));
    if (tag_title_)
        tag_title_->setText(tr("Per-Tag Overrides"));
    if (tag_desc_)
        tag_desc_->setText(tr("Override the log level for a specific tag (e.g. ExchangeService, AgentService)."));
    if (add_btn_)
        add_btn_->setText(tr("+ Add Tag Override"));
    if (save_btn_)
        save_btn_->setText(fincept::ui::literal_ampersands(tr("Apply & Save")));
    if (diag_title_)
        diag_title_->setText(tr("Diagnostics"));
    if (diag_desc_)
        diag_desc_->setText(tr("Copies a summary of this installation and the last 200 warnings and errors, "
                               "with file paths, addresses, keys and tokens removed, for a support request."));
    if (copy_diag_btn_)
        copy_diag_btn_->setText(tr("Copy Diagnostics (Redacted)"));
    if (override_lbl_) {
        const QString active = Logger::instance().level_overrides();
        override_lbl_->setText(active.isEmpty()
                                   ? QString()
                                   : tr("An environment override is in force and wins over the levels saved "
                                        "here until it is removed: %1")
                                         .arg(active));
        override_lbl_->setVisible(!active.isEmpty());
    }

    // Re-apply text on dynamically-created per-tag rows: the "Tag name"
    // placeholder and the "Remove" button. The tag text and level are user data.
    if (log_tag_list_) {
        const auto rows = log_tag_list_->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
        for (auto* row : rows) {
            if (auto* tag_edit = row->findChild<QLineEdit*>())
                tag_edit->setPlaceholderText(tr("Tag name"));
            if (auto* del_btn = row->findChild<QPushButton*>())
                del_btn->setText(tr("Remove"));
        }
    }
}

QString LoggingSection::diagnostics_text() const {
    auto& log = Logger::instance();
    QStringList out;
    out << QStringLiteral("Fincept Terminal diagnostics (redacted)");
#ifdef NDEBUG
    const QString build = QStringLiteral("release");
#else
    const QString build = QStringLiteral("debug");
#endif
    out << logf(QStringLiteral("app"), {{"version", QCoreApplication::applicationVersion()},
                                        {"build", build},
                                        {"edition", fincept::edition::kId}});
    out << logf(QStringLiteral("system"), {{"os", QSysInfo::prettyProductName()},
                                           {"kernel", QSysInfo::kernelVersion()},
                                           {"arch", QSysInfo::currentCpuArchitecture()},
                                           {"qt", qVersion()}});
    // The KIND of data root, never its path.
    out << logf(QStringLiteral("data"),
                {{"data_root", AppPaths::root_override().isEmpty() ? "default" : "custom (FINCEPT_DATA_DIR)"},
                 {"schema", MigrationRunner::highest_registered_version()},
                 {"schema_at_open", Database::instance().found_schema_version()},
                 {"sqlite", Database::instance().sqlite_version()},
                 {"journal", Database::instance().journal_mode()}});
    QStringList tag_items;
    const QHash<QString, LogLevel> tags = log.tag_levels();
    for (auto it = tags.cbegin(); it != tags.cend(); ++it)
        tag_items << it.key() + QLatin1Char('=') + Logger::level_name(it.value());
    tag_items.sort(Qt::CaseInsensitive);
    const LogStats st = log.stats();
    out << logf(QStringLiteral("logging"), {{"level", Logger::level_name(log.global_level())},
                                            {"tags", tag_items.join(QLatin1Char(','))},
                                            {"override", log.level_overrides()},
                                            {"dropped", st.dropped_lines},
                                            {"lost", st.lost_lines}});
    // Harness CLIs as last detected (no probe is started from here).
    auto& harnesses = harness::HarnessRegistry::instance();
    QStringList harness_items;
    for (const QString& id : harnesses.ids()) {
        const harness::HarnessStatus s = harnesses.status(id);
        QString v;
        if (s.installed())
            v = s.installation->version.isEmpty() ? QStringLiteral("installed") : s.installation->version;
        else
            v = s.detection_complete ? QStringLiteral("not_found") : QStringLiteral("not_checked");
        harness_items << id + QLatin1Char('=') + v;
    }
    out << QStringLiteral("harnesses ") + harness_items.join(QLatin1Char(' '));

    const QStringList problems = log.recent_problems(200); // already scrubbed by the logger
    out << QString();
    out << QStringLiteral("Last %1 warnings and errors:").arg(problems.size());
    out << problems;
    return out.join(QLatin1Char('\n'));
}

} // namespace fincept::screens
