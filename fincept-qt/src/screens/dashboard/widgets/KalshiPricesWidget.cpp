#include "screens/dashboard/widgets/KalshiPricesWidget.h"

#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "services/prediction/PredictionTypes.h"
#include "services/prediction/kalshi/KalshiMarketProducer.h"
#include "ui/theme/Theme.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QJsonArray>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QStyle>
#include <QVBoxLayout>

#include <iterator>

namespace fincept::screens::widgets {

namespace {

// Upper bound on configured markets: one batched request either way, but the tile is a strip.
constexpr int kKalshiPricesMaxEntries = 25;
constexpr const char* kKalshiPricesScreen = "polymarket"; // the Prediction Markets screen (Polymarket + Kalshi)

// Colour state lives in dynamic properties matched by the rows host's stylesheet; a label is
// only re-polished when its state actually changes (no per-update setStyleSheet, P7).
void kalshi_prices_set_prop(QWidget* w, const char* name, const char* value) {
    if (w->property(name).toString() == QLatin1String(value))
        return;
    w->setProperty(name, QString::fromLatin1(value));
    w->style()->unpolish(w);
    w->style()->polish(w);
}

/// "[CODE] message" → "message" (KalshiMarketProducer prefixes its errors).
QString kalshi_prices_strip_code(const QString& error) {
    if (error.startsWith(QLatin1Char('['))) {
        const int close = error.indexOf(QLatin1String("] "));
        if (close > 0)
            return error.mid(close + 2);
    }
    return error;
}

/// Errors retrying cannot fix: the ticker is dropped from the subscription until refresh.
bool kalshi_prices_is_permanent(const QString& error) {
    return error.startsWith(QLatin1String("[NOT_FOUND]")) || error.startsWith(QLatin1String("[INVALID]"));
}

} // namespace

KalshiPricesWidget::KalshiPricesWidget(const QJsonObject& cfg, QWidget* parent) : BaseWidget(tr("KALSHI"), parent) {
    auto* vl = content_layout();
    vl->setContentsMargins(10, 8, 10, 6);
    vl->setSpacing(4);

    set_configurable(true);
    connect(this, &BaseWidget::refresh_requested, this, &KalshiPricesWidget::refresh_data);
    apply_config(cfg); // builds the rows; the summary pass shows the loading overlay until data arrives
}

QVector<KalshiPricesWidget::Entry> KalshiPricesWidget::default_entries() {
    // Long-dated, high-volume markets (close Nov 2029) checked against the live API on
    // 2026-11-09, so the default strip does not go stale within months.
    return {
        {QStringLiteral("KXPRESPARTY-2028-D"), tr("Democrat wins 2028")},
        {QStringLiteral("KXPRESPARTY-2028-R"), tr("Republican wins 2028")},
        {QStringLiteral("KXPRESPERSON-28-JVAN"), tr("J.D. Vance wins 2028")},
        {QStringLiteral("KXPRESPERSON-28-GNEWS"), tr("Gavin Newsom wins 2028")},
        {QStringLiteral("KXPRESPERSON-28-JOSS"), tr("Jon Ossoff wins 2028")},
    };
}

QString KalshiPricesWidget::normalize_ticker(const QString& raw) {
    // ':' would break the hub topic, ',' the batched `tickers=` query.
    static const QRegularExpression kTicker(QStringLiteral("^[A-Z0-9][A-Z0-9._\\-]{0,99}$"));
    const QString t = raw.trimmed().toUpper();
    return kTicker.match(t).hasMatch() ? t : QString();
}

QJsonObject KalshiPricesWidget::config() const {
    QJsonArray arr;
    for (const auto& e : entries_) {
        QJsonObject item;
        item.insert("ticker", e.ticker);
        item.insert("label", e.label);
        arr.append(item);
    }
    QJsonObject o;
    o.insert("entries", arr);
    return o;
}

void KalshiPricesWidget::apply_config(const QJsonObject& cfg) {
    QVector<Entry> next;
    if (!cfg.contains("entries")) {
        next = default_entries();
    } else {
        QSet<QString> seen;
        const QJsonArray arr = cfg.value("entries").toArray();
        for (const auto& v : arr) {
            const QJsonObject obj = v.toObject();
            const QString ticker = normalize_ticker(obj.value("ticker").toString());
            if (ticker.isEmpty() || seen.contains(ticker))
                continue;
            seen.insert(ticker);
            next.append(Entry{ticker, obj.value("label").toString().trimmed()});
            if (next.size() >= kKalshiPricesMaxEntries)
                break;
        }
    }
    entries_ = next;

    // Keep last-known-good data only for markets still on the list; a config change retries
    // every ticker (including ones that were dropped as unknown).
    QSet<QString> keep;
    for (const auto& e : entries_)
        keep.insert(e.ticker);
    for (auto it = cache_.begin(); it != cache_.end();)
        it = keep.contains(it.key()) ? std::next(it) : cache_.erase(it);
    errors_.clear();
    halted_.clear();

    build_rows();
    if (isVisible())
        hub_resubscribe();
}

QDialog* KalshiPricesWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle(tr("Configure \xe2\x80\x94 Kalshi"));
    auto* form = new QFormLayout(dlg);

