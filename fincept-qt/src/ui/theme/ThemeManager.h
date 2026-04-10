#pragma once
#include "ui/theme/ThemeTokens.h"

#include <QFont>
#include <QObject>
#include <QString>

namespace fincept::ui {

/// Central theming singleton.
/// Owns the active ThemeTokens, builds the global QSS string, and notifies
/// all custom-painted widgets via theme_changed so they can re-render.
///
/// Usage:
///   ThemeManager::instance().apply_theme("Obsidian");
///   ThemeManager::instance().apply_font("JetBrains Mono", 14);
///   connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this, [...]);
class ThemeManager : public QObject {
    Q_OBJECT

  public:
    static ThemeManager& instance();

    /// Apply theme (always Obsidian — single theme mode).
    /// Builds global QSS from tokens and calls qApp->setStyleSheet().
    /// Emits theme_changed(tokens) so custom painters can update.
    void apply_theme(const QString& name);

    /// Change the application font family and base size.
    /// Calls qApp->setFont() immediately — takes effect on all widgets.
    void apply_font(const QString& family, int size_px);

    /// Change content density: "Compact" (2px), "Default" (4px), "Comfortable" (8px).
    /// Adjusts padding token in global QSS and re-applies.
    void apply_density(const QString& density);

    /// Read access to the currently active token set.
    /// Custom painters call: QColor(ThemeManager::instance().tokens().accent)
    const ThemeTokens& tokens() const { return current_; }

    QString current_theme_name() const;
    QFont current_font() const;
    static QStringList available_densities(); // single source of truth for density names

    /// Returns QSS that must be applied directly to the CDockManager widget
    /// to override ADS's internally loaded default.css / focus_highlighting.css.
    QString build_ads_qss() const;

  signals:
    /// Emitted after every successful apply_theme / apply_density call.
    /// Custom paintEvent widgets connect to this to invalidate caches and call update().
    void theme_changed(const fincept::ui::ThemeTokens& tokens);

  private:
    ThemeManager();

    void rebuild_and_apply();
    QString build_global_qss() const;

    ThemeTokens current_;
    QString font_family_ = "Consolas";
    int font_size_px_ = 14;
    int density_pad_ = 4;        // px padding driven by density setting
    mutable QString cached_qss_; // last built QSS — skip rebuild if tokens unchanged
};

} // namespace fincept::ui
