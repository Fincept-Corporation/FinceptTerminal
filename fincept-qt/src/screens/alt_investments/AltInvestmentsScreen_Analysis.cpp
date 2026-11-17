// src/screens/alt_investments/AltInvestmentsScreen_Analysis.cpp
//
// Running an analysis and showing it: on_analyze (run / cancel), run_analysis, cancel_analysis,
// display_result (summary line + the shared ui::ResultView), add_analyzer_widgets, and the result
// actions (copy, CSV, image).
//
// The result area is the shared ui::AutoLoadPane: replies are tied to the pane's request ids, so
// changing analyzer or method, cancelling, or starting another run makes an earlier reply unwanted,
// and it is dropped instead of being drawn under whatever is selected by then.
//
// Part of the partial-class split of AltInvestmentsScreen.cpp.

#include "core/logging/Logger.h"
#include "ui/formatting/NumberFormat.h"
#include "core/report/ReportBlocks.h"
#include "screens/alt_investments/AltInvestmentsScreen.h"
#include "services/python_cli/PythonCliService.h"
#include "storage/cache/CacheManager.h"
#include "ui/charts/ChartExport.h"
#include "ui/charts/DonutChart.h"
#include "ui/charts/RangeBar.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/AutoLoadPolicy.h"
#include "ui/widgets/ResultLayout.h"
#include "ui/widgets/ResultView.h"
#include "ui/widgets/report/AddToReportButton.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <memory>

