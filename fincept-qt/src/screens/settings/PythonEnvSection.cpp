// PythonEnvSection.cpp — Python venv package manager panel.
#include "screens/settings/PythonEnvSection.h"

#include "core/logging/Logger.h"
#include "python/PythonSetupManager.h"
#include "ui/theme/Theme.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QScrollArea>
#include <QShowEvent>
#include <QVBoxLayout>

namespace fincept::screens {

// ── Style helpers (live — read active theme tokens) ───────────────────────────

static QString section_title_ss() {
    return QString("color:%1;font-weight:bold;letter-spacing:0.5px;background:transparent;")
        .arg(ui::colors::AMBER());
}
static QString label_ss() {
    return QString("color:%1;background:transparent;").arg(ui::colors::TEXT_SECONDARY());
}
static QString input_ss() {
    return QString("QLineEdit{background:%1;color:%2;border:1px solid %3;padding:5px 8px;}"
                   "QLineEdit:focus{border:1px solid %4;}")
        .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED(), ui::colors::AMBER());
}
static QString combo_ss() {
    return QString(
               "QComboBox{background:%1;color:%2;border:1px solid %3;padding:5px 8px;min-width:120px;}"
               "QComboBox:focus{border:1px solid %4;}"
               "QComboBox::drop-down{border:none;width:20px;}"
               "QComboBox QAbstractItemView{background:%1;color:%2;"
               "selection-background-color:%5;border:1px solid %3;}")
        .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED(),
             ui::colors::AMBER(), ui::colors::BG_HOVER());
}
static QString btn_primary_ss() {
    return QString("QPushButton{background:%1;color:%2;border:none;font-weight:700;"
                   "padding:0 14px;height:30px;}"
                   "QPushButton:hover{background:%3;}"
                   "QPushButton:disabled{background:%4;color:%5;}")
        .arg(ui::colors::AMBER(), ui::colors::BG_BASE(), ui::colors::AMBER_DIM(),
             ui::colors::BG_RAISED(), ui::colors::TEXT_TERTIARY());
}
static QString btn_secondary_ss() {
    return QString("QPushButton{background:%1;color:%2;border:1px solid %3;"
                   "padding:0 12px;height:30px;}"
                   "QPushButton:hover{background:%4;}"
                   "QPushButton:disabled{color:%5;border-color:%6;}")
        .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_BRIGHT(),
             ui::colors::BG_HOVER(), ui::colors::TEXT_TERTIARY(), ui::colors::BORDER_DIM());
}
static QString table_ss() {
    return QString(
               "QTableWidget{background:%1;color:%2;border:1px solid %3;"
               "gridline-color:%3;outline:none;}"
               "QTableWidget::item{padding:4px 6px;border:none;}"
               "QTableWidget::item:selected{background:%4;color:%2;}"
               "QHeaderView::section{background:%5;color:%6;border:none;"
               "border-bottom:1px solid %3;padding:4px 6px;font-weight:600;}"
               "QScrollBar:vertical{width:6px;background:transparent;}"
               "QScrollBar::handle:vertical{background:%3;border-radius:3px;}"
               "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}"
               "QScrollBar:horizontal{height:6px;background:transparent;}"
               "QScrollBar::handle:horizontal{background:%3;border-radius:3px;}"
               "QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal{width:0;}")
        .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(),
             ui::colors::BG_HOVER(), ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY());
}
static QString progress_ss() {
    return QString("QProgressBar{background:%1;border:1px solid %2;border-radius:3px;}"
                   "QProgressBar::chunk{background:%3;border-radius:3px;}")
        .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM(), ui::colors::AMBER());
}

// ── Canonical name normalisation (mirrors PythonSetupManager logic) ───────────

QString PythonEnvSection::canonicalise(const QString& name) {
    QString n = name.toLower();
    n.replace('-', '_');
    n.replace('.', '_');
    return n;
}

// ── Constructor ───────────────────────────────────────────────────────────────

PythonEnvSection::PythonEnvSection(QWidget* parent) : QWidget(parent) {
    list_proc_   = new QProcess(this);
    action_proc_ = new QProcess(this);

#ifdef _WIN32
    auto hide_window = [](QProcess::CreateProcessArguments* cpa) {
        cpa->flags |= 0x08000000; // CREATE_NO_WINDOW
    };
    list_proc_->setCreateProcessArgumentsModifier(hide_window);
    action_proc_->setCreateProcessArgumentsModifier(hide_window);
#endif

    build_ui();
}