    auto* edit = new QPlainTextEdit(dlg);
    edit->setPlaceholderText(tr("One per line:  <ticker> | <label (optional)>"));
    edit->setAccessibleName(tr("Kalshi markets"));
    QStringList lines;
    for (const auto& e : entries_)
        lines.append(e.label.isEmpty() ? e.ticker : e.ticker + QStringLiteral(" | ") + e.label);
    edit->setPlainText(lines.join(QLatin1Char('\n')));
    edit->setFixedHeight(140);
    form->addRow(tr("Markets"), edit);

    auto* hint = new QLabel(tr("Market tickers are shown on kalshi.com market pages and in the Prediction Markets "
                               "screen (Kalshi). Prices are public \xe2\x80\x94 no Kalshi account needed."),
                            dlg);
    hint->setWordWrap(true);
    form->addRow(hint);

    auto* problem = new QLabel(dlg);
    problem->setWordWrap(true);
    problem->setStyleSheet(QString("color:%1;").arg(ui::colors::NEGATIVE()));
    problem->setVisible(false);
    form->addRow(problem);

    auto* buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults, dlg);
    form->addRow(buttons);

    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, dlg, [edit, problem]() {
        QStringList defaults;
        for (const auto& e : default_entries())
            defaults.append(e.ticker + QStringLiteral(" | ") + e.label);
        edit->setPlainText(defaults.join(QLatin1Char('\n')));
        problem->setVisible(false);
    });
    connect(buttons, &QDialogButtonBox::accepted, dlg, [this, dlg, edit, problem]() {
        QJsonArray arr;
        QStringList bad;
        const QStringList rows = edit->toPlainText().split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (const auto& line : rows) {
            if (line.trimmed().isEmpty())
                continue;
            const QString raw_ticker = line.section(QLatin1Char('|'), 0, 0).trimmed();
            const QString ticker = normalize_ticker(raw_ticker);
            if (ticker.isEmpty()) {
                bad.append(raw_ticker);
                continue;
            }
            QJsonObject item;
            item.insert("ticker", ticker);
            item.insert("label", line.section(QLatin1Char('|'), 1).trimmed());
            arr.append(item);
        }
        if (!bad.isEmpty()) {
            problem->setText(tr("Not a Kalshi ticker: %1").arg(bad.join(QStringLiteral(", "))));
            problem->setVisible(true);
            return;
        }
        if (arr.size() > kKalshiPricesMaxEntries) {
            problem->setText(tr("At most %1 markets per tile.").arg(kKalshiPricesMaxEntries));
            problem->setVisible(true);
            return;
        }
        QJsonObject cfg;
        cfg.insert("entries", arr);
        apply_config(cfg);
        emit config_changed(cfg);
        dlg->accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    return dlg;
}

// ── Rows ────────────────────────────────────────────────────────────────────

void KalshiPricesWidget::build_rows() {
    auto* vl = content_layout();
    while (QLayoutItem* it = vl->takeAt(0)) {
        if (auto* w = it->widget())
            w->deleteLater();
        delete it;
    }
    rows_.clear();
    status_label_ = nullptr;

    // One host carries the stylesheet for the hint / rows / status line (objectName + property
    // selectors) — see apply_styles().
    rows_host_ = new QWidget(this);
    rows_host_->setObjectName(QStringLiteral("kalshiPriceRows"));
    rows_host_->setAccessibleName(tr("Kalshi markets"));
    auto* host_layout = new QVBoxLayout(rows_host_);
    host_layout->setContentsMargins(0, 0, 0, 0);
    host_layout->setSpacing(4);

    if (entries_.isEmpty()) {
        auto* hint = new QLabel(tr("No markets configured \xe2\x80\x94 click gear to add"), rows_host_);
        hint->setObjectName(QStringLiteral("kalshiPriceHint"));
        host_layout->addWidget(hint);
        host_layout->addStretch(1);
        vl->addWidget(rows_host_, 1);
        apply_styles();
        update_summary(); // nothing to load: clears any overlay left from the previous list
        return;
    }

    auto* grid = new QGridLayout;
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(3);
    grid->setColumnStretch(0, 1);
    const QString open_hint = tr("Double-click to open the Prediction Markets screen");
    for (int i = 0; i < entries_.size(); ++i) {
        const auto& e = entries_[i];
        Row r;
        r.label = new QLabel(e.label.isEmpty() ? e.ticker : e.label, rows_host_);
        r.label->setObjectName(QStringLiteral("kalshiPriceLabel"));
        r.label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); // clip, never widen the tile
        r.yes = new QLabel(QString(QChar(0x2014)), rows_host_);
        r.yes->setObjectName(QStringLiteral("kalshiPriceYes"));
        r.yes->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        r.change = new QLabel(rows_host_);
        r.change->setObjectName(QStringLiteral("kalshiPriceChange"));
        r.change->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        r.change->setMinimumWidth(44);
        grid->addWidget(r.label, i, 0);
        grid->addWidget(r.yes, i, 1);
        grid->addWidget(r.change, i, 2);
        link_screen(r.label, QString::fromLatin1(kKalshiPricesScreen), open_hint);
        link_screen(r.yes, QString::fromLatin1(kKalshiPricesScreen), open_hint);
        link_screen(r.change, QString::fromLatin1(kKalshiPricesScreen), open_hint);
        rows_.insert(e.ticker, r);
    }
    host_layout->addLayout(grid);
    host_layout->addStretch(1);

    status_label_ = new QLabel(rows_host_);
    status_label_->setObjectName(QStringLiteral("kalshiPriceStatus"));
    status_label_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    host_layout->addWidget(status_label_);

    vl->addWidget(rows_host_, 1);
    apply_styles();
    for (const auto& e : entries_)
        render_row(e.ticker);
    update_summary();
}

