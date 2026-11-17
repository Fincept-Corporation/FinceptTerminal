#pragma once

#include "services/wallet/WalletTypes.h"

#include <QEvent>
#include <QString>
#include <QVariant>
#include <QVector>
#include <QWidget>

class QHideEvent;
class QLabel;
class QPushButton;
class QShowEvent;
class QTableWidget;

namespace fincept::ui {
class EmptyState;
class LoadingSkeleton;
} // namespace fincept::ui

namespace fincept::screens {

/// ACTIVITY tab — last 50 parsed wallet operations (Phase 2 §2B).
///
/// Subscribes to `wallet:activity:<pubkey>` (visibility-driven, P3 + D3).
/// Renders a table with TIMESTAMP / EVENT / ASSET / AMOUNT / STATUS
/// columns. Filter chips (ALL / SWAP / SEND / RECEIVE / OTHER) narrow the
/// view client-side.
///
/// Whole-row click opens the transaction's signature in Solscan via the
/// system browser. No in-app explorer.
///
/// When no Helius API key is configured, the producer falls back to raw
/// signatures from the standard RPC and rows show only TIMESTAMP +
/// SIGNATURE + STATUS. The footer hints "Add a Helius key in Settings for
/// parsed activity".
///
/// Never a bare header: until the first answer a table skeleton; a failed first
/// fetch (RPC error, Helius key unreadable) an error page with RETRY (forced
/// re-request); zero transactions "No transactions yet"; a filter that matches
/// nothing "No <kind> transactions" with SHOW ALL. A later failure keeps the
/// rows and says so in the footer.
class ActivityTab : public QWidget {
    Q_OBJECT
  public:
    explicit ActivityTab(QWidget* parent = nullptr);
    ~ActivityTab() override;

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void changeEvent(QEvent* event) override;

  private:
    using Activity = fincept::wallet::ParsedActivity;
    using Kind = Activity::Kind;

    void build_ui();
    void apply_theme();
    void retranslateUi();

    void on_wallet_connected(const QString& pubkey, const QString& label);
    void on_wallet_disconnected();
    void on_activity_update(const QVariant& v);
    void on_topic_error(const QString& topic, const QString& error);
    void on_filter_changed();
    void on_row_clicked(int row, int column);

    void refresh_subscription();
    void rebuild_table();
    bool helius_key_present() const;
    /// Table / skeleton / state page — whichever the current data calls for.
    void update_view_state(int filtered_rows);
    fincept::ui::EmptyState* ensure_state_page();
    /// RETRY on the error page: forced re-request of the activity topic.
    void retry_fetch();

    QTableWidget* table_ = nullptr;
    QLabel* footer_ = nullptr;
    fincept::ui::EmptyState* state_page_ = nullptr;    ///< built on first need
    fincept::ui::LoadingSkeleton* skeleton_ = nullptr; ///< built on first need
    /// What the state page shows, so its buttons know what they mean.
    enum class PageKind { None, NoWallet, Error, NoTransactions, FilteredOut };
    PageKind page_kind_ = PageKind::None;
    bool has_data_ = false; ///< an activity list arrived for the current wallet
    QString fetch_error_;   ///< last fetch error while no list has arrived
    QPushButton* filter_all_ = nullptr;
    QPushButton* filter_swap_ = nullptr;
    QPushButton* filter_send_ = nullptr;
    QPushButton* filter_receive_ = nullptr;
    QPushButton* filter_other_ = nullptr;

    QString current_pubkey_;
    QString current_topic_;
    QVector<Activity> latest_;
    // (Removed `active_filter_kind_` / `show_all_`: never read or written —
    // `rebuild_table()` derives the active filter from the chips directly.)
};

} // namespace fincept::screens
