// src/screens/quantlib/QuantLibParamForm.cpp
//
// Typed request-body editor for the QuantLib screen. See the header for the
// field model. Styling comes from the QuantLib screen's root stylesheet
// (#qlField*, #qlForm* selectors) — nothing here calls setStyleSheet.

#include "screens/quantlib/QuantLibParamForm.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStyle>
#include <QVBoxLayout>

#include <cmath>

namespace fincept::screens {

namespace {

// Keys whose value is a count / index: fractional input is rejected client-side.
bool ql_form_is_int_key(const QString& key) {
    static const QSet<QString> kIntKeys = {
        "n",          "steps",     "paths",      "seed",       "periods",  "dim",          "order",
        "degree",     "lag",       "lags",       "window",     "levels",   "iterations",   "k",
        "max_depth",  "max_iter",  "max_lag",    "n_states",   "n_levels", "n_estimators", "n_samples_per_level",
        "useful_life_rd", "asset_index", "forecast_years",
    };
    const QString k = key.toLower();
    return kIntKeys.contains(k) || k.startsWith(QLatin1String("n_")) || k.startsWith(QLatin1String("num_")) ||
           k.endsWith(QLatin1String("_steps")) || k.endsWith(QLatin1String("_paths"));
}

bool ql_form_is_bps_key(const QString& k) {
    // The XVA spreads (risk/xva/cva) are basis points although their names do not say so.
    static const QSet<QString> kBpsSpreads = {"counterparty_spread", "own_spread", "funding_spread"};
    return k == QLatin1String("bps") || k.endsWith(QLatin1String("_bps")) || kBpsSpreads.contains(k);
}

bool ql_form_is_vol_key(const QString& k) {
    return k.contains(QLatin1String("volatility")) || k == QLatin1String("vol") || k == QLatin1String("xi") ||
           k.startsWith(QLatin1String("sigma"));
}

// A field literally named "rate" is an FX rate on money/convert and an event
// intensity (lambda) on the exponential / poisson endpoints — not an interest rate.
bool ql_form_rate_is_fx(const QString& endpoint, const QString& k) {
    return k == QLatin1String("rate") && endpoint == QLatin1String("core/types/money/convert");
}

bool ql_form_rate_is_intensity(const QString& endpoint, const QString& k) {
    return k == QLatin1String("rate") &&
           (endpoint.contains(QLatin1String("/exponential/")) || endpoint.contains(QLatin1String("/poisson/")));
}

bool ql_form_is_rate_key(const QString& endpoint, const QString& k) {
    if (ql_form_rate_is_fx(endpoint, k) || ql_form_rate_is_intensity(endpoint, k))
        return false;
    static const QSet<QString> kRateKeys = {"r",      "q",           "q1",     "q2",        "mu",
                                            "drift",  "yield",       "spread", "wacc",      "terminal_growth",
                                            "growth", "inflation",   "hurdle", "r0",        "dividend_yield",
                                            "cost_of_equity", "cost_of_debt", "tax_rate", "coupon"};
    return kRateKeys.contains(k) || k.endsWith(QLatin1String("_rate")) || k == QLatin1String("rate");
}

bool ql_form_is_time_key(const QString& k) {
    static const QSet<QString> kTimeKeys = {"t",          "time",      "time_to_maturity", "t_expiry", "maturity",
                                            "years",      "query_tenor", "start_tenor",    "end_tenor", "t_start",
                                            "t_end",      "t1",        "t2",               "expiry",    "tenor"};
    return kTimeKeys.contains(k);
}

bool ql_form_is_probability_key(const QString& k) {
    static const QSet<QString> kProbKeys = {"confidence", "confidence_level", "p",  "pd",
                                            "lgd",        "threshold_quantile", "quantile"};
    return kProbKeys.contains(k);
}

// Unit hint for a scalar numeric field. Deliberately says nothing about
// compounding: that is endpoint-specific and the API docs own it.
QString ql_form_unit_hint(const QString& endpoint, const QString& key) {
    const QString k = key.toLower();
    if (ql_form_is_bps_key(k))
        return QuantLibParamForm::tr("basis points");
    if (k == QLatin1String("bump_size") && endpoint.startsWith(QLatin1String("risk/sensitivities/")))
        return QuantLibParamForm::tr("decimal · 0.0001 = 1 bp");
    if (k == QLatin1String("weight_change"))
        return QuantLibParamForm::tr("increment to the asset's weight");
    if (k == QLatin1String("kwh_generated"))
        return QuantLibParamForm::tr("kWh");
    if (k.endsWith(QLatin1String("_mw")))
        return QuantLibParamForm::tr("MW");
    if (k == QLatin1String("heat_rate_btu"))
        return QuantLibParamForm::tr("total fuel, BTU");
    if (k == QLatin1String("normal_volatility") || k == QLatin1String("normal_vol"))
        return QuantLibParamForm::tr("absolute, price units");
    if ((k == QLatin1String("v0") || k == QLatin1String("theta")) && endpoint.contains(QLatin1String("heston")))
        return QuantLibParamForm::tr("variance · 0.04 = 20% vol");
    if (ql_form_is_vol_key(k))
        return QuantLibParamForm::tr("decimal · 0.20 = 20% (20% accepted)");
    if (ql_form_is_probability_key(k))
        return QuantLibParamForm::tr("probability 0–1");
    if (ql_form_rate_is_fx(endpoint, k))
        return QuantLibParamForm::tr("FX rate · units of to-currency per from-currency");
    if (ql_form_rate_is_intensity(endpoint, k))
        return QuantLibParamForm::tr("events per unit time (λ)");
    if (ql_form_is_rate_key(endpoint, k))
        return QuantLibParamForm::tr("decimal · 0.05 = 5% (5% accepted)");
    if (ql_form_is_time_key(k))
        return QuantLibParamForm::tr("years");
    if (k == QLatin1String("rho") || k == QLatin1String("correlation"))
        return QuantLibParamForm::tr("correlation −1 to 1");
    if (k == QLatin1String("n_steps") || k == QLatin1String("steps"))
        return QuantLibParamForm::tr("time steps");
    if (k == QLatin1String("n_paths") || k == QLatin1String("paths"))
        return QuantLibParamForm::tr("simulated paths");
    if (k == QLatin1String("seed"))
        return QuantLibParamForm::tr("RNG seed, optional");
    if (k == QLatin1String("kappa"))
        return QuantLibParamForm::tr("mean reversion, per year");
    if (k == QLatin1String("lam") || k == QLatin1String("intensity"))
        return QuantLibParamForm::tr("events per year");
    if (k == QLatin1String("strike"))
        return QuantLibParamForm::tr("same units as spot / forward");
    static const QSet<QString> kPriceKeys = {"spot", "s0", "s", "s1", "s2", "forward", "f1", "f2",
                                             "price", "market_price", "barrier", "futures_price", "clean_price"};
    if (kPriceKeys.contains(k))
        return QuantLibParamForm::tr("price");
    static const QSet<QString> kMoneyKeys = {"notional", "portfolio_value", "amount", "principal",
                                             "face_value", "ead", "investment", "pre_money", "par_value"};
    if (kMoneyKeys.contains(k))
        return QuantLibParamForm::tr("currency amount");
    return {};
}

// Known enumerations, taken from the server handlers. Free text everywhere else.
QStringList ql_form_choices(const QString& endpoint, const QString& key) {
    if (key == QLatin1String("option_type"))
        return {"call", "put"}; // the server treats anything but "call" as a put
    if (key == QLatin1String("method") && endpoint.startsWith(QLatin1String("pricing/binomial/")))
        return {"CRR", "JR", "LR", "TIAN"};
    if (key == QLatin1String("direction") && endpoint == QLatin1String("pricing/bachelier/vol-conversion"))
        return {"lognormal_to_normal", "normal_to_lognormal"};
    if (key == QLatin1String("convention") && endpoint.startsWith(QLatin1String("core/conventions/")))
        return {"actual", "365", "360", "business"};
    if (key == QLatin1String("unit") && endpoint == QLatin1String("core/types/rate/convert"))
        return {"decimal", "percentage", "bps"};
    if (key == QLatin1String("unit") && endpoint == QLatin1String("core/conventions/normalize-rate"))
        return {"decimal", "percent", "bps"};
    if (key == QLatin1String("vol_type"))
        return {"lognormal", "normal"};
    // The server lists linear_amortizing / mortgage_style / bullet_amortizing too, but its handler calls the
    // library without the dates those schedules need and answers HTTP 500 (live and overhaul API alike).
    if (key == QLatin1String("schedule_type"))
        return {"constant"};
    if (key == QLatin1String("distribution") && endpoint == QLatin1String("stochastic/sampling/distribution"))
        return {"exponential", "gamma", "poisson"};
    if (key == QLatin1String("distribution") && endpoint == QLatin1String("stochastic/sampling/jump"))
        return {"normal", "double_exp", "uniform"};
    if (key == QLatin1String("game_name"))
        return {"prisoners_dilemma", "battle_of_sexes", "chicken", "stag_hunt", "matching_pennies"};
    if (key == QLatin1String("system") && endpoint == QLatin1String("numerical/ode/solve"))
        return {"exponential_decay", "harmonic", "lotka_volterra", "van_der_pol"};
    if (key == QLatin1String("system") && endpoint == QLatin1String("numerical/roots/find-nd"))
        return {"linear_2x2", "nonlinear_2x2"};
    if (key == QLatin1String("model") && endpoint == QLatin1String("numerical/least-squares/fit"))
        return {"exponential_fit", "polynomial_fit"};
    // Multi-variable test functions (_get_multi_func); a scalar name such as "sin" is a 400 here.
    if (key == QLatin1String("func_name") && (endpoint == QLatin1String("numerical/differentiation/gradient") ||
                                              endpoint == QLatin1String("numerical/differentiation/hessian") ||
                                              endpoint == QLatin1String("numerical/optimize/minimize")))
        return {"rosenbrock", "sphere", "rastrigin", "quadratic"};
    if (key == QLatin1String("model_predict") && endpoint == QLatin1String("ml/validation/interpretability"))
        return {"linear", "logistic"}; // logistic needs a 0/1 y
    if (key == QLatin1String("method") && endpoint == QLatin1String("regulatory/basel/credit-rwa"))
        return {"standardized", "irb"};
    if (key == QLatin1String("interpolation") &&
        (endpoint.startsWith(QLatin1String("curves/")) || endpoint == QLatin1String("solver/bootstrap/curve")))
        return {"linear", "log_linear", "cubic_spline"};
    if (key == QLatin1String("method") && endpoint == QLatin1String("numerical/integration/quadrature"))
        return {"simpson", "trapezoid", "gauss_legendre", "romberg", "adaptive"};
    if (key == QLatin1String("screen_type") && endpoint == QLatin1String("analysis/valuation/screen"))
        return {"value", "growth", "quality"};
    if (key == QLatin1String("goodwill_approach") &&
        endpoint == QLatin1String("analysis/valuation/proforma/adjustments"))
        return {"conservative", "moderate", "aggressive"};
    return {};
}

// Numeric fields the server treats as absent when null: an empty editor sends
// null instead of being refused. On the pro-forma endpoint each of these
// switches a section on, so clearing it skips that section.
bool ql_form_is_optional(const QString& endpoint, const QString& key) {
    if (endpoint == QLatin1String("analysis/valuation/proforma/adjustments")) {
        static const QSet<QString> kSectionInputs = {"goodwill",           "pension_obligation", "pension_plan_assets",
                                                     "stock_compensation", "diluted_shares",     "stock_price"};
        return kSectionInputs.contains(key);
    }
    return false;
}

QString ql_form_number_text(double v) {
    return QString::number(v, 'g', 15);
}

// Checks across fields that the server does not make itself (it answers them
// with a bare HTTP 500). Returns the reason a body cannot be sent, naming the
// field in `*field`, or an empty string.
QString ql_form_cross_check(const QString& endpoint, const QJsonObject& body, QString* field) {
    if (endpoint == QLatin1String("numerical/ode/solve")) {
        // _get_ode_system(): exponential_decay is 1-dimensional, the others 2-dimensional.
        const QString system = body.value(QStringLiteral("system")).toString();
        const int dim = system == QLatin1String("exponential_decay") ? 1 : 2;
        const int got = int(body.value(QStringLiteral("y0")).toArray().size());
        if (got != dim) {
            *field = QStringLiteral("y0");
            return QuantLibParamForm::tr("y0 needs %n initial value(s) for the %1 system (it has %2), e.g. %3.", "",
                                         dim)
                .arg(system)
                .arg(got)
                .arg(dim == 1 ? QStringLiteral("[1.0]") : QStringLiteral("[1.0, 0.0]"));
        }
    }
    if (endpoint == QLatin1String("ml/validation/interpretability") &&
        body.value(QStringLiteral("model_predict")).toString() == QLatin1String("logistic")) {
        // The server refuses it too (HTTP 400); saying so here keeps the inputs and names the fix.
        for (const QJsonValue& v : body.value(QStringLiteral("y")).toArray()) {
            if (v.toDouble(-1.0) != 0.0 && v.toDouble(-1.0) != 1.0) {
                *field = QStringLiteral("y");
                return QuantLibParamForm::tr("A logistic model needs y as 0/1 labels — y has %1. Use the linear "
                                             "model for a continuous y.")
                    .arg(v.isDouble() ? QString::number(v.toDouble()) : v.toVariant().toString());
            }
        }
    }
    if (endpoint == QLatin1String("core/types/notional-schedule")) {
        const QString type = body.value(QStringLiteral("schedule_type")).toString();
        if (type != QLatin1String("constant")) {
            *field = QStringLiteral("schedule_type");
            return QuantLibParamForm::tr("schedule_type \"%1\" cannot be computed by the QuantLib service today — "
                                         "only \"constant\" schedules work.")
                .arg(type);
        }
    }
    return {};
}

// Top-level keys of a JSON object in SOURCE order. QJsonObject iterates keys
// alphabetically, which would put option_type above spot/strike.
QStringList ql_form_top_level_keys(const QString& json) {
    QStringList keys;
    int depth = 0;
    for (int i = 0; i < json.size(); ++i) {
        const QChar c = json.at(i);
        if (c == QLatin1Char('"')) {
            QString s;
            int j = i + 1;
            while (j < json.size() && json.at(j) != QLatin1Char('"')) {
                if (json.at(j) == QLatin1Char('\\') && j + 1 < json.size()) {
                    s += json.at(j + 1);
                    j += 2;
                    continue;
                }
                s += json.at(j);
                ++j;
            }
            int k = j + 1;
            while (k < json.size() && json.at(k).isSpace())
                ++k;
            if (depth == 1 && k < json.size() && json.at(k) == QLatin1Char(':') && !keys.contains(s))
                keys << s;
            i = j;
            continue;
        }
        if (c == QLatin1Char('{') || c == QLatin1Char('['))
            ++depth;
        else if (c == QLatin1Char('}') || c == QLatin1Char(']'))
            --depth;
    }
    return keys;
}

QString ql_form_compact(const QJsonValue& v) {
    if (v.isArray())
        return QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
    if (v.isObject())
        return QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
    return {};
}

// Parses a numeric editor's text. Accepts a trailing "%" on non-bps fields
// ("20%" -> 0.20), since vols and rates are quoted in percent on the desk
// while the API takes decimals.
bool ql_form_parse_number(const QString& key, QString text, double* out) {
    text = text.trimmed();
    text.remove(QLatin1Char(','));
    double scale = 1.0;
    if (text.endsWith(QLatin1Char('%')) && !ql_form_is_bps_key(key.toLower())) {
        text.chop(1);
        text = text.trimmed();
        scale = 0.01;
    }
    bool ok = false;
    const double v = text.toDouble(&ok);
    if (!ok || !std::isfinite(v))
        return false;
    *out = v * scale;
    return true;
}

} // namespace

QuantLibParamForm::QuantLibParamForm(QWidget* parent) : QWidget(parent) {
    setObjectName("qlParamForm");
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(6);

    auto* bar = new QHBoxLayout;
    bar->setSpacing(6);
    title_label_ = new QLabel(tr("PARAMETERS"));
    title_label_->setObjectName("qlLabel");
    bar->addWidget(title_label_);
    bar->addStretch(1);
    reset_btn_ = new QPushButton(tr("RESET"));
    reset_btn_->setObjectName("qlFormBtn");
    reset_btn_->setCursor(Qt::PointingHandCursor);
    reset_btn_->setToolTip(tr("Restore this endpoint's example values"));
    connect(reset_btn_, &QPushButton::clicked, this, &QuantLibParamForm::reset_to_defaults);
    bar->addWidget(reset_btn_);
    mode_btn_ = new QPushButton(tr("JSON"));
    mode_btn_->setObjectName("qlFormBtn");
    mode_btn_->setCursor(Qt::PointingHandCursor);
    mode_btn_->setCheckable(true);
    mode_btn_->setToolTip(tr("Edit the raw JSON request body (add optional fields the form does not show)"));
    connect(mode_btn_, &QPushButton::toggled, this, &QuantLibParamForm::set_json_mode);
    bar->addWidget(mode_btn_);
    root->addLayout(bar);

    stack_ = new QStackedWidget(this);

    fields_page_ = new QWidget(this);
    grid_ = new QGridLayout(fields_page_);
    grid_->setContentsMargins(0, 0, 0, 0);
    grid_->setHorizontalSpacing(8);
    grid_->setVerticalSpacing(6);
    grid_->setColumnStretch(1, 1);
    stack_->addWidget(fields_page_);

    json_edit_ = new QPlainTextEdit(this);
    json_edit_->setObjectName("qlJsonEdit");
    json_edit_->setMinimumHeight(160);
    json_edit_->setAccessibleName(tr("Request body (JSON)"));
    stack_->addWidget(json_edit_);

    empty_label_ = new QLabel(tr("This endpoint takes no parameters."), this);
    empty_label_->setObjectName("qlFieldHint");
    stack_->addWidget(empty_label_);

    root->addWidget(stack_);

    warn_label_ = new QLabel(this);
    warn_label_->setObjectName("qlFormWarn");
    warn_label_->setWordWrap(true);
    warn_label_->hide();
    root->addWidget(warn_label_);
}

void QuantLibParamForm::retranslate() {
    title_label_->setText(tr("PARAMETERS"));
    reset_btn_->setText(tr("RESET"));
    reset_btn_->setToolTip(tr("Restore this endpoint's example values"));
    mode_btn_->setText(tr("JSON"));
    mode_btn_->setToolTip(tr("Edit the raw JSON request body (add optional fields the form does not show)"));
    empty_label_->setText(tr("This endpoint takes no parameters."));
    json_edit_->setAccessibleName(tr("Request body (JSON)"));
    if (!json_mode_ && takes_body_ && !fields_.isEmpty()) {
        // Hints are translated strings: rebuild the rows from the current values.
        QJsonObject current;
        QString err;
        QWidget* bad = nullptr;
        if (collect(&current, &err, &bad))
            rebuild(current);
    }
    update_warnings();
}

void QuantLibParamForm::set_endpoint(const QString& endpoint, const QString& example_json,
                                     const QJsonObject& values, bool takes_body) {
    endpoint_ = endpoint;
    defaults_ = QJsonDocument::fromJson(example_json.toUtf8()).object();
    key_order_ = ql_form_top_level_keys(example_json);
    takes_body_ = takes_body;

    const QJsonObject shown = values.isEmpty() ? defaults_ : values;
    const bool no_params = !takes_body || (defaults_.isEmpty() && shown.isEmpty());
    mode_btn_->setEnabled(takes_body);
    reset_btn_->setEnabled(!no_params);

    if (!takes_body) {
        // GET endpoint: nothing to send.
        json_mode_ = false;
        {
            const QSignalBlocker block(mode_btn_);
            mode_btn_->setChecked(false);
        }
        rebuild(QJsonObject{});
        stack_->setCurrentIndex(2);
        update_warnings();
        return;
    }

    rebuild(shown); // the field model also backs the switch out of JSON mode
    if (json_mode_) {
        json_edit_->setPlainText(QString::fromUtf8(QJsonDocument(shown).toJson(QJsonDocument::Indented)));
        stack_->setCurrentIndex(1);
    } else {
        stack_->setCurrentIndex(no_params ? 2 : 0);
    }
    update_warnings();
}

void QuantLibParamForm::rebuild(const QJsonObject& values) {
    // Clear the grid (widgets are owned by fields_page_; delete them now so a
    // rapid endpoint switch never leaves stale editors behind).
    while (QLayoutItem* item = grid_->takeAt(0)) {
        if (QWidget* w = item->widget())
            w->deleteLater();
        delete item;
    }
    fields_.clear();

    // Field order: the example's keys in source order, then any extra keys the
    // user added in JSON mode. A rebuild() with no example (GET endpoints)
    // produces no rows.
    QStringList keys;
    for (const QString& k : key_order_) {
        if (defaults_.contains(k))
            keys << k;
    }
    for (const QString& k : defaults_.keys()) {
        if (!keys.contains(k))
            keys << k;
    }
    for (auto it = values.begin(); it != values.end(); ++it) {
        if (!keys.contains(it.key()))
            keys << it.key();
    }

    int row = 0;
    for (const QString& key : keys) {
        Field f;
        f.key = key;
        const QJsonValue def = defaults_.value(key);
        const QJsonValue val = values.contains(key) ? values.value(key) : def;
        const QJsonValue type_src = def.isUndefined() ? val : def;

        if (!ql_form_choices(endpoint_, key).isEmpty())
            f.kind = Kind::Choice;
        else if (type_src.isBool())
            f.kind = Kind::Bool;
        else if (type_src.isDouble())
            f.kind = (ql_form_is_int_key(key) && std::floor(type_src.toDouble()) == type_src.toDouble())
                         ? Kind::Integer
                         : Kind::Number;
        else if (type_src.isArray() || type_src.isObject())
            f.kind = Kind::Json;
        else if (type_src.isNull()) {
            f.kind = ql_form_is_int_key(key) ? Kind::Integer : Kind::Number;
            f.nullable = true;
        } else
            f.kind = Kind::Text;
        if ((f.kind == Kind::Number || f.kind == Kind::Integer) && ql_form_is_optional(endpoint_, key))
            f.nullable = true;

        auto* label = new QLabel(key, fields_page_);
        label->setObjectName("qlFieldLabel");
        QWidget* editor = make_editor(f, val);
        label->setBuddy(editor);

        QString hint;
        if (f.kind == Kind::Number || f.kind == Kind::Integer)
            hint = ql_form_unit_hint(endpoint_, key);
        else if (f.kind == Kind::Json)
            hint = type_src.isArray() ? tr("JSON array") : tr("JSON object");
        if (f.nullable)
            hint = hint.isEmpty() ? tr("optional") : hint;
        auto* hint_label = new QLabel(hint, fields_page_);
        hint_label->setObjectName("qlFieldHint");

        grid_->addWidget(label, row, 0);
        grid_->addWidget(editor, row, 1);
        grid_->addWidget(hint_label, row, 2);
        fields_.append(f);
        ++row;
    }
}

QWidget* QuantLibParamForm::make_editor(Field& field, const QJsonValue& value) {
    QWidget* editor = nullptr;
    switch (field.kind) {
        case Kind::Bool: {
            auto* cb = new QCheckBox(fields_page_);
            cb->setChecked(value.toBool());
            connect(cb, &QCheckBox::toggled, this, &QuantLibParamForm::update_warnings);
            editor = cb;
            break;
        }
        case Kind::Choice: {
            auto* combo = new QComboBox(fields_page_);
            combo->addItems(ql_form_choices(endpoint_, field.key));
            const QString current = value.isString() ? value.toString() : value.toVariant().toString();
            int idx = combo->findText(current, Qt::MatchFixedString);
            if (idx < 0 && !current.isEmpty()) {
                combo->addItem(current); // keep a custom value rather than silently replacing it
                idx = combo->count() - 1;
            }
            combo->setCurrentIndex(idx < 0 ? 0 : idx);
            editor = combo;
            break;
        }
        case Kind::Number:
        case Kind::Integer:
        case Kind::Text:
        case Kind::Json: {
            auto* le = new QLineEdit(fields_page_);
            if (field.kind == Kind::Json)
                le->setText(ql_form_compact(value));
            else if (value.isDouble())
                le->setText(ql_form_number_text(value.toDouble()));
            else if (!value.isNull() && !value.isUndefined())
                le->setText(value.toVariant().toString());
            if (field.kind == Kind::Number || field.kind == Kind::Integer)
                le->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            connect(le, &QLineEdit::returnPressed, this, &QuantLibParamForm::run_requested);
            connect(le, &QLineEdit::textEdited, this, [this, le]() {
                mark_invalid(le, false);
                update_warnings();
            });
            editor = le;
            break;
        }
    }
    editor->setAccessibleName(field.key);
    field.editor = editor;
    return editor;
}

void QuantLibParamForm::mark_invalid(QWidget* editor, bool invalid) const {
    if (!editor || editor->property("invalid").toBool() == invalid)
        return;
    editor->setProperty("invalid", invalid);
    editor->style()->unpolish(editor);
    editor->style()->polish(editor);
}

bool QuantLibParamForm::collect(QJsonObject* out, QString* error, QWidget** bad_editor) const {
    QJsonObject body;
    for (const Field& f : fields_) {
        if (!f.editor)
            continue;
        switch (f.kind) {
            case Kind::Bool:
                body.insert(f.key, static_cast<QCheckBox*>(f.editor)->isChecked());
                break;
            case Kind::Choice:
                body.insert(f.key, static_cast<QComboBox*>(f.editor)->currentText());
                break;
            case Kind::Text:
                body.insert(f.key, static_cast<QLineEdit*>(f.editor)->text());
                break;
            case Kind::Number:
            case Kind::Integer: {
                const QString text = static_cast<QLineEdit*>(f.editor)->text().trimmed();
                if (text.isEmpty()) {
                    if (f.nullable) {
                        body.insert(f.key, QJsonValue::Null);
                        break;
                    }
                    *error = tr("%1 is required.").arg(f.key);
                    *bad_editor = f.editor;
                    return false;
                }
                double v = 0.0;
                if (!ql_form_parse_number(f.key, text, &v)) {
                    *error = tr("%1: \"%2\" is not a number.").arg(f.key, text);
                    *bad_editor = f.editor;
                    return false;
                }
                if (f.kind == Kind::Integer && std::floor(v) != v) {
                    *error = tr("%1 must be a whole number.").arg(f.key);
                    *bad_editor = f.editor;
                    return false;
                }
                body.insert(f.key, v);
                break;
            }
            case Kind::Json: {
                const QString text = static_cast<QLineEdit*>(f.editor)->text().trimmed();
                QJsonParseError perr{};
                const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &perr);
                if (perr.error != QJsonParseError::NoError || doc.isNull()) {
                    *error = tr("%1: invalid JSON — %2").arg(f.key, perr.errorString());
                    *bad_editor = f.editor;
                    return false;
                }
                body.insert(f.key, doc.isArray() ? QJsonValue(doc.array()) : QJsonValue(doc.object()));
                break;
            }
        }
    }
    *out = body;
    return true;
}

