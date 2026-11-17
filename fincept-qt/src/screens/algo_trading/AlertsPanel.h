// src/screens/algo_trading/AlertsPanel.h
#pragma once
#include "services/algo_trading/AlgoTradingTypes.h"
#include "storage/repositories/ScanEventRepository.h"
#include "storage/repositories/ScanWatchRepository.h"

#include <QEvent>
#include <QHash>
#include <QJsonArray>
#include <QShowEvent>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include <functional>

class QTimer;

namespace fincept::ui::algo {
class SymbolChipInput;
}

namespace fincept::screens {

class AlertsView;

/// ALERTS sub-tab controller — background watches (ScanWatchRepository + ScanMonitor) and the
/// alerts they fired (ScanEventRepository). The layout is AlertsView; this class reads / writes
/// the repositories on a worker thread, drives ScanMonitor on the GUI thread and confirms deletes.
class AlertsPanel : public QWidget {
    Q_OBJECT
  public:
    explicit AlertsPanel(QWidget* parent = nullptr);

    /// The Scanner's "+ CREATE ALERT": the composer takes its rules, symbols and feed.
    void prefill(const QJsonArray& conditions, const QString& logic, const QStringList& symbols,
                 const QString& timeframe, const QString& data_source, const QString& account_id);

  signals:
    /// "OPEN SCANNER" (the screen switches tabs).
    void open_scanner_requested();

  private slots:
    void on_save_watch();
    void on_refresh_watches();

  protected:
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;

  private:
    // (Re)fills the broker-account combo, keeping the current selection.
    void populate_accounts();
    void refresh_history();
    /// Coalesces ScanMonitor status / fire notifications into one read (and none while hidden).
    void schedule_refresh(bool history_only);
    void apply_watches(const QVector<fincept::ScanWatch>& watches, const QString& error);
    void apply_history(const QVector<fincept::ScanWatchEvent>& events, const QString& error);
    void prefill_close_from_price(const QString& symbol, double price);
    void load_watch(const fincept::ScanWatch& w); // an existing watch into the composer for editing
    void new_watch();
    void request_delete(const QString& id);
    void set_active(const QString& id, bool on);
    /// Runs `work` (a repository write; returns an error text, empty = ok) on a worker, then
    /// `done(error)` on the GUI thread while this panel lives.
    void run_write(std::function<QString()> work, std::function<void(const QString&)> done);
    fincept::ScanWatch watch_from_form(const QString& name) const;

    AlertsView* view_ = nullptr;
    fincept::ui::algo::SymbolChipInput* symbols_input_ = nullptr;
    QString editing_id_; // non-empty when editing an existing watch
    QHash<QString, fincept::ScanWatch> watches_; ///< last read, by id (edit / names / toggles)
    quint64 watches_seq_ = 0;
    quint64 history_seq_ = 0;
    QTimer* refresh_timer_ = nullptr;
    bool refresh_history_only_ = true;
    bool dirty_ = true; ///< a refresh was asked while hidden: read on the next show
};

} // namespace fincept::screens
