#include "screens/dashboard/widgets/MacroSeriesWidget.h"

#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "services/dbnomics/DBnomicsService.h"
#include "services/economics/EconomicsService.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFontMetrics>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QStyle>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens::widgets {

namespace {

constexpr const char* kMacroSourceFred = "fred";
constexpr const char* kMacroSourceDbn = "dbnomics";
constexpr const char* kMacroDefaultFred = "DGS10";             // 10-Year Treasury constant maturity
constexpr const char* kMacroDefaultDbn = "BLS/ln/LNS14000000"; // US civilian unemployment rate
constexpr const char* kMacroDefaultRange = "5Y";
// Caller tag for EconomicsService: keeps this tile's topic (`econ:dashboard:fred_<id>`) and
// result_ready traffic apart from the Economics screen's FRED panel, which reacts to every
// `fred`-tagged result.
constexpr const char* kMacroEconCallerTag = "dashboard";
constexpr const char* kMacroFredScript = "fred_data.py";
// Fetch window for FRED. Pinned to 1 January so the script args (and EconomicsService's
// cache key) stay the same all year; the chart range is applied client-side.
constexpr int kMacroFredHistoryYears = 10;

QString macro_norm_source(const QString& s) {
    return s.trimmed().compare(QLatin1String(kMacroSourceDbn), Qt::CaseInsensitive) == 0
               ? QString::fromLatin1(kMacroSourceDbn)
               : QString::fromLatin1(kMacroSourceFred);
}

QString macro_norm_series(const QString& source, const QString& s) {
    const QString t = s.trimmed();
    if (source == QLatin1String(kMacroSourceFred))
        return t.isEmpty() ? QString::fromLatin1(kMacroDefaultFred) : t.toUpper();
    // DBnomics codes are case-sensitive ("BLS/ln/…").
    return t.isEmpty() ? QString::fromLatin1(kMacroDefaultDbn) : t;
}

const QStringList& macro_range_ids() {
    static const QStringList ids = {QStringLiteral("1Y"), QStringLiteral("2Y"), QStringLiteral("5Y"),
                                    QStringLiteral("10Y")};
    return ids;
}

QString macro_norm_range(const QString& r) {
    const QString t = r.trimmed().toUpper();
    return macro_range_ids().contains(t) ? t : QString::fromLatin1(kMacroDefaultRange);
}

int macro_range_years(const QString& r) {
    return r.left(r.size() - 1).toInt(); // "10Y" → 10
}

QString macro_screen_for(const QString& source) {
    return source == QLatin1String(kMacroSourceDbn) ? QStringLiteral("dbnomics") : QStringLiteral("economics");
}

/// Decimals for a level and its change, from the level's magnitude.
int macro_decimals(double level) {
    const double a = std::abs(level);
    if (a >= 10000.0)
        return 0;
    if (a >= 100.0)
        return 1;
    return 2;
}

QString macro_fmt(double v, int decimals) {
    if (v != 0.0 && std::abs(v) < 0.01 && decimals <= 2)
        return QString::number(v, 'g', 3);
    return QString::number(v, 'f', decimals);
}

/// Units that are themselves a percentage: their change is shown in percentage
/// points, not as a percent of a percent.
bool macro_units_are_percent(const QString& units) {
    return units.contains(QLatin1String("percent"), Qt::CaseInsensitive) || units.contains(QLatin1Char('%'));
}

/// "[CODE] message" → "message" (EconomicsService prefixes structured script errors).
QString macro_strip_code(const QString& error) {
    if (error.startsWith(QLatin1Char('['))) {
        const int close = error.indexOf(QLatin1String("] "));
        if (close > 0)
            return error.mid(close + 2);
    }
    return error;
}

bool macro_is_missing_key(const QString& e) {
    return e.startsWith(QLatin1String("[MISSING_API_KEY]")) || e.contains(QLatin1String("API key not configured")) ||
           e.contains(QLatin1String("FRED_API_KEY"));
}

bool macro_is_invalid_key(const QString& e) {
    return e.startsWith(QLatin1String("[INVALID_API_KEY]"));
}

/// Errors a retry cannot fix: the hub would otherwise re-run the fetch every
/// min_interval (60 s) for as long as the tile is visible.
bool macro_is_permanent(const QString& e) {
    return macro_is_missing_key(e) || macro_is_invalid_key(e) || e.contains(QLatin1String("400 Client Error")) ||
           e.contains(QLatin1String("404 Client Error")) || e.startsWith(QLatin1String("HTTP_400")) ||
           e.startsWith(QLatin1String("HTTP_404")) || e.contains(QLatin1String("No data returned"));
}

// The change label's colour is a "dir" property matched by the body stylesheet; the label
// is only re-polished when the direction actually changes (no per-update setStyleSheet).
void macro_set_prop(QWidget* w, const char* name, const char* value) {
    if (w->property(name).toString() == QLatin1String(value))
        return;
    w->setProperty(name, QString::fromLatin1(value));
    w->style()->unpolish(w);
    w->style()->polish(w);
}

} // namespace

