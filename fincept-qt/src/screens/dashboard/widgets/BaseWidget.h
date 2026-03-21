#pragma once
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace fincept::screens::widgets {

/// Reusable widget container — title bar with accent color, close/refresh buttons, content area.
/// Matches the Tauri BaseWidget pattern.
class BaseWidget : public QFrame {
    Q_OBJECT
public:
    explicit BaseWidget(const QString& title, QWidget* parent = nullptr,
                        const QString& accent_color = {});

    QVBoxLayout* content_layout() const { return content_layout_; }

    void set_loading(bool loading);
    void set_error(const QString& error);
    void set_title(const QString& title);

signals:
    void close_requested();
    void refresh_requested();

private:
    QLabel*      title_label_   = nullptr;
    QLabel*      accent_bar_    = nullptr;
    QLabel*      loading_label_ = nullptr;
    QWidget*     content_       = nullptr;
    QVBoxLayout* content_layout_= nullptr;
    QPushButton* refresh_btn_   = nullptr;
    QString      accent_color_;
};

} // namespace fincept::screens::widgets
