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
#include "services/options/OptionChainTypes.h"

#include <QHash>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>
#include <QWidget>

class QComboBox;
class QListWidget;
class QPushButton;

namespace fincept::screens::fno {

class MultiStraddleChart;

class MultiStraddleSubTab : public QWidget {
    Q_OBJECT
  public:
    explicit MultiStraddleSubTab(QWidget* parent = nullptr);
    ~MultiStraddleSubTab() override;

    QVariantMap save_state() const;
    void restore_state(const QVariantMap& state);

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private slots:
    void on_add_clicked();
    void on_selection_double_clicked();

  private:
    struct Selection {
        QString label;
        QString broker;
        qint64 ce_token = 0;
        qint64 pe_token = 0;
    };

    void setup_ui();
    void on_chain_published(const QVariant& v);
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
    QListWidget* selection_list_ = nullptr;
    MultiStraddleChart* chart_ = nullptr;

    QVector<Selection> selections_;
    fincept::services::options::OptionChain last_chain_;
    QString current_underlying_;
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