// ══════════════════════════════════════════════════════════════════════════════
//  MacroSeriesCanvas
// ══════════════════════════════════════════════════════════════════════════════

MacroSeriesCanvas::MacroSeriesCanvas(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(48);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void MacroSeriesCanvas::set_points(const QVector<MacroSeriesPoint>& points) {
    points_ = points;
    dirty_ = true;
    update();
}

void MacroSeriesCanvas::refresh_theme() {
    dirty_ = true;
    update();
}

void MacroSeriesCanvas::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    dirty_ = true;
}

void MacroSeriesCanvas::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        dirty_ = true;
        update();
    }
    QWidget::changeEvent(event);
}

void MacroSeriesCanvas::paintEvent(QPaintEvent*) {
    if (dirty_) {
        rebuild_cache();
        dirty_ = false;
    }
    QPainter p(this);
    p.drawPixmap(0, 0, cache_);
}

void MacroSeriesCanvas::rebuild_cache() {
    const int W = width();
    const int H = height();
    if (W <= 0 || H <= 0) {
        cache_ = QPixmap();
        return;
    }
    const qreal dpr = devicePixelRatioF();
    cache_ = QPixmap(qRound(W * dpr), qRound(H * dpr));
    cache_.setDevicePixelRatio(dpr);
    cache_.fill(QColor(ui::colors::BG_BASE()));

    QPainter p(&cache_);
    QFont font;
    font.setFamily(ui::fonts::DATA_FAMILY);
    font.setPixelSize(9);
    p.setFont(font);
    const QFontMetrics fm(font);

    const int n = static_cast<int>(points_.size());
    if (n < 2) {
        p.setPen(QColor(ui::colors::TEXT_SECONDARY()));
        p.drawText(QRect(0, 0, W, H), Qt::AlignCenter,
                   n == 0 ? tr("Waiting for data...") : tr("Not enough observations to plot"));
        return;
    }

    double lo = points_.first().value;
    double hi = lo;
    for (const auto& pt : points_) {
        lo = std::min(lo, pt.value);
        hi = std::max(hi, pt.value);
    }
    if (hi - lo < 1e-12) { // flat series: give it a band so it plots mid-height
        const double pad = std::max(std::abs(hi) * 0.01, 1e-6);
        lo -= pad;
        hi += pad;
    }
    const double margin = (hi - lo) * 0.08;
    lo -= margin;
    hi += margin;

    const int decimals = macro_decimals(std::max(std::abs(lo), std::abs(hi)));
    const QString hi_txt = macro_fmt(hi, decimals);
    const QString lo_txt = macro_fmt(lo, decimals);
    const int axis_w = std::max(fm.horizontalAdvance(hi_txt), fm.horizontalAdvance(lo_txt)) + 8;
    const int time_h = fm.height() + 2;
    const QRect plot(0, 2, W - axis_w, H - time_h - 2);
    if (plot.width() <= 4 || plot.height() <= 4)
        return;

    const qint64 first_day = points_.first().date.toJulianDay();
    const qint64 span_days = std::max<qint64>(1, points_.last().date.toJulianDay() - first_day);
    auto x_of = [&](const MacroSeriesPoint& pt) {
        return plot.left() +
               static_cast<double>(pt.date.toJulianDay() - first_day) / static_cast<double>(span_days) * plot.width();
    };
    auto y_of = [&](double v) { return plot.top() + (hi - v) / (hi - lo) * plot.height(); };

    // Grid + value labels: top / middle / bottom.
    p.setPen(QPen(QColor(ui::colors::BORDER_DIM()), 1, Qt::DotLine));
    for (int g = 0; g <= 2; ++g) {
        const int gy = plot.top() + plot.height() * g / 2;
        p.drawLine(plot.left(), gy, plot.right(), gy);
    }
    p.setPen(QColor(ui::colors::TEXT_SECONDARY()));
    for (int g = 0; g <= 2; ++g) {
        const double v = hi - (hi - lo) * g / 2.0;
        const int gy = plot.top() + plot.height() * g / 2;
        // Keep the top label inside the pixmap and the bottom one above the date row.
        const int ty = std::min(std::max(gy + fm.ascent() / 2, plot.top() + fm.ascent()), plot.bottom());
        p.drawText(plot.right() + 4, ty, macro_fmt(v, decimals));
    }

    // Axis lines.
    p.setPen(QPen(QColor(ui::colors::BORDER_DIM()), 1));
    p.drawLine(plot.right(), plot.top(), plot.right(), plot.bottom());
    p.drawLine(plot.left(), plot.bottom(), plot.right(), plot.bottom());

    // First / last date.
    const bool long_span = span_days > 400;
    const QString fmt = long_span ? QStringLiteral("MMM yy") : QStringLiteral("dd MMM");
    const QString d0 = QLocale().toString(points_.first().date, fmt);
    const QString d1 = QLocale().toString(points_.last().date, fmt);
    p.setPen(QColor(ui::colors::TEXT_SECONDARY()));
    const int ty = H - 2 - fm.descent();
    p.drawText(plot.left() + 2, ty, d0);
    p.drawText(plot.right() - fm.horizontalAdvance(d1), ty, d1);

    // The series line. Antialiasing on a thin stroke only (no large fills, P9).
    QPainterPath path;
    path.moveTo(x_of(points_.first()), y_of(points_.first().value));
    for (int i = 1; i < n; ++i)
        path.lineTo(x_of(points_[i]), y_of(points_[i].value));
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor(ui::colors::CYAN()), 1.4));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);

    // Latest observation marker.
    const QPointF last(x_of(points_.last()), y_of(points_.last().value));
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(ui::colors::AMBER()));
    p.drawEllipse(last, 2.5, 2.5);
}

