#include "screens/file_manager/FileManagerScreen.h"

#include "core/logging/Logger.h"
#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QMessageBox>
#include <QMimeDatabase>
#include <QScrollArea>
#include <QShowEvent>
#include <QStandardPaths>
#include <QUuid>

namespace fincept::screens {

using namespace fincept::ui;

static const char* MF = "font-family:'Consolas','Courier New',monospace;";

static const char* PANEL = "background: #0a0a0a; border: 1px solid #1a1a1a; border-radius: 2px;";

// ── Constructor ──────────────────────────────────────────────────────────────

FileManagerScreen::FileManagerScreen(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void FileManagerScreen::build_ui() {
    setStyleSheet(QString("QWidget#FileManagerRoot { background: %1; }").arg(colors::BG_BASE));
    setObjectName("FileManagerRoot");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header bar ───────────────────────────────────────────────────────────
    auto* header = new QWidget;
    header->setStyleSheet(
        QString("background: %1; border-bottom: 1px solid %2;").arg(colors::BG_RAISED, colors::BORDER_DIM));
    auto* hhl = new QHBoxLayout(header);
    hhl->setContentsMargins(14, 10, 14, 10);

    auto* title = new QLabel("FILE MANAGER");
    title->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: 700; "
                                 "letter-spacing: 0.5px; %2")
                             .arg(colors::AMBER, MF));
    hhl->addWidget(title);

    auto* sub = new QLabel("Manage your local files");
    sub->setStyleSheet(QString("color: %1; font-size: 12px; %2").arg(colors::TEXT_TERTIARY, MF));
    hhl->addWidget(sub);

    hhl->addStretch();

    stats_label_ = new QLabel("0 files | 0 B");
    stats_label_->setStyleSheet(QString("color: %1; font-size: 11px; %2").arg(colors::TEXT_DIM, MF));
    hhl->addWidget(stats_label_);

    refresh_btn_ = new QPushButton("REFRESH");
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    refresh_btn_->setStyleSheet(QString("QPushButton { background: transparent; color: %1; border: 1px solid %2; "
                                        "padding: 4px 12px; font-size: 11px; %3 }"
                                        "QPushButton:hover { color: #e5e5e5; background: %4; }")
                                    .arg(colors::TEXT_SECONDARY, colors::BORDER_DIM, MF, colors::BG_HOVER));
    connect(refresh_btn_, &QPushButton::clicked, this, &FileManagerScreen::load_files);
    hhl->addWidget(refresh_btn_);

    upload_btn_ = new QPushButton("UPLOAD FILES");
    upload_btn_->setCursor(Qt::PointingHandCursor);
    upload_btn_->setStyleSheet(
        QString("QPushButton { background: rgba(217,119,6,0.1); color: %1; border: 1px solid #78350f; "
                "padding: 4px 12px; font-size: 11px; font-weight: 700; %2 }"
                "QPushButton:hover { background: %1; color: #080808; }")
            .arg(colors::AMBER, MF));
    connect(upload_btn_, &QPushButton::clicked, this, &FileManagerScreen::upload_files);
    hhl->addWidget(upload_btn_);

    root->addWidget(header);

    // ── Search bar ───────────────────────────────────────────────────────────
    auto* search_bar = new QWidget;
    search_bar->setStyleSheet(QString("background: %1;").arg(colors::BG_BASE));
    auto* shl = new QHBoxLayout(search_bar);
    shl->setContentsMargins(14, 8, 14, 8);

    search_input_ = new QLineEdit;
    search_input_->setPlaceholderText("Search files by name or type...");
    search_input_->setStyleSheet(
        QString("QLineEdit { background: %1; color: %2; border: 1px solid %3; "
                "padding: 6px 10px; font-size: 12px; %4 }"
                "QLineEdit:focus { border-color: %5; }")
            .arg(colors::BG_SURFACE, colors::TEXT_PRIMARY, colors::BORDER_DIM, MF, colors::AMBER));
    connect(search_input_, &QLineEdit::textChanged, this, [this]() { render_files(); });
    shl->addWidget(search_input_);

    root->addWidget(search_bar);

    // ── Scrollable file grid ─────────────────────────────────────────────────
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border: none; background: transparent; }"
                          "QScrollBar:vertical { background: transparent; width: 6px; }"
                          "QScrollBar::handle:vertical { background: #222222; }"
                          "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");

    file_container_ = new QWidget;
    file_container_->setStyleSheet(QString("background: %1;").arg(colors::BG_BASE));
    file_layout_ = new QVBoxLayout(file_container_);
    file_layout_->setContentsMargins(14, 8, 14, 8);
    file_layout_->setSpacing(6);
    file_layout_->addStretch();

    scroll->setWidget(file_container_);
    root->addWidget(scroll, 1);
}

