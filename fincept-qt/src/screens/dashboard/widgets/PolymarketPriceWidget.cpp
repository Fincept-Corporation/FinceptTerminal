#include "screens/dashboard/widgets/PolymarketPriceWidget.h"

#include "datahub/DataHub.h"
#include "network/http/HttpClient.h"
#include "services/polymarket/PolymarketWebSocket.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/LoadingSkeleton.h"

#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPlainTextEdit>
#include <QStyle>
#include <QVBoxLayout>

namespace fincept::screens::widgets {

namespace {

// Fresh-tile defaults: the most-traded open markets. Fetched here with a
// callback (HttpClient, scoped to the tile) rather than through
// PolymarketService::fetch_markets(), whose markets_ready is broadcast to the
// Polymarket screen and would replace the list it is showing.
// Order key: Gamma's "volumeNum" — the same one PolymarketService sorts "volume" by.
constexpr const char* kPmDefaultsUrl =
    "https://gamma-api.polymarket.com/markets?closed=false&active=true&limit=12&order=volumeNum&ascending=false";
constexpr int kPmDefaultsCount = 5;
constexpr int kPmDefaultLabelChars = 60;

// Row label for a market: its question, plus the outcome priced when that is
// not plain "Yes" (the first token is the one the row streams).
QString pm_default_label(const services::polymarket::Market& m) {
    QString label = m.question.simplified();
    const QString outcome = m.outcomes.isEmpty() ? QString() : m.outcomes.first().name.trimmed();
    if (!outcome.isEmpty() && outcome.compare(QLatin1String("yes"), Qt::CaseInsensitive) != 0)
        label += QStringLiteral(" \xc2\xb7 ") + outcome;
    if (label.size() > kPmDefaultLabelChars)
        label = label.left(kPmDefaultLabelChars - 1).trimmed() + QChar(0x2026);
    return label;
}

// The probability label's colour band ("hi" >= 65 %, "lo" <= 35 %, else "mid")
// is a property matched in the rows host's stylesheet. Every WS price message
// used to call setStyleSheet() (full CSS re-parse); now the label is only
// re-polished when the band actually changes (CLAUDE.md P7).
void pm_price_set_band(QLabel* label, const char* band) {
    if (label->property("lvl").toString() == QLatin1String(band))
        return;
    label->setProperty("lvl", QString::fromLatin1(band));
    label->style()->unpolish(label);
    label->style()->polish(label);
}
} // namespace

PolymarketPriceWidget::PolymarketPriceWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget(tr("POLYMARKET"), parent) {
    auto* vl = content_layout();
    vl->setContentsMargins(10, 8, 10, 8);
    vl->setSpacing(4);

    // Title-bar refresh = "retry" (it was wired to nothing): re-subscribing hands
    // the rows the socket's latest price / book again and makes sure every
    // configured token is being streamed.
    connect(this, &BaseWidget::refresh_requested, this, [this]() {
        if (isVisible())
            hub_resubscribe(); // showEvent() subscribes; never keep a hidden tile subscribed
    });

    set_configurable(true);
    apply_styles();
    apply_config(cfg);
}

QJsonObject PolymarketPriceWidget::config() const {
    QJsonObject o;
    // No list chosen yet (defaults not fetched): keep "entries" out, so the next
    // load still fills the tile instead of persisting an empty list.
    if (defaults_pending_ && entries_.isEmpty())
        return o;
    QJsonArray arr;
    for (const auto& e : entries_) {
        QJsonObject item;
        item.insert("asset_id", e.asset_id);
        item.insert("label", e.label);
        arr.append(item);
    }
    o.insert("entries", arr);
    return o;
}

void PolymarketPriceWidget::apply_config(const QJsonObject& cfg) {
    entries_.clear();
    // A config naming its list (even an empty one) ends any pending default
    // fetch; its late answer is dropped (defaults_req_ / defaults_pending_).
    defaults_pending_ = !cfg.contains(QStringLiteral("entries"));
    if (!defaults_pending_) {
        defaults_inflight_ = false;
        defaults_failed_ = false;
        ++defaults_req_;
        hide_empty_state();
    }
    const QJsonArray arr = cfg.value("entries").toArray();
    for (const auto& v : arr) {
        const QJsonObject obj = v.toObject();
        Entry e;
        e.asset_id = obj.value("asset_id").toString().trimmed();
        e.label = obj.value("label").toString().trimmed();
        if (e.label.isEmpty())
            e.label = e.asset_id.left(12);
        if (!e.asset_id.isEmpty())
            entries_.append(e);
    }

    build_rows();
    if (isVisible())
        hub_resubscribe();
}

void PolymarketPriceWidget::build_rows() {
    auto* vl = content_layout();
    while (QLayoutItem* it = vl->takeAt(0)) {
        if (auto* w = it->widget())
            w->deleteLater();
        else if (auto* sub = it->layout()) {
            while (QLayoutItem* ci = sub->takeAt(0)) {
                if (auto* cw = ci->widget())
                    cw->deleteLater();
                delete ci;
            }
            delete sub;
        }
        delete it;
    }
    rows_.clear();

    // One host carries the stylesheet for the hint / every row label
    // (objectName + "lvl" property selectors) — see apply_styles().
    rows_host_ = new QWidget(this);
    rows_host_->setObjectName("pmPriceRows");

    if (entries_.isEmpty()) {
        auto* host_layout = new QVBoxLayout(rows_host_);
        host_layout->setContentsMargins(0, 0, 0, 0);
        if (defaults_inflight_) {
            // The first list is on its way: placeholder lines, not a blank tile.
            auto* sk = new ui::LoadingSkeleton(ui::LoadingSkeleton::Shape::Text, rows_host_);
            sk->set_opaque(false);
            sk->set_row_count(kPmDefaultsCount);
            sk->set_message(tr("Finding the most-traded markets\xe2\x80\xa6"));
            host_layout->addWidget(sk, 1);
            vl->addWidget(rows_host_, 1);
            apply_styles();
            return;
        }
        auto* hint = new QLabel(tr("No markets configured — click gear to add"), rows_host_);
        hint->setObjectName("pmPriceHint");
        host_layout->addWidget(hint);
        vl->addWidget(rows_host_);
        vl->addStretch(1);
        apply_styles();
        return;
    }

    auto* grid = new QGridLayout(rows_host_);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(3);
    for (int i = 0; i < entries_.size(); ++i) {
        const auto& e = entries_[i];
        Row r;
        r.label = new QLabel(e.label, rows_host_);
        r.label->setObjectName("pmPriceLabel");
        r.label->setToolTip(e.asset_id);
        r.pct = new QLabel("—", rows_host_);
        r.pct->setObjectName("pmPricePct");
        r.pct->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(r.label, i, 0);
        grid->addWidget(r.pct, i, 1);
        // The label's tooltip used to be the asset id; keep it in the new one.
        link_screen(r.label, QStringLiteral("polymarket"),
                    e.asset_id + QStringLiteral(" | ") + tr("Double-click to open the Polymarket screen"));
        link_screen(r.pct, QStringLiteral("polymarket"), tr("Double-click to open the Polymarket screen"));
        rows_.insert(e.asset_id, r);
    }
    vl->addWidget(rows_host_);
    vl->addStretch(1);
    apply_styles();
}

void PolymarketPriceWidget::hub_resubscribe() {
    auto& hub = datahub::DataHub::instance();
    hub.unsubscribe(this);

    // `prediction:polymarket:price:*` is push-only: the hub never fetches it,
    // PolymarketWebSocket publishes only for tokens it has been asked to stream.
    // Nothing on the dashboard asked, so the tile sat at "—" forever unless the
    // Polymarket screen happened to be streaming the same tokens. Ask for them
    // here. The socket reference-counts subscribers per token, so each token is
    // asked for once per tile (ws_tokens_) — re-asking on every show / refresh
    // stacked references. We never unsubscribe (the stream is shared with the
    // Polymarket screen).
    QStringList token_ids;
    token_ids.reserve(entries_.size());
    for (const auto& e : entries_) {
        if (!ws_tokens_.contains(e.asset_id)) {
            ws_tokens_.insert(e.asset_id);
            token_ids.append(e.asset_id);
        }
    }
    if (!token_ids.isEmpty())
        services::polymarket::PolymarketWebSocket::instance().subscribe(token_ids);

    for (const auto& e : entries_) {
        const QString topic = QStringLiteral("prediction:polymarket:price:") + e.asset_id;
        const QString asset_copy = e.asset_id;
        hub.subscribe(this, topic, [this, asset_copy](const QVariant& v) {
            bool ok = false;
            const double p = v.toDouble(&ok);
            if (ok) {
                traded_.insert(asset_copy);
                on_price(asset_copy, p);
            }
        });
        // The price topic only carries last-trade prices, so a quiet market
        // stayed on "—" until someone traded after the tile subscribed. The
        // book (seeded by the subscribe-time snapshot, moved by every
        // price_change) gives a probability at once: its midpoint.
        const QString book_topic = QStringLiteral("prediction:polymarket:orderbook:") + e.asset_id;
        hub.subscribe(this, book_topic, [this, asset_copy](const QVariant& v) {
            if (!v.canConvert<services::polymarket::OrderBook>())
                return;
            on_book(asset_copy, v.value<services::polymarket::OrderBook>());
        });
    }
    hub_active_ = true;
}

void PolymarketPriceWidget::on_book(const QString& asset_id, const services::polymarket::OrderBook& book) {
    // Levels are sorted best-first by PolymarketWebSocket (bids high->low, asks low->high).
    if (book.bids.isEmpty() || book.asks.isEmpty())
        return;
    const double bid = book.bids.first().price;
    const double ask = book.asks.first().price;
    if (bid <= 0.0 || ask <= 0.0 || ask < bid)
        return;
    // Polymarket's own display rule: the midpoint, unless the spread is wider
    // than 10 cents — then the last trade, when there is one.
    if (ask - bid > 0.10 && traded_.contains(asset_id))
        return;
    on_price(asset_id, (bid + ask) / 2.0);
}

void PolymarketPriceWidget::hub_unsubscribe_all() {
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
}

void PolymarketPriceWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    // Fresh tile: choose its markets once, when it is first seen (class B — a
    // free public API, about a second). A failure waits for RETRY.
    if (defaults_pending_ && !defaults_inflight_ && !defaults_failed_)
        resolve_defaults();
    if (!hub_active_)
        hub_resubscribe();
}