// ── build_ui ──────────────────────────────────────────────────────────────────

void PythonEnvSection::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 16);
    root->setSpacing(0);

    // Title
    auto* title = new QLabel("PYTHON ENVIRONMENTS");
    title->setStyleSheet(section_title_ss());
    root->addWidget(title);
    root->addSpacing(4);

    auto* info = new QLabel(
        "Inspect and manage packages installed in both Python environments. "
        "Trading (venv-numpy1) contains NumPy 1.x-dependent libraries. "
        "Analytics (venv-numpy2) contains NumPy 2.x / ML / AI libraries.");
    info->setWordWrap(true);
    info->setStyleSheet(label_ss());
    root->addWidget(info);
    root->addSpacing(10);

    // ── Warning banner ────────────────────────────────────────────────────────
    auto* warn_frame = new QFrame(this);
    warn_frame->setFrameShape(QFrame::NoFrame);
    warn_frame->setStyleSheet(
        QString("QFrame{background:%1;border:1px solid %2;padding:2px;}")
            .arg(ui::colors::BG_RAISED(), ui::colors::AMBER()));
    auto* warn_layout = new QHBoxLayout(warn_frame);
    warn_layout->setContentsMargins(10, 7, 10, 7);
    warn_layout->setSpacing(8);

    auto* warn_icon = new QLabel("⚠", this);
    warn_icon->setStyleSheet(
        QString("color:%1;font-size:14px;background:transparent;font-weight:bold;")
            .arg(ui::colors::AMBER()));
    warn_layout->addWidget(warn_icon);

    auto* warn_text = new QLabel(
        "<b>Upgrading packages may break the terminal.</b> "
        "Only proceed if you know what you are doing. "
        "Incompatible version changes can cause analytics scripts to crash or produce incorrect results.",
        this);
    warn_text->setWordWrap(true);
    warn_text->setStyleSheet(
        QString("color:%1;background:transparent;font-size:11px;")
            .arg(ui::colors::AMBER()));
    warn_layout->addWidget(warn_text, 1);

    root->addWidget(warn_frame);
    root->addSpacing(14);

    // ── Toolbar row ───────────────────────────────────────────────────────────
    auto* toolbar = new QWidget(this);
    auto* thl = new QHBoxLayout(toolbar);
    thl->setContentsMargins(0, 0, 0, 0);
    thl->setSpacing(8);

    search_input_ = new QLineEdit(this);
    search_input_->setPlaceholderText("Filter packages...");
    search_input_->setFixedWidth(200);
    search_input_->setStyleSheet(input_ss());
    thl->addWidget(search_input_);

    venv_filter_ = new QComboBox(this);
    venv_filter_->addItems({"All", "Trading", "Analytics"});
    venv_filter_->setStyleSheet(combo_ss());
    thl->addWidget(venv_filter_);

    thl->addStretch();

    refresh_btn_ = new QPushButton("Refresh", this);
    refresh_btn_->setStyleSheet(btn_secondary_ss());
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    thl->addWidget(refresh_btn_);

    install_missing_btn_ = new QPushButton("Install Missing", this);
    install_missing_btn_->setStyleSheet(btn_secondary_ss());
    install_missing_btn_->setCursor(Qt::PointingHandCursor);
    thl->addWidget(install_missing_btn_);

    upgrade_all_btn_ = new QPushButton("Upgrade All", this);
    upgrade_all_btn_->setStyleSheet(btn_secondary_ss());
    upgrade_all_btn_->setCursor(Qt::PointingHandCursor);
    thl->addWidget(upgrade_all_btn_);

    root->addWidget(toolbar);
    root->addSpacing(10);

    // ── Package table ─────────────────────────────────────────────────────────
    pkg_table_ = new QTableWidget(0, 7, this);
    pkg_table_->setHorizontalHeaderLabels(
        {"", "Package", "Venv", "Required", "Installed", "Status", "Action"});
    pkg_table_->setStyleSheet(table_ss());
    pkg_table_->setSelectionMode(QAbstractItemView::NoSelection);
    pkg_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    pkg_table_->setAlternatingRowColors(false);
    pkg_table_->verticalHeader()->setVisible(false);
    pkg_table_->horizontalHeader()->setStretchLastSection(false);
    pkg_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    pkg_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    pkg_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    pkg_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    pkg_table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    pkg_table_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    pkg_table_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Fixed);
    pkg_table_->setColumnWidth(0, 30);   // checkbox
    pkg_table_->setColumnWidth(2, 90);   // venv
    pkg_table_->setColumnWidth(5, 76);   // status
    pkg_table_->setColumnWidth(6, 80);   // action button
    pkg_table_->setShowGrid(true);
    root->addWidget(pkg_table_, 1);
    root->addSpacing(8);

    // ── Bottom bar: batch button + status ─────────────────────────────────────
    auto* bottom = new QWidget(this);
    auto* bhl = new QHBoxLayout(bottom);
    bhl->setContentsMargins(0, 0, 0, 0);
    bhl->setSpacing(10);

    batch_action_btn_ = new QPushButton("Install / Upgrade Selected", this);
    batch_action_btn_->setStyleSheet(btn_primary_ss());
    batch_action_btn_->setCursor(Qt::PointingHandCursor);
    bhl->addWidget(batch_action_btn_);
    bhl->addStretch();

    status_lbl_ = new QLabel(this);
    status_lbl_->setStyleSheet(label_ss());
    bhl->addWidget(status_lbl_);

    root->addWidget(bottom);
    root->addSpacing(8);

    // ── Progress bar + log line ───────────────────────────────────────────────
    install_bar_ = new QProgressBar(this);
    install_bar_->setRange(0, 100);
    install_bar_->setValue(0);
    install_bar_->setTextVisible(false);
    install_bar_->setFixedHeight(6);
    install_bar_->setStyleSheet(progress_ss());
    install_bar_->setVisible(false);
    root->addWidget(install_bar_);

    install_log_ = new QLabel(this);
    install_log_->setStyleSheet(
        QString("color:%1;background:transparent;font-size:10px;")
            .arg(ui::colors::TEXT_TERTIARY()));
    install_log_->setVisible(false);
    root->addWidget(install_log_);

    // ── Signal connections ────────────────────────────────────────────────────
    connect(refresh_btn_, &QPushButton::clicked, this, &PythonEnvSection::load_packages);

    connect(search_input_, &QLineEdit::textChanged, this, &PythonEnvSection::apply_filter);
    connect(venv_filter_,  qOverload<int>(&QComboBox::currentIndexChanged),
            this, &PythonEnvSection::apply_filter);

    connect(install_missing_btn_, &QPushButton::clicked, this, [this]() {
        ActionBatch b1, b2;
        b1.venv    = "venv-numpy1";
        b1.upgrade = false;
        b2.venv    = "venv-numpy2";
        b2.upgrade = false;
        for (const auto& row : std::as_const(all_packages_)) {
            if (row.missing) {
                if (row.venv == "venv-numpy1")
                    b1.packages << row.required_spec;
                else
                    b2.packages << row.required_spec;
            }
        }
        QList<ActionBatch> q;
        if (!b1.packages.isEmpty()) q << b1;
        if (!b2.packages.isEmpty()) q << b2;
        if (!q.isEmpty()) start_action(q);
    });

    connect(upgrade_all_btn_, &QPushButton::clicked, this, [this]() {
        ActionBatch b1, b2;
        b1.venv    = "venv-numpy1";
        b1.upgrade = true;
        b2.venv    = "venv-numpy2";
        b2.upgrade = true;
        for (const auto& row : std::as_const(all_packages_)) {
            if (!row.missing) {
                if (row.venv == "venv-numpy1")
                    b1.packages << row.required_spec;
                else
                    b2.packages << row.required_spec;
            }
        }
        QList<ActionBatch> q;
        if (!b1.packages.isEmpty()) q << b1;
        if (!b2.packages.isEmpty()) q << b2;
        if (!q.isEmpty()) start_action(q);
    });

    connect(batch_action_btn_, &QPushButton::clicked, this, [this]() {
        auto batches = build_batches_for_selected();
        if (!batches.isEmpty())
            start_action(batches);
    });
}