namespace fincept::screens {

using namespace fincept::ui;

namespace {
// A run is ~3 s (interpreter start + imports); a minute means something is wrong.
constexpr int kAltRunTimeoutMs = 60'000;
// The largest analyzer reply is a few tens of KB.
constexpr qint64 kAltMaxOutputBytes = 8LL * 1024 * 1024;
const char* const kAltScript = "Analytics/alternateInvestment/cli.py";

// Calculator amounts: the user denominates them in their display currency (no
// conversion). The code is read at format time so a re-render follows a change, and it is
// the same setting as the "($)" form labels (fmt::bind_currency_label).
QString alt_money(double v, fincept::ui::formatting::Compact scheme) {
    return fincept::ui::formatting::money(v, fincept::ui::formatting::display_currency(), scheme);
}
} // namespace

// ── Run / cancel ─────────────────────────────────────────────────────────────

void AltInvestmentsScreen::on_analyze() {
    if (!pane_)
        return;
    if (loading_) {
        pane_->cancel(); // the result on screen stays; with none, the pane offers RUN
        return;
    }
    pane_->run();
}

void AltInvestmentsScreen::cancel_analysis() {
    if (pane_ && pane_->is_running())
        pane_->cancel();
}

QString AltInvestmentsScreen::view_key() const {
    const QString command = current_analyzer_id();
    if (command.isEmpty())
        return {};
    return command + QLatin1Char('|') + current_method().id;
}

QString AltInvestmentsScreen::selection_title() const {
    const AltMethod method = current_method();
    QStringList parts = {current_analyzer_name(), method.label};
    if (!method.guide) {
        const QString name = collect_form_data(nullptr).value(QStringLiteral("name")).toString();
        if (!name.isEmpty())
            parts << name;
    }
    parts.removeAll(QString());
    return parts.join(QStringLiteral("  \xB7  "));
}

void AltInvestmentsScreen::select_view() {
    const QString key = view_key();
    if (!pane_ || key.isEmpty())
        return;
    const QString title = selection_title();
    if (result_title_) {
        result_title_->setText(title);
        result_title_->setToolTip(title); // the bar cuts a long title at the action buttons
    }
    pane_->set_loading_text(current_method().guide ? tr("Opening the product guide…")
                                                   : tr("Analyzing %1…").arg(current_analyzer_name()));
    pane_->set_view(key);
    update_signature();
    update_running();
}

void AltInvestmentsScreen::update_signature() {
    // Only for the view on screen (a form rebuilt before the pane switched must not re-sign the old view).
    if (!pane_ || view_key().isEmpty() || pane_->view() != view_key())
        return;
    // The same JSON run_analysis sends (a guide has no inputs: one constant signature).
    const QJsonObject form = current_method().guide ? QJsonObject() : collect_form_data(nullptr);
    pane_->set_signature(ui::autoload::signature_of(QJsonValue(form)));
}

void AltInvestmentsScreen::update_running() {
    const bool running = pane_ && pane_->is_running() && pane_->running_view() == pane_->view();
    if (running != loading_)
        set_loading(running);
}

void AltInvestmentsScreen::run_analysis(quint64 request) {
    const QString command = current_analyzer_id();
    const AltMethod method = current_method();
    if (command.isEmpty()) {
        pane_->fail(request, tr("Pick an analyzer."));
        return;
    }

    QStringList problems;
    const QJsonObject form = method.guide ? QJsonObject() : collect_form_data(&problems);
    if (form_problems_) {
        form_problems_->setText(problems.join(QLatin1Char('\n')));
        form_problems_->setVisible(!problems.isEmpty());
    }
    // A cash-flow / return grid with no usable row: say what to enter instead of running.
    for (const AltField& f : current_fields_) {
        if ((f.type == AltField::CashFlows || f.type == AltField::Series) && form.value(f.key).toArray().isEmpty()) {
            pane_->fail(request,
                        f.type == AltField::CashFlows
                                     ? tr("Enter at least one dated capital call (and a later distribution or NAV) "
                                          "in the cash-flow grid.")
                                     : tr("Enter the monthly returns in the grid - type them, paste a column from a "
                                          "spreadsheet or import a CSV."),
                        tr("Check the inputs"));
            return;
        }
    }

    const QByteArray payload = QJsonDocument(form).toJson(QJsonDocument::Compact);
    // `--data -`: the form goes through stdin (a long cash-flow or return series must not meet the
    // command-line length limit).
    QStringList args = {command, QStringLiteral("--data"), QStringLiteral("-")};
    if (!method.id.isEmpty())
        args << QStringLiteral("--method") << method.id;

    // The cache key includes the method and the whole form: the same inputs under another method
    // are another result.
    const QString cache_key = QStringLiteral("alt_screen:%1:%2:%3").arg(command, method.id, QString::fromUtf8(payload));
    const QVariant cached = fincept::CacheManager::instance().get(cache_key);
    if (!cached.isNull()) {
        const auto doc = QJsonDocument::fromJson(cached.toString().toUtf8());
        if (doc.isObject()) {
            pane_->deliver(request, doc.object());
            return;
        }
    }

    if (cancel_token_)
        cancel_token_->store(true); // nothing else may be running for this screen
    cancel_token_ = std::make_shared<std::atomic<bool>>(false);

    services::python_cli::CliRunOptions opts;
    opts.timeout_ms = kAltRunTimeoutMs;
    opts.cancel_token = cancel_token_;
    opts.stdin_data = payload;
    opts.max_output_bytes = kAltMaxOutputBytes;

    const QString view = pane_->view();
    QPointer<AltInvestmentsScreen> self = this;
    services::python_cli::PythonCliService::instance().run(
        QString::fromLatin1(kAltScript), args, opts,
        [self, request, command, method, cache_key, payload, view](const services::python_cli::CliResult& r) {
            if (!self || !self->pane_)
                return;
            // Another run superseded it, it was cancelled, or its view was left as a default run:
            // this reply belongs to nothing on screen any more.
            if (!self->pane_->is_current(request))
                return;
            self->cancel_token_.reset();
            if (!r.success) {
                self->pane_->fail(request, r.cancelled ? AltInvestmentsScreen::tr("Analysis cancelled.")
                                                       : AltInvestmentsScreen::plain_error(r.error));
                return;
            }
            // Analyzers report "cannot compute" in-band ({"metrics": {"error": "Insufficient data"}}
            // with success=true): a failure, never cached.
            const QJsonObject metrics = r.data.contains("metrics") ? r.data["metrics"].toObject() : r.data;
            const QJsonValue inband = metrics.value("error");
            if (inband.isString() && !inband.toString().trimmed().isEmpty()) {
                self->pane_->fail(request, AltInvestmentsScreen::plain_error(inband.toString()),
                                  AltInvestmentsScreen::tr("Check the inputs"));
                return;
            }
            fincept::CacheManager::instance().put(
                cache_key, QVariant(QString::fromUtf8(QJsonDocument(r.data).toJson(QJsonDocument::Compact))),
                10 * 60, "alt_investments");
            self->pane_->deliver(request, r.data);
            // Inputs edited while it ran: the pane compares the signature it started with against the
            // current one and shows its "Inputs changed — RUN AGAIN" notice.
            Q_UNUSED(view);
            Q_UNUSED(payload);
            LOG_DEBUG("AltInvestments", "Analysis complete: " + command + " / " + method.id);
        });
}

void AltInvestmentsScreen::render_result(const QJsonValue& payload) {
    // The pane draws only the view on screen: the selection is the one the payload belongs to.
    last_title_ = selection_title();
    last_inputs_echo_ = input_echo();
    display_result(payload.toObject(), current_analyzer_id(), current_method());
}

// ── Result display ───────────────────────────────────────────────────────────

static QString stars_for(const QString& rating_str) {
    const QRegularExpression re(R"((\d+)\s*/\s*10)");
    const auto match = re.match(rating_str);
    if (!match.hasMatch())
        return rating_str;
    const int n = match.captured(1).toInt();
    const int stars = qMax(1, qMin(5, (n + 1) / 2));
    return QString("★").repeated(stars) + QString("☆").repeated(5 - stars) + "  " + rating_str;
}

fincept::ui::result::Options AltInvestmentsScreen::result_options(const QString& command) const {
    ui::result::Options o;
    o.unit_for = [](const QString&, const QString& key, double v) {
        return AltInvestmentsScreen::result_unit_for(key, v);
    };
    o.money_format = [](double v) {
        return alt_money(v, std::fabs(v) >= 1e7 ? fincept::ui::formatting::Compact::FromSettings
                                                : fincept::ui::formatting::Compact::None);
    };
    // Shown in the summary line above, or bookkeeping.
    o.hidden = {"success",     "asset_type",        "method",   "product_type", "fund_type",
                "strategy",    "analysis_category", "category", "rating",       "analysis_recommendation",
                "bottom_line", "analysis_summary",  "analysis_type", "guide",   "guide_note",
                "guide_title"};
    // Scenario tables are charted against the return that drives them.
    o.x_keys << QStringLiteral("stock_return") << QStringLiteral("index_return");
    if (command == QLatin1String("tips"))
        o.bar_columns.insert(QStringLiteral("scenarios"), QStringLiteral("real_return"));
    else if (command == QLatin1String("leveraged-funds"))
        o.bar_columns.insert(QStringLiteral("scenarios"), QStringLiteral("leveraged_final,unleveraged_final"));
    if (command == QLatin1String("private-capital"))
        o.kpi_paths = {QStringLiteral("irr"), QStringLiteral("tvpi"), QStringLiteral("dpi"), QStringLiteral("rvpi"),
                       QStringLiteral("moic")};
    if (command == QLatin1String("real-estate")) {
        const QString m = current_method().id;
        if (m == QLatin1String("financing"))
            o.kpi_paths = {QStringLiteral("levered_irr"),        QStringLiteral("dscr_year1"),
                           QStringLiteral("loan_to_value"),      QStringLiteral("cash_on_cash_year1"),
                           QStringLiteral("debt_yield"),         QStringLiteral("equity_multiple")};
        else if (m == QLatin1String("dcf"))
            o.kpi_paths = {QStringLiteral("dcf_value"), QStringLiteral("value_minus_price"),
                           QStringLiteral("unlevered_irr"), QStringLiteral("implied_cap_rate")};
        else
            o.kpi_paths = {QStringLiteral("noi"), QStringLiteral("cap_rate"), QStringLiteral("effective_gross_income"),
                           QStringLiteral("operating_expense_ratio")};
    }
    if (command == QLatin1String("high-yield"))
        o.consumed.insert(QStringLiteral("typical_spread_range_bps")); // drawn as a range below
    return o;
}

void AltInvestmentsScreen::display_result(const QJsonObject& result, const QString& command,
                                          const AltMethod& method) {
    const auto metrics = result.contains("metrics") ? result["metrics"].toObject() : result;

    // Analyzers report "cannot compute" in-band ({"metrics": {"error": "Insufficient
    // data"}} with success=true). That rendered as a normal verdict with an "Error"
    // row — say it failed instead.
    const QJsonValue inband = metrics.value("error");
    if (inband.isString() && !inband.toString().trimmed().isEmpty()) {
        display_error(plain_error(inband.toString()));
        return;
    }

    const bool guide = method.guide || metrics.value("guide").toBool() || result.value("guide").toBool();
    QString badge_text, badge_color, badge_bg, rec;
    if (guide) {
        // General product text: labelled as such, no score, no approve / reject colour.
        badge_text = tr("PRODUCT GUIDE");
        badge_color = colors::TEXT_SECONDARY();
        badge_bg = colors::BG_RAISED();
        rec = metrics.value("guide_note").toString(
            tr("General product guide: the same text for every input, not an analysis of your numbers."));
        verdict_rating_->clear();
        verdict_rating_->setVisible(false);
    } else {
        badge_text = method.label.toUpper();
        badge_color = colors::CYAN();
        badge_bg = colors::alpha(colors::CYAN, 0.15);
        // A computed method may still carry a rating ("x/10"); show it as stars.
        const QString rating = metrics.value("rating").toString(result.value("rating").toString());
        verdict_rating_->setText(rating.isEmpty() ? QString() : stars_for(rating));
        verdict_rating_->setVisible(!rating.isEmpty());
        rec = metrics.value("analysis_recommendation")
                  .toString(metrics.value("bottom_line").toString(metrics.value("analysis_summary").toString()));
    }
    verdict_badge_->setText(badge_text);
    verdict_badge_->setToolTip(badge_text);
    verdict_badge_->setStyleSheet(QString("color:%1; background:%2; font-size:" + fincept::ui::fonts::px(-1) + "px; font-weight:700;"
                                          " padding:4px 14px; letter-spacing:0.5px;")
                                      .arg(badge_color, badge_bg));
    verdict_rec_->setText(rec);

    last_result_ = result;
    last_command_ = command;
    last_method_ = method;
    result_stale_ = false;
    if (stale_note_)
        stale_note_->setVisible(false);
    if (result_title_) {
        result_title_->setText(last_title_);
        result_title_->setToolTip(last_title_);
    }

    if (!result_view_)
        return;
    result_view_->set_options(result_options(command));
    result_view_->show_result(metrics, result);
    add_analyzer_widgets(command, metrics);
    update_actions();
}

void AltInvestmentsScreen::add_analyzer_widgets(const QString& command, const QJsonObject& metrics) {
    if (!result_view_)
        return;
    if (command == QLatin1String("high-yield")) {
        // The bond's spread against the typical range of its rating.
        const QJsonArray range = metrics.value(QStringLiteral("typical_spread_range_bps")).toArray();
        const double spread = metrics.value(QStringLiteral("spread_basis_points")).toDouble(std::nan(""));
        if (range.size() != 2 || !std::isfinite(spread))
            return;
        const double lo = range.at(0).toDouble(), hi = range.at(1).toDouble();
        if (!(hi > lo))
            return;
        const double low = std::min(lo, spread), high = std::max(hi, spread);
        const double pad = (high - low) * 0.1;
        ui::RangeBarData d;
        d.low = low - pad;
        d.high = high + pad;
        d.fill_to_current = false;
        ui::RangeBand band;
        band.from = lo;
        band.to = hi;
        band.tone = ui::RangeBand::Tone::Neutral;
        band.label = tr("Typical range for the rating");
        d.bands = {band};
        ui::RangeMarker mark;
        mark.value = spread;
        mark.label = tr("This bond");
        mark.role = ui::RangeMarker::Role::Current;
        d.markers = {mark};
        auto* bar = new ui::RangeBar;
        bar->set_data(d);
        bar->set_value_formatter([](double v) { return QString::number(v, 'f', 0) + QStringLiteral(" bp"); });
        bar->setMinimumHeight(64);
        result_view_->add_custom_widget(
            ui::ResultView::make_card(tr("Credit spread"), bar, tr("vs the typical range of its rating")));
        return;
    }
    if (command == QLatin1String("private-capital")) {
        const double called = metrics.value(QStringLiteral("called_capital")).toDouble(0.0);
        const double uncalled = metrics.value(QStringLiteral("uncalled_commitment")).toDouble(0.0);
        if (called <= 0.0 && uncalled <= 0.0)
            return;
        auto* donut = new ui::DonutChart;
        ui::DonutSlice c;
        c.id = QStringLiteral("called");
        c.label = tr("Called");
        c.value = called;
        ui::DonutSlice u;
        u.id = QStringLiteral("uncalled");
        u.label = tr("Uncalled");
        u.value = uncalled;
        donut->set_slices({c, u});
        donut->set_value_formatter(
            [](double v) { return alt_money(v, fincept::ui::formatting::Compact::FromSettings); });
        donut->setMinimumHeight(200);
        result_view_->add_custom_widget(ui::ResultView::make_card(tr("Commitment"), donut, tr("called vs uncalled")));
    }
}

void AltInvestmentsScreen::update_actions() {
    const bool has = !last_result_.isEmpty() && !loading_;
    if (copy_btn_)
        copy_btn_->setEnabled(has);
    if (csv_btn_)
        csv_btn_->setEnabled(has);
    if (image_btn_)
        image_btn_->setEnabled(has);
    if (report_btn_)
        report_btn_->setEnabled(has);
}

fincept::report::ReportPayload AltInvestmentsScreen::report_payload() const {
    fincept::report::ReportPayload p;
    const QVector<ExportTable> tables = export_tables();
    if (tables.isEmpty())
        return p;
    p.title = last_title_;
    p.source = QStringLiteral("Alternative Investments");
    p.as_of = QDateTime::currentDateTime();
    // Inputs echo and the figures: the summary table holds (section, item, value); inputs go to the
    // report's "Inputs" block, the rest stays a table.
    fincept::report::PayloadTable figures;
    figures.title = tr("Figures");
    figures.columns = {tr("Section"), tr("Item"), tr("Value")};
    for (const QStringList& r : tables.first().rows) {
        if (r.value(0) == tr("Inputs"))
            p.inputs.append({r.value(1), r.value(2)});
        else
            figures.rows << r;
    }
    if (!figures.rows.isEmpty())
        p.tables << figures;
    for (int i = 1; i < tables.size(); ++i) {
        fincept::report::PayloadTable t;
        t.title = tables.at(i).title;
        t.columns = tables.at(i).headers;
        t.rows = tables.at(i).rows;
        p.tables << t;
    }
    p.notes = last_method_.guide ? tr("Product guide: general text about this kind of product, the same whatever is "
                                      "entered - not a rating of any product.")
                                 : tr("Computed locally from the inputs above (Python analytics on this computer).");
    return p;
}

// ── Copy / export ────────────────────────────────────────────────────────────

// Walk the presentation blocks of the shown result: scalar figures become (section, item, value)
// rows of the summary table, tables are kept as tables. Charts are drawn from the same tables.
static void alt_walk_blocks(const ui::result::Block& b, const QString& section, const ui::result::Options& o,
                            QVector<QStringList>& summary, QVector<std::pair<QString, ui::result::Block>>& tables) {
    using ui::result::Kind;
    const QString here = b.title.isEmpty() ? section
                         : section.isEmpty() ? b.title
                                             : section + QStringLiteral(" / ") + b.title;
    switch (b.kind) {
    case Kind::Kpis:
    case Kind::Fields:
        for (const auto& f : b.fields)
            summary << QStringList{section.isEmpty() ? QStringLiteral("Result") : section, f.label,
                                   ui::result::format_value(f.value, f.unit, o)};
        break;
    case Kind::Caption:
        summary << QStringList{section.isEmpty() ? QStringLiteral("Result") : section, b.title, b.text};
        break;
    case Kind::List:
        for (const QString& item : b.items)
            summary << QStringList{here.isEmpty() ? QStringLiteral("Result") : here, QString(), item};
        break;
    case Kind::Table:
        tables.append({here.isEmpty() ? QStringLiteral("Table") : here, b});
        break;
    case Kind::Section:
        for (const auto& child : b.children)
            alt_walk_blocks(child, here, o, summary, tables);
        break;
    default:
        break; // charts: their data is in the tables / figures
    }
}

QVector<AltInvestmentsScreen::ExportTable> AltInvestmentsScreen::export_tables() const {
    QVector<ExportTable> out;
    if (last_result_.isEmpty())
        return out;
    const QJsonObject metrics =
        last_result_.contains("metrics") ? last_result_["metrics"].toObject() : last_result_;
    const ui::result::Options o = result_options(last_command_);
    const ui::result::Block root = ui::result::build(metrics, o);

    ExportTable summary;
    summary.title = tr("Summary");
    summary.headers = {tr("Section"), tr("Item"), tr("Value")};
    // The inputs the result was computed from.
    for (auto it = last_inputs_echo_.constBegin(); it != last_inputs_echo_.constEnd(); ++it)
        summary.rows << QStringList{tr("Inputs"), it.key().section(QLatin1Char('\x1f'), 1), it.value().toString()};
    if (last_method_.guide)
        summary.rows << QStringList{tr("Note"), QString(),
                                    tr("Product guide: general text, the same for every input")};
    QVector<std::pair<QString, ui::result::Block>> tables;
    alt_walk_blocks(root, QString(), o, summary.rows, tables);
    out << summary;

    for (const auto& [title, b] : tables) {
        ExportTable t;
        t.title = title;
        t.headers = b.column_labels;
        for (int r = 0; r < b.rows.size(); ++r) {
            QStringList row;
            const auto& cells = b.rows.at(r);
            for (int c = 0; c < cells.size(); ++c) {
                ui::result::Unit unit = c < b.column_units.size() ? b.column_units.at(c) : ui::result::Unit::Plain;
                if (c > 0 && r < b.row_units.size())
                    unit = b.row_units.at(r);
                row << ui::result::format_value(cells.at(c), unit, o);
            }
            t.rows << row;
        }
        out << t;
    }
    return out;
}

QString AltInvestmentsScreen::export_base_name() const {
    QString base = QStringLiteral("alt_%1_%2").arg(last_command_, last_method_.id);
    base.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]+")), QStringLiteral("_"));
    return base + QStringLiteral("_") + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmm"));
}

