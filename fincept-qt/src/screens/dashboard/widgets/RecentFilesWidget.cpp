// src/screens/dashboard/widgets/RecentFilesWidget.cpp
#include "screens/dashboard/widgets/RecentFilesWidget.h"

#include "services/file_manager/FileManagerService.h"
#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QShowEvent>

namespace fincept::screens::widgets {

using namespace fincept::ui;
using fincept::services::FileManagerService;

static const char* MF = "font-family:'Consolas','Courier New',monospace;";

RecentFilesWidget::RecentFilesWidget(QWidget* parent) : BaseWidget("Recent Files", parent, ui::colors::AMBER.get()) {

    scroll_ = new QScrollArea;
    scroll_->setWidgetResizable(true);

    auto* container = new QWidget(this);
    container->setStyleSheet("background:transparent;");
    list_layout_ = new QVBoxLayout(container);
    list_layout_->setContentsMargins(4, 4, 4, 4);
    list_layout_->setSpacing(3);
    list_layout_->addStretch();
    scroll_->setWidget(container);

    content_layout()->addWidget(scroll_);

    // Refresh when service changes
    connect(&FileManagerService::instance(), &FileManagerService::files_changed, this,
            &RecentFilesWidget::refresh_data);
    connect(this, &BaseWidget::refresh_requested, this, &RecentFilesWidget::refresh_data);

    apply_styles();
}

void RecentFilesWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!loaded_) {
        loaded_ = true;
        refresh_data();
    }
}

void RecentFilesWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
}

void RecentFilesWidget::refresh_data() {
    while (QLayoutItem* item = list_layout_->takeAt(0)) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    QJsonArray all = FileManagerService::instance().all_files();

    // Sort by uploadedAt descending, take top 8
    QVector<QJsonObject> files;
    for (const auto& v : all)
        if (v.isObject())
            files.append(v.toObject());

    std::sort(files.begin(), files.end(), [](const QJsonObject& a, const QJsonObject& b) {
        return a["uploadedAt"].toString() > b["uploadedAt"].toString();
    });
    if (files.size() > 8)
        files.resize(8);

    if (files.isEmpty()) {
        auto* empty = new QLabel("No files yet");
        empty->setAlignment(Qt::AlignCenter);
        empty->setStyleSheet(
            QString("color:%1;font-size:11px;background:transparent;padding:16px;%2").arg(colors::TEXT_DIM, MF));
        list_layout_->addWidget(empty);
        list_layout_->addStretch();
        return;
    }

    for (const QJsonObject& f : files) {
        auto* row = new QWidget(this);
        row->setStyleSheet(QString("background:%1;border:1px solid %2;border-radius:2px;")
                               .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(8, 5, 8, 5);
        rl->setSpacing(6);

        // MIME badge
        QString mime = f["type"].toString();
        QString badge_text = mime.section('/', 1, 1).left(3).toUpper();
        auto* badge = new QLabel(badge_text);
        badge->setFixedWidth(28);
        badge->setAlignment(Qt::AlignCenter);
        badge->setStyleSheet(
            QString("color:%1;font-size:9px;font-weight:700;background:transparent;%2").arg(ui::colors::AMBER, MF));
        rl->addWidget(badge);

        // Name
        auto* name_lbl = new QLabel(f["originalName"].toString());
        name_lbl->setStyleSheet(
            QString("color:%1;font-size:11px;background:transparent;%2").arg(ui::colors::TEXT_PRIMARY, MF));
        name_lbl->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        rl->addWidget(name_lbl, 1);

        // Date
        QString date_str =
            QDateTime::fromString(f["uploadedAt"].toString(), Qt::ISODate).toLocalTime().toString("MM/dd HH:mm");
        auto* date_lbl = new QLabel(date_str);
        date_lbl->setStyleSheet(QString("color:%1;font-size:10px;background:transparent;%2").arg(colors::TEXT_DIM, MF));
        rl->addWidget(date_lbl);

        list_layout_->addWidget(row);
    }

    list_layout_->addStretch();
}

void RecentFilesWidget::apply_styles() {
    scroll_->setStyleSheet("QScrollArea{border:none;background:transparent;}"
                           "QScrollBar:vertical{background:transparent;width:4px;}" +
                           QString("QScrollBar::handle:vertical{background:%1;}").arg(ui::colors::BORDER_MED));

    // Rows are built dynamically in refresh_data() using current tokens,
    // so re-running refresh_data() picks up the new theme.
    if (loaded_)
        refresh_data();
}

void RecentFilesWidget::on_theme_changed() {
    apply_styles();
}

} // namespace fincept::screens::widgets
