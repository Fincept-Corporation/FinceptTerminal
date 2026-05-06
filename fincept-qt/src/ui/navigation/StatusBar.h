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
class StatusBar : public QWidget {
    Q_OBJECT
  public:
    explicit StatusBar(QWidget* parent = nullptr);
    void set_ready(bool ready);

  private:
    void refresh_theme();

    /// Subscribes to SymbolContext::active_symbol_changed to mirror the most recent group publish.
    void wire_link_indicator();
    void update_link_label(SymbolGroup g, const SymbolRef& ref);

    QLabel* ready_label_ = nullptr;
    QLabel* link_label_ = nullptr;
};

} // namespace fincept::ui
