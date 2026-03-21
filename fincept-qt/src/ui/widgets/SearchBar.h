#pragma once
#include <QWidget>
#include <QLineEdit>
#include <QHBoxLayout>

namespace fincept::ui {

/// Bloomberg-style search/command input bar.
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