// ── showEvent ─────────────────────────────────────────────────────────────────

void PythonEnvSection::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (all_packages_.isEmpty())
        load_packages();
}

void PythonEnvSection::reload() {
    load_packages();
}

// ── Requirements file resolution ──────────────────────────────────────────────
// Mirrors PythonSetupManager::find_requirements_file() — walks up from exe dir.

QString PythonEnvSection::find_req_file(const QString& filename) const {
    QString exe_dir = QCoreApplication::applicationDirPath();

    // Direct candidate beside the exe
    QStringList candidates = {
        exe_dir + "/resources/" + filename,
        exe_dir + "/../resources/" + filename,
        exe_dir + "/../../resources/" + filename,
    };

    // Walk up from exe dir
    QDir dir(exe_dir);
    while (dir.cdUp()) {
        QString c = dir.filePath("resources/" + filename);
        if (QFileInfo::exists(c))
            return QDir::cleanPath(c);
        if (dir.isRoot())
            break;
    }

    for (const auto& c : candidates) {
        if (QFileInfo::exists(c))
            return QDir::cleanPath(c);
    }

    LOG_WARN("PythonEnv", "Requirements file not found: " + filename);
    return {};
}

// ── parse_requirements ────────────────────────────────────────────────────────
// Reads one requirements file, appends PackageRow entries to all_packages_.
// Mirrors PythonSetupManager::read_packages_from_file() parsing rules.

