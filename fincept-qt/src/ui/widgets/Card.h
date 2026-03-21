#pragma once
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace fincept::ui {

/// Reusable card widget — dark panel with title bar and content area.
class Card : public QFrame {
    Q_OBJECT
public:
    explicit Card(const QString& title, QWidget* parent = nullptr);

    QWidget* content_area() const { return content_; }
    QVBoxLayout* content_layout() const { return content_layout_; }
    void set_title(const QString& title);

signals:
    void close_requested();

private:
    QLabel*      title_label_   = nullptr;
    QWidget*     content_       = nullptr;
    QVBoxLayout* content_layout_= nullptr;
};

} // namespace fincept::ui