// ── Show event ───────────────────────────────────────────────────────────────

void FileManagerScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!loaded_) {
        load_files();
    }
}

// ── Storage helpers ──────────────────────────────────────────────────────────

QString FileManagerScreen::storage_dir() const {
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return base + "/fincept-files";
}

QString FileManagerScreen::metadata_path() const {
    return storage_dir() + "/metadata.json";
}

QJsonArray FileManagerScreen::read_metadata() const {
    QFile file(metadata_path());
    if (!file.exists())
        return {};
    if (!file.open(QIODevice::ReadOnly))
        return {};

    auto doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (doc.isObject() && doc.object().contains("files"))
        return doc.object()["files"].toArray();
    return {};
}

void FileManagerScreen::write_metadata(const QJsonArray& files) {
    QDir().mkpath(storage_dir());

    QJsonObject root;
    root["files"] = files;

    QFile file(metadata_path());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
        file.close();
    }
}

QJsonObject FileManagerScreen::find_file(const QString& id) const {
    for (const auto& f : files_) {
        if (f.isObject() && f.toObject()["id"].toString() == id)
            return f.toObject();
    }
    return {};
}

// ── File operations ──────────────────────────────────────────────────────────

void FileManagerScreen::load_files() {
    loaded_ = true;
    QDir().mkpath(storage_dir());
    files_ = read_metadata();
    render_files();
    update_stats();
}