// ══════════════════════════════════════════════════════════════════════════════
//  MacroSeriesWidget
// ══════════════════════════════════════════════════════════════════════════════

MacroSeriesWidget::MacroSeriesWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget(title_for(macro_norm_source(cfg.value("source").toString()),
                           macro_norm_series(macro_norm_source(cfg.value("source").toString()),
                                             cfg.value("series").toString())),
                 parent, ui::colors::CYAN()),
      source_(macro_norm_source(cfg.value("source").toString())),
      series_(macro_norm_series(source_, cfg.value("series").toString())),
      range_(macro_norm_range(cfg.value("range").toString())) {
    build_ui();
    update_title();
    update_link();
    apply_styles();
    render();

    set_configurable(true);
    connect(this, &BaseWidget::refresh_requested, this, &MacroSeriesWidget::refresh_data);
    set_loading(true);
}

QString MacroSeriesWidget::series_problem(const QString& source, const QString& series) {
    if (source == QLatin1String(kMacroSourceFred)) {
        static const QRegularExpression kFredId(QStringLiteral("^[A-Z0-9][A-Z0-9_.\\-]{0,63}$"));
        if (!kFredId.match(series).hasMatch())
            return tr("Enter a FRED series id such as DGS10, CPIAUCSL or UNRATE.");
        return {};
    }
    // ':' would break the `dbnomics:<provider>:<dataset>:<series>` topic split.
    static const QRegularExpression kDbnBadChar(QStringLiteral("[:\\s]"));
    const QStringList parts = series.split(QLatin1Char('/'));
    const bool ok = parts.size() == 3 && !parts[0].isEmpty() && !parts[1].isEmpty() && !parts[2].isEmpty() &&
                    !series.contains(kDbnBadChar);
    if (!ok)
        return tr("Enter a DBnomics series as PROVIDER/DATASET/SERIES, e.g. %1.")
            .arg(QString::fromLatin1(kMacroDefaultDbn));
    return {};
}

