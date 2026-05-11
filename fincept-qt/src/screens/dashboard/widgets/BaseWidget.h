#pragma once
#include "ui/theme/ThemeTokens.h"

#include <QDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace fincept::screens::widgets {

class LoadingOverlay;

/// Reusable widget container — title bar with accent color, close/refresh/config buttons.
///
/// Configurable widgets override `make_config_dialog(parent)` to return a
/// modal dialog; BaseWidget then shows a gear icon in the title bar that
/// opens it. On `accept()`, the subclass should call `apply_config()` and
/// emit `config_changed()`, which DashboardCanvas persists back to the
/// layout repository. Widgets without config return nullptr (default) —
/// the gear icon stays hidden.
class BaseWidget : public QFrame {
    Q_OBJECT
  public:
    explicit BaseWidget(const QString& title, QWidget* parent = nullptr, const QString& accent_color = {});

    QVBoxLayout* content_layout() const { return content_layout_; }
    QWidget* drag_handle() const { return title_bar_; }

    void set_loading(bool loading);

    /// Determinate-progress variant of `set_loading`. Shows an animated
    /// "LOADING X / Y ITEMS" overlay over the content area; widget
    /// subclasses call this from their data-arrival slot with the running
    /// (loaded, expected) counts. When `loaded >= expected` the overlay
    /// fades out automatically. If `expected <= 0`, falls back to the
    /// indeterminate "LOADING…" animation. The title-bar text indicator is
    /// also toggled in lockstep.
    void set_loading_progress(int loaded, int expected);

    void set_error(const QString& error);
    void set_title(const QString& title);

    /// External-trigger entry point — emits `refresh_requested` so the
    /// widget runs the same refresh path as a user clicking the title-bar
    /// refresh button. Used by the dashboard "REFRESH ALL" toolbar button.
    void request_refresh() { emit refresh_requested(); }

    /// Current per-instance config. Default: empty — subclasses override to
    /// return their live state (symbol, broker id, filters, etc.).
    virtual QJsonObject config() const { return {}; }

    /// Apply a persisted config. Called by the factory for fresh tiles and
    /// by the canvas when the user edits a config. Default: no-op.
    virtual void apply_config(const QJsonObject& /*cfg*/) {}

  signals:
    void close_requested();
    void refresh_requested();
    /// Emitted after the user edits config; canvas listens and persists.
    void config_changed(QJsonObject new_config);

  protected:
    /// Override in subclasses to re-apply styles when theme/font changes.
    /// Called automatically — no manual connection needed.
    virtual void on_theme_changed() {}

    /// Override to return a modal config dialog (takes ownership: parent =
    /// `parent`). Return nullptr (default) to disable configuration and
    /// hide the gear icon.
    virtual QDialog* make_config_dialog(QWidget* /*parent*/) { return nullptr; }

    /// Subclasses call this from their ctor once they support configuration
    /// so the gear icon appears. Also hides the icon if set to false later.
    void set_configurable(bool configurable);

  private:
    void refresh_base_theme();
    void on_config_clicked();
    void arm_watchdog();
    void disarm_watchdog();
    void on_watchdog_fired();

  private:
    QWidget* title_bar_ = nullptr;
    QLabel* title_label_ = nullptr;
    QLabel* accent_bar_ = nullptr;
    QLabel* loading_label_ = nullptr;
    QWidget* content_ = nullptr;
    QVBoxLayout* content_layout_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QPushButton* config_btn_ = nullptr;
    LoadingOverlay* loading_overlay_ = nullptr;
    QTimer* loading_watchdog_ = nullptr;
    int last_progress_loaded_ = 0;
    QString accent_color_;
};

} // namespace fincept::screens::widgets