void KalshiPricesWidget::render_row(const QString& ticker) {
    const auto row_it = rows_.constFind(ticker);
    if (row_it == rows_.constEnd())
        return;
    const Row& r = row_it.value();
    QString label;
    for (const auto& e : entries_) {
        if (e.ticker == ticker) {
            label = e.label;
            break;
        }
    }
    const QString open_hint = tr("Double-click to open the Prediction Markets screen");
    const QString dash = QString(QChar(0x2014));
    const QString error = errors_.value(ticker);
    const auto snap_it = cache_.constFind(ticker);

    if (snap_it == cache_.constEnd()) {
        r.label->setText(label.isEmpty() ? ticker : label);
        r.label->setToolTip(ticker + QLatin1Char('\n') + open_hint);
        r.change->clear();
        kalshi_prices_set_prop(r.change, "dir", "flat");
        if (error.startsWith(QLatin1String("[NOT_FOUND]")) || error.startsWith(QLatin1String("[INVALID]"))) {
            r.yes->setText(tr("N/A"));
            r.yes->setToolTip(
                tr("Kalshi has no market %1 \xe2\x80\x94 check the ticker with the gear icon.").arg(ticker));
        } else {
            r.yes->setText(dash);
            r.yes->setToolTip(error.isEmpty() ? tr("Waiting for Kalshi\xe2\x80\xa6") : kalshi_prices_strip_code(error));
        }
        kalshi_prices_set_prop(r.yes, "lvl", "none");
        return;
    }

    const Snapshot& s = snap_it.value();
    const QString name = !label.isEmpty()             ? label
                         : !s.yes_sub_title.isEmpty() ? s.yes_sub_title
                         : !s.question.isEmpty()      ? s.question
                                                      : ticker;
    r.label->setText(name);
    QStringList tip;
    if (!s.question.isEmpty())
        tip << s.question;
    if (!s.yes_sub_title.isEmpty() && s.yes_sub_title != s.question)
        tip << s.yes_sub_title;
    tip << ticker << open_hint;
    r.label->setToolTip(tip.join(QLatin1Char('\n')));

    // YES price: last trade; a market that never traded falls back to the bid/ask midpoint.
    double price = s.last;
    if (price <= 0.0 && s.bid > 0.0 && s.ask > 0.0 && s.ask < 1.0)
        price = (s.bid + s.ask) / 2.0;
    else if (price <= 0.0)
        price = s.bid;

    const bool settled = !s.result.isEmpty();
    const bool live = s.status == QLatin1String("active") || s.status == QLatin1String("open");
    QString yes_tip = tr("YES bid %1\xc2\xa2 \xc2\xb7 ask %2\xc2\xa2 \xc2\xb7 last %3\xc2\xa2")
                          .arg(s.bid * 100.0, 0, 'f', 1)
                          .arg(s.ask * 100.0, 0, 'f', 1)
                          .arg(s.last * 100.0, 0, 'f', 1);
    if (!error.isEmpty())
        yes_tip += QLatin1Char('\n') + tr("Last refresh failed: %1").arg(kalshi_prices_strip_code(error));
    r.yes->setToolTip(yes_tip);

    if (settled) {
        r.yes->setText(tr("SETTLED %1").arg(s.result.toUpper()));
        kalshi_prices_set_prop(r.yes, "lvl", "mid");
    } else if (price > 0.0) {
        const double pct = qBound(0.0, price, 1.0) * 100.0;
        r.yes->setText(QString::number(pct, 'f', 1) + QLatin1Char('%'));
        kalshi_prices_set_prop(r.yes, "lvl", pct >= 65.0 ? "hi" : (pct <= 35.0 ? "lo" : "mid"));
    } else {
        r.yes->setText(dash);
        kalshi_prices_set_prop(r.yes, "lvl", "none");
    }

    if (settled) {
        r.change->clear();
        r.change->setToolTip(QString());
        kalshi_prices_set_prop(r.change, "dir", "flat");
    } else if (!live) {
        r.change->setText(s.status.toUpper()); // e.g. CLOSED / PAUSED, as Kalshi reports it
        r.change->setToolTip(tr("Kalshi market status"));
        kalshi_prices_set_prop(r.change, "dir", "flat");
    } else if (s.last > 0.0 && s.previous > 0.0) {
        const double delta = (s.last - s.previous) * 100.0;
        const QString sign = delta > 0.0 ? QStringLiteral("+") : QString();
        r.change->setText(sign + QString::number(delta, 'f', 1));
        r.change->setToolTip(tr("24 h change in percentage points (24 h ago: %1%)").arg(s.previous * 100.0, 0, 'f', 1));
        kalshi_prices_set_prop(r.change, "dir", delta > 0.05 ? "up" : (delta < -0.05 ? "down" : "flat"));
    } else {
        r.change->clear();
        r.change->setToolTip(tr("No trade 24 h ago to compare with"));
        kalshi_prices_set_prop(r.change, "dir", "flat");
    }
}

