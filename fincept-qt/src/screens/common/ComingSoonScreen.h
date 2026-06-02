#pragma once
#include <QEvent>
#include <QLabel>
#include <QWidget>

namespace fincept::screens {

/// Placeholder screen for tabs not yet implemented.
class ComingSoonScreen : public QWidget {
    Q_OBJECT
  public:
    explicit ComingSoonScreen(const QString& tab_name, QWidget* parent = nullptr);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void retranslateUi();

    QLabel* sub_label_ = nullptr;
    QLabel* desc_label_ = nullptr;
};

} // namespace fincept::screens