void PolymarketPriceWidget::resolve_defaults() {
    defaults_inflight_ = true;
    defaults_failed_ = false;
    hide_empty_state();
    const quint64 id = ++defaults_req_;
    build_rows(); // the skeleton
    // `this` scopes the callback: a tile closed meanwhile never hears the answer.
    HttpClient::instance().get(
        QString::fromLatin1(kPmDefaultsUrl),
        [this, id](Result<QJsonDocument> reply) { on_defaults(id, reply); }, this);
}

void PolymarketPriceWidget::on_defaults(quint64 request_id, const Result<QJsonDocument>& reply) {
    if (request_id != defaults_req_ || !defaults_pending_)
        return; // superseded (RETRY, or the user set the list meanwhile)
    defaults_inflight_ = false;
    if (reply.is_err()) {
        const QString why = HttpClient::message_from_error(reply.error());
        show_defaults_failure(why.isEmpty() ? QString::fromStdString(reply.error()) : why);
        return;
    }
    const QJsonArray arr = reply.value().array();
    const QDateTime now = QDateTime::currentDateTimeUtc();
    QVector<Entry> picks;
    QSet<QString> seen;
    for (const auto& v : arr) {
        const auto m = services::polymarket::Market::from_json(v.toObject());
        if (m.closed || m.clob_token_ids.isEmpty() || m.question.trimmed().isEmpty())
            continue;
        // Gamma keeps markets "open" until resolution: skip ones already past their end.
        const QDateTime end = QDateTime::fromString(m.end_date, Qt::ISODate);
        if (end.isValid() && end < now)
            continue;
        const QString token = m.clob_token_ids.first().trimmed();
        if (token.isEmpty() || seen.contains(token))
            continue;
        seen.insert(token);
        picks.append(Entry{token, pm_default_label(m)});
        if (picks.size() >= kPmDefaultsCount)
            break;
    }
    if (picks.isEmpty()) {
        show_defaults_failure(tr("Polymarket returned no open markets."));
        return;
    }
    entries_ = picks;
    defaults_pending_ = false;
    build_rows();
    if (isVisible())
        hub_resubscribe();
    // Written back: the tile keeps these markets (edit them with the gear).
    emit config_changed(config());
}

