#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"

namespace fincept::screens::widgets {

/// Dashboard widget — shows the 8 most recently added files from FileManagerService.
class RecentFilesWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit RecentFilesWidget(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    void refresh_data();
    QVBoxLayout* list_layout_ = nullptr;
    bool loaded_ = false;
};

} // namespace fincept::screens::widgets