void PythonEnvSection::parse_requirements(const QString& req_file,
                                          const QString& venv_name,
                                          const QString& venv_label) {
    QString path = find_req_file(req_file);
    if (path.isEmpty()) {
        LOG_ERROR("PythonEnv", "Cannot load requirements: " + req_file);
        return;
    }

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        LOG_ERROR("PythonEnv", "Cannot open: " + path);
        return;
    }

    // Version specifier regex — strips ">=1.2,<3.0" and extras "[torch]" from name
    static const QRegularExpression kVerRe(R"([><=!~\s\[].*)");

    while (!f.atEnd()) {
        QString line = QString::fromUtf8(f.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith('#') || line.startsWith('-'))
            continue;
        int comment = line.indexOf('#');
        if (comment > 0)
            line = line.left(comment).trimmed();
        if (line.isEmpty())
            continue;

        PackageRow row;
        row.required_spec = line;
        row.venv          = venv_name;
        row.venv_label    = venv_label;

        QString base = line;
        base.remove(kVerRe);
        row.display_name = base.trimmed();
        row.name         = canonicalise(row.display_name);
        row.missing      = true;

        if (!row.name.isEmpty())
            all_packages_ << row;
    }
}

// ── load_packages ─────────────────────────────────────────────────────────────

void PythonEnvSection::load_packages() {
    using python::PythonSetupManager;

    if (!QFileInfo::exists(PythonSetupManager::instance().uv_path())) {
        show_status("Python environment not set up — run Setup first", true);
        return;
    }

    all_packages_.clear();
    installed_v1_.clear();
    installed_v2_.clear();
    v1_loaded_ = false;
    v2_loaded_ = false;
    pkg_table_->setRowCount(0);
    show_status("Loading...");

    parse_requirements("requirements-numpy1.txt", "venv-numpy1", "Trading");
    parse_requirements("requirements-numpy2.txt", "venv-numpy2", "Analytics");

    LOG_INFO("PythonEnv",
             QString("Parsed %1 packages from requirements files").arg(all_packages_.size()));

    start_list_venv("venv-numpy1");
}

// ── start_list_venv ───────────────────────────────────────────────────────────

void PythonEnvSection::start_list_venv(const QString& venv_name) {
    using python::PythonSetupManager;
    auto& mgr = PythonSetupManager::instance();

    QString python = mgr.python_path(venv_name);
    if (!QFileInfo::exists(python)) {
        LOG_WARN("PythonEnv", "Venv python not found: " + python);
        on_list_finished(venv_name, -1);
        return;
    }

    list_stdout_buf_.clear();

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("UV_PYTHON_INSTALL_DIR", mgr.install_dir() + "/python");
    list_proc_->setProcessEnvironment(env);

    list_proc_->disconnect();

    connect(list_proc_, &QProcess::readyReadStandardOutput, this, [this]() {
        list_stdout_buf_ += list_proc_->readAllStandardOutput();
    });

    connect(list_proc_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this, venv_name](int exit_code, QProcess::ExitStatus) {
                on_list_finished(venv_name, exit_code);
            });

    list_proc_->start(mgr.uv_path(), {"pip", "list", "--python", python});
    LOG_DEBUG("PythonEnv", "Started uv pip list for " + venv_name);
}