// ── Hub wiring ──────────────────────────────────────────────────────────────

void KalshiPricesWidget::hub_resubscribe() {
    hub_unsubscribe_all();
    auto& hub = datahub::DataHub::instance();
    // Registered at startup via KalshiAdapter (deferred after the first paint); a restored tile
    // can get here first.
    services::prediction::kalshi_ns::KalshiMarketProducer::instance().ensure_registered_with_hub();

    resolved_.clear();
    for (const auto& e : entries_) {
        const QString ticker = e.ticker;
        if (cache_.contains(ticker) || halted_.contains(ticker))
            resolved_.insert(ticker); // already showing something; no spinner for it
        if (halted_.contains(ticker))
            continue;
        const QString topic = services::prediction::kalshi_ns::KalshiMarketProducer::topic_for(ticker);
        hub.subscribe_errors(this, topic, [this, ticker](const QString& error) { on_market_error(ticker, error); });
        hub.subscribe<services::prediction::PredictionMarket>(
            this, topic, [this, ticker](const services::prediction::PredictionMarket& m) { on_market(ticker, m); });
        subscribed_topics_.append(topic);
        // subscribe() only hands over a fresh value; show a stale last-known-good one meanwhile (P11).
        if (!cache_.contains(ticker)) {
            const QVariant cached = hub.peek_raw(topic);
            if (cached.canConvert<services::prediction::PredictionMarket>())
                on_market(ticker, cached.value<services::prediction::PredictionMarket>());
        }
    }
    hub_active_ = true;
    update_summary();
}