void PolymarketPriceWidget::show_defaults_failure(const QString& reason) {
    defaults_failed_ = true;
    build_rows(); // drop the skeleton (no animation under the error)
    ui::EmptyState::Spec spec =
        ui::EmptyState::error_spec(tr("The list of most-traded markets could not be loaded: %1").arg(reason),
                                   tr("Could not load Polymarket markets"), tr("RETRY"));
    spec.secondary = tr("CHOOSE MARKETS");
    show_empty_state(spec, [this]() { resolve_defaults(); }, [this]() { open_config(); });
}

void PolymarketPriceWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    if (hub_active_)
        hub_unsubscribe_all();
}

void PolymarketPriceWidget::on_price(const QString& asset_id, double price) {
    auto it = rows_.find(asset_id);
    if (it == rows_.end())
        return;
    const double pct = qBound(0.0, price, 1.0) * 100.0;
    it->pct->setText(QString::number(pct, 'f', 1) + "%");
    pm_price_set_band(it->pct, pct >= 65 ? "hi" : (pct <= 35 ? "lo" : "mid"));
    set_loading(false);
}

QDialog* PolymarketPriceWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle(tr("Configure — Polymarket"));
    auto* form = new QFormLayout(dlg);

    auto* edit = new QPlainTextEdit(dlg);
    edit->setPlaceholderText(tr("One per line:  <asset_id> | <label>"));
    QStringList lines;
    for (const auto& e : entries_)
        lines.append(e.asset_id + " | " + e.label);
    edit->setPlainText(lines.join("\n"));
    edit->setFixedHeight(120);
    form->addRow(tr("Markets"), edit);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    form->addRow(buttons);

    connect(buttons, &QDialogButtonBox::accepted, dlg, [this, dlg, edit]() {
        QJsonArray arr;
        const QStringList rows = edit->toPlainText().split('\n', Qt::SkipEmptyParts);
        for (const auto& line : rows) {
            const QStringList parts = line.split('|');
            if (parts.isEmpty())
                continue;
            QJsonObject item;
            item.insert("asset_id", parts.value(0).trimmed());
            item.insert("label", parts.value(1).trimmed());
            arr.append(item);
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

void PolymarketPriceWidget::on_theme_changed() {
    apply_styles();
}

void PolymarketPriceWidget::apply_styles() {
    if (!rows_host_)
        return; // ctor runs this before the first build_rows()
    // A probability label with no "lvl" yet (still "—") keeps the tertiary colour.
    set_css_cached(rows_host_, 
        QString("QLabel#pmPriceHint{color:%2;font-size:" + fincept::ui::fonts::px(-4) + "px;background:transparent;padding:4px 0;}"
                "QLabel#pmPriceLabel{color:%1;font-size:" + fincept::ui::fonts::px(-3) + "px;font-weight:600;background:transparent;}"
                "QLabel#pmPricePct{color:%2;font-size:" + fincept::ui::fonts::px(-3) + "px;font-weight:700;background:transparent;}"
                "QLabel#pmPricePct[lvl=\"hi\"]{color:%3;}"
                "QLabel#pmPricePct[lvl=\"lo\"]{color:%4;}"
                "QLabel#pmPricePct[lvl=\"mid\"]{color:%1;}")
            .arg(ui::colors::TEXT_PRIMARY(), ui::colors::TEXT_TERTIARY(), ui::colors::POSITIVE(),
                 ui::colors::NEGATIVE()));
}

void PolymarketPriceWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    set_title(tr("POLYMARKET"));
    // Only the "no markets configured" hint is translatable; rebuilding
    // populated rows blanked every probability until the next push.
    if (entries_.isEmpty())
        build_rows();
}

} // namespace fincept::screens::widgets
