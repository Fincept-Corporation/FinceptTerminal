#pragma once
#include <QHash>
#include <QObject>
#include <QStackedWidget>
#include <QWidget>

#include <functional>

namespace fincept {

/// Manages screen navigation using QStackedWidget.
/// Supports both eager (register_screen) and lazy (register_factory) registration.
/// Lazy screens are only constructed on first navigation — critical for startup performance.
class ScreenRouter : public QObject {
    Q_OBJECT
  public:
    using ScreenFactory = std::function<QWidget*()>;

    explicit ScreenRouter(QStackedWidget* stack, QObject* parent = nullptr);

    /// Register a pre-built screen (eager — used for lightweight screens)
    void register_screen(const QString& id, QWidget* screen);

    /// Register a factory that creates the screen on first navigation (lazy)
    void register_factory(const QString& id, ScreenFactory factory);

    void navigate(const QString& id);
    QString current_screen_id() const { return current_id_; }

  signals:
    void screen_changed(const QString& id);

  private:
    QStackedWidget* stack_ = nullptr;
    QHash<QString, QWidget*> screens_;
    QHash<QString, ScreenFactory> factories_;
    QString current_id_;
};

} // namespace fincept