// ── on_list_finished ──────────────────────────────────────────────────────────

void PythonEnvSection::on_list_finished(const QString& venv_name, int exit_code) {
    QMap<QString, QString>& target = (venv_name == "venv-numpy1") ? installed_v1_ : installed_v2_;

    if (exit_code == 0) {
        const QString output = QString::fromUtf8(list_stdout_buf_);
        for (const QString& line : output.split('\n', Qt::SkipEmptyParts)) {
            QStringList parts = line.split(' ', Qt::SkipEmptyParts);
            if (parts.size() < 2)
                continue;
            QString pkg_name = canonicalise(parts.value(0));
            QString version  = parts.value(1);
            if (!pkg_name.isEmpty() && !version.isEmpty())
                target.insert(pkg_name, version);
        }
        LOG_INFO("PythonEnv",
                 QString("[%1] parsed %2 installed packages").arg(venv_name).arg(target.size()));
    } else {
        LOG_WARN("PythonEnv",
                 QString("[%1] uv pip list failed (exit=%2) — treating as empty")
                     .arg(venv_name).arg(exit_code));
    }

    list_stdout_buf_.clear();

    if (venv_name == "venv-numpy1") {
        v1_loaded_ = true;
        start_list_venv("venv-numpy2");
    } else {
        v2_loaded_ = true;
        merge_and_populate_table();
    }
}

// ── merge_and_populate_table ──────────────────────────────────────────────────

void PythonEnvSection::merge_and_populate_table() {
    for (auto& row : all_packages_) {
        const QMap<QString, QString>& installed_map =
            (row.venv == "venv-numpy1") ? installed_v1_ : installed_v2_;

        auto it = installed_map.find(row.name);
        if (it != installed_map.end()) {
            row.installed_ver = it.value();
            row.missing       = false;
        } else {
            row.installed_ver.clear();
            row.missing = true;
        }
    }

    int total   = all_packages_.size();
    int missing = 0;
    for (const auto& row : std::as_const(all_packages_))
        if (row.missing) ++missing;

    pkg_table_->setUpdatesEnabled(false);
    pkg_table_->setRowCount(0);

    for (int i = 0; i < all_packages_.size(); ++i) {
        const PackageRow& row = all_packages_[i];

        pkg_table_->insertRow(i);
        pkg_table_->setRowHeight(i, 28);

        // Col 0: checkbox
        auto* chk = new QCheckBox(this);
        chk->setStyleSheet(
            QString("QCheckBox{background:transparent;}"
                    "QCheckBox::indicator{width:13px;height:13px;}"
                    "QCheckBox::indicator:unchecked{border:1px solid %1;background:%2;}"
                    "QCheckBox::indicator:checked{border:1px solid %3;background:%3;}")
                .arg(ui::colors::BORDER_BRIGHT(), ui::colors::BG_RAISED(), ui::colors::AMBER()));
        auto* chk_cell = new QWidget(this);
        auto* chk_hl   = new QHBoxLayout(chk_cell);
        chk_hl->setContentsMargins(4, 0, 4, 0);
        chk_hl->setAlignment(Qt::AlignCenter);
        chk_hl->addWidget(chk);
        pkg_table_->setCellWidget(i, 0, chk_cell);

        // Col 1: package name
        auto* name_item = new QTableWidgetItem(row.display_name);
        name_item->setForeground(QColor(ui::colors::TEXT_PRIMARY()));
        pkg_table_->setItem(i, 1, name_item);

        // Col 2: venv label — Trading=blue, Analytics=amber so they're visually distinct
        auto* venv_item = new QTableWidgetItem(row.venv_label);
        venv_item->setForeground(QColor(
            row.venv == "venv-numpy1" ? "#38bdf8" : ui::colors::AMBER()));
        venv_item->setTextAlignment(Qt::AlignCenter);
        QFont venv_font = venv_item->font();
        venv_font.setBold(true);
        venv_item->setFont(venv_font);
        pkg_table_->setItem(i, 2, venv_item);

        // Col 3: required spec
        auto* req_item = new QTableWidgetItem(row.required_spec);
        req_item->setForeground(QColor(ui::colors::TEXT_SECONDARY()));
        pkg_table_->setItem(i, 3, req_item);

        // Col 4: installed version
        auto* inst_item = new QTableWidgetItem(row.missing ? QString("—") : row.installed_ver);
        inst_item->setForeground(QColor(row.missing ? ui::colors::TEXT_TERTIARY() : ui::colors::TEXT_PRIMARY()));
        inst_item->setTextAlignment(Qt::AlignCenter);
        pkg_table_->setItem(i, 4, inst_item);

        // Col 5: status badge
        auto* status_item = new QTableWidgetItem(row.missing ? "MISSING" : "OK");
        status_item->setForeground(QColor(row.missing ? ui::colors::NEGATIVE() : ui::colors::POSITIVE()));
        status_item->setTextAlignment(Qt::AlignCenter);
        pkg_table_->setItem(i, 5, status_item);

        // Col 6: action button
        auto* btn = new QPushButton(row.missing ? "Install" : "Upgrade", this);
        btn->setFixedHeight(22);
        btn->setStyleSheet(
            row.missing
                ? QString("QPushButton{background:%1;color:%2;border:none;font-size:10px;"
                          "font-weight:700;}"
                          "QPushButton:hover{background:%3;}")
                      .arg(ui::colors::AMBER(), ui::colors::BG_BASE(), ui::colors::AMBER_DIM())
                : QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                          "font-size:10px;}"
                          "QPushButton:hover{background:%3;}")
                      .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM(), ui::colors::BG_RAISED()));
        btn->setCursor(Qt::PointingHandCursor);
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_row_action_clicked(i); });
        pkg_table_->setCellWidget(i, 6, btn);
    }

    pkg_table_->setUpdatesEnabled(true);

    show_status(
        QString("%1 packages — %2 missing").arg(total).arg(missing),
        missing > 0);

    LOG_INFO("PythonEnv",
             QString("Table populated: %1 total, %2 missing").arg(total).arg(missing));

    apply_filter();
}