void AltInvestmentsScreen::copy_result() {
    const QVector<ExportTable> tables = export_tables();
    if (tables.isEmpty())
        return;
    QStringList lines;
    lines << last_title_;
    for (const ExportTable& t : tables) {
        lines << QString();
        if (t.title != tr("Summary"))
            lines << t.title;
        lines << t.headers.join(QLatin1Char('\t'));
        for (const QStringList& r : t.rows)
            lines << r.join(QLatin1Char('\t'));
    }
    QApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
    // Brief confirmation on the button itself.
    copy_btn_->setText(tr("COPIED"));
    QPointer<QPushButton> btn = copy_btn_;
    QTimer::singleShot(1500, this, [btn]() {
        if (btn)
            btn->setText(AltInvestmentsScreen::tr("COPY"));
    });
}

void AltInvestmentsScreen::export_csv() {
    const QVector<ExportTable> tables = export_tables();
    if (tables.isEmpty())
        return;
    // One file: the summary (section, item, value), then each table under its title.
    const ExportTable& summary = tables.first();
    QVector<QStringList> rows = summary.rows;
    for (int i = 1; i < tables.size(); ++i) {
        rows << QStringList{};
        rows << QStringList{tables[i].title};
        rows << tables[i].headers;
        rows += tables[i].rows;
    }
    ui::ChartExport::save_csv(summary.headers, rows, export_base_name(), this);
}

void AltInvestmentsScreen::export_image() {
    if (!result_view_ || last_result_.isEmpty())
        return;
    QWidget* page = result_view_->content_widget();
    ui::ChartExport::save_png(page ? page : static_cast<QWidget*>(result_view_), export_base_name());
}

} // namespace fincept::screens