bool QuantLibParamForm::body(QJsonObject* out, QString* error) {
    if (!takes_body_) {
        *out = QJsonObject{};
        return true;
    }
    if (json_mode_) {
        const QString text = json_edit_->toPlainText().trimmed();
        if (text.isEmpty()) {
            *out = QJsonObject{};
            return true;
        }
        QJsonParseError perr{};
        const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &perr);
        if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
            *error = perr.error != QJsonParseError::NoError
                         ? tr("Invalid JSON in the request body — %1 (offset %2).").arg(perr.errorString()).arg(perr.offset)
                         : tr("The request body must be a JSON object: {\"key\": value, ...}.");
            json_edit_->setFocus();
            return false;
        }
        QString field;
        const QString reason = ql_form_cross_check(endpoint_, doc.object(), &field);
        if (!reason.isEmpty()) {
            *error = reason;
            json_edit_->setFocus();
            return false;
        }
        *out = doc.object();
        return true;
    }
    QWidget* bad = nullptr;
    if (!collect(out, error, &bad)) {
        mark_invalid(bad, true);
        if (bad)
            bad->setFocus();
        return false;
    }
    QString field;
    const QString reason = ql_form_cross_check(endpoint_, *out, &field);
    if (!reason.isEmpty()) {
        *error = reason;
        for (const Field& f : fields_) {
            if (f.key == field && f.editor) {
                mark_invalid(f.editor, true);
                f.editor->setFocus();
                break;
            }
        }
        return false;
    }
    return true;
}

