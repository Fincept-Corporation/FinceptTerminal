#pragma once
// ProfilesSection.h — list / switch / create isolated profile workspaces.
// Each profile owns its own DB, credentials, logs, and workspaces.

#include <QEvent>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

class ProfilesSection : public QWidget {
    Q_OBJECT
  public:
    explicit ProfilesSection(QWidget* parent = nullptr);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();
    /// Build the swappable content widget (header, profile list, create form).
    QWidget* build_content();
    /// Rebuild content — used on QEvent::LanguageChange so the dynamically
    /// generated profile rows re-run their tr() lookups.
    void rebuild();

    QVBoxLayout* host_layout_ = nullptr;
    QWidget*     content_     = nullptr;
};

} // namespace fincept::screens