void KalshiPricesWidget::hub_unsubscribe_all() {
    // Topic-scoped: never drop a subscription BaseWidget itself may own.
    auto& hub = datahub::DataHub::instance();
    for (const QString& topic : std::as_const(subscribed_topics_)) {
        hub.unsubscribe(this, topic);
        hub.unsubscribe_errors(this, topic);
    }
    subscribed_topics_.clear();
    hub_active_ = false;
}

void KalshiPricesWidget::refresh_data() {
    // Retry everything, including tickers dropped as unknown.
    halted_.clear();
    errors_.clear();
    if (!isVisible()) {
        hub_active_ = false; // "refresh all" reaches hidden tiles too: subscribe on the next show (P3)
        return;
    }
    hub_resubscribe();
    if (!subscribed_topics_.isEmpty())
        datahub::DataHub::instance().request(subscribed_topics_, /*force=*/true);
}

void KalshiPricesWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!hub_active_)
        hub_resubscribe();
}

void KalshiPricesWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e); // drops every hub subscription this tile owns
    subscribed_topics_.clear();
    hub_active_ = false;
}

void KalshiPricesWidget::on_market(const QString& ticker, const services::prediction::PredictionMarket& market) {
    Snapshot s;
    s.question = market.question;
    s.yes_sub_title = market.extras.value(QStringLiteral("yes_sub_title")).toString();
    s.status = market.extras.value(QStringLiteral("status")).toString().toLower();
    s.result = market.extras.value(QStringLiteral("result")).toString().toLower();
    s.last = market.extras.value(QStringLiteral("last_price_dollars")).toDouble();
    s.previous = market.extras.value(QStringLiteral("previous_price_dollars")).toDouble();
    s.bid = market.outcomes.isEmpty() ? 0.0 : market.outcomes.first().price;
    s.ask = market.extras.value(QStringLiteral("yes_ask_dollars")).toDouble();
    cache_.insert(ticker, s);
    errors_.remove(ticker);
    resolved_.insert(ticker);
    last_update_ = QDateTime::currentDateTime();
    render_row(ticker);
    // One delivery per ticker per refresh: collapse the burst into one summary pass.
    schedule_render([this]() { update_summary(); });
}

void KalshiPricesWidget::on_market_error(const QString& ticker, const QString& error) {
    errors_.insert(ticker, error);
    resolved_.insert(ticker);
    if (kalshi_prices_is_permanent(error)) {
        // Stop the hub re-requesting a ticker Kalshi does not know (every min_interval while visible).
        const QString topic = services::prediction::kalshi_ns::KalshiMarketProducer::topic_for(ticker);
        auto& hub = datahub::DataHub::instance();
        hub.unsubscribe(this, topic);
        hub.unsubscribe_errors(this, topic);
        subscribed_topics_.removeAll(topic);
        halted_.insert(ticker);
    }
    render_row(ticker);
    schedule_render([this]() { update_summary(); });
}