bool QuantLibParamForm::peek_body(QJsonObject* out) const {
    if (!takes_body_) {
        *out = QJsonObject{};
        return true;
    }
    QString field;
    if (json_mode_) {
        const QString text = json_edit_->toPlainText().trimmed();
        if (text.isEmpty()) {
            *out = QJsonObject{};
            return true;
        }
        QJsonParseError perr{};
        const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &perr);
        if (perr.error != QJsonParseError::NoError || !doc.isObject())
            return false;
        *out = doc.object();
        return ql_form_cross_check(endpoint_, *out, &field).isEmpty();
    }
    QString error;
    QWidget* bad = nullptr;
    if (!collect(out, &error, &bad))
        return false;
    return ql_form_cross_check(endpoint_, *out, &field).isEmpty();
}

void QuantLibParamForm::set_json_mode(bool on) {
    if (on == json_mode_)
        return;
    if (on) {
        QJsonObject current;
        QString err;
        QWidget* bad = nullptr;
        if (!collect(&current, &err, &bad)) {
            // Can't serialise an invalid field: stay in form mode and say why.
            mark_invalid(bad, true);
            warn_label_->setText(err);
            warn_label_->show();
            const QSignalBlocker block(mode_btn_);
            mode_btn_->setChecked(false);
            return;
        }
        json_edit_->setPlainText(QString::fromUtf8(QJsonDocument(current).toJson(QJsonDocument::Indented)));
        json_mode_ = true;
        stack_->setCurrentIndex(1);
        json_edit_->setFocus();
    } else {
        const QString text = json_edit_->toPlainText().trimmed();
        QJsonObject parsed;
        if (!text.isEmpty()) {
            QJsonParseError perr{};
            const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &perr);
            if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
                warn_label_->setText(tr("Fix the JSON before switching back to the form: %1")
                                         .arg(perr.error != QJsonParseError::NoError
                                                  ? perr.errorString()
                                                  : tr("the body must be a JSON object")));
                warn_label_->show();
                const QSignalBlocker block(mode_btn_);
                mode_btn_->setChecked(true);
                return;
            }
            parsed = doc.object();
        }
        json_mode_ = false;
        rebuild(parsed);
        stack_->setCurrentIndex(fields_.isEmpty() ? 2 : 0);
    }
    update_warnings();
}