QString MacroSeriesWidget::title_for(const QString& source, const QString& series) {
    // DBnomics ids are long ("BLS/ln/LNS14000000"): the series code alone fits the title bar.
    const QString label = source == QLatin1String(kMacroSourceDbn) ? series.section(QLatin1Char('/'), -1) : series;
    return tr("MACRO \xc2\xb7 %1").arg(label);
}

void MacroSeriesWidget::build_ui() {
    auto* vl = content_layout();
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    body_ = new QWidget(this);
    body_->setObjectName(QStringLiteral("macroSeriesBody"));
    auto* bl = new QVBoxLayout(body_);
    bl->setContentsMargins(8, 4, 8, 4);
    bl->setSpacing(2);

    auto* top = new QHBoxLayout;
    top->setContentsMargins(0, 0, 0, 0);
    top->setSpacing(8);
    value_label_ = new QLabel(body_);
    value_label_->setObjectName(QStringLiteral("macroValue"));
    change_label_ = new QLabel(body_);
    change_label_->setObjectName(QStringLiteral("macroChange"));
    date_label_ = new QLabel(body_);
    date_label_->setObjectName(QStringLiteral("macroDate"));
    date_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    top->addWidget(value_label_);
    top->addWidget(change_label_);
    top->addStretch(1);
    top->addWidget(date_label_);
    bl->addLayout(top);

    name_label_ = new QLabel(body_);
    name_label_->setObjectName(QStringLiteral("macroName"));
    // Ignored: a long series title is clipped instead of widening the tile (full text in the tooltip).
    name_label_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    bl->addWidget(name_label_);

    canvas_ = new MacroSeriesCanvas(body_);
    canvas_->setAccessibleName(tr("Macro series chart"));
    bl->addWidget(canvas_, 1);

    meta_label_ = new QLabel(body_);
    meta_label_->setObjectName(QStringLiteral("macroMeta"));
    meta_label_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    bl->addWidget(meta_label_);

    vl->addWidget(body_, 1);
}

void MacroSeriesWidget::apply_styles() {
    if (!body_)
        return;
    body_->setStyleSheet(QString("QLabel#macroValue{color:%1;font-size:16px;font-weight:700;background:transparent;}"
                                 "QLabel#macroChange{color:%2;font-size:11px;font-weight:700;background:transparent;}"
                                 "QLabel#macroChange[dir=\"up\"]{color:%3;}"
                                 "QLabel#macroChange[dir=\"down\"]{color:%4;}"
                                 "QLabel#macroDate{color:%2;font-size:9px;background:transparent;}"
                                 "QLabel#macroName{color:%2;font-size:10px;background:transparent;}"
                                 "QLabel#macroMeta{color:%5;font-size:9px;background:transparent;}"
                                 "QLabel#macroMeta[state=\"warn\"]{color:%6;}")
                             .arg(ui::colors::TEXT_PRIMARY(), ui::colors::TEXT_SECONDARY(), ui::colors::POSITIVE(),
                                  ui::colors::NEGATIVE(), ui::colors::TEXT_TERTIARY(), ui::colors::WARNING()));
}

void MacroSeriesWidget::update_title() {
    set_title(title_for(source_, series_));
}

void MacroSeriesWidget::update_link() {
    const QString screen = macro_screen_for(source_);
    link_screen(body_, screen,
                screen == QLatin1String("dbnomics") ? tr("Double-click to open the DBnomics screen")
                                                    : tr("Double-click to open the Economics screen"));
}

QJsonObject MacroSeriesWidget::config() const {
    QJsonObject o;
    o.insert("source", source_);
    o.insert("series", series_);
    o.insert("range", range_);
    return o;
}

