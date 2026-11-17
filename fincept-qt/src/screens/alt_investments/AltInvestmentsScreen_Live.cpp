// src/screens/alt_investments/AltInvestmentsScreen_Live.cpp
//
// A4 — LIVE DATA prefill for the analyzer forms. Free, keyless sources only, each run on a click
// (several are rate-limited), each answer said with its source and "as of":
//   * Treasury par yield at the bond's own maturity / the 3-month bill — the shared
//     services::options::CurveDataService (U.S. Treasury daily par curve, 6-hour cache);
//   * a futures curve's front / 3 / 6 / 12-month contracts — CurveDataService (Yahoo, delayed);
//   * a stock or preferred quote (+ dividend) — services::CompanySnapshotService (Yahoo, 10-minute cache);
//   * a coin's market data — CoinGecko through alt_market_snapshot.py (10-minute cache);
//   * a commodity's CFTC positioning and World Bank price history — alt_market_snapshot.py (12 hours).
// Filled values go through the fields' own editors, so they are stored, persisted and marked as
// changed inputs exactly like typing; the user can overwrite any of them.
//
// Part of the partial-class split of AltInvestmentsScreen.cpp.

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/alt_investments/AltInvestmentsScreen.h"
#include "screens/alt_investments/AltLiveMath.h"
#include "services/alt_investments/AltInvestmentsService.h"
#include "services/company_snapshot/CompanySnapshotService.h"
#include "services/options/CurveDataService.h"
#include "ui/widgets/symbol/SymbolSearchBox.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QVBoxLayout>

#include <cmath>

