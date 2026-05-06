#include "ui/workspace/LayoutSaveAsDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

namespace fincept::ui {

LayoutSaveAsDialog::LayoutSaveAsDialog(QWidget* parent, const QString& initial_name)
    : QDialog(parent) {
    setWindowTitle("Save Layout As");
    setMinimumWidth(360);

    auto* vl = new QVBoxLayout(this);
    vl->addWidget(new QLabel("Layout name:"));

    name_edit_ = new QLineEdit;
    name_edit_->setText(initial_name);
    name_edit_->selectAll();
    vl->addWidget(name_edit_);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    vl->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        if (!name_edit_->text().trimmed().isEmpty())
            accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(name_edit_, &QLineEdit::returnPressed, this, [buttons]() {
        emit buttons->accepted();
    });
}

QString LayoutSaveAsDialog::name() const {
    return name_edit_ ? name_edit_->text().trimmed() : QString();
}

} // namespace fincept::ui