void MacroSeriesWidget::apply_config(const QJsonObject& cfg) {
    const QString source = macro_norm_source(cfg.value("source").toString());
    const QString series = macro_norm_series(source, cfg.value("series").toString());
    const QString range = macro_norm_range(cfg.value("range").toString());
    const bool series_changed = source != source_ || series != series_;
    if (!series_changed && range == range_)
        return;
    source_ = source;
    series_ = series;
    range_ = range;
    if (!series_changed) {
        render(); // range only: re-slice what is already loaded
        return;
    }

    update_title();
    update_link();
    points_.clear();
    series_title_.clear();
    units_.clear();
    frequency_.clear();
    last_error_.clear();
    halted_ = false;
    render();
    set_loading(true);
    if (isVisible())
        hub_resubscribe();
}

QDialog* MacroSeriesWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle(tr("Configure \xe2\x80\x94 Macro Series"));
    auto* form = new QFormLayout(dlg);

    auto* source = new QComboBox(dlg);
    source->addItem(tr("FRED (free API key)"), QString::fromLatin1(kMacroSourceFred));
    source->addItem(tr("DBnomics (no key)"), QString::fromLatin1(kMacroSourceDbn));
    source->setCurrentIndex(source_ == QLatin1String(kMacroSourceDbn) ? 1 : 0);
    source->setAccessibleName(tr("Data source"));
    form->addRow(tr("Source"), source);

    auto* series = new QLineEdit(dlg);
    series->setText(series_);
    series->setAccessibleName(tr("Series ID"));
    form->addRow(tr("Series"), series);

    auto* hint = new QLabel(dlg);
    hint->setWordWrap(true);
    form->addRow(hint);

    auto* range = new QComboBox(dlg);
    for (const QString& id : macro_range_ids())
        range->addItem(tr("%n year(s)", nullptr, macro_range_years(id)), id);
    range->setCurrentIndex(std::max(0, static_cast<int>(macro_range_ids().indexOf(range_))));
    range->setAccessibleName(tr("Chart range"));
    form->addRow(tr("Range"), range);

    auto* problem = new QLabel(dlg);
    problem->setWordWrap(true);
    problem->setStyleSheet(QString("color:%1;").arg(ui::colors::NEGATIVE()));
    problem->setVisible(false);
    form->addRow(problem);

    auto sync_source = [source, series, hint, problem]() {
        const QString src = source->currentData().toString();
        const bool fred = src == QLatin1String(kMacroSourceFred);
        series->setPlaceholderText(fred ? tr("e.g. DGS10, CPIAUCSL, UNRATE") : tr("PROVIDER/DATASET/SERIES"));
        hint->setText(fred ? tr("FRED needs a free API key: Settings \xe2\x80\xba Credentials \xe2\x80\xba FRED "
                                "(Federal Reserve). Series ids are listed at fred.stlouisfed.org.")
                           : tr("Any series from db.nomics.world, e.g. %1. No key needed.")
                                 .arg(QString::fromLatin1(kMacroDefaultDbn)));
        // A FRED id is never a valid DBnomics path (and vice versa): offer the source's example.
        if (!series_problem(src, macro_norm_series(src, series->text())).isEmpty())
            series->setText(QString::fromLatin1(fred ? kMacroDefaultFred : kMacroDefaultDbn));
        problem->setVisible(false);
    };
    connect(source, &QComboBox::currentIndexChanged, dlg, sync_source);
    sync_source();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    form->addRow(buttons);

    connect(buttons, &QDialogButtonBox::accepted, dlg, [this, dlg, source, series, range, problem]() {
        const QString src = source->currentData().toString();
        const QString ser = macro_norm_series(src, series->text());
        const QString why = series_problem(src, ser);
        if (!why.isEmpty()) {
            problem->setText(why);
            problem->setVisible(true);
            return;
        }
        QJsonObject cfg;
        cfg.insert("source", src);
        cfg.insert("series", ser);
        cfg.insert("range", range->currentData().toString());
        apply_config(cfg);
        emit config_changed(cfg);
        dlg->accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    return dlg;
}

// ── Hub wiring ──────────────────────────────────────────────────────────────

QString MacroSeriesWidget::prepare_topic() {
    if (source_ == QLatin1String(kMacroSourceFred)) {
        auto& svc = services::EconomicsService::instance();
        svc.ensure_registered_with_hub(); // idempotent — normally done in main.cpp already
        const QString start = QDate(QDate::currentDate().year() - kMacroFredHistoryYears, 1, 1).toString(Qt::ISODate);
        return svc.remember_topic(QString::fromLatin1(kMacroEconCallerTag), QString::fromLatin1(kMacroFredScript),
                                  QStringLiteral("series"), {series_, start}, QStringLiteral("fred_") + series_);
    }
    // main.cpp registers DBnomics in a deferred batch after the first paint; a restored tile
    // can subscribe before that, which would orphan the topic.
    services::DBnomicsService::instance().ensure_registered_with_hub();
    return QStringLiteral("dbnomics:") + series_.split(QLatin1Char('/')).join(QLatin1Char(':'));
}

void MacroSeriesWidget::hub_unsubscribe() {
    // Topic-scoped: never drop a subscription BaseWidget itself may own.
    if (!topic_.isEmpty()) {
        auto& hub = datahub::DataHub::instance();
        hub.unsubscribe(this, topic_);
        hub.unsubscribe_errors(this, topic_);
    }
    hub_active_ = false;
}

void MacroSeriesWidget::hub_resubscribe() {
    auto& hub = datahub::DataHub::instance();
    hub_unsubscribe();

    const QString problem = series_problem(source_, series_);
    if (!problem.isEmpty()) {
        set_error(problem);
        return;
    }

    topic_ = prepare_topic();
    hub.subscribe_errors(this, topic_, [this](const QString& error) { on_hub_error(error); });
    if (source_ == QLatin1String(kMacroSourceFred)) {
        hub.subscribe<services::EconomicsResult>(this, topic_,
                                                 [this](const services::EconomicsResult& r) { on_fred_result(r); });
    } else {
        hub.subscribe<services::DbnDataPoint>(this, topic_,
                                              [this](const services::DbnDataPoint& dp) { on_dbnomics_result(dp); });
    }
    hub_active_ = true;

    // subscribe() only hands over a fresh value; paint a stale last-known-good one while the
    // cold-start refresh runs rather than a spinner (P11).
    if (points_.isEmpty()) {
        const QVariant cached = hub.peek_raw(topic_);
        if (source_ == QLatin1String(kMacroSourceFred) && cached.canConvert<services::EconomicsResult>())
            on_fred_result(cached.value<services::EconomicsResult>());
        else if (source_ == QLatin1String(kMacroSourceDbn) && cached.canConvert<services::DbnDataPoint>())
            on_dbnomics_result(cached.value<services::DbnDataPoint>());
    }
}

void MacroSeriesWidget::refresh_data() {
    halted_ = false;
    set_loading(true);
    // "Refresh all" reaches hidden tiles too: a hidden tile subscribes on its next show (P3).
    if (!hub_active_ && isVisible())
        hub_resubscribe();
    if (hub_active_)
        datahub::DataHub::instance().request(topic_, /*force=*/true);
}

void MacroSeriesWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!hub_active_ && !halted_)
        hub_resubscribe();
}

void MacroSeriesWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e); // drops every hub subscription this tile owns
    hub_active_ = false;
}

// ── Data ────────────────────────────────────────────────────────────────────

void MacroSeriesWidget::on_fred_result(const services::EconomicsResult& result) {
    if (!result.success) {
        on_hub_error(result.error);
        return;
    }
    const QJsonObject d = result.data;
    // fred_data.py series: {series_id, title, units, frequency, observations:[{date, value}]}.
    QJsonArray obs = d.value("observations").toArray();
    if (obs.isEmpty())
        obs = d.value("data").toArray();

    QVector<MacroSeriesPoint> pts;
    pts.reserve(static_cast<int>(obs.size()));
    for (const auto& v : obs) {
        const QJsonObject o = v.toObject();
        const QDate date = QDate::fromString(o.value("date").toString(), Qt::ISODate);
        if (!date.isValid())
            continue;
        const QJsonValue val = o.value("value");
        double x = 0.0;
        if (val.isDouble()) {
            x = val.toDouble();
        } else {
            bool ok = false;
            x = val.toString().toDouble(&ok); // FRED marks gaps with "."
            if (!ok)
                continue;
        }
        pts.push_back(MacroSeriesPoint{date, x});
    }
    auto meta = [&d](const char* key) {
        const QString s = d.value(QLatin1String(key)).toString().trimmed();
        return s == QLatin1String("N/A") ? QString() : s;
    };
    set_series(std::move(pts), meta("title"), meta("units"), meta("frequency"));
}

