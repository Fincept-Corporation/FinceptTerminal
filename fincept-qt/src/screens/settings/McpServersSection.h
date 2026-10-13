#pragma once
// McpServersSection.h — Settings → MCP Servers. A short explanation plus a link
// to the MCP Servers screen (screens/mcp_servers), which owns all server and
// tool management.

#include <QEvent>
#include <QLabel>
#include <QPushButton>
#include <QWidget>

namespace fincept::screens {

class McpServersSection : public QWidget {
    Q_OBJECT
  public:
    explicit McpServersSection(QWidget* parent = nullptr);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();

    /// Apply the section's single objectName-keyed stylesheet from the live
    /// theme tokens. Called from the constructor and on ThemeManager::theme_changed.
    void refresh_theme();

    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from the constructor and from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();

    QLabel* title_lbl_ = nullptr;
    QLabel* about_lbl_ = nullptr;
    QLabel* manage_lbl_ = nullptr;
    QLabel* tools_lbl_ = nullptr;
    QPushButton* open_btn_ = nullptr;
};

} // namespace fincept::screens
