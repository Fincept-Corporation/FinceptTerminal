#pragma once
#include <QSettings>
#include <QString>
#include <QVariant>

namespace fincept {

/// Application configuration backed by QSettings (persistent across sessions).
class AppConfig {
  public:
    static AppConfig& instance();

    QVariant get(const QString& key, const QVariant& default_val = {}) const;
    void set(const QString& key, const QVariant& value);
    void remove(const QString& key);

    // Typed accessors for common settings
    QString api_base_url() const;
    bool dark_mode() const;
    int refresh_interval_ms() const;

  private:
    AppConfig();
    QSettings settings_;
};

} // namespace fincept