void QuantLibParamForm::reset_to_defaults() {
    if (json_mode_) {
        json_edit_->setPlainText(QString::fromUtf8(QJsonDocument(defaults_).toJson(QJsonDocument::Indented)));
    } else {
        rebuild(defaults_);
        stack_->setCurrentIndex(fields_.isEmpty() ? 2 : 0);
    }
    update_warnings();
}

// Soft checks: flag the classic unit slips without blocking the request (the
// server stays the judge of what it accepts).
void QuantLibParamForm::update_warnings() {
    QStringList warnings;
    if (!json_mode_) {
        for (const Field& f : fields_) {
            if (f.kind != Kind::Number || !f.editor)
                continue;
            const QString text = static_cast<QLineEdit*>(f.editor)->text().trimmed();
            double v = 0.0;
            if (text.isEmpty() || !ql_form_parse_number(f.key, text, &v))
                continue;
            const QString k = f.key.toLower();
            const bool percent_typed = text.endsWith(QLatin1Char('%'));
            if (!percent_typed && ql_form_is_vol_key(k) && !k.startsWith(QLatin1String("normal")) && v > 2.0)
                warnings << tr("%1 = %2 reads as %3% — this API takes decimals (0.20 = 20%). Type \"%2%\" if you "
                               "meant percent.")
                                .arg(f.key, text, QString::number(v * 100.0, 'g', 6));
            else if (!percent_typed && ql_form_is_rate_key(endpoint_, k) && std::fabs(v) > 1.0)
                warnings << tr("%1 = %2 reads as %3% — this API takes decimals (0.05 = 5%). Type \"%2%\" if you "
                               "meant percent.")
                                .arg(f.key, text, QString::number(v * 100.0, 'g', 6));
            else if (ql_form_is_probability_key(k) && (v < 0.0 || v > 1.0))
                warnings << tr("%1 must be between 0 and 1.").arg(f.key);
            else if (ql_form_is_time_key(k) && v <= 0.0)
                warnings << tr("%1 is a time in years and should be positive.").arg(f.key);
        }
    }
    warn_label_->setText(warnings.join(QLatin1Char('\n')));
    warn_label_->setVisible(!warnings.isEmpty());
}

} // namespace fincept::screens
