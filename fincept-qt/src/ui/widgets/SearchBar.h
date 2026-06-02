#pragma once
#include <QEvent>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QWidget>

namespace fincept::ui {

/// Search/command input bar.
class SearchBar : public QWidget {
    Q_OBJECT
  public:
    explicit SearchBar(QWidget* parent = nullptr);
    QString text() const;
    void clear();

  signals:
    void search_submitted(const QString& query);
    void text_changed(const QString& text);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    /// Re-apply tr() lookups to the input placeholder after a language switch.
    void retranslateUi();

    QLineEdit* input_ = nullptr;
};

} // namespace fincept::ui
