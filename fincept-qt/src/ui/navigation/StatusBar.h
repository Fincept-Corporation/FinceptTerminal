#pragma once
#include <QHBoxLayout>
#include <QLabel>
#include <QWidget>

namespace fincept {
struct SymbolRef;
enum class SymbolGroup : char;
} // namespace fincept

namespace fincept::ui {

/// Bottom status bar — version, feed indicators, ready status, and the
/// Phase 7-polish active-symbol indicator.
class StatusBar : public QWidget {
    Q_OBJECT
  public:
    explicit StatusBar(QWidget* parent = nullptr);
    void set_ready(bool ready);

  private:
    void refresh_theme();

    /// Phase 7 polish: subscribe to SymbolContext to display the most-
    /// recently-published symbol + which colour group it lives in.
    /// Bloomberg's bottom bar shows the active security; this is the
    /// equivalent. Drives off SymbolContext::active_symbol_changed
    /// (which fires whenever any panel anywhere publishes via
    /// set_group_symbol).
    void wire_link_indicator();
    void update_link_label(SymbolGroup g, const SymbolRef& ref);

    QLabel* ready_label_ = nullptr;
    QLabel* link_label_ = nullptr;
};

} // namespace fincept::ui
