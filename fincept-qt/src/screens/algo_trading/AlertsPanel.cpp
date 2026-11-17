// src/screens/algo_trading/AlertsPanel.cpp
#include "screens/algo_trading/AlertsPanel.h"

#include "algo_engine/ScanMonitor.h"
#include "core/logging/Logger.h"
#include "screens/algo_trading/AlertsView.h"
#include "services/algo_trading/AlgoRuleLogic.h"
#include "trading/AccountManager.h"
#include "ui/widgets/algo/RuleTableEditor.h"
#include "ui/widgets/algo/SymbolChipInput.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent>

#include <memory>

namespace fincept::screens {

using namespace fincept::services::algo;
namespace kit = fincept::screens::algo_kit;
namespace algo_rules = fincept::services::algo::rules;

namespace {

/// "AAPL, MSFT, NVDA, TSLA +3"
QString apl_symbols_text(const QStringList& symbols) {
    constexpr int kShown = 4;
    if (symbols.size() <= kShown)
        return symbols.join(QStringLiteral(", "));
    return symbols.mid(0, kShown).join(QStringLiteral(", ")) + QStringLiteral(" +%1").arg(symbols.size() - kShown);
}

} // namespace

AlertsPanel::AlertsPanel(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    view_ = new AlertsView(this);
    root->addWidget(view_);

    // The symbol chip input lives in the composer's slot (it talks to the quote service,
    // which the view stays free of).
    symbols_input_ = new fincept::ui::algo::SymbolChipInput(view_->symbols_slot());
    symbols_input_->setAccessibleName(tr("Symbols to watch"));
    view_->symbols_slot()->layout()->addWidget(symbols_input_);
    connect(symbols_input_, &fincept::ui::algo::SymbolChipInput::price_resolved, this,
            [this](const QString& s, double px) { prefill_close_from_price(s, px); });

    connect(view_, &AlertsView::save_requested, this, &AlertsPanel::on_save_watch);
    connect(view_, &AlertsView::new_requested, this, &AlertsPanel::new_watch);
    connect(view_, &AlertsView::refresh_requested, this, &AlertsPanel::on_refresh_watches);
    connect(view_, &AlertsView::retry_watches_requested, this, &AlertsPanel::on_refresh_watches);
    connect(view_, &AlertsView::retry_history_requested, this, &AlertsPanel::refresh_history);
    connect(view_, &AlertsView::open_scanner_requested, this, &AlertsPanel::open_scanner_requested);
    connect(view_, &AlertsView::quick_symbols_requested, this,
            [this](const QStringList& syms) { symbols_input_->set_symbols(syms); });
    connect(view_, &AlertsView::edit_requested, this, [this](const QString& id) {
        const auto it = watches_.constFind(id);
        if (it != watches_.constEnd())
            load_watch(it.value());
    });
    connect(view_, &AlertsView::test_requested, this,
            [](const QString& id) { fincept::algo::ScanMonitor::instance().test_fire(id, QString()); });
    connect(view_, &AlertsView::active_toggle_requested, this, &AlertsPanel::set_active);
    connect(view_, &AlertsView::delete_requested, this, &AlertsPanel::request_delete);

    // Status changes and fires (incl. TEST and universe-scan matches, recorded before the
    // signal) refresh the tables — coalesced, and only while the tab is shown.
    refresh_timer_ = new QTimer(this);
    refresh_timer_->setSingleShot(true);
    refresh_timer_->setInterval(250);
    connect(refresh_timer_, &QTimer::timeout, this, [this]() {
        if (refresh_history_only_)
            refresh_history();
        else
            on_refresh_watches();
        refresh_history_only_ = true;
    });
    connect(&fincept::algo::ScanMonitor::instance(), &fincept::algo::ScanMonitor::watch_status_changed, this,
            [this](const QString&, const QString&) { schedule_refresh(false); });
    connect(&fincept::algo::ScanMonitor::instance(), &fincept::algo::ScanMonitor::watch_fired, this,
            [this](const QString&, const QString&, const QString&) { schedule_refresh(true); });
    LOG_DEBUG("AlgoTrading", "AlertsPanel constructed");
}

// ── Reads (worker thread) ───────────────────────────────────────────────────

void AlertsPanel::schedule_refresh(bool history_only) {
    if (!isVisible()) {
        dirty_ = true; // read on the next show
        return;
    }
    if (!history_only)
        refresh_history_only_ = false;
    refresh_timer_->start();
}

void AlertsPanel::on_refresh_watches() {
    const quint64 seq = ++watches_seq_;
    const quint64 hseq = ++history_seq_;
    view_->set_loading();
    QPointer<AlertsPanel> self = this;
    (void)QtConcurrent::run([self, seq, hseq]() {
        QVector<fincept::ScanWatch> watches;
        QString werr;
        auto w = fincept::ScanWatchRepository::instance().list_all();
        if (w.is_err())
            werr = QString::fromStdString(w.error());
        else
            watches = w.value();
        QVector<fincept::ScanWatchEvent> events;
        QString herr;
        auto h = fincept::ScanEventRepository::instance().recent(50);
        if (h.is_err())
            herr = QString::fromStdString(h.error());
        else
            events = h.value();
        QMetaObject::invokeMethod(
            qApp,
            [self, seq, hseq, watches, werr, events, herr]() {
                if (!self)
                    return;
                if (seq == self->watches_seq_)
                    self->apply_watches(watches, werr);
                if (hseq == self->history_seq_)
                    self->apply_history(events, herr);
            },
            Qt::QueuedConnection);
    });
}

void AlertsPanel::refresh_history() {
    const quint64 hseq = ++history_seq_;
    QPointer<AlertsPanel> self = this;
    (void)QtConcurrent::run([self, hseq]() {
        QVector<fincept::ScanWatchEvent> events;
        QString herr;
        auto h = fincept::ScanEventRepository::instance().recent(50);
        if (h.is_err())
            herr = QString::fromStdString(h.error());
        else
            events = h.value();
        QMetaObject::invokeMethod(
            qApp,
            [self, hseq, events, herr]() {
                if (self && hseq == self->history_seq_)
                    self->apply_history(events, herr);
            },
            Qt::QueuedConnection);
    });
}

void AlertsPanel::apply_watches(const QVector<fincept::ScanWatch>& watches, const QString& error) {
    if (!error.isEmpty()) {
        LOG_WARN("AlgoTrading", QString("Live watches could not be read error=\"%1\"").arg(error));
        view_->set_watches_error(error);
        return;
    }
    watches_.clear();
    QVector<AlertsWatchRow> rows;
    rows.reserve(watches.size());
    for (const auto& w : watches) {
        watches_.insert(w.id, w);
        AlertsWatchRow r;
        r.id = w.id;
        r.name = w.name;
        r.symbols_text = w.symbols.isEmpty() && !w.universe.isEmpty() ? w.universe : apl_symbols_text(w.symbols);
        r.symbol_count = static_cast<int>(w.symbols.size());
        r.timeframe = w.timeframe;
        r.rule_text = algo_rules::rules_sentence(w.conditions, w.logic, algo_rules::Side::Alert);
        r.status = w.status;
        r.data_source = w.data_source;
        r.active = w.active;
        r.interval_sec = w.interval_sec;
        r.last_eval_at = w.last_eval_at;
        r.last_fired_at = w.last_fired_at;
        rows.append(r);
    }
    view_->set_watches(rows);
    view_->set_as_of(QDateTime::currentDateTime());
}

void AlertsPanel::apply_history(const QVector<fincept::ScanWatchEvent>& events, const QString& error) {
    if (!error.isEmpty()) {
        LOG_WARN("AlgoTrading", QString("Alert history could not be read error=\"%1\"").arg(error));
        view_->set_history_error(error);
        return;
    }
    QVector<AlertsFiredRow> rows;
    rows.reserve(events.size());
    for (const auto& e : events) {
        AlertsFiredRow r;
        r.id = e.id.isEmpty() ? QStringLiteral("%1|%2|%3").arg(e.watch_id).arg(e.fired_at).arg(e.symbol) : e.id;
        r.fired_at = e.fired_at;
        r.symbol = e.symbol;
        r.detail = e.detail;
        // A deleted watch keeps its fired alerts: it is then named by nothing (shown "—").
        r.watch_name = watches_.value(e.watch_id).name;
        rows.append(r);
    }
    view_->set_history(rows);
}

// ── Writes (worker thread, ScanMonitor on the GUI thread) ───────────────────

void AlertsPanel::run_write(std::function<QString()> work, std::function<void(const QString&)> done) {
    QPointer<AlertsPanel> self = this;
    (void)QtConcurrent::run([self, work = std::move(work), done = std::move(done)]() {
        const QString err = work();
        QMetaObject::invokeMethod(
            qApp,
            [self, err, done]() {
                if (self)
                    done(err);
            },
            Qt::QueuedConnection);
    });
}

fincept::ScanWatch AlertsPanel::watch_from_form(const QString& name) const {
    fincept::ScanWatch w;
    w.name = name;
    w.conditions = view_->rules()->conditions();
    w.logic = view_->rules()->combined_logic();
    w.symbols = symbols_input_->symbols();
    w.timeframe = view_->timeframe_combo()->currentText();
    w.lookback_days = algo_default_lookback_days(w.timeframe); // auto — no UI field
    w.data_source = view_->data_source_combo()->currentData().toString();
    w.account_id = view_->account_combo()->isEnabled() ? view_->account_combo()->currentData().toString() : QString();
    if (!w.account_id.isEmpty())
        w.broker_id = fincept::trading::AccountManager::instance().get_account(w.account_id).broker_id;
    w.mode = QStringLiteral("poll");
    w.interval_sec = view_->interval_spin()->value();
    w.cooldown_min = view_->cooldown_spin()->value();
    QJsonObject actions;
    actions["toast"] = true;
    actions["providers"] = view_->providers_check()->isChecked();
    w.actions = actions;
    w.active = true;
    return w;
}

void AlertsPanel::on_save_watch() {
    if (view_->rules()->conditions().isEmpty()) {
        view_->set_message(tr("Add at least one rule before saving a watch."), kit::Tone::Danger);
        return;
    }
    for (const auto& issue : view_->rules()->issues()) {
        if (!issue.blocking)
            continue;
        const QString where = issue.child >= 0 ? QStringLiteral("%1.%2").arg(issue.index + 1).arg(issue.child + 1)
                                               : QString::number(issue.index + 1);
        view_->set_message(tr("Rule %1: %2").arg(where, issue.reason), kit::Tone::Danger);
        return;
    }
    QString name = view_->name_edit()->text().trimmed();
    if (name.isEmpty())
        name = tr("Watch %1").arg(watches_.size() + 1);
    fincept::ScanWatch w = watch_from_form(name);
    if (w.symbols.isEmpty()) {
        view_->set_message(tr("Enter at least one symbol."), kit::Tone::Danger);
        return;
    }
    // DATA SOURCE "Broker only" with no account can never fetch a candle: the watch would
    // sit "watching" forever without ever evaluating.
    if (w.data_source == QLatin1String("Broker") && w.account_id.isEmpty()) {
        view_->set_message(tr("Data source is 'Broker only' but no broker account is selected — pick an account, or "
                              "switch the data source to Auto / Yahoo."),
                           kit::Tone::Danger);
        return;
    }
    if (!editing_id_.isEmpty()) {
        // Editing an existing watch → UPDATE in place + restart its monitor.
        w.id = editing_id_;
        const QString id = editing_id_;
        view_->set_message(tr("Updating '%1'…").arg(name), kit::Tone::Dim);
        run_write(
            [w]() {
                auto r = fincept::ScanWatchRepository::instance().update(w);
                return r.is_err() ? QString::fromStdString(r.error()) : QString();
            },
            [this, id, name](const QString& err) {
                if (!err.isEmpty()) {
                    view_->set_message(tr("Update failed: %1").arg(err), kit::Tone::Danger);
                    return;
                }
                fincept::algo::ScanMonitor::instance().reload(id);
                if (editing_id_ == id) {
                    editing_id_.clear();
                    view_->set_editing(QString());
                    view_->name_edit()->clear();
                }
                view_->set_message(tr("Watch '%1' updated and re-watching.").arg(name), kit::Tone::Ok);
                on_refresh_watches();
            });
        return;
    }
    view_->set_message(tr("Saving '%1'…").arg(name), kit::Tone::Dim);
    auto created = std::make_shared<QString>();
    run_write(
        [w, created]() {
            auto r = fincept::ScanWatchRepository::instance().create(w);
            if (r.is_err())
                return QString::fromStdString(r.error());
            *created = r.value().id;
            return QString();
        },
        [this, created, name](const QString& err) {
            if (!err.isEmpty()) {
                view_->set_message(tr("Save failed: %1").arg(err), kit::Tone::Danger);
                return;
            }
            fincept::algo::ScanMonitor::instance().reload(*created);
            view_->set_message(tr("Watch '%1' saved and monitoring.").arg(name), kit::Tone::Ok);
            view_->name_edit()->clear();
            on_refresh_watches();
        });
}

void AlertsPanel::set_active(const QString& id, bool on) {
    run_write(
        [id, on]() {
            auto r = fincept::ScanWatchRepository::instance().set_active(id, on);
            return r.is_err() ? QString::fromStdString(r.error()) : QString();
        },
        [this, id](const QString& err) {
            if (!err.isEmpty()) {
                view_->set_message(tr("Could not change the watch: %1").arg(err), kit::Tone::Danger);
                return;
            }
            fincept::algo::ScanMonitor::instance().reload(id);
            on_refresh_watches();
        });
}

void AlertsPanel::request_delete(const QString& id) {
    const QString wname = watches_.value(id).name;
    // Deleting a watch stops a live monitor and drops its configuration with no recovery
    // path — confirm first.
    const auto ans = QMessageBox::question(
        this, tr("Delete Watch"),
        tr("Delete watch \"%1\"?\n\nIt stops monitoring immediately. Its alert history is kept.").arg(wname),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ans != QMessageBox::Yes)
        return;
    fincept::algo::ScanMonitor::instance().stop_watch(id);
    // Editing state can't survive its own row being deleted.
    if (editing_id_ == id) {
        editing_id_.clear();
        view_->set_editing(QString());
        view_->set_message(tr("The watch being edited was deleted — this is now a new watch."), kit::Tone::Warn);
    }
    run_write(
        [id]() {
            // The fired history is kept: deleting a live watch must NOT erase its events.
            auto r = fincept::ScanWatchRepository::instance().remove(id);
            return r.is_err() ? QString::fromStdString(r.error()) : QString();
        },
        [this](const QString& err) {
            if (!err.isEmpty())
                view_->set_message(tr("Delete failed: %1").arg(err), kit::Tone::Danger);
            on_refresh_watches();
        });
}

// ── Composer ────────────────────────────────────────────────────────────────

void AlertsPanel::load_watch(const fincept::ScanWatch& w) {
    view_->rules()->set_conditions(w.conditions, w.logic);
    symbols_input_->set_symbols(w.symbols);
    if (int i = view_->timeframe_combo()->findText(w.timeframe); i >= 0)
        view_->timeframe_combo()->setCurrentIndex(i);
    if (int i = view_->data_source_combo()->findData(w.data_source); i >= 0)
        view_->data_source_combo()->setCurrentIndex(i);
    if (int i = view_->account_combo()->findData(w.account_id); i >= 0)
        view_->account_combo()->setCurrentIndex(i);
    view_->interval_spin()->setValue(w.interval_sec);
    view_->cooldown_spin()->setValue(w.cooldown_min);
    view_->providers_check()->setChecked(w.actions.value("providers").toBool(false));
    view_->name_edit()->setText(w.name);
    editing_id_ = w.id;
    view_->set_editing(w.name);
    view_->set_message(tr("Editing '%1' — change rules / symbols and press UPDATE WATCH (NEW starts over).").arg(w.name),
                       kit::Tone::Info);
}

void AlertsPanel::new_watch() {
    editing_id_.clear();
    view_->set_editing(QString());
    view_->name_edit()->clear();
    view_->rules()->clear_all();
    symbols_input_->clear();
    view_->set_message(QString());
    view_->rules()->focus_first();
}

void AlertsPanel::prefill_close_from_price(const QString& /*symbol*/, double price) {
    if (price <= 0)
        return;
    QJsonArray conds = view_->rules()->conditions();
    bool changed = false;
    std::function<void(QJsonArray&)> walk = [&](QJsonArray& arr) {
        for (int i = 0; i < arr.size(); ++i) {
            QJsonObject o = arr[i].toObject();
            if (o.contains("children")) {
                QJsonArray ch = o.value("children").toArray();
                walk(ch);
                o["children"] = ch;
            } else if (o.value("indicator").toString() == "CLOSE" &&
                       o.value("compare_mode").toString("value") == "value" &&
                       qFuzzyIsNull(o.value("value").toDouble())) {
                o["value"] = price;
                changed = true;
            }
            arr[i] = o;
        }
    };
    walk(conds);
    if (changed)
        view_->rules()->set_conditions(conds, view_->rules()->combined_logic());
}

void AlertsPanel::prefill(const QJsonArray& conditions, const QString& logic, const QStringList& symbols,
                          const QString& timeframe, const QString& data_source, const QString& account_id) {
    view_->rules()->set_conditions(conditions, logic);
    symbols_input_->set_symbols(symbols);
    if (int i = view_->timeframe_combo()->findText(timeframe); i >= 0)
        view_->timeframe_combo()->setCurrentIndex(i);
    if (int i = view_->data_source_combo()->findData(data_source); i >= 0)
        view_->data_source_combo()->setCurrentIndex(i);
    if (int i = view_->account_combo()->findData(account_id); i >= 0)
        view_->account_combo()->setCurrentIndex(i);
    editing_id_.clear();
    view_->set_editing(QString());
    view_->set_message(tr("Pre-filled from Scanner — name it and SAVE WATCH."), kit::Tone::Info);
}

void AlertsPanel::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    // Re-read on entry: alerts that fired while another tab was open appear now (two
    // capped SELECTs on a worker, no timer).
    dirty_ = false;
    on_refresh_watches();
    populate_accounts(); // a broker connected after this tab was built
}

void AlertsPanel::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        populate_accounts(); // the "none" row is translated
    QWidget::changeEvent(e);
}

void AlertsPanel::populate_accounts() {
    QVector<QPair<QString, QString>> rows;
    rows.append({tr("None (Yahoo fallback)"), QString()});
    for (const auto& acct : fincept::trading::AccountManager::instance().list_accounts())
        rows.append({QStringLiteral("%1 (%2)").arg(acct.display_name, acct.broker_id), acct.account_id});
    view_->set_accounts(rows);
}

} // namespace fincept::screens