namespace fincept::screens {

namespace {

struct AltLiveSpec {
    QString input;          ///< "" | "root" | "equity" | "coin"
    QStringList actions;    ///< treasury | tbill | curve | context | quote | crypto
    double fixed_years = 0; ///< treasury tenor when the form has no maturity field
};

QString alt_live_tr(const char* s) {
    return QCoreApplication::translate("AltInvestmentsScreen", s);
}

bool alt_live_has(const QList<AltField>& fields, const char* key) {
    for (const AltField& f : fields)
        if (f.key == QLatin1String(key))
            return true;
    return false;
}

AltLiveSpec alt_live_spec(const QString& id, const QList<AltField>& fields) {
    AltLiveSpec s;
    const bool treasury = alt_live_has(fields, "treasury_yield");
    if (id == QLatin1String("high-yield") || id == QLatin1String("em-bonds")) {
        if (treasury)
            s.actions << QStringLiteral("treasury");
    } else if (id == QLatin1String("convertible-bonds")) {
        if (alt_live_has(fields, "stock_price")) {
            s.input = QStringLiteral("equity");
            s.actions << QStringLiteral("quote");
        }
        if (treasury)
            s.actions << QStringLiteral("treasury");
    } else if (id == QLatin1String("stable-value")) {
        if (treasury) {
            s.actions << QStringLiteral("treasury");
            s.fixed_years = 5.0;
        }
    } else if (id == QLatin1String("hedge-funds")) {
        if (alt_live_has(fields, "risk_free_rate"))
            s.actions << QStringLiteral("tbill");
    } else if (id == QLatin1String("natural-resources")) {
        s.input = QStringLiteral("root");
        s.actions << QStringLiteral("curve") << QStringLiteral("context");
    } else if (id == QLatin1String("preferred-stocks")) {
        if (alt_live_has(fields, "current_price")) {
            s.input = QStringLiteral("equity");
            s.actions << QStringLiteral("quote");
        }
    } else if (id == QLatin1String("digital-assets")) {
        s.input = QStringLiteral("coin");
        s.actions << QStringLiteral("crypto");
    }
    return s;
}

QString alt_live_button_text(const QString& action) {
    if (action == QLatin1String("treasury"))
        return alt_live_tr("TREASURY YIELD");
    if (action == QLatin1String("tbill"))
        return alt_live_tr("3-MONTH T-BILL");
    if (action == QLatin1String("curve"))
        return alt_live_tr("FUTURES CURVE");
    if (action == QLatin1String("context"))
        return alt_live_tr("MARKET CONTEXT");
    if (action == QLatin1String("quote"))
        return alt_live_tr("QUOTE");
    return alt_live_tr("COINGECKO");
}

QString alt_live_tip(const QString& action) {
    if (action == QLatin1String("treasury"))
        return alt_live_tr("Fill the Treasury par yield at this bond's maturity (U.S. Treasury daily curve, "
                           "interpolated between tenors)");
    if (action == QLatin1String("tbill"))
        return alt_live_tr("Fill the risk-free rate with the 3-month Treasury bill yield (U.S. Treasury daily curve)");
    if (action == QLatin1String("curve"))
        return alt_live_tr("Fill spot (front contract) and the contracts nearest to 3, 6 and 12 months out "
                           "(Yahoo Finance, delayed)");
    if (action == QLatin1String("context"))
        return alt_live_tr("Show speculators' positioning (CFTC Commitments of Traders) and 36 months of World Bank "
                           "monthly prices for this commodity");
    if (action == QLatin1String("quote"))
        return alt_live_tr("Fill the market price (and, for a preferred, the dividend rate) from Yahoo Finance");
    return alt_live_tr("Fill market cap, 24h volume and supply from CoinGecko's public API");
}

QString alt_live_date(const QDate& d) {
    return d.isValid() ? QLocale().toString(d, QLocale::ShortFormat) : QString();
}

QString alt_live_num(double v, int decimals) {
    return QLocale().toString(v, 'f', decimals);
}

} // namespace

QWidget* AltInvestmentsScreen::build_live_strip(const QString& analyzer_id, const AltMethod& method, QWidget* parent) {
    cancel_live();
    live_note_.clear();
    live_input_.clear();
    live_buttons_.clear();
    if (analyzer_id == QLatin1String("covered-calls")) {
        // The Derivatives tab prices the same strategy on real option chains; this analyzer stays
        // until that builder is verified, with a way across.
        auto* strip = new QWidget(parent);
        auto* hl = new QHBoxLayout(strip);
        hl->setContentsMargins(0, 0, 0, 4);
        hl->setSpacing(6);
        auto* note = new QLabel(tr("Price covered calls on live option chains, with payoff and Greeks, in the "
                                   "Derivatives tab."),
                                strip);
        note->setObjectName(QStringLiteral("altMethodNote"));
        note->setWordWrap(true);
        auto* open = new QPushButton(tr("OPEN DERIVATIVES"), strip);
        open->setObjectName(QStringLiteral("altSecondaryBtn"));
        open->setCursor(Qt::PointingHandCursor);
        connect(open, &QPushButton::clicked, this, []() {
            fincept::EventBus::instance().publish(QStringLiteral("nav.switch_screen"),
                                                  {{QStringLiteral("screen_id"), QStringLiteral("derivatives")}});
        });
        hl->addWidget(note, 1);
        hl->addWidget(open, 0, Qt::AlignTop);
        return strip;
    }
    if (method.guide)
        return nullptr;
    const AltLiveSpec spec = alt_live_spec(analyzer_id, current_fields_);
    if (spec.actions.isEmpty())
        return nullptr;

    const QJsonObject vals = inputs_.value(analyzer_id);
    auto* strip = new QWidget(parent);
    strip->setObjectName(QStringLiteral("altLiveStrip"));
    auto* vl = new QVBoxLayout(strip);
    vl->setContentsMargins(0, 0, 0, 4);
    vl->setSpacing(4);
    auto* title = new QLabel(tr("LIVE DATA (free sources, on request)"), strip);
    title->setObjectName(QStringLiteral("altFieldLabel"));
    vl->addWidget(title);

    auto* row = new QWidget(strip);
    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(6);
    if (spec.input == QLatin1String("root")) {
        auto* root = new QComboBox(row);
        static const char* const kRoots[][2] = {
            {"CL", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "WTI crude oil (CL)")},
            {"BZ", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Brent crude oil (BZ)")},
            {"NG", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Natural gas (NG)")},
            {"RB", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "RBOB gasoline (RB)")},
            {"HO", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Heating oil (HO)")},
            {"GC", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Gold (GC)")},
            {"SI", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Silver (SI)")},
            {"HG", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Copper (HG)")},
            {"ZC", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Corn (ZC)")},
            {"ZS", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Soybeans (ZS)")},
            {"ZW", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Wheat (ZW)")},
        };
        for (const auto& r : kRoots)
            root->addItem(alt_live_tr(r[1]), QString::fromLatin1(r[0]));
        const int at = root->findData(vals.value(QStringLiteral("@live_root")).toString(QStringLiteral("CL")));
        root->setCurrentIndex(at >= 0 ? at : 0);
        root->setAccessibleName(tr("Futures market"));
        connect(root, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this, root](int) { store_live(QStringLiteral("@live_root"), root->currentData().toString()); });
        hl->addWidget(root, 1);
        live_input_ = root;
    } else if (spec.input == QLatin1String("equity")) {
        services::ResolveOptions opts;
        opts.ctx.allowed_classes = {core::instrument::AssetClass::Equity, core::instrument::AssetClass::Etf};
        auto* box = new ui::SymbolSearchBox(row);
        box->set_compact(true);
        box->set_clear_on_pick(false);
        box->set_options(opts);
        box->set_raw_text_allowed(true); // preferred share tickers (JPM-PD) are often not in the index
        box->set_recent_key(QStringLiteral("alt_investments_quote"));
        box->set_placeholder(analyzer_id == QLatin1String("preferred-stocks") ? tr("Preferred ticker (e.g. JPM-PD)")
                                                                              : tr("Stock: ticker or name"));
        box->set_text(vals.value(QStringLiteral("@live_symbol")).toString());
        connect(box, &ui::SymbolSearchBox::instrument_picked, this,
                [this, box](const fincept::core::instrument::Instrument&) {
                    store_live(QStringLiteral("@live_symbol"), box->text().trimmed());
                });
        connect(box, &ui::SymbolSearchBox::text_committed_raw, this,
                [this](const QString& t) { store_live(QStringLiteral("@live_symbol"), t.trimmed()); });
        hl->addWidget(box, 1);
        live_input_ = box;
    } else if (spec.input == QLatin1String("coin")) {
        auto* coin = new QLineEdit(vals.value(QStringLiteral("@live_coin")).toString(QStringLiteral("bitcoin")), row);
        coin->setPlaceholderText(tr("CoinGecko id (bitcoin, ethereum, solana)"));
        coin->setAccessibleName(tr("CoinGecko coin id"));
        connect(coin, &QLineEdit::textEdited, this,
                [this](const QString& t) { store_live(QStringLiteral("@live_coin"), t.trimmed().toLower()); });
        connect(coin, &QLineEdit::returnPressed, this, [this]() { run_live_action(QStringLiteral("crypto")); });
        hl->addWidget(coin, 1);
        live_input_ = coin;
    }
    for (const QString& action : spec.actions) {
        auto* b = new QPushButton(alt_live_button_text(action), row);
        b->setObjectName(QStringLiteral("altSecondaryBtn"));
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(alt_live_tip(action));
        b->setProperty("altLiveYears", spec.fixed_years);
        connect(b, &QPushButton::clicked, this, [this, action]() { run_live_action(action); });
        hl->addWidget(b);
        live_buttons_ << b;
    }
    if (spec.input.isEmpty())
        hl->addStretch(1);
    vl->addWidget(row);

    auto* note = new QLabel(strip);
    note->setObjectName(QStringLiteral("altMethodNote"));
    note->setWordWrap(true);
    note->setTextInteractionFlags(Qt::TextSelectableByMouse);
    const QString last = vals.value(QStringLiteral("@live_note")).toString();
    note->setText(last);
    note->setVisible(!last.isEmpty());
    vl->addWidget(note);
    live_note_ = note;
    return strip;
}

void AltInvestmentsScreen::store_live(const QString& key, const QJsonValue& value) {
    const QString id = current_analyzer_id();
    if (id.isEmpty())
        return;
    // Not a form field: never sent to the analyzer, never marks the result as out of date.
    inputs_[id].insert(key, value);
    if (!restoring_)
        fincept::ScreenStateManager::instance().notify_changed(this);
}

bool AltInvestmentsScreen::set_live_value(const QString& key, double value) {
    auto* sp = qobject_cast<QDoubleSpinBox*>(field_widgets_.value(key).data());
    if (!sp || !std::isfinite(value))
        return false;
    sp->setValue(value); // valueChanged -> store_input: stored, persisted, signature updated
    return true;
}

void AltInvestmentsScreen::set_live_note(const QString& text, bool problem) {
    if (!live_note_)
        return;
    live_note_->setText(text);
    live_note_->setVisible(!text.isEmpty());
    live_note_->setProperty("problem", problem);
    if (!problem)
        store_live(QStringLiteral("@live_note"), text);
}

void AltInvestmentsScreen::set_live_busy(bool busy) {
    for (const auto& b : std::as_const(live_buttons_))
        if (b)
            b->setEnabled(!busy);
    if (busy && live_note_) {
        live_note_->setText(tr("Fetching…"));
        live_note_->setVisible(true);
    }
}

void AltInvestmentsScreen::cancel_live() {
    ++live_request_; // a reply to anything started before is dropped
    if (live_cancel_) {
        auto c = std::move(live_cancel_);
        live_cancel_ = nullptr;
        c();
    }
}

void AltInvestmentsScreen::run_live_action(const QString& action) {
    cancel_live();
    const quint64 req = live_request_;
    const QString analyzer = current_analyzer_id();
    auto current = [this, req, analyzer]() { return req == live_request_ && analyzer == current_analyzer_id(); };
    set_live_busy(true);
    auto done = [this](const QString& note, bool problem) {
        set_live_busy(false);
        set_live_note(note, problem);
        live_cancel_ = nullptr;
    };
    LOG_INFO("AltInvestments", QStringLiteral("Live prefill requested analyzer=%1 action=%2").arg(analyzer, action));

    // ── Treasury curve: par yield at the bond's maturity, or the 3-month bill ─────
    if (action == QLatin1String("treasury") || action == QLatin1String("tbill")) {
        double years = 0.25;
        if (action == QLatin1String("treasury")) {
            years = 5.0;
            for (const auto& b : std::as_const(live_buttons_))
                if (b && b->property("altLiveYears").toDouble() > 0)
                    years = b->property("altLiveYears").toDouble();
            if (auto* m = qobject_cast<QDoubleSpinBox*>(field_widgets_.value(QStringLiteral("maturity_years")).data()))
                years = m->value();
        }
        const QString key =
            action == QLatin1String("tbill") ? QStringLiteral("risk_free_rate") : QStringLiteral("treasury_yield");
        auto& curves = services::options::CurveDataService::instance();
        const quint64 id = curves.treasury_curve(
            false, this, [this, current, done, years, key, action](const services::options::CurveDataService::TreasuryReply& r) {
                if (!current())
                    return;
                if (!r.curve || !r.curve->ok()) {
                    done(r.message.isEmpty() ? tr("The Treasury curve could not be loaded. Enter the yield by hand.")
                                             : r.message,
                         true);
                    return;
                }
                const double y = alt_live::interpolate(r.curve->tenors, r.curve->yields, years);
                if (!set_live_value(key, y)) {
                    done(tr("The Treasury curve has no yield for that maturity. Enter it by hand."), true);
                    return;
                }
                QString note = action == QLatin1String("tbill")
                                   ? tr("Risk-free rate: 3-month T-bill %1% · %2 · %3")
                                         .arg(alt_live_num(y, 2), r.curve->source_label, alt_live_date(r.curve->date))
                                   : tr("Treasury par yield at %1 years: %2% (interpolated between curve tenors) · %3 · %4")
                                         .arg(alt_live_num(years, 1), alt_live_num(y, 2), r.curve->source_label,
                                              alt_live_date(r.curve->date));
                if (r.stale)
                    note += tr(" · saved copy (%1)").arg(r.message);
                done(note, false);
            });
        live_cancel_ = [id]() { services::options::CurveDataService::instance().cancel(id); };
        return;
    }

    // ── Futures curve: spot proxy and the 3 / 6 / 12-month contracts ─────────────
    if (action == QLatin1String("curve")) {
        auto* root_box = qobject_cast<QComboBox*>(live_input_.data());
        const QString root = root_box ? root_box->currentData().toString() : QStringLiteral("CL");
        auto& curves = services::options::CurveDataService::instance();
        const quint64 id = curves.futures_curve(
            root, 15, false, this, [this, current, done, root](const services::options::CurveDataService::FuturesReply& r) {
                if (!current())
                    return;
                if (!r.curve || !r.curve->ok()) {
                    done(r.message.isEmpty() ? tr("No futures prices for %1 right now. Enter them by hand.").arg(root)
                                             : r.message,
                         true);
                    return;
                }
                QVector<QDate> months;
                QVector<double> prices;
                for (const auto& c : r.curve->contracts) {
                    months << c.month;
                    prices << c.last;
                }
                QStringList used;
                const QDate today = QDate::currentDate();
                // Spot proxy: the continuous front contract, else the nearest listed month.
                double spot = r.curve->continuous;
                QString spot_label = root + QStringLiteral("=F");
                if (!(std::isfinite(spot) && spot > 0)) {
                    const int i = alt_live::nearest_contract(months, prices, today);
                    spot = i >= 0 ? prices.at(i) : std::nan("");
                    spot_label = i >= 0 ? r.curve->contracts.at(i).symbol : QString();
                }
                if (set_live_value(QStringLiteral("spot_price"), spot))
                    used << tr("spot = front contract %1").arg(spot_label);
                const QList<QPair<int, QString>> targets = {{3, QStringLiteral("three_month_futures")},
                                                           {6, QStringLiteral("six_month_futures")},
                                                           {12, QStringLiteral("twelve_month_futures")}};
                for (const auto& t : targets) {
                    const int i = alt_live::nearest_contract(months, prices, today.addMonths(t.first));
                    if (i >= 0 && set_live_value(t.second, prices.at(i)))
                        used << tr("%1 m = %2").arg(t.first).arg(r.curve->contracts.at(i).symbol);
                }
                if (used.isEmpty()) {
                    done(tr("The %1 curve had no usable prices. Enter them by hand.").arg(r.curve->name), true);
                    return;
                }
                done(tr("%1 (%2): %3 · %4 · %5")
                         .arg(r.curve->name, r.curve->unit, used.join(QStringLiteral(", ")), r.curve->source_label,
                              QLocale().toString(r.curve->fetched_at.toLocalTime(), QLocale::ShortFormat)),
                     false);
            });
        live_cancel_ = [id]() { services::options::CurveDataService::instance().cancel(id); };
        return;
    }

    // ── Commodity context: CFTC positioning + World Bank monthly prices ─────────
    if (action == QLatin1String("context")) {
        auto* root_box = qobject_cast<QComboBox*>(live_input_.data());
        const QString root = root_box ? root_box->currentData().toString() : QStringLiteral("CL");
        services::alt::AltCall call;
        call.script = services::alt::kMarketSnapshotScript;
        call.command = QStringLiteral("commodity_context");
        call.input = QJsonObject{{QStringLiteral("root"), root}};
        call.timeout_ms = 120 * 1000; // the Pink Sheet workbook is a few MB on its first download of the day
        call.cache_ttl_sec = 12 * 3600;
        call.source = QStringLiteral("CFTC + World Bank");
        const quint64 id = services::alt::AltInvestmentsService::instance().call(
            call, this, [current, done](const services::alt::AltReply& r) {
                if (!current())
                    return;
                if (!r.ok) {
                    done(r.error, true);
                    return;
                }
                QStringList parts;
                const QJsonObject pos = r.data.value(QStringLiteral("positioning")).toObject();
                if (!pos.contains(QStringLiteral("error"))) {
                    const double net = pos.value(QStringLiteral("non_commercial_net")).toDouble();
                    QString p = tr("CFTC %1: speculators net %2 %3 contracts")
                                    .arg(pos.value(QStringLiteral("report_date")).toString(),
                                         net >= 0 ? tr("long") : tr("short"), QLocale().toString(qint64(std::llround(std::fabs(net)))));
                    if (pos.contains(QStringLiteral("non_commercial_net_change")))
                        p += tr(" (%1 on the week)").arg(QLocale().toString(
                            qint64(std::llround(pos.value(QStringLiteral("non_commercial_net_change")).toDouble()))));
                    parts << p;
                } else {
                    parts << pos.value(QStringLiteral("error")).toString();
                }
                const QJsonObject hist = r.data.value(QStringLiteral("history")).toObject();
                if (!hist.contains(QStringLiteral("error"))) {
                    QString h = tr("World Bank %1: %2 %3 (%4), 36-month range %5 - %6")
                                    .arg(hist.value(QStringLiteral("title")).toString(),
                                         alt_live_num(hist.value(QStringLiteral("last")).toDouble(), 2),
                                         hist.value(QStringLiteral("unit")).toString(),
                                         hist.value(QStringLiteral("last_date")).toString(),
                                         alt_live_num(hist.value(QStringLiteral("low")).toDouble(), 2),
                                         alt_live_num(hist.value(QStringLiteral("high")).toDouble(), 2));
                    if (hist.value(QStringLiteral("change_12m_pct")).isDouble())
                        h += tr(", %1% over 12 months")
                                 .arg(alt_live_num(hist.value(QStringLiteral("change_12m_pct")).toDouble(), 1));
                    parts << h;
                } else {
                    parts << hist.value(QStringLiteral("error")).toString();
                }
                parts << tr("as of %1").arg(QLocale().toString(r.as_of, QLocale::ShortFormat));
                done(parts.join(QStringLiteral(" · ")), false);
            });
        live_cancel_ = [id]() { services::alt::AltInvestmentsService::instance().cancel(id); };
        return;
    }

    // ── Stock / preferred quote ─────────────────────────────────────────────────
    if (action == QLatin1String("quote")) {
        auto* box = qobject_cast<ui::SymbolSearchBox*>(live_input_.data());
        QString symbol;
        if (box) {
            const auto inst = box->current_instrument();
            symbol = inst.is_valid() && !inst.symbol.isEmpty() ? inst.symbol : box->text().trimmed();
        }
        if (symbol.isEmpty()) {
            done(tr("Enter a ticker first."), true);
            return;
        }
        store_live(QStringLiteral("@live_symbol"), symbol);
        services::CompanySnapshotService::Options o;
        o.financials = false; // the quote side only: one call
        o.sec_fallback = false;
        const bool preferred = analyzer == QLatin1String("preferred-stocks");
        const quint64 id = services::CompanySnapshotService::instance().fetch(
            symbol, o, this, [this, current, done, preferred, symbol](const services::CompanySnapshot& s) {
                if (!current())
                    return;
                if (!s.ok || !s.has(services::CompanySnapshot::kPrice)) {
                    done(s.error.isEmpty() ? tr("No price for %1. Enter it by hand.").arg(symbol) : s.error, true);
                    return;
                }
                QStringList filled;
                if (set_live_value(preferred ? QStringLiteral("current_price") : QStringLiteral("stock_price"), s.price))
                    filled << tr("price %1 %2").arg(alt_live_num(s.price, 2), s.currency);
                if (preferred) {
                    auto* par = qobject_cast<QDoubleSpinBox*>(field_widgets_.value(QStringLiteral("par_value")).data());
                    if (s.has(services::CompanySnapshot::kDividendPerShare) && par && par->value() > 0 &&
                        set_live_value(QStringLiteral("dividend_rate"), s.dividend_per_share / par->value() * 100.0))
                        filled << tr("dividend rate from a %1 yield on the price (%2 per share a year)")
                                      .arg(alt_live_num(s.dividend_yield * 100.0, 2) + QLatin1Char('%'),
                                           alt_live_num(s.dividend_per_share, 2));
                    else
                        filled << tr("no dividend figure: enter the dividend rate by hand");
                }
                done(tr("%1 (%2): %3 · %4").arg(s.name.isEmpty() ? symbol : s.name, symbol, filled.join(QStringLiteral(", ")),
                                                s.caption()),
                     false);
            });
        live_cancel_ = [id]() { services::CompanySnapshotService::instance().cancel(id); };
        return;
    }

    // ── Coin market data ────────────────────────────────────────────────────────
    if (action == QLatin1String("crypto")) {
        auto* coin = qobject_cast<QLineEdit*>(live_input_.data());
        const QString id_text = coin ? coin->text().trimmed().toLower() : QString();
        if (id_text.isEmpty()) {
            done(tr("Enter a CoinGecko coin id first (bitcoin, ethereum)."), true);
            return;
        }
        store_live(QStringLiteral("@live_coin"), id_text);
        services::alt::AltCall call;
        call.script = services::alt::kMarketSnapshotScript;
        call.command = QStringLiteral("crypto");
        call.input = QJsonObject{{QStringLiteral("id"), id_text}};
        call.timeout_ms = 30 * 1000;
        call.cache_ttl_sec = 10 * 60; // CoinGecko's keyless tier is rate-limited
        call.source = QStringLiteral("CoinGecko");
        const quint64 id = services::alt::AltInvestmentsService::instance().call(
            call, this, [this, current, done](const services::alt::AltReply& r) {
                if (!current())
                    return;
                if (!r.ok) {
                    done(r.error, true);
                    return;
                }
                const QJsonObject d = r.data;
                QStringList filled;
                auto fill = [&](const char* field, const char* src, double scale, const QString& what) {
                    const QJsonValue v = d.value(QLatin1String(src));
                    if (v.isDouble() && set_live_value(QString::fromLatin1(field), v.toDouble() / scale))
                        filled << what;
                };
                fill("market_cap", "market_cap_usd", 1e9, tr("market cap"));
                fill("volume_24h", "volume_24h_usd", 1e9, tr("24h volume"));
                fill("circulating_supply", "circulating_supply", 1e6, tr("circulating supply"));
                if (d.value(QStringLiteral("max_supply")).isDouble())
                    fill("max_supply", "max_supply", 1e6, tr("maximum supply"));
                else
                    filled << tr("no maximum supply (uncapped): enter one to model dilution");
                done(tr("%1 (%2): %3 · CoinGecko (USD) · as of %4")
                         .arg(d.value(QStringLiteral("name")).toString(), d.value(QStringLiteral("symbol")).toString(),
                              filled.join(QStringLiteral(", ")), QLocale().toString(r.as_of, QLocale::ShortFormat)),
                     false);
            });
        live_cancel_ = [id]() { services::alt::AltInvestmentsService::instance().cancel(id); };
        return;
    }
    done(QString(), false);
}

} // namespace fincept::screens
