#pragma once
// ProfilesSection.h — list / switch / create isolated profile workspaces.
// Each profile owns its own DB, credentials, logs, and workspaces.

#include <QWidget>

namespace fincept::screens {

class ProfilesSection : public QWidget {
    Q_OBJECT
  public:
    explicit ProfilesSection(QWidget* parent = nullptr);

  private:
    void build_ui();
};

} // namespace fincept::screens
