#pragma once
// LayoutSaveAsDialog — single text field for layout name. Used by toolbar
// "Save Layout" (when no layout loaded) and "Save Layout As…".

#include <QDialog>
#include <QEvent>

class QLineEdit;
class QLabel;

namespace fincept::ui {

class LayoutSaveAsDialog : public QDialog {
    Q_OBJECT
  public:
    explicit LayoutSaveAsDialog(QWidget* parent = nullptr,
                                const QString& initial_name = {});

    /// Trimmed name. Valid only after exec() returns Accepted.
    QString name() const;

  protected:
    void changeEvent(QEvent* event) override;

  private:
    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();

    QLineEdit* name_edit_ = nullptr;
    QLabel* name_label_ = nullptr;
};

} // namespace fincept::ui
