#pragma once
// FnoScreen — Sensibull-style F&O analytics tab.
//
// Top-level shell. Seven sub-tabs hosted in a QStackedWidget; each sub-tab
// is lazy-constructed on first reveal (P2 spirit — chain assembly + WS
// subscriptions for the OI sub-tab don't fire until the user navigates
// there). Only Chain is built eagerly, so the screen shows data immediately.
//
// All seven are implemented: Chain, Builder, OI Analytics, Multi-Straddle,
// FII/DII, Screener, Positions. The QStackedWidget is seeded with placeholder
// widgets purely so slot indices map 1:1 onto SubTab values; each is swapped
// for the real widget before it can ever be shown.

#include "core/symbol/IGroupLinked.h"
#include "screens/common/ISectionLinked.h"
#include "screens/common/IStatefulScreen.h"
#include "screens/fno/FnoChainState.h"
#include "services/options/OptionChainTypes.h"

#include <QEvent>
#include <QHash>
#include <QPointer>
#include <QPushButton>
#include <QStackedWidget>
#include <QString>
#include <QVariantMap>
#include <QWidget>

#include <functional>

class QLabel;

namespace fincept::screens::common {
class PaperBlotterPanel;
}

namespace fincept::screens::fno {

class BuilderSubTab;
class ChainSubTab;

class FnoScreen : public QWidget,
                  public fincept::screens::IStatefulScreen,
                  public fincept::IGroupLinked,
                  public fincept::screens::ISectionLinked {
    Q_OBJECT
    Q_INTERFACES(fincept::IGroupLinked)
  public:
    explicit FnoScreen(QWidget* parent = nullptr);

    /// ISectionLinked — chain | builder | oi | multi_straddle | fii_dii | screener | positions (also
    /// "tab:<id>"): the tab bar's click path (on_tab_clicked). focus = underlying ("NIFTY",
    /// "RELIANCE.NS", "SPX"): applied as a linked symbol (the Chain's underlying, validated by its
    /// picker). Nothing is placed or squared off.
    bool open_section(const QString& section, const QString& focus) override;

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
    void changeEvent(QEvent* event) override;

  public:
    enum SubTab : int {
        TabChain = 0,
        TabBuilder = 1,
        TabOI = 2,
        TabMultiStraddle = 3,
        TabFiiDii = 4,
        TabScreener = 5,
        TabPositions = 6,
        TabCount
    };

  private slots:
    void on_tab_clicked(int index);
    /// Chain LTP click / Enter / "Add to Builder": builds the Builder tab on demand
    /// (it is lazy) and appends the leg. Connected at setup, so a click works even
    /// when the Builder tab has never been opened.
    void on_chain_leg_clicked(qint64 token, double strike, bool is_call, int lots);

  private:
    void setup_ui();
    QWidget* build_tab_bar();
    QWidget* build_placeholder(const QString& tab_name, const QString& detail);

    /// Re-apply tr() lookups to the tab-bar buttons on QEvent::LanguageChange.
    void retranslateUi();

    /// Translated tab label / detail for the given slot index.
    static QString tab_label_for(int index);
    static QString tab_detail_for(int index);

    /// Lazy-construct the requested sub-tab and insert it into the stack at
    /// its slot index. No-op if already present.
    void ensure_tab_built(SubTab which);

    /// The Chain tab's state / free delayed chain, relayed to every built sub-tab
    /// that reads the chain (a sub-tab built later gets them at construction).
    void on_chain_status(FnoChainStatus status, const QString& reason);
    void on_free_chain(const fincept::services::options::OptionChain& chain);
    /// Wire a chain-reading sub-tab: OPEN CHAIN + the current status / free chain.
    template <typename Sub>
    void attach_chain_reader(Sub* tab);
    /// "DELAYED US OPTIONS · as of …" strip under the tab bar, on the tabs that show
    /// the free chain's data.
    void update_source_strip();

    /// Apply pressed/normal styling to tab buttons based on `active_tab_`.
    void refresh_tab_button_styles();

    SubTab active_tab_ = TabChain;
    QStackedWidget* stack_ = nullptr;
    QVector<QPushButton*> tab_btns_;
    QHash<int, QWidget*> tabs_; // slot index → widget

    // Direct pointer to the chain sub-tab so showEvent can poke its
    // visibility-driven subscription path.
    QPointer<ChainSubTab> chain_tab_;
    QPointer<BuilderSubTab> builder_tab_;
    QPointer<class OISubTab> oi_tab_;
    QPointer<class FiiDiiSubTab> fii_dii_tab_;
    QPointer<class MultiStraddleSubTab> multi_straddle_tab_;
    QPointer<class ScreenerSubTab> screener_tab_;
    QPointer<fincept::screens::common::PaperBlotterPanel> positions_tab_;

    FnoChainStatus chain_status_ = FnoChainStatus::Loading;
    QString chain_reason_;
    fincept::services::options::OptionChain free_chain_; ///< empty unless the free source is on screen
    QWidget* source_strip_ = nullptr;
    QLabel* source_strip_label_ = nullptr;

    // Group G is the "yellow" slot in the default palette (per SymbolGroup.h
    // line 16). Plan called for Yellow as the F&O default — that's slot G.
    fincept::SymbolGroup link_group_ = fincept::SymbolGroup::G;
};

} // namespace fincept::screens::fno
