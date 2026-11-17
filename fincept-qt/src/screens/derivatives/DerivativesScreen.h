#pragma once
// DerivativesScreen — the Derivatives tab: analysis for anyone on free delayed data
// (live trading with a broker is the F&O tab, linked from the header).
//
// A shell — a one-line header (title, page segments, engine chip, refresh, F&O link) and a
// status line — over lazily built pages:
//   PRICER       the calculators (bonds, equity / FX options, swaps, CDS) — built at once;
//   CHAIN & VOL  free delayed option chain, smile + SVI, term structure, surface, OI;
//   STRATEGY     multi-leg positions from templates or the chain;
//   SCENARIOS    spot × vol grid, probability cone, stress presets;
//   CURVES       U.S. Treasury curve (editable; zero / forward, swap, FRA, cap, key-rate
//                DV01, swaption, bond Z-spread / ASW), futures curves and cost of carry;
//   CREDIT       CDS hazard bootstrap, survival curve, CS01, upfront ↔ running spread;
//   BOOK         saved positions with live delayed marks, P&L and aggregated Greeks.
// Each page is its own widget (src/screens/derivatives/Deriv*Page), created the first
// time it is opened. The pages share a DerivContext (the chain in view, the position,
// the hand-offs between pages). State: the pricer's inputs under the long-standing
// keys ("instrument", "inputs") plus the open page; the other pages remember their
// inputs through ui::InputMemory.
//
// Between tabs: the screen takes a symbol through nav.open_symbol / symbol-group linking
// (IGroupLinked): it opens that underlying's chain on CHAIN & VOL. The F&O link is enabled
// only while a broker account is connected (trading::AccountManager, read-only).

#include "core/symbol/IGroupLinked.h"
#include "screens/common/ISectionLinked.h"
#include "screens/common/IStatefulScreen.h"

#include <QEvent>
#include <QList>
#include <QVariantMap>
#include <QVector>
#include <QWidget>

class QLabel;
class QPushButton;
class QStackedWidget;
class QToolButton;

namespace fincept::screens::deriv {
class DerivBookPage;
class DerivChainPage;
class DerivContext;
class DerivCreditPage;
class DerivCurvesPage;
class DerivPricerPage;
class DerivScenarioPage;
class DerivStrategyPage;
} // namespace fincept::screens::deriv

namespace fincept::screens {

class DerivativesScreen : public QWidget,
                          public IStatefulScreen,
                          public fincept::IGroupLinked,
                          public fincept::screens::ISectionLinked {
    Q_OBJECT
    Q_INTERFACES(fincept::IGroupLinked)
  public:
    explicit DerivativesScreen(QWidget* parent = nullptr);

    // ISectionLinked — "pricer" "chain" "strategy" "scenarios" "curves" "credit" "book" (the page
    // segments); "pricer:<instrument>" / "pricer/<instrument>" with bond(s), equity(_options), fx(_options),
    // swap(s) / ir_swaps / irs, cds / credit (the pricer's instrument buttons). `focus` = an
    // underlying: opens its chain (the same path as a linked symbol); with an empty section it acts
    // on the page in view like a linked symbol does.
    bool open_section(const QString& section, const QString& focus) override;

    QVariantMap save_state() const override;
    void restore_state(const QVariantMap& state) override;
    QString state_key() const override { return "derivatives"; }

    // IGroupLinked — subscribe-only: a symbol from a linked group or nav.open_symbol opens
    // that underlying's option chain (never re-published).
    void set_group(fincept::SymbolGroup g) override { link_group_ = g; }
    fincept::SymbolGroup group() const override { return link_group_; }
    void on_group_symbol_changed(const fincept::SymbolRef& ref) override;
    fincept::SymbolRef current_symbol() const override;

  protected:
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

  private:
    enum Page { kPricer = 0, kChain, kStrategy, kScenarios, kCurves, kCredit, kBook, kPageCount };
    void update_fno_link();
    void open_chain_symbol(const QString& symbol);

    void setup_ui();
    /// The one-line header: title, the page segments (create_page_bar), chip, refresh, F&O link.
    QWidget* create_header_bar();
    QWidget* create_page_bar();
    QWidget* create_status_bar();
    /// Narrow / large font: the engine chip, then the title give way (resize, appearance_settled).
    void fit_header();
    /// `persist`: tell ScreenStateManager (not while constructing: nothing is restored yet).
    void show_page(int index, bool persist = true);
    QWidget* ensure_page(int index);
    void refresh_current();
    void retranslate();
    QString page_name(int index) const;

    deriv::DerivContext* ctx_ = nullptr;
    QStackedWidget* pages_ = nullptr;
    QVector<QWidget*> holders_;
    deriv::DerivPricerPage* pricer_ = nullptr;
    deriv::DerivChainPage* chain_ = nullptr;
    deriv::DerivStrategyPage* strategy_ = nullptr;
    deriv::DerivScenarioPage* scenarios_ = nullptr;
    deriv::DerivCurvesPage* curves_ = nullptr;
    deriv::DerivCreditPage* credit_ = nullptr;
    deriv::DerivBookPage* book_ = nullptr;
    int page_ = kPricer;
    fincept::SymbolGroup link_group_ = fincept::SymbolGroup::None;
    QString pending_symbol_; ///< a linked symbol that arrived while the screen was hidden
    QString last_symbol_;

    QWidget* header_bar_ = nullptr;
    QWidget* page_bar_ = nullptr;
    QLabel* header_title_ = nullptr;
    QLabel* header_sub_ = nullptr;
    QLabel* header_badge_ = nullptr;
    QToolButton* refresh_btn_ = nullptr;
    QPushButton* fno_btn_ = nullptr;
    QList<QPushButton*> page_btns_;
    QLabel* status_left_ = nullptr;
    QLabel* status_page_ = nullptr;
    QLabel* status_engine_ = nullptr;
    QString pricer_instrument_;
};

} // namespace fincept::screens
