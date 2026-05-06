#pragma once
// LayoutSaveAsDialog — single text field for layout name. Used by toolbar
// "Save Layout" (when no layout loaded) and "Save Layout As…".

#include <QDialog>

class QLineEdit;

namespace fincept::ui {

class LayoutSaveAsDialog : public QDialog {
    Q_OBJECT
  public:
    explicit LayoutSaveAsDialog(QWidget* parent = nullptr,
                                const QString& initial_name = {});

    /// Trimmed name. Valid only after exec() returns Accepted.
    QString name() const;

  private:
    QLineEdit* name_edit_ = nullptr;
};

} // namespace fincept::ui
