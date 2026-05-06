#pragma once
// AppearanceSection.h — typography, theme density, and interface toggles.
// Live preview is debounced (300ms) to coalesce rapid combobox changes.

#include <QCheckBox>
#include <QComboBox>
#include <QTimer>
#include <QWidget>

namespace fincept::screens {

class AppearanceSection : public QWidget {
    Q_OBJECT
  public:
    explicit AppearanceSection(QWidget* parent = nullptr);

    /// Repopulate combo boxes / toggles from SettingsRepository.
    void reload();

  protected:
    void showEvent(QShowEvent* e) override;

  private:
    void build_ui();

    QComboBox* app_font_size_       = nullptr;
    QComboBox* app_font_family_     = nullptr;
    QComboBox* app_density_         = nullptr;
    QCheckBox* chat_bubble_toggle_  = nullptr;
    QCheckBox* ticker_bar_toggle_   = nullptr;
    QCheckBox* animations_toggle_   = nullptr;
    QTimer*    appearance_debounce_ = nullptr;
};

} // namespace fincept::screens