void KalshiPricesWidget::update_summary() {
    if (entries_.isEmpty()) {
        set_loading(false);
        return;
    }
    const int total = static_cast<int>(entries_.size());
    int answered = 0;
    int live = 0;
    QString failure; // a network / HTTP failure, as opposed to unknown tickers
    for (const auto& e : entries_) {
        if (resolved_.contains(e.ticker))
            ++answered;
        if (cache_.contains(e.ticker))
            ++live;
        const QString err = errors_.value(e.ticker);
        if (!err.isEmpty() && !kalshi_prices_is_permanent(err) && failure.isEmpty())
            failure = kalshi_prices_strip_code(err);
    }
    if (answered < total) {
        // Some market has not answered since the last (re)subscribe and has nothing cached:
        // determinate "LOADING x / y" overlay.
        set_loading_progress(answered, total);
    } else if (live == 0) {
        // Every market answered and none has data to show.
        if (!failure.isEmpty())
            set_error(tr("Kalshi is unreachable: %1\nRetrying automatically.").arg(failure.left(160)));
        else
            set_error(tr("None of these Kalshi markets exist any more. Edit the list with the gear icon."));
        if (status_label_)
            status_label_->clear();
        return;
    } else {
        set_loading(false); // all answered: drop the overlay, including an earlier error
    }

    if (!status_label_)
        return;
    const QString when = last_update_.isValid() ? last_update_.toString(QStringLiteral("HH:mm:ss")) : QString();
    if (!failure.isEmpty()) {
        status_label_->setText(
            tr("Refresh failed \xe2\x80\x94 showing prices from %1. %2").arg(when, failure.left(120)));
        kalshi_prices_set_prop(status_label_, "state", "warn");
    } else {
        status_label_->setText(tr("YES = last trade \xc2\xb7 change = 24 h, pp \xc2\xb7 %1").arg(when));
        kalshi_prices_set_prop(status_label_, "state", "ok");
    }
}

// ── Theme / i18n ────────────────────────────────────────────────────────────

void KalshiPricesWidget::on_theme_changed() {
    apply_styles();
}

void KalshiPricesWidget::apply_styles() {
    if (!rows_host_)
        return;
    // Labels with no "lvl"/"dir" state yet keep the base colour of their selector.
    rows_host_->setStyleSheet(
        QString("QLabel#kalshiPriceHint{color:%2;font-size:10px;background:transparent;padding:4px 0;}"
                "QLabel#kalshiPriceLabel{color:%1;font-size:11px;font-weight:600;background:transparent;}"
                "QLabel#kalshiPriceYes{color:%2;font-size:11px;font-weight:700;background:transparent;}"
                "QLabel#kalshiPriceYes[lvl=\"hi\"]{color:%3;}"
                "QLabel#kalshiPriceYes[lvl=\"lo\"]{color:%4;}"
                "QLabel#kalshiPriceYes[lvl=\"mid\"]{color:%1;}"
                "QLabel#kalshiPriceChange{color:%5;font-size:10px;background:transparent;}"
                "QLabel#kalshiPriceChange[dir=\"up\"]{color:%3;}"
                "QLabel#kalshiPriceChange[dir=\"down\"]{color:%4;}"
                "QLabel#kalshiPriceStatus{color:%2;font-size:9px;background:transparent;}"
                "QLabel#kalshiPriceStatus[state=\"warn\"]{color:%6;}")
            .arg(ui::colors::TEXT_PRIMARY(), ui::colors::TEXT_TERTIARY(), ui::colors::POSITIVE(),
                 ui::colors::NEGATIVE(), ui::colors::TEXT_SECONDARY(), ui::colors::WARNING()));
}

void KalshiPricesWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    set_title(tr("KALSHI"));
    build_rows(); // re-renders every row from cache_, so no price is blanked
}

} // namespace fincept::screens::widgets
