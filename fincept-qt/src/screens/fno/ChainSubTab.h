#pragma once
// ChainSubTab — F&O Chain sub-tab content widget.
//
// Composition:
//   ┌─ FnoHeaderBar ──────────────────────────────────────────────────┐
//   │ Broker | Underlying | Expiry | Refresh   Spot ATM PCR ...       │
//   ├──────────────────────────────────────────────────────────────────┤
//   │                       OptionChainTable                           │
//   └──────────────────────────────────────────────────────────────────┘
//
// Lifecycle:
//   - showEvent: subscribes to current option:chain:* topic on the hub.
//   - hideEvent: unsubscribes (P3 visibility-driven lifecycle, D3).
//   - Combo changes: tear down old subscription, subscribe to new topic,
//     fire one-shot DataHub::request(topic, force=true) for cold start.
//
// State:
//   - Broker chain snapshots arrive via DataHub.
//   - The free DELAYED US chain (source fno_free_source(): Cboe delayed quotes,
//     Yahoo fallback) comes from FreeOptionChainService — no broker, no key. It is
//     the source picked when nothing else is set up, so the tab never opens blank;
//     it is fetched only while the F&O screen is visible and cancelled on hide.
//   - While nothing is on screen: a table skeleton (loading) or a designed page
//     (ui::EmptyState: needs setup with the button that fixes it, or an error with
//     RETRY) — never a bare line of text.

#include "screens/fno/FnoChainState.h"
#include "services/options/OptionChainTypes.h"

#include <QEvent>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QWidget>

#include <functional>
#include <memory>

class QLabel;
class QPushButton;
class QStackedLayout;

namespace fincept::services::options::market {
struct FreeChain;
}

namespace fincept::ui {
class EmptyState;
class LoadingSkeleton;
class SymbolSearchBox;
} // namespace fincept::ui

namespace fincept::screens::fno {

class FnoHeaderBar;
class OptionChainTable;

class ChainSubTab : public QWidget {
    Q_OBJECT
  public:
    explicit ChainSubTab(QWidget* parent = nullptr);
    ~ChainSubTab() override;

    OptionChainTable* table() const { return table_; }

    // Lightweight save/restore for the F&O screen aggregator.
    QVariantMap save_state() const;
    void restore_state(const QVariantMap& state);

    /// Group-link entry point — request the picker switch to `underlying`
    /// when it's already in the broker's loaded instrument set. No-op
    /// otherwise. Used by FnoScreen::on_group_symbol_changed (Yellow sync).
    void request_underlying(const QString& underlying);

    /// Currently selected underlying as carried by the picker (empty when
    /// no broker is connected). Used by FnoScreen::current_symbol() so
    /// other Yellow-group panels can follow.
    QString active_underlying() const;

    /// Screen-level lifecycle, driven by FnoScreen. The chain topic must stay
    /// subscribed for as long as the F&O SCREEN is visible, not just this sub-tab:
    /// the OI / Builder / Multi-Straddle / Screener tabs all read the chain this tab
    /// subscribes to, and the hub only refreshes topics with a concrete subscriber —
    /// so switching to any of them used to freeze every chain after the first view.
    void on_screen_shown();
    void on_screen_hidden();

    /// The source the picker is on (a broker id, "databento" or fno_free_source()).
    QString active_source() const;
    /// What the tab is doing now, and the one-line reason when nothing is shown.
    FnoChainStatus chain_status() const { return status_; }
    QString chain_status_reason() const { return status_reason_; }
    /// The free delayed chain on screen (empty when the source is not the free one).
    const fincept::services::options::OptionChain& free_chain_shown() const { return free_shown_; }
    /// Switch the picker to the free delayed US chain (the "no broker" route).
    void use_free_source();

  signals:
    /// Loading / on screen / not available (reason = the page's title).
    void chain_status_changed(fincept::screens::fno::FnoChainStatus status, const QString& reason);
    /// The free delayed chain now on screen — the other sub-tabs cannot get it from
    /// DataHub. An empty chain when the tab leaves the free source.
    void free_chain_changed(const fincept::services::options::OptionChain& chain);

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void changeEvent(QEvent* event) override;

  private slots:
    void on_broker_changed(const QString& broker_id);
    void on_underlying_changed(const QString& underlying);
    void on_expiry_changed(const QString& expiry);
    void on_refresh_clicked();
    /// Right-click Buy/Sell on a chain leg → build a 1-lot market order, confirm,
    /// and route to the connected broker (live or paper per its trading mode).
    void on_order_requested(qint64 token, double strike, bool is_call, bool is_buy);

  private:
    void retranslateUi();
    /// (Re)builds the Broker combo from the accounts; keeps the current selection
    /// when it is still listed (called at construction and on every screen show).
    void refresh_broker_list();
    void rebuild_picker_for_broker(const QString& broker_id, bool keep_selection);
    void rebuild_expiries_for_underlying(const QString& broker_id, const QString& underlying, bool keep_selection);
    void resubscribe();
    /// Unsubscribe the chain topic + per-leg tick pattern (hub + error channel).
    void drop_subscription();
    /// True while the owning FnoScreen is on screen (this tab may be a hidden page).
    bool owning_screen_visible() const;