void FileManagerScreen::upload_files() {
    QStringList paths = QFileDialog::getOpenFileNames(this, "Select Files to Upload");
    if (paths.isEmpty())
        return;

    QMimeDatabase mime_db;
    QJsonArray files = read_metadata();

    for (const QString& path : paths) {
        QFileInfo info(path);
        QString id = QString::number(QDateTime::currentMSecsSinceEpoch()) + "_" +
                     QUuid::createUuid().toString(QUuid::Id128).left(8);
        QString ext = info.suffix();
        QString stored_name = id + (ext.isEmpty() ? "" : "." + ext);

        // Copy file to storage directory
        QString dest = storage_dir() + "/" + stored_name;
        if (!QFile::copy(path, dest)) {
            LOG_ERROR("FileManager", "Failed to copy: " + path);
            continue;
        }

        QJsonObject entry;
        entry["id"] = id;
        entry["name"] = stored_name;
        entry["originalName"] = info.fileName();
        entry["size"] = info.size();
        entry["type"] = mime_db.mimeTypeForFile(path).name();
        entry["uploadedAt"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        entry["path"] = "fincept-files/" + stored_name;

        files.append(entry);
    }

    write_metadata(files);
    load_files();
    LOG_INFO("FileManager", QString("Uploaded %1 file(s)").arg(paths.size()));
}

void FileManagerScreen::download_file(const QString& file_id) {
    QJsonObject file = find_file(file_id);
    if (file.isEmpty())
        return;

    QString original = file["originalName"].toString();
    QString src = storage_dir() + "/" + file["name"].toString();

    QString dest = QFileDialog::getSaveFileName(this, "Save File As", original);
    if (dest.isEmpty())
        return;

    if (QFile::exists(dest))
        QFile::remove(dest);

    if (QFile::copy(src, dest)) {
        LOG_INFO("FileManager", "Downloaded: " + original);
    } else {
        QMessageBox::warning(this, "Download Failed", "Could not save file to selected location.");
    }
}

void FileManagerScreen::delete_file(const QString& file_id) {
    QJsonObject file = find_file(file_id);
    if (file.isEmpty())
        return;

    auto reply = QMessageBox::question(
        this, "Delete File", QString("Delete \"%1\"? This cannot be undone.").arg(file["originalName"].toString()),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    // Remove physical file
    QString src = storage_dir() + "/" + file["name"].toString();
    QFile::remove(src);

    // Remove from metadata
    QJsonArray updated;
    for (const auto& f : files_) {
        if (f.isObject() && f.toObject()["id"].toString() != file_id)
            updated.append(f);
    }
    write_metadata(updated);
    load_files();

    LOG_INFO("FileManager", "Deleted: " + file["originalName"].toString());
}

// ── Rendering ────────────────────────────────────────────────────────────────

void FileManagerScreen::render_files() {
    // Clear existing
    QLayoutItem* item;
    while ((item = file_layout_->takeAt(0)) != nullptr) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    QString filter = search_input_ ? search_input_->text().toLower() : QString();

    int shown = 0;
    for (const auto& f : files_) {
        if (!f.isObject())
            continue;
        QJsonObject file = f.toObject();

        QString name = file["originalName"].toString();
        QString type = file["type"].toString();

        // Filter
        if (!filter.isEmpty() && !name.toLower().contains(filter) && !type.toLower().contains(filter))
            continue;

        shown++;

        // File row card
        auto* card = new QWidget;
        card->setStyleSheet(QString("%1").arg(PANEL));
        auto* hl = new QHBoxLayout(card);
        hl->setContentsMargins(12, 8, 12, 8);
        hl->setSpacing(10);

        // Type indicator
        QString type_color = file_type_color(type);
        auto* type_dot = new QLabel("[" + type.section('/', 1, 1).left(4).toUpper() + "]");
        type_dot->setFixedWidth(50);
        type_dot->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: 700; "
                                        "background: transparent; %2")
                                    .arg(type_color, MF));
        hl->addWidget(type_dot);

        // Name + details
        auto* info = new QWidget;
        info->setStyleSheet("background: transparent;");
        auto* info_vl = new QVBoxLayout(info);
        info_vl->setContentsMargins(0, 0, 0, 0);
        info_vl->setSpacing(1);

        auto* name_lbl = new QLabel(name);
        name_lbl->setStyleSheet(QString("color: #e5e5e5; font-size: 12px; font-weight: 600; "
                                        "background: transparent; %1")
                                    .arg(MF));
        info_vl->addWidget(name_lbl);

        auto* meta_lbl = new QLabel(format_size(file["size"].toInteger()) + "  |  " +
                                    QDateTime::fromString(file["uploadedAt"].toString(), Qt::ISODate)
                                        .toLocalTime()
                                        .toString("yyyy-MM-dd HH:mm"));
        meta_lbl->setStyleSheet(
            QString("color: %1; font-size: 10px; background: transparent; %2").arg(colors::TEXT_DIM, MF));
        info_vl->addWidget(meta_lbl);

        hl->addWidget(info, 1);

        // Download button
        auto* dl_btn = new QPushButton("SAVE");
        dl_btn->setCursor(Qt::PointingHandCursor);
        dl_btn->setFixedSize(60, 26);
        dl_btn->setStyleSheet(QString("QPushButton { background: transparent; color: %1; border: 1px solid %2; "
                                      "font-size: 10px; font-weight: 700; %3 }"
                                      "QPushButton:hover { color: #38bdf8; background: #111111; }")
                                  .arg(colors::CYAN, colors::BORDER_DIM, MF));
        QString fid = file["id"].toString();
        connect(dl_btn, &QPushButton::clicked, this, [this, fid]() { download_file(fid); });
        hl->addWidget(dl_btn);

        // Delete button
        auto* del_btn = new QPushButton("DEL");
        del_btn->setCursor(Qt::PointingHandCursor);
        del_btn->setFixedSize(50, 26);
        del_btn->setStyleSheet(QString("QPushButton { background: transparent; color: %1; border: 1px solid %2; "
                                       "font-size: 10px; font-weight: 700; %3 }"
                                       "QPushButton:hover { color: #ef4444; background: rgba(239,68,68,0.1); }")
                                   .arg(colors::TEXT_DIM, colors::BORDER_DIM, MF));
        connect(del_btn, &QPushButton::clicked, this, [this, fid]() { delete_file(fid); });
        hl->addWidget(del_btn);

        file_layout_->addWidget(card);
    }

    // Empty state
    if (shown == 0) {
        auto* empty = new QLabel(files_.isEmpty() ? "No files yet. Click UPLOAD FILES to get started."
                                                  : "No files match your search.");
        empty->setAlignment(Qt::AlignCenter);
        empty->setStyleSheet(QString("color: %1; font-size: 13px; padding: 40px; %2").arg(colors::TEXT_DIM, MF));
        file_layout_->addWidget(empty);
    }

    file_layout_->addStretch();
}

void FileManagerScreen::update_stats() {
    qint64 total_size = 0;
    for (const auto& f : files_) {
        if (f.isObject())
            total_size += f.toObject()["size"].toInteger();
    }
    stats_label_->setText(QString("%1 files | %2").arg(files_.size()).arg(format_size(total_size)));
}

// ── Static helpers ───────────────────────────────────────────────────────────

QString FileManagerScreen::format_size(qint64 bytes) {
    if (bytes < 1024)
        return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024LL * 1024 * 1024)
        return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    return QString("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}

QString FileManagerScreen::file_type_color(const QString& mime) {
    if (mime.contains("spreadsheet") || mime.contains("csv") || mime.contains("excel"))
        return "#16a34a"; // green
    if (mime.contains("image"))
        return "#2563eb"; // blue
    if (mime.contains("video"))
        return "#7c3aed"; // purple
    if (mime.contains("audio"))
        return "#db2777"; // pink
    if (mime.contains("zip") || mime.contains("archive") || mime.contains("compressed"))
        return "#ca8a04"; // yellow
    if (mime.contains("pdf"))
        return "#dc2626"; // red
    return "#808080";     // gray default
}

} // namespace fincept::screens
