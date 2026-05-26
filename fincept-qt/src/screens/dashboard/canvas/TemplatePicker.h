#pragma once
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QVector>

namespace fincept::screens {

/// Dialog showing 6 dashboard template cards with mini previews.
/// Emits template_selected(id) when user picks one and clicks APPLY.
class TemplatePicker : public QDialog {
    Q_OBJECT
  public:
    explicit TemplatePicker(QWidget* parent = nullptr);

  signals:
    void template_selected(const QString& template_id);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void retranslateUi();

    QLabel* title_lbl_ = nullptr;
    QLabel* sub_lbl_ = nullptr;
    QPushButton* cancel_btn_ = nullptr;

    struct CardRefs {
        QLabel* name = nullptr;
        QLabel* desc = nullptr;
        QPushButton* apply = nullptr;
        QString id;
    };
    QVector<CardRefs> cards_;
};

} // namespace fincept::screens
