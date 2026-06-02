#pragma once
// GeneralSection.h — application-wide behaviour toggles that don't fit
// any of the more specific sections. Houses:
//   * "On last window close" preference (workspace/launchpad refactor)
//   * "Language" preference (i18n)

#include <QComboBox>
#include <QEvent>
#include <QWidget>

class QLabel;

namespace fincept::screens {

class GeneralSection : public QWidget {
    Q_OBJECT
  public:
    explicit GeneralSection(QWidget* parent = nullptr);

    /// Restore field values from SettingsRepository.
    void reload();

  protected:
    void showEvent(QShowEvent* e) override;
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();

    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();

    QComboBox* on_close_combo_ = nullptr;
    QComboBox* language_combo_ = nullptr;
    QComboBox* currency_combo_ = nullptr;

    // Section titles + row labels/descriptions (cached for retranslateUi).
    QLabel* window_title_   = nullptr;
    QLabel* lang_title_     = nullptr;
    QLabel* currency_title_ = nullptr;
    QLabel* on_close_label_ = nullptr;
    QLabel* on_close_desc_  = nullptr;
    QLabel* language_label_ = nullptr;
    QLabel* language_desc_  = nullptr;
    QLabel* currency_label_ = nullptr;
    QLabel* currency_desc_  = nullptr;
};

} // namespace fincept::screens
