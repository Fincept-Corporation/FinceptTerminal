#pragma once
// GeneralSection.h — application-wide behaviour toggles that don't fit
// any of the more specific sections. Currently houses the "on last window
// close" preference (Phase L1 of the workspace/launchpad refactor).

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
};

} // namespace fincept::screens
