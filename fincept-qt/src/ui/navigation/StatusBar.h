#pragma once
#include <QHBoxLayout>
#include <QLabel>
#include <QWidget>

namespace fincept {
struct SymbolRef;
enum class SymbolGroup : char;
} // namespace fincept

namespace fincept::ui {

/// Bottom status bar: version, feed indicators, ready status, active-symbol indicator.
/// Internationalised — READY/BUSY label retranslates on QEvent::LanguageChange.
class StatusBar : public QWidget {
    Q_OBJECT
  public:
    explicit StatusBar(QWidget* parent = nullptr);
    void set_ready(bool ready);

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
    /// Tracks the current ready/busy state so retranslateUi() can recompute
    /// the translated label without depending on the existing widget text.
    bool ready_state_ = true;
};

} // namespace fincept::ui
