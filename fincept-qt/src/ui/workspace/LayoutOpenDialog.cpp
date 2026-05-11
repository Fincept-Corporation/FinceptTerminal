#include "ui/workspace/LayoutOpenDialog.h"

#include "core/layout/LayoutCatalog.h"
#include "core/profile/ProfilePaths.h"

#include <QDateTime>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QIcon>
#include <QListWidget>
#include <QListWidgetItem>
#include <QSize>
#include <QVBoxLayout>

namespace fincept::ui {

LayoutOpenDialog::LayoutOpenDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Open Layout");
    setMinimumSize(560, 400);

    auto* vl = new QVBoxLayout(this);
    list_ = new QListWidget;
    vl->addWidget(list_, 1);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Open | QDialogButtonBox::Cancel, this);
    vl->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        if (auto* item = list_->currentItem()) {
            selected_id_ = LayoutId::from_string(item->data(Qt::UserRole).toString());
            accept();
        }
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(list_, &QListWidget::itemDoubleClicked, this,
            [buttons](QListWidgetItem*) { emit buttons->accepted(); });

    populate();
}

void LayoutOpenDialog::populate() {
    auto r = LayoutCatalog::instance().list_layouts();
    if (r.is_err()) return;

    // Phase L5: thumbnail icons next to each name.
    list_->setIconSize(QSize(64, 36));
    const QString layouts_dir = ProfilePaths::layouts_dir();
    for (const auto& e : r.value()) {
        const QString updated = QDateTime::fromSecsSinceEpoch(e.updated_at_unix)
                                    .toString("yyyy-MM-dd HH:mm");
        auto* item = new QListWidgetItem(QString("%1   [%2]   %3").arg(e.name, e.kind, updated));
        item->setData(Qt::UserRole, e.id.to_string());
        if (!e.thumbnail_path.isEmpty()) {
            const QString abs = layouts_dir + "/" + e.thumbnail_path;
            if (QFileInfo::exists(abs))
                item->setIcon(QIcon(abs));
        }
        list_->addItem(item);
    }
    if (list_->count() > 0) list_->setCurrentRow(0);
}

} // namespace fincept::ui
