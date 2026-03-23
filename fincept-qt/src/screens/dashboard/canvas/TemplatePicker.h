#pragma once
#include <QDialog>
#include <QString>

namespace fincept::screens {

/// Dialog showing 6 dashboard template cards with mini previews.
/// Emits template_selected(id) when user picks one and clicks APPLY.
class TemplatePicker : public QDialog {
    Q_OBJECT
  public:
    explicit TemplatePicker(QWidget* parent = nullptr);

  signals:
    void template_selected(const QString& template_id);
};

} // namespace fincept::screens
