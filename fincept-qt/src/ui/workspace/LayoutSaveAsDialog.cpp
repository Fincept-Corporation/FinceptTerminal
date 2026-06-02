#include "ui/workspace/LayoutSaveAsDialog.h"

#include <QDialogButtonBox>
#include <QEvent>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

namespace fincept::ui {

LayoutSaveAsDialog::LayoutSaveAsDialog(QWidget* parent, const QString& initial_name)
    : QDialog(parent) {
    setWindowTitle(tr("Save Layout As"));
    setMinimumWidth(360);

    auto* vl = new QVBoxLayout(this);
    name_label_ = new QLabel(tr("Layout name:"));
    vl->addWidget(name_label_);

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

void LayoutSaveAsDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void LayoutSaveAsDialog::retranslateUi() {
    setWindowTitle(tr("Save Layout As"));
    if (name_label_) name_label_->setText(tr("Layout name:"));
}

} // namespace fincept::ui
