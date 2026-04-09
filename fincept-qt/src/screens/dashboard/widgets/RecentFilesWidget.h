#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"

#include <QScrollArea>

namespace fincept::screens::widgets {

/// Dashboard widget — shows the 8 most recently added files from FileManagerService.
class RecentFilesWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit RecentFilesWidget(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void on_theme_changed() override;

  private:
    void apply_styles();
    void refresh_data();
    QScrollArea* scroll_ = nullptr;
    QVBoxLayout* list_layout_ = nullptr;
    bool loaded_ = false;
};

} // namespace fincept::screens::widgets
