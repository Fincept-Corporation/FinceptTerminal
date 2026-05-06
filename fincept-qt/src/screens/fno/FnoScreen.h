#pragma once
// FnoScreen — Sensibull-style F&O analytics tab.
//
// Top-level shell. Six sub-tabs hosted in a QStackedWidget; each sub-tab
// is lazy-constructed on first reveal (P2 spirit — chain assembly + WS
// subscriptions for the OI sub-tab don't fire until the user navigates
// there).
//
// Phase 2 ships the Chain sub-tab fully wired. The other five render a
// ComingSoon placeholder until their phase lands:
//   - Chain         (Phase 2)  ✅
//   - Builder       (Phase 5)
//   - OI Analytics  (Phase 7)
//   - Multi-Stra    (Phase 9)
//   - FII / DII     (Phase 8)
//   - Screener      (Phase 9)

#include "core/symbol/IGroupLinked.h"
#include "screens/IStatefulScreen.h"

#include <QHash>
#include <QPointer>
#include <QPushButton>
#include <QStackedWidget>
#include <QString>
#include <QVariantMap>
#include <QWidget>

#include <functional>

namespace fincept::screens::fno {

class BuilderSubTab;
class ChainSubTab;

class FnoScreen : public QWidget,
                  public fincept::screens::IStatefulScreen,
                  public fincept::IGroupLinked {
    Q_OBJECT
    Q_INTERFACES(fincept::IGroupLinked)
  public:
    explicit FnoScreen(QWidget* parent = nullptr);

    // ── IStatefulScreen ────────────────────────────────────────────────────
    QVariantMap save_state() const override;
    void restore_state(const QVariantMap& state) override;
    QString state_key() const override { return "fno"; }
    int state_version() const override { return 1; }

    // ── IGroupLinked — symbol-group sync ───────────────────────────────────
    void set_group(fincept::SymbolGroup g) override { link_group_ = g; }
    fincept::SymbolGroup group() const override { return link_group_; }
    void on_group_symbol_changed(const fincept::SymbolRef& ref) override;
    fincept::SymbolRef current_symbol() const override;

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  public:
    enum SubTab : int {
        TabChain = 0,
        TabBuilder = 1,
        TabOI = 2,
        TabMultiStraddle = 3,
        TabFiiDii = 4,
        TabScreener = 5,
        TabCount
    };

  private slots:
    void on_tab_clicked(int index);

  private:
    void setup_ui();
    QWidget* build_tab_bar();
    QWidget* build_placeholder(const QString& tab_name, const QString& detail);

    /// Lazy-construct the requested sub-tab and insert it into the stack at
    /// its slot index. No-op if already present.
    void ensure_tab_built(SubTab which);

    /// Apply pressed/normal styling to tab buttons based on `active_tab_`.
    void refresh_tab_button_styles();

    SubTab active_tab_ = TabChain;
    QStackedWidget* stack_ = nullptr;
    QVector<QPushButton*> tab_btns_;
    QHash<int, QWidget*> tabs_;  // slot index → widget

    // Direct pointer to the chain sub-tab so showEvent can poke its
    // visibility-driven subscription path.
    QPointer<ChainSubTab> chain_tab_;
    QPointer<BuilderSubTab> builder_tab_;
    QPointer<class OISubTab> oi_tab_;
    QPointer<class FiiDiiSubTab> fii_dii_tab_;
    QPointer<class MultiStraddleSubTab> multi_straddle_tab_;
    QPointer<class ScreenerSubTab> screener_tab_;

    // Group G is the "yellow" slot in the default palette (per SymbolGroup.h
    // line 16). Plan called for Yellow as the F&O default — that's slot G.
    fincept::SymbolGroup link_group_ = fincept::SymbolGroup::G;
};

} // namespace fincept::screens::fno