void MacroSeriesWidget::on_dbnomics_result(const services::DbnDataPoint& point) {
    QVector<MacroSeriesPoint> pts;
    pts.reserve(static_cast<int>(point.observations.size()));
    for (const auto& o : point.observations) {
        if (o.valid && o.start.isValid()) // DBnomics "NA" gaps are never plotted as 0
            pts.push_back(MacroSeriesPoint{o.start, o.value});
    }
    set_series(std::move(pts), point.series_name, point.unit, point.frequency);
}

void MacroSeriesWidget::set_series(QVector<MacroSeriesPoint> points, const QString& title, const QString& units,
                                   const QString& frequency) {
    std::stable_sort(points.begin(), points.end(),
                     [](const MacroSeriesPoint& a, const MacroSeriesPoint& b) { return a.date < b.date; });
    points_ = std::move(points);
    series_title_ = title;
    units_ = units;
    frequency_ = frequency;
    last_error_.clear();
    if (points_.isEmpty()) {
        render();
        set_error(tr("No observations returned for %1.").arg(series_));
        return;
    }
    set_loading(false);
    render();
}

void MacroSeriesWidget::on_hub_error(const QString& error) {
    last_error_ = error;
    if (macro_is_permanent(error)) {
        // Stop the hub re-running a fetch that cannot succeed (every min_interval while visible).
        // The title-bar refresh or a config change subscribes again.
        hub_unsubscribe();
        halted_ = true;
    }
    if (points_.isEmpty() || halted_) {
        set_error(error_message(error));
        return;
    }
    // Last-known-good data stays on screen; say the refresh failed.
    set_loading(false);
    render();
}

QString MacroSeriesWidget::error_message(const QString& error) const {
    if (source_ == QLatin1String(kMacroSourceFred)) {
        if (macro_is_missing_key(error))
            return tr("FRED API key not set. Add a free key in Settings \xe2\x80\xba Credentials \xe2\x80\xba FRED "
                      "(Federal Reserve), then click refresh. Or switch this tile to a DBnomics series (no key) "
                      "with the gear icon.");
        if (macro_is_invalid_key(error))
            return tr("FRED rejected the API key. Check it in Settings \xe2\x80\xba Credentials \xe2\x80\xba FRED "
                      "(Federal Reserve), then click refresh.");
        if (error.startsWith(QLatin1String("[RATE_LIMITED]")))
            return tr("FRED rate limit hit \xe2\x80\x94 retrying automatically.");
    }
    if (error.contains(QLatin1String("400 Client Error")) || error.contains(QLatin1String("404 Client Error")) ||
        error.startsWith(QLatin1String("HTTP_400")) || error.startsWith(QLatin1String("HTTP_404")) ||
        error.contains(QLatin1String("No data returned")))
        return tr("Series %1 was not found. Check the id with the gear icon.").arg(series_);
    return tr("Could not load %1: %2").arg(series_, macro_strip_code(error).left(160));
}

