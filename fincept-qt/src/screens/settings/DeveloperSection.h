#pragma once
// DeveloperSection.h — DataHub Inspector and other devtools surfaces.

#include <QCheckBox>
#include <QEvent>
#include <QLabel>
#include <QShowEvent>
#include <QWidget>

namespace fincept::screens {

class DeveloperSection : public QWidget {
    Q_OBJECT
  public:
    explicit DeveloperSection(QWidget* parent = nullptr);

  protected:
    /// Re-reads the Agentic Mode flag on every show, so a change made elsewhere
    /// (MCP `settings.changed`, another window) is reflected when the section is
    /// revisited instead of only at construction.
    void showEvent(QShowEvent* e) override;
    void changeEvent(QEvent* event) override;

  private:
    void sync_agentic_toggle();

    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();

    QLabel* agentic_title_ = nullptr;
    QLabel* agentic_desc_ = nullptr;
    QCheckBox* agentic_toggle_ = nullptr;
    QLabel* inspector_title_ = nullptr;
    QLabel* inspector_desc_ = nullptr;
};

} // namespace fincept::screens