    // ── Body pages (table / skeleton / designed state) ─────────────────────
    /// Skeleton in the shape of the chain, with what is being loaded.
    void show_loading(const QString& caption);
    /// A designed page; `primary` / `secondary` run on its buttons.
    void show_state(const fincept::ui::EmptyState::Spec& spec, std::function<void()> primary,
                    std::function<void()> secondary = {});
    /// Neutral one-sentence page without an action (a transient step).
    void show_note(const QString& title, const QString& message);
    /// Failure page: plain sentence, RETRY (`retry`), the raw text behind DETAILS.
    void show_error(const QString& title, const QString& message, std::function<void()> retry,
                    const QString& details = QString());
    /// A missing requirement (broker account, key, Python) — the terminal's wording
    /// and the button that fixes it; RETRY when it could not be checked. With
    /// `offer_free` the page also offers the free delayed US chain.
    void show_requirement(const fincept::ui::RequirementStatus& status, std::function<void()> retry,
                          bool offer_free);
    void hide_empty_state();
    void set_status(FnoChainStatus status, const QString& reason);

    // ── Free delayed US chain ──────────────────────────────────────────────
    void rebuild_picker_for_free_source(bool keep_selection);
    /// Fetch the free chain of the picked underlying (memory copy when < 5 min old,
    /// else the saved copy at once + a fresh one). Visible screen only.
    void start_free_fetch(bool force);
    void cancel_free_fetch();
    /// Fill the expiry picker from the held free chain (keeps the user's expiry).
    void apply_free_expiries();
    /// Convert the held free chain's picked expiry and put it on screen.
    void render_free_expiry();
    /// Tell the other sub-tabs the free chain went away (leaving the free source).
    void clear_free_shown();
    /// The strip under the header on the free source: a symbol box for any US-listed
    /// stock / ETF / index (ui::SymbolSearchBox) and the liquid defaults as chips.
    void build_free_bar(QWidget* parent);
    void set_free_bar_visible(bool visible);
    /// Box text and the highlighted chip follow the picked underlying.
    void sync_free_bar();
    /// Put the free chain on `symbol` (plain US ticker, fno_us_chain_symbol): the
    /// box, a chip, BACK TO on the failure page. The same symbol on a failure page
    /// retries.
    void choose_free_underlying(const QString& symbol);
    /// The page for a free symbol without a chain ("No listed options for X"), with
    /// BACK TO the last symbol whose chain loaded (still held here and by the service).
    void show_free_failure(const QString& symbol, const QString& status, const QString& message);

    QString current_topic() const;
    /// True when the table currently holds rows belonging to `topic`'s
    /// (broker, underlying, expiry) — i.e. what's on screen is not a leftover
    /// from a previous picker selection.
    bool holds_rows_for(const QString& topic) const;

    FnoHeaderBar* header_ = nullptr;
    OptionChainTable* table_ = nullptr;
    QStackedLayout* body_stack_ = nullptr; // table_ / skeleton_ / state_
    fincept::ui::LoadingSkeleton* skeleton_ = nullptr;
    fincept::ui::EmptyState* state_ = nullptr;
    std::function<void()> state_primary_;
    std::function<void()> state_secondary_;
    FnoChainStatus status_ = FnoChainStatus::Loading;
    QString status_reason_;

    /// Free chain: the request in flight (0 = none) and for which symbol, the chain
    /// held (all expiries) and the converted expiry on screen.
    quint64 free_req_ = 0;
    QString free_req_symbol_;
    std::shared_ptr<const fincept::services::options::market::FreeChain> free_chain_;
    QString free_chain_symbol_;
    fincept::services::options::OptionChain free_shown_;
    /// Last underlying whose free chain loaded (sticky, saved with the state as
    /// "free_underlying" — a symbol without listed options is not remembered).
    QString last_free_underlying_;
    /// Free-source symbol strip (hidden on broker / Databento sources).
    QWidget* free_bar_ = nullptr;
    QLabel* free_bar_label_ = nullptr;
    QLabel* free_chips_label_ = nullptr;
    fincept::ui::SymbolSearchBox* free_box_ = nullptr;
    QList<QPushButton*> free_chips_;

    /// Topic we're currently subscribed to. Empty when not subscribed.
    QString active_topic_;

    /// Per-leg live-tick pattern we're subscribed to (`option:tick:<broker>:*`),
    /// used to patch single cells from the WS feed. Empty when not subscribed.
    QString tick_pattern_;

    /// Whether the chain subscription is wanted: this tab is shown, or the owning F&O
    /// screen is (see on_screen_shown). Gates re-subscribe paths triggered by combo
    /// changes.
    bool is_visible_ = false;

    /// Last-applied selection — used to avoid clobbering user choice on
    /// re-population when broker pickers refresh.
    QString last_broker_;
    QString last_underlying_;
    QString last_expiry_;
};

} // namespace fincept::screens::fno