void MacroSeriesWidget::render() {
    const QString dash = QString(QChar(0x2014));
    if (points_.isEmpty()) {
        value_label_->setText(dash);
        change_label_->clear();
        change_label_->setToolTip(QString());
        macro_set_prop(change_label_, "dir", "flat");
        date_label_->clear();
        name_label_->setText(series_);
        name_label_->setToolTip(QString());
        meta_label_->setText(source_ == QLatin1String(kMacroSourceFred) ? tr("FRED") : tr("DBnomics"));
        macro_set_prop(meta_label_, "state", "ok");
        canvas_->set_points({});
        canvas_->setAccessibleDescription(QString());
        return;
    }

    // Visible slice: the configured number of years back from the latest observation.
    const QDate cutoff = points_.last().date.addYears(-macro_range_years(range_));
    const auto first_in = std::lower_bound(points_.cbegin(), points_.cend(), cutoff,
                                           [](const MacroSeriesPoint& p, const QDate& d) { return p.date < d; });
    canvas_->set_points(QVector<MacroSeriesPoint>(first_in, points_.cend()));

    const bool pct_units = macro_units_are_percent(units_);
    const MacroSeriesPoint& latest = points_.last();
    const int dec = macro_decimals(latest.value);
    const QString value_txt = macro_fmt(latest.value, dec) + (pct_units ? QStringLiteral("%") : QString());
    value_label_->setText(value_txt);
    date_label_->setText(latest.date.toString(Qt::ISODate));

    QString change_txt;
    if (points_.size() >= 2) {
        const MacroSeriesPoint& prev = points_[points_.size() - 2];
        const double delta = latest.value - prev.value;
        const QString sign = delta > 0 ? QStringLiteral("+") : QString();
        if (pct_units) {
            change_txt = sign + macro_fmt(delta, dec) + tr(" pp");
        } else {
            change_txt = sign + macro_fmt(delta, dec);
            if (prev.value != 0.0)
                change_txt += QStringLiteral(" (%1%2%)").arg(sign).arg(delta / std::abs(prev.value) * 100.0, 0, 'f', 2);
        }
        change_label_->setText(change_txt);
        change_label_->setToolTip(tr("Change vs previous observation (%1: %2)")
                                      .arg(prev.date.toString(Qt::ISODate), macro_fmt(prev.value, dec)));
        macro_set_prop(change_label_, "dir", delta > 0 ? "up" : (delta < 0 ? "down" : "flat"));
    } else {
        change_label_->clear();
        change_label_->setToolTip(QString());
        macro_set_prop(change_label_, "dir", "flat");
    }

    name_label_->setText(series_title_.isEmpty() ? series_ : series_title_);
    QStringList tip;
    tip << (series_title_.isEmpty() ? series_ : series_title_) << series_;
    if (!units_.isEmpty())
        tip << units_;
    if (!frequency_.isEmpty())
        tip << frequency_;
    name_label_->setToolTip(tip.join(QLatin1Char('\n')));

    if (!last_error_.isEmpty()) {
        meta_label_->setText(tr("Refresh failed \xe2\x80\x94 showing last data. %1").arg(error_message(last_error_)));
        macro_set_prop(meta_label_, "state", "warn");
    } else {
        QStringList meta;
        meta << (source_ == QLatin1String(kMacroSourceFred) ? tr("FRED") : tr("DBnomics"));
        if (!units_.isEmpty())
            meta << units_;
        if (!frequency_.isEmpty())
            meta << frequency_;
        meta_label_->setText(meta.join(QStringLiteral(" \xc2\xb7 ")));
        macro_set_prop(meta_label_, "state", "ok");
    }

    canvas_->setAccessibleDescription(tr("%1: latest %2 on %3, change %4")
                                          .arg(series_title_.isEmpty() ? series_ : series_title_, value_txt,
                                               latest.date.toString(Qt::ISODate),
                                               change_txt.isEmpty() ? dash : change_txt));
}

// ── Theme / i18n ────────────────────────────────────────────────────────────

void MacroSeriesWidget::on_theme_changed() {
    apply_styles();
    if (canvas_)
        canvas_->refresh_theme();
}

void MacroSeriesWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    update_title();
    update_link();
    if (canvas_)
        canvas_->setAccessibleName(tr("Macro series chart"));
    render();
    if (is_showing_error() && !last_error_.isEmpty())
        set_error(error_message(last_error_));
}

} // namespace fincept::screens::widgets
