#pragma once
#include <QHBoxLayout>
#include <QLabel>
#include <QWidget>

class QPushButton;

namespace fincept {
struct SymbolRef;
enum class SymbolGroup : char;
} // namespace fincept

namespace fincept::ui {

/// Bottom status bar: version + edition (OPEN SOURCE · ENTERPRISE ↗), feed
/// indicators, status chips, active-symbol indicator, ready status.
/// Internationalised — labels retranslate on QEvent::LanguageChange.
class StatusBar : public QWidget {
    Q_OBJECT
  public:
    explicit StatusBar(QWidget* parent = nullptr);
    void set_ready(bool ready);

    /// Insertion point for status chips (e.g. the Python setup chip): `w` is
    /// reparented to the bar and placed after the feed indicators, left of the
    /// stretch. Chips are laid out in the order they are added.
    void add_status_widget(QWidget* w);

  protected:
    void changeEvent(QEvent* e) override;

  private:
    void refresh_theme();
    void retranslateUi();

    /// Subscribes to SymbolContext::active_symbol_changed to mirror the most recent group publish.
    void wire_link_indicator();
    void update_link_label(SymbolGroup g, const SymbolRef& ref);

    QLabel* ready_label_ = nullptr;
    QLabel* link_label_ = nullptr;
    QLabel* edition_label_ = nullptr;
    QPushButton* enterprise_btn_ = nullptr;
    /// Holds widgets added through add_status_widget().
    QHBoxLayout* chips_layout_ = nullptr;
    /// Tracks the current ready/busy state so retranslateUi() can recompute
    /// the translated label without depending on the existing widget text.
    bool ready_state_ = true;
};

} // namespace fincept::ui
