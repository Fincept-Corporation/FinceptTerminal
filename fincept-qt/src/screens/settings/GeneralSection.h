#pragma once
// GeneralSection.h — application-wide behaviour toggles that don't fit
// any of the more specific sections. Houses:
//   * "On last window close" preference (workspace/launchpad refactor)
//   * "Language" preference (i18n)

#include <QComboBox>
#include <QWidget>

namespace fincept::screens {

class GeneralSection : public QWidget {
    Q_OBJECT
  public:
    explicit GeneralSection(QWidget* parent = nullptr);

    /// Restore field values from SettingsRepository.
    void reload();

  protected:
    void showEvent(QShowEvent* e) override;

  private:
    void build_ui();

    QComboBox* on_close_combo_ = nullptr;
    QComboBox* language_combo_ = nullptr;
};

} // namespace fincept::screens