// ── on_row_action_clicked ────────────────────────────────────────────────────

void PythonEnvSection::on_row_action_clicked(int row) {
    if (row < 0 || row >= all_packages_.size())
        return;
    const PackageRow& pkg = all_packages_[row];

    ActionBatch batch;
    batch.venv     = pkg.venv;
    batch.packages = {pkg.required_spec};
    batch.upgrade  = !pkg.missing;

    start_action({batch});
}

// ── build_batches_for_selected ────────────────────────────────────────────────

QList<PythonEnvSection::ActionBatch> PythonEnvSection::build_batches_for_selected() const {
    ActionBatch b1, b2;
    b1.venv    = "venv-numpy1";
    b1.upgrade = false;
    b2.venv    = "venv-numpy2";
    b2.upgrade = false;

    for (int i = 0; i < pkg_table_->rowCount(); ++i) {
        if (pkg_table_->isRowHidden(i))
            continue;
        auto* cell = pkg_table_->cellWidget(i, 0);
        if (!cell)
            continue;
        auto* chk = cell->findChild<QCheckBox*>();
        if (!chk || !chk->isChecked())
            continue;

        if (i >= all_packages_.size())
            continue;
        const PackageRow& row = all_packages_[i];

        if (row.venv == "venv-numpy1") {
            b1.packages << row.required_spec;
            b1.upgrade  = !row.missing;
        } else {
            b2.packages << row.required_spec;
            b2.upgrade  = !row.missing;
        }
    }

    QList<ActionBatch> result;
    if (!b1.packages.isEmpty()) result << b1;
    if (!b2.packages.isEmpty()) result << b2;
    return result;
}

// ── start_action ──────────────────────────────────────────────────────────────

void PythonEnvSection::start_action(const QList<ActionBatch>& batches) {
    if (batches.isEmpty())
        return;

    action_queue_ = batches;
    set_actions_enabled(false);
    install_bar_->setValue(0);
    install_bar_->setVisible(true);
    install_log_->setVisible(true);
    install_log_->setText("Starting...");

    run_next_batch();
}

// ── run_next_batch ────────────────────────────────────────────────────────────

