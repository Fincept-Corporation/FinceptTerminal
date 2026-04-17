#pragma once
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

  private:
    QLineEdit* input_ = nullptr;
};

} // namespace fincept::ui
