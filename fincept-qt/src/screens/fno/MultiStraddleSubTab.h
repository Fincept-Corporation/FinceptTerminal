#pragma once
// MultiStraddleSubTab — F&O Multi-Straddle / Strangle content widget.
//
// User picks an anchor strike + a "type" (Straddle / Strangle ±1/±2/±3) and
// hits ADD. Each active selection contributes one synthetic-premium line
// to MultiStraddleChart by joining the per-minute LTPs from the two
// constituent legs' `oi:history:<broker>:<token>:1d` topics.
//
// Subscription lifecycle
// ──────────────────────
// On showEvent: subscribes to `option:chain:*` (pattern).
// On ADD:       subscribes to two `oi:history:*` topics (CE + PE).
// On REMOVE:    unsubscribes those two topics.
// On hideEvent: unsubscribes everything.
//
// Per-token sample caches let a single side update trigger a recompute of
// all selections that include that token.

#include "screens/common/IStatefulScreen.h"
#include "screens/fno/FnoChainState.h"
#include "services/options/OptionChainTypes.h"

#include <QEvent>
#include <QHash>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>
#include <QWidget>

class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QTimer;

namespace fincept::screens::fno {

class MultiStraddleChart;

class MultiStraddleSubTab : public QWidget {
    Q_OBJECT
  public:
    explicit MultiStraddleSubTab(QWidget* parent = nullptr);
    ~MultiStraddleSubTab() override;

    /// The selections (anchor strike + wing) of the series on screen; restored when
    /// that series' chain arrives again.
    QVariantMap save_state() const;
    void restore_state(const QVariantMap& state);

    /// The Chain tab's state (FnoScreen relays it): skeleton / pages while this tab
    /// holds no chain.
    void set_chain_status(FnoChainStatus status, const QString& reason);
    /// A chain that does not travel on DataHub (the free delayed US chain — it has
    /// no intraday history, so this tab says it needs a broker feed), or an empty
    /// chain when the Chain tab left it.
    void set_external_chain(const fincept::services::options::OptionChain& chain);

  signals:
    /// OPEN CHAIN on this tab's pages.
    void open_chain_requested();

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void changeEvent(QEvent* event) override;

  private slots:
    void on_add_clicked();
    void on_selection_double_clicked();

  private:
    struct Selection {
        QString label;
        QString broker;
        qint64 ce_token = 0;
        qint64 pe_token = 0;
        double anchor_strike = 0; ///< for save_state (tokens are session-bound)
        int wing = 0;             ///< 0 = straddle, N = strangle ±N
    };

    void setup_ui();
    void retranslateUi();
    void on_chain_published(const QVariant& v);
    /// Render `chain` (already accepted as the Chain tab's series).
    void apply_chain(const fincept::services::options::OptionChain& chain);
    /// Add a straddle / strangle by anchor strike and wing; "" on success, else why not.
    QString add_selection(double anchor_strike, int wing);
    /// First chain of a series: put back the saved selections, else (broker chain)
    /// add the ATM straddle once — the chart is never an empty frame.
    void seed_selections();
    /// Body (content / skeleton / page) and the chart area (chart / wait / note).
    void update_body();
    void update_chart_area();
    void rebuild_strike_combo(const fincept::services::options::OptionChain& chain);
    void subscribe_token(const QString& broker, qint64 token);
    void unsubscribe_token(qint64 token);
    void on_oi_history(qint64 token, const QVariant& v);
    void rebuild_chart();
    /// Drop a selection by index — unsubscribes any tokens not used by
    /// other selections.
    void remove_selection(int index);

    QComboBox* type_combo_ = nullptr;
    QComboBox* strike_combo_ = nullptr;
    QPushButton* add_btn_ = nullptr;
    QLabel* add_status_ = nullptr; // why the last ADD did nothing (cleared on success)
    QListWidget* selection_list_ = nullptr;
    MultiStraddleChart* chart_ = nullptr;

    // States: body_ = content_ / skeleton_ / page_ (no chain, or no broker feed);
    // chart_stack_ = chart_ / chart_wait_ (history loading) / chart_note_.
    QStackedWidget* body_ = nullptr;
    QWidget* content_ = nullptr;
    fincept::ui::LoadingSkeleton* skeleton_ = nullptr;
    fincept::ui::EmptyState* page_ = nullptr;
    QStackedWidget* chart_stack_ = nullptr;
    fincept::ui::LoadingSkeleton* chart_wait_ = nullptr;
    fincept::ui::EmptyState* chart_note_ = nullptr;
    QTimer* wait_timer_ = nullptr;
    bool waited_out_ = false;         ///< history did not arrive within the wait
    bool page_is_feed_ = false;       ///< page_ shows the "needs a broker feed" page
    fincept::ui::RequirementStatus feed_req_;
    FnoChainStatus chain_status_ = FnoChainStatus::Loading;
    QString chain_reason_;
    /// Saved selections waiting for their series' chain (restore_state).
    QString pending_series_;
    QVariantList pending_items_;
    /// Series already seeded this session (restored or ATM straddle added once).
    QSet<QString> seeded_series_;

    // Field labels (cached for retranslateUi)
    QLabel* type_label_ = nullptr;
    QLabel* anchor_label_ = nullptr;

    QVector<Selection> selections_;
    fincept::services::options::OptionChain last_chain_;
    QString current_underlying_;
    /// Expiry the anchor combo was last built for — a new expiry means a new
    /// strike ladder even when the underlying is unchanged.
    QString current_expiry_;
    bool chain_subscribed_ = false;

    /// Per-token sample cache populated by oi:history subscription callbacks.
    QHash<qint64, QVector<fincept::services::options::OISample>> samples_by_token_;
    /// Per-token topic string so we can call DataHub::unsubscribe(this, topic)
    /// when removing a selection.
    QHash<qint64, QString> topic_for_token_;
    /// Reference count of selections holding each token — drives unsubscribe.
    QHash<qint64, int> token_refcount_;
};

} // namespace fincept::screens::fno