void PythonEnvSection::run_next_batch() {
    using python::PythonSetupManager;
    auto& mgr = PythonSetupManager::instance();

    if (action_queue_.isEmpty()) {
        on_action_finished(0);
        return;
    }

    ActionBatch batch = action_queue_.takeFirst();
    QString venv_python = mgr.python_path(batch.venv);

    QStringList args = {"pip", "install", "--python", venv_python};
    if (batch.upgrade)
        args << "--upgrade";
    args << batch.packages;

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("UV_PYTHON_INSTALL_DIR", mgr.install_dir() + "/python");
    env.insert("PEEWEE_NO_SQLITE_EXTENSIONS", "1");
    env.insert("PEEWEE_NO_C_EXTENSION", "1");
    action_proc_->setProcessEnvironment(env);

    action_stdout_buf_.clear();
    action_proc_->disconnect();

    connect(action_proc_, &QProcess::readyReadStandardOutput, this, [this]() {
        action_stdout_buf_ += action_proc_->readAllStandardOutput();
        const QString out = QString::fromUtf8(action_stdout_buf_);
        const QStringList lines = out.split('\n', Qt::SkipEmptyParts);
        if (!lines.isEmpty())
            install_log_->setText(lines.last().trimmed().left(120));
    });

    connect(action_proc_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this](int exit_code, QProcess::ExitStatus) {
                action_stdout_buf_.clear();
                if (!action_queue_.isEmpty()) {
                    run_next_batch();
                } else {
                    on_action_finished(exit_code);
                }
            });

    LOG_INFO("PythonEnv",
             QString("uv pip install: venv=%1  upgrade=%2  packages=%3")
                 .arg(batch.venv).arg(batch.upgrade).arg(batch.packages.join(", ").left(200)));

    install_log_->setText(
        QString("Installing into %1...").arg(batch.venv == "venv-numpy1" ? "Trading" : "Analytics"));
    install_bar_->setValue(10);

    action_proc_->start(mgr.uv_path(), args);
}

// ── on_action_finished ────────────────────────────────────────────────────────

void PythonEnvSection::on_action_finished(int exit_code) {
    install_bar_->setVisible(false);
    install_log_->setVisible(false);
    set_actions_enabled(true);

    if (exit_code == 0) {
        show_status("Install complete — refreshing...");
        LOG_INFO("PythonEnv", "Install/upgrade finished successfully");
    } else {
        show_status(QString("Install finished with errors (exit %1)").arg(exit_code), true);
        LOG_WARN("PythonEnv", QString("Install finished with exit code %1").arg(exit_code));
    }

    load_packages();
}

// ── apply_filter ──────────────────────────────────────────────────────────────

void PythonEnvSection::apply_filter() {
    const QString search   = search_input_->text().trimmed().toLower();
    const int     venv_idx = venv_filter_->currentIndex(); // 0=All, 1=Trading, 2=Analytics

    for (int i = 0; i < pkg_table_->rowCount(); ++i) {
        if (i >= all_packages_.size()) {
            pkg_table_->setRowHidden(i, false);
            continue;
        }
        const PackageRow& row = all_packages_[i];

        bool venv_match = true;
        if (venv_idx == 1) venv_match = (row.venv == "venv-numpy1");
        if (venv_idx == 2) venv_match = (row.venv == "venv-numpy2");

        bool name_match = search.isEmpty() ||
                          row.display_name.toLower().contains(search);

        pkg_table_->setRowHidden(i, !(venv_match && name_match));
    }
}

// ── set_actions_enabled ───────────────────────────────────────────────────────

void PythonEnvSection::set_actions_enabled(bool enabled) {
    refresh_btn_->setEnabled(enabled);
    install_missing_btn_->setEnabled(enabled);
    upgrade_all_btn_->setEnabled(enabled);
    batch_action_btn_->setEnabled(enabled);

    for (int i = 0; i < pkg_table_->rowCount(); ++i) {
        auto* w = pkg_table_->cellWidget(i, 6);
        if (w) {
            auto* btn = qobject_cast<QPushButton*>(w);
            if (!btn) btn = w->findChild<QPushButton*>();
            if (btn) btn->setEnabled(enabled);
        }
    }
}

// ── show_status ───────────────────────────────────────────────────────────────

void PythonEnvSection::show_status(const QString& msg, bool error) {
    status_lbl_->setText(msg);
    status_lbl_->setStyleSheet(
        QString("color:%1;background:transparent;")
            .arg(error ? ui::colors::NEGATIVE() : ui::colors::TEXT_SECONDARY()));
}

} // namespace fincept::screens
