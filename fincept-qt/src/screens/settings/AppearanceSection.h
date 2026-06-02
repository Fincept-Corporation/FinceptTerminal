#pragma once
// AppearanceSection.h — typography, theme density, and interface toggles.
// Live preview is debounced (300ms) to coalesce rapid combobox changes.

#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QLabel>
#include <QPushButton>
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
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();

    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();

    QComboBox* app_font_size_       = nullptr;
    QComboBox* app_font_family_     = nullptr;
    QComboBox* app_density_         = nullptr;
    QCheckBox* chat_bubble_toggle_  = nullptr;
    QCheckBox* ticker_bar_toggle_   = nullptr;
    QCheckBox* animations_toggle_   = nullptr;
    QTimer*    appearance_debounce_ = nullptr;

    // Section titles.
    QLabel* typography_title_ = nullptr;
    QLabel* theme_title_      = nullptr;
    QLabel* interface_title_  = nullptr;

    // Row labels + descriptions captured from make_row() (the shared helper
    // builds the QLabel internally, so we grab it back from the row's children).
    QLabel* font_size_label_      = nullptr;
    QLabel* font_family_label_    = nullptr;
    QLabel* density_label_        = nullptr;
    QLabel* density_desc_         = nullptr;
    QLabel* chat_bubble_label_    = nullptr;
    QLabel* chat_bubble_desc_     = nullptr;
    QLabel* ticker_bar_label_     = nullptr;
    QLabel* ticker_bar_desc_      = nullptr;
    QLabel* animations_label_     = nullptr;
    QLabel* animations_desc_      = nullptr;

    QPushButton* save_btn_ = nullptr;
};

} // namespace fincept::screens
