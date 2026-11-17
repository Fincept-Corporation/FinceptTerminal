// src/screens/ai_quant_lab/QuantModulePanel_Results.cpp
//
// Result rendering — the generic display_result() and the big on_result()
// dispatcher (rl_trading / online_learning / meta_learning / factor_discovery
// / model_library / live_signals / cfa_quant / gs_quant / functime / statsmodels
// / fortitudo / quant_reporting / gluonts / advanced_* / langgraph / rd_agent
// / backtesting / hft / rolling_retraining).
//
// Part of the partial-class split of QuantModulePanel.cpp.

#include "core/agent/AgentRiskMarkers.h"
#include "core/logging/Logger.h"
#include "screens/ai_quant_lab/QuantModulePanel.h"
#include "screens/ai_quant_lab/QuantModulePanel_Common.h"
#include "screens/ai_quant_lab/QuantModulePanel_GsHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_ResultCharts.h"
#include "screens/ai_quant_lab/QuantModulePanel_Styles.h"
#include "services/ai_quant_lab/AIQuantLabService.h"
#include "services/file_manager/FileManagerService.h"
#include "storage/repositories/LlmProfileRepository.h"
#include "ui/markdown/MarkdownRenderer.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/StatTile.h"

#include <QAreaSeries>
#include <QChart>
#include <QChartView>
#include <QDateTime>
#include <QDateTimeAxis>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLineSeries>
#include <QLocale>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QUrl>
#include <QValueAxis>

#include <cmath>

namespace fincept::screens {

using namespace fincept::services::quant;
using namespace fincept::screens::quant_styles;
using namespace fincept::screens::quant_common;
using namespace fincept::screens::quant_gs_helpers;
using namespace fincept::screens::quant_charts;

namespace {

/// BUILD FROM YAHOO / UPDATE TO TODAY results of the dataset bar (QuantModulePanel.cpp):
/// what was written, what Yahoo was asked for, and what was skipped.
[[maybe_unused]] void render_qlib_dataset_result(QVBoxLayout* out, QWidget* parent, const QString& accent, const QString& command,
                                const QJsonObject& p) {
    const QLocale loc;
    auto num = [&loc](const QJsonValue& v) { return loc.toString(v.toInt()); };
    auto note = [out, parent, &accent](const QString& text) {
        auto* lbl = new QLabel(text, parent);
        lbl->setWordWrap(true);
        lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
        lbl->setStyleSheet(QString("color:%1; font-size:" + ui::fonts::px(-4) +
                                   "px; padding:6px 10px; background:%2; border-left:3px solid %3;")
                               .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BG_SURFACE(), accent));
        out->addWidget(lbl);
    };
    const QString ok_color = ui::colors::SUCCESS();
    const QString warn_color = ui::colors::WARNING();

    if (command == QLatin1String("build_dataset")) {
        const QJsonArray skipped = p.value("skipped").toArray();
        const QJsonArray late = p.value("late_start").toArray();
        out->addWidget(gs_section_header(QuantModulePanel::tr("DATASET BUILT  |  %1").arg(p.value("dataset_id").toString()),
                                         accent));
        out->addWidget(gs_card_row(
            {gs_make_card(QuantModulePanel::tr("RANGE"),
                          QuantModulePanel::tr("%1 → %2").arg(p.value("calendar_start").toString(),
                                                              p.value("calendar_end").toString()),
                          parent),
             gs_make_card(QuantModulePanel::tr("TRADING DAYS"), num(p.value("trading_days")), parent),
             gs_make_card(QuantModulePanel::tr("NAMES"),
                          QuantModulePanel::tr("%1 + %2").arg(num(p.value("members")), p.value("benchmark").toString()),
                          parent),
             gs_make_card(QuantModulePanel::tr("SIZE"),
                          QuantModulePanel::tr("%1 MB").arg(p.value("size_bytes").toDouble() / 1e6, 0, 'f', 1), parent)},
            parent));
        out->addWidget(gs_card_row(
            {gs_make_card(QuantModulePanel::tr("YAHOO REQUESTS"), num(p.value("requests")), parent),
             gs_make_card(QuantModulePanel::tr("FROM CACHE"), num(p.value("from_cache")), parent),
             gs_make_card(QuantModulePanel::tr("TIME"),
                          QuantModulePanel::tr("%1 s").arg(p.value("seconds").toDouble(), 0, 'f', 0), parent),
             gs_make_card(QuantModulePanel::tr("SKIPPED"), QString::number(skipped.size()), parent,
                          skipped.isEmpty() ? ok_color : warn_color)},
            parent));
        if (!skipped.isEmpty()) {
            QStringList items;
            for (const auto& v : skipped)
                items << QuantModulePanel::tr("%1 (%2)").arg(v.toObject().value("ticker").toString(),
                                                            v.toObject().value("reason").toString().left(80));
            note(QuantModulePanel::tr("Skipped: %1").arg(items.join(QStringLiteral("; "))));
        }
        if (!late.isEmpty()) {
            QStringList items;
            for (const auto& v : late)
                items << QuantModulePanel::tr("%1 from %2").arg(v.toObject().value("ticker").toString(),
                                                               v.toObject().value("first_day").toString());
            note(QuantModulePanel::tr("Listed later than the start: %1").arg(items.join(QStringLiteral(", "))));
        }
        note(QuantModulePanel::tr("Stored in %1").arg(p.value("path").toString()));
        return;
    }

    // update_dataset: one entry per dataset; the bar updates one at a time.
    const QJsonObject u = p.value("updates").toArray().first().toObject();
    const bool current = u.value("already_up_to_date").toBool();
    const QJsonArray rescaled = u.value("rescaled").toArray();
    out->addWidget(gs_section_header(QuantModulePanel::tr("DATASET UPDATE  |  %1").arg(u.value("dataset_id").toString()),
                                     accent));
    out->addWidget(gs_card_row(
        {gs_make_card(QuantModulePanel::tr("STATUS"),
                      current ? QuantModulePanel::tr("UP TO DATE") : QuantModulePanel::tr("UPDATED"), parent, ok_color),
         gs_make_card(QuantModulePanel::tr("PREVIOUS END"), u.value("previous_end").toString(), parent),
         gs_make_card(QuantModulePanel::tr("NEW END"), u.value("calendar_end").toString(), parent),
         gs_make_card(QuantModulePanel::tr("NEW DAYS"), num(u.value("new_trading_days")), parent)},
        parent));
    out->addWidget(gs_card_row(
        {gs_make_card(QuantModulePanel::tr("YAHOO REQUESTS"), num(u.value("requests")), parent),
         gs_make_card(QuantModulePanel::tr("RE-ADJUSTED"), QString::number(rescaled.size()), parent),
         gs_make_card(QuantModulePanel::tr("LAST SESSION"), u.value("latest_session").toString(), parent),
         gs_make_card(QuantModulePanel::tr("TIME"),
                      QuantModulePanel::tr("%1 s").arg(p.value("seconds").toDouble(), 0, 'f', 0), parent)},
        parent));
    if (!rescaled.isEmpty()) {
        QStringList items;
        for (const auto& v : rescaled)
            items << QuantModulePanel::tr("%1 ×%2").arg(v.toObject().value("ticker").toString())
                         .arg(v.toObject().value("ratio").toDouble(), 0, 'f', 5);
        note(QuantModulePanel::tr("Adjusted history rescaled after a dividend or split since the last update: %1")
                 .arg(items.join(QStringLiteral(", "))));
    }
    if (current)
        note(QuantModulePanel::tr("Nothing to download: the dataset already ends on the last completed US session "
                                  "(%1). Today's bar is added after the 17:00 New York close.")
                 .arg(u.value("latest_session").toString()));
}

} // namespace

void QuantModulePanel::display_result(const QJsonObject& payload) {
    clear_results();

    auto* header = new QLabel(tr("RESULTS"));
    header->setStyleSheet(QString("color:%1; font-weight:700; font-family:%2; letter-spacing:1px;")
                              .arg(ui::colors::legible(module_.color))
                              .arg(ui::fonts::DATA_FAMILY));
    results_layout_->addWidget(header);

    // Fallback renderer for commands without a dedicated display. It used to
    // show only the top-level scalars plus the whole payload as a JSON text
    // dump, so any list result (models, factors, windows, trades) was readable
    // only as raw JSON. Now: scalars → key/value table, arrays of objects →
    // column tables, scalar arrays → a summary row, nested objects → their own
    // key/value section. Raw JSON stays available behind a toggle.
    // Tables take their look from the panel stylesheet (refresh_theme →
    // table_ss), so nothing here calls setStyleSheet per table.
    constexpr int kMaxRows = 200;
    constexpr int kMaxCols = 12;
    auto pretty_key = [](QString k) {
        k.replace('_', ' ');
        return k.toUpper();
    };
    auto cell_text = [](const QJsonValue& v) -> QString {
        if (v.isObject() || v.isArray()) {
            const QJsonDocument doc = v.isObject() ? QJsonDocument(v.toObject()) : QJsonDocument(v.toArray());
            return QString::fromUtf8(doc.toJson(QJsonDocument::Compact)).left(120);
        }
        if (v.isNull() || v.isUndefined())
            return QString::fromUtf8("—");
        return format_val(v);
    };
    auto make_table = [this](int rows, const QStringList& headers) {
        auto* t = new QTableWidget(rows, headers.size(), this);
        t->setHorizontalHeaderLabels(headers);
        t->setEditTriggers(QAbstractItemView::NoEditTriggers);
        t->setAlternatingRowColors(true);
        t->setSortingEnabled(false);
        t->horizontalHeader()->setStretchLastSection(true);
        t->verticalHeader()->setVisible(false);
        t->setMaximumHeight(qMin(rows * row_px(26) + row_px(32), 420));
        return t;
    };
    // A numeric cell sorts by its value; anything else by its text.
    auto make_cell = [&cell_text](const QJsonValue& v) -> QTableWidgetItem* {
        if (v.isDouble())
            return num_item(cell_text(v), v.toDouble());
        return new QTableWidgetItem(cell_text(v));
    };
    // Rows of objects that carry an x (date / index / step ...) and numeric columns are a
    // series: draw up to four numeric columns over the whole array above the table.
    auto add_rows_chart = [this](const QJsonArray& arr, const QStringList& cols, const QString& title) {
        static const QStringList kXKeys = {QStringLiteral("date"),  QStringLiteral("datetime"),
                                           QStringLiteral("time"),  QStringLiteral("timestamp"),
                                           QStringLiteral("index"), QStringLiteral("step"),
                                           QStringLiteral("epoch"), QStringLiteral("end_index"),
                                           QStringLiteral("period"), QStringLiteral("window")};
        if (arr.size() < 3)
            return;
        QString x_key;
        for (const auto& k : kXKeys) {
            if (cols.contains(k)) {
                x_key = k;
                break;
            }
        }
        if (x_key.isEmpty())
            return;
        const QJsonObject first = arr.first().toObject();
        QStringList y_keys;
        for (const auto& c : cols) {
            if (c != x_key && first.value(c).isDouble() && y_keys.size() < 4)
                y_keys << c;
        }
        if (y_keys.isEmpty())
            return;
        const XAxis x = qc_x_axis(arr, {x_key});
        QcChartSpec spec;
        spec.title = title;
        spec.x_time = x.time;
        spec.xs = x.xs;
        spec.fmt = qc_fmt_auto();
        for (const auto& k : y_keys) {
            QcLine l;
            l.name = k;
            l.ys = qc_column(arr, k);
            spec.lines.append(l);
        }
        results_layout_->addWidget(qc_series_chart(spec, this));
    };
    auto add_kv_table = [&](const QJsonObject& obj) {
        QStringList keys;
        for (auto it = obj.begin(); it != obj.end(); ++it)
            if (!it.value().isObject() && !it.value().isArray())
                keys.append(it.key());
        if (keys.isEmpty())
            return;
        auto* table = make_table(keys.size(), {tr("Metric"), tr("Value")});
        for (int r = 0; r < keys.size(); ++r) {
            table->setItem(r, 0, new QTableWidgetItem(pretty_key(keys[r])));
            auto* vi = new QTableWidgetItem(cell_text(obj.value(keys[r])));
            vi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            table->setItem(r, 1, vi);
        }
        table->resizeColumnsToContents();
        results_layout_->addWidget(table);
    };

    add_kv_table(payload);

    // Objects and arrays, recursively (nested objects two levels deep get their own
    // key / value section; deeper ones are listed and stay in the raw JSON).
    std::function<void(const QJsonObject&, const QString&, int)> render_members;
    render_members = [&](const QJsonObject& container, const QString& prefix, int depth) {
        for (auto it = container.begin(); it != container.end(); ++it) {
            const QString title = prefix.isEmpty() ? pretty_key(it.key()) : prefix + QStringLiteral(" › ") + pretty_key(it.key());
            if (it.value().isObject()) {
                const QJsonObject obj = it.value().toObject();
                if (obj.isEmpty())
                    continue;
                results_layout_->addWidget(gs_section_header(title, ui::colors::legible(module_.color)));
                add_kv_table(obj);
                if (depth < 2) {
                    render_members(obj, title, depth + 1);
                    continue;
                }
                QStringList nested;
                for (auto n = obj.begin(); n != obj.end(); ++n)
                    if (n.value().isObject() || n.value().isArray())
                        nested << pretty_key(n.key());
                if (!nested.isEmpty()) {
                    auto* note = new QLabel(tr("Nested fields (see raw JSON): %1").arg(nested.join(QStringLiteral(", "))));
                    note->setWordWrap(true);
                    results_layout_->addWidget(note);
                }
                continue;
            }
            if (!it.value().isArray())
                continue;
            const QJsonArray arr = it.value().toArray();
            if (arr.isEmpty()) {
                results_layout_->addWidget(gs_section_header(tr("%1  |  empty").arg(title), ui::colors::legible(module_.color)));
                continue;
            }
            if (arr.first().isObject()) {
                // Column set = union of keys over the shown rows, in first-seen order.
                QStringList cols;
                const int shown = qMin(static_cast<int>(arr.size()), kMaxRows);
                for (int r = 0; r < shown && cols.size() < kMaxCols; ++r) {
                    const QJsonObject row = arr.at(r).toObject();
                    for (auto c = row.begin(); c != row.end() && cols.size() < kMaxCols; ++c)
                        if (!cols.contains(c.key()))
                            cols << c.key();
                }
                const QString count = shown < arr.size() ? tr("first %1 of %2 rows").arg(shown).arg(arr.size())
                                                         : tr("%n row(s)", "", static_cast<int>(arr.size()));
                results_layout_->addWidget(
                    gs_section_header(QStringLiteral("%1  |  %2").arg(title, count), ui::colors::legible(module_.color)));
                add_rows_chart(arr, cols, title);
                QStringList headers;
                for (const auto& c : cols)
                    headers << pretty_key(c);
                auto* table = make_table(shown, headers);
                for (int r = 0; r < shown; ++r) {
                    const QJsonObject row = arr.at(r).toObject();
                    for (int c = 0; c < cols.size(); ++c)
                        table->setItem(r, c, make_cell(row.value(cols[c])));
                }
                table->resizeColumnsToContents();
                finish_table(table);
                results_layout_->addWidget(table);
                continue;
            }
            // Scalar array: numeric → summary statistics; otherwise a joined list.
            bool numeric = true;
            double mn = 0.0, mx = 0.0, sum = 0.0;
            int n_num = 0;
            for (const auto& v : arr) {
                if (!v.isDouble()) {
                    numeric = false;
                    break;
                }
                const double x = v.toDouble();
                mn = n_num == 0 ? x : qMin(mn, x);
                mx = n_num == 0 ? x : qMax(mx, x);
                sum += x;
                ++n_num;
            }
            if (numeric && n_num > 0) {
                results_layout_->addWidget(gs_section_header(
                    tr("%1  |  series of %2 values").arg(title).arg(n_num), ui::colors::legible(module_.color)));
                // The whole series as a line (by position), with the summary row below it.
                if (n_num >= 3) {
                    QcChartSpec spec;
                    spec.xs = qc_x_positions(arr.size()).xs;
                    spec.fmt = qc_fmt_auto();
                    spec.x_label = [](double v) {
                        return QStringLiteral("#%1").arg(static_cast<qlonglong>(std::llround(v)));
                    };
                    QcLine l;
                    l.name = title;
                    l.ys = qc_values(arr);
                    l.color = ui::ChartColorRole::Accent;
                    spec.lines.append(l);
                    results_layout_->addWidget(qc_series_chart(spec, this));
                }
                auto* table = make_table(1, {tr("Count"), tr("First"), tr("Last"), tr("Min"), tr("Max"), tr("Mean")});
                const QStringList cells = {QString::number(n_num), format_val(arr.first()), format_val(arr.last()),
                                           format_val(QJsonValue(mn)), format_val(QJsonValue(mx)),
                                           format_val(QJsonValue(sum / n_num))};
                for (int c = 0; c < cells.size(); ++c) {
                    auto* item = new QTableWidgetItem(cells[c]);
                    item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    table->setItem(0, c, item);
                }
                results_layout_->addWidget(table);
            } else {
                QStringList parts;
                for (int i = 0; i < arr.size() && i < kMaxRows; ++i)
                    parts << cell_text(arr.at(i));
                results_layout_->addWidget(
                    gs_section_header(tr("%1  |  %n item(s)", "", static_cast<int>(arr.size())).arg(title),
                                      ui::colors::legible(module_.color)));
                auto* list = new QLabel(parts.join(QStringLiteral(", ")) +
                                        (arr.size() > kMaxRows ? tr(" … (+%1 more)").arg(arr.size() - kMaxRows) : QString()));
                list->setWordWrap(true);
                list->setTextInteractionFlags(Qt::TextSelectableByMouse);
                results_layout_->addWidget(list);
            }
        }
    };
    render_members(payload, QString(), 0);

    // Raw JSON — collapsed by default; for checking exact values / bug reports.
    auto* raw = new QTextEdit;
    raw->setReadOnly(true);
    raw->setMaximumHeight(240);
    raw->setPlainText(QJsonDocument(payload).toJson(QJsonDocument::Indented));
    raw->setStyleSheet(output_ss());
    raw->hide();
    auto* raw_toggle = new QPushButton(tr("SHOW RAW JSON"));
    raw_toggle->setCursor(Qt::PointingHandCursor);
    raw_toggle->setMinimumHeight(24);
    raw_toggle->setObjectName(QStringLiteral("quantQuietButton")); // styled by refresh_theme()
    connect(raw_toggle, &QPushButton::clicked, this, [raw, raw_toggle]() {
        raw->setVisible(!raw->isVisible());
        raw_toggle->setText(raw->isVisible() ? QuantModulePanel::tr("HIDE RAW JSON")
                                             : QuantModulePanel::tr("SHOW RAW JSON"));
    });
    results_layout_->addWidget(raw_toggle);
    results_layout_->addWidget(raw);

    // Export button — saves raw JSON to File Manager
    auto* export_btn = new QPushButton(tr("EXPORT RESULTS"));
    export_btn->setCursor(Qt::PointingHandCursor);
    export_btn->setMinimumHeight(28);
    export_btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %2; "
                                      "font-size:%3px; font-family:%4; padding:0 12px; }"
                                      "QPushButton:hover { color:%1; background:%5; }")
                                  .arg(ui::colors::legible(module_.color), ui::colors::BORDER_DIM())
                                  .arg(ui::fonts::px(-1))
                                  .arg(ui::fonts::DATA_FAMILY)
                                  .arg(ui::colors::overlay(0.05)));
    QString json_str = QJsonDocument(payload).toJson(QJsonDocument::Indented);
    connect(export_btn, &QPushButton::clicked, this, [this, json_str]() {
        QString safe_name = module_.label;
        safe_name.replace(QRegularExpression("[^a-zA-Z0-9_\\-]"), "_");
        QString stored_name = safe_name + "_" + QString::number(QDateTime::currentMSecsSinceEpoch()) + ".json";
        QString dest = services::FileManagerService::instance().storage_dir() + "/" + stored_name;

        QFile f(dest);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            f.write(json_str.toUtf8());
            f.close();
            services::FileManagerService::instance().register_file(stored_name, module_.label + "_result.json",
                                                                   QFileInfo(dest).size(), "application/json",
                                                                   "ai_quant_lab");
            LOG_INFO("AIQuantLab", "Exported result: " + stored_name);
            if (status_label_)
                status_label_->setText(tr("Exported to File Manager: %1").arg(stored_name));
        } else {
            // A failed export used to do nothing at all.
            LOG_WARN("AIQuantLab", fincept::logf("Result export could not be written",
                                                 {{"path", fincept::log_path(dest)}, {"error", f.errorString()}}));
            if (status_label_)
                status_label_->setText(tr("Export failed: %1").arg(f.errorString()));
        }
    });
    results_layout_->addWidget(export_btn);

    status_label_->setText(tr("Done"));
}

// ── Signal handlers ──────────────────────────────────────────────────────────

void QuantModulePanel::on_result(const QString& module_id, const QString& command, const QJsonObject& payload) {
    // Untagged replies only: every view's run is tagged and answered through on_request_finished().
    // What is left: the dataset bar (probe, BUILD, UPDATE), the RD-Agent tasks (status, monitor,
    // results, stop / resume, log viewer, MCP) and the live events of a rolling retrain.
    if (module_id != module_.id)
        return;
    // The dataset bar's probe (QuantModulePanel.cpp) answers on this signal too; the bar handles it.
    if (command == QLatin1String("dataset_info") || command == QLatin1String("list_datasets"))
        return;
    // The saved models / pipelines behind the id combos (QuantModulePanel_AutoLoad.cpp).
    if (apply_saved_ids(command, payload))
        return;

    // BUILD FROM YAHOO / UPDATE TO TODAY from the dataset bar: what was written, as a notice over
    // the result pane (the tab's own result stays on screen).
    if (command == QLatin1String("build_dataset") || command == QLatin1String("update_dataset")) {
        if (!pane_)
            return;
        if (!payload.value("success").toBool()) {
            display_error(payload.value("error").toString(tr("The dataset command failed")));
            return;
        }
        QString summary;
        if (command == QLatin1String("build_dataset")) {
            summary = tr("Dataset %1 built: %2 → %3, %4 names, %5 skipped.")
                          .arg(payload.value("dataset_id").toString(), payload.value("calendar_start").toString(),
                               payload.value("calendar_end").toString())
                          .arg(QLocale().toString(payload.value("members").toInt()))
                          .arg(payload.value("skipped").toArray().size());
        } else {
            const QJsonObject u = payload.value("updates").toArray().first().toObject();
            summary = u.value("already_up_to_date").toBool()
                          ? tr("Dataset %1 is up to date (last session %2).")
                                .arg(u.value("dataset_id").toString(), u.value("latest_session").toString())
                          : tr("Dataset %1 updated to %2 (%3 new days).")
                                .arg(u.value("dataset_id").toString(), u.value("calendar_end").toString())
                                .arg(QLocale().toString(u.value("new_trading_days").toInt()));
        }
        pane_->set_notice(summary, ui::EmptyState::Tone::Success, tr("OK"));
        return;
    }

    if (module_id == QLatin1String("deep_agent") && command != QLatin1String("execute_task")) {

        // RD-Agent branches. The RD-Agent tab has its own status strip ("Checking...", "Refreshing
        // task list...") that the buttons set when they fire a request; nothing ever cleared it, so
        // it read "Checking..." forever. And display_error() paints into the Deep Analysis tab's
        // results pane, which is on the other page of the outer QTabWidget -- a failed RD-Agent call
        // showed the user nothing. Route both to this tab.
        auto rd_status = [this](const QString& text) {
            status_label_->setText(text);
            if (auto* lbl = findChild<QLabel*>(QStringLiteral("rdStatusTxt")))
                lbl->setText(text);
        };
        auto rd_fail = [this, &rd_status](const QString& text) {
            rd_status(text);
            if (rd_agent_output_)
                rd_agent_output_->setPlainText(text);
        };
        // Structured RD-Agent results (task status, factors, models) go into the host under
        // the message pane: a KPI strip and a sortable table. Returns the emptied layout.
        auto rd_results = [this]() -> QVBoxLayout* {
            auto* host = findChild<QWidget*>(QStringLiteral("rdResultsHost"));
            auto* vl = host ? qobject_cast<QVBoxLayout*>(host->layout()) : nullptr;
            if (!vl)
                return nullptr;
            while (vl->count() > 0) {
                auto* item = vl->takeAt(0);
                if (item->widget())
                    item->widget()->deleteLater();
                delete item;
            }
            host->show();
            return vl;
        };
        using Tone = ui::StatTile::Tone;
        auto rd_tile = [](ui::KpiStrip* strip, const QString& label, const QString& value, Tone tone = Tone::Neutral,
                          const QString& sub = {}) {
            auto* t = strip->add_tile(label);
            t->set_value(value);
            t->set_tone(tone);
            if (!sub.isEmpty())
                t->set_sub_label(sub);
            return t;
        };
        auto rd_num = [](const QJsonValue& v, int dec) {
            return v.isNull() || v.isUndefined() ? QString::fromUtf8("—") : QLocale().toString(v.toDouble(), 'f', dec);
        };
        // # / NAME / IC / SHARPE (/ DESCRIPTION) — numeric columns sort by value, coloured by sign.
        auto rd_table = [this](const QJsonArray& rows, const QString& name_key, const QString& name_fallback,
                               const QStringList& num_keys, const QStringList& headers, bool with_desc) {
            auto* t = new QTableWidget(rows.size(), headers.size(), this);
            t->setHorizontalHeaderLabels(headers);
            t->verticalHeader()->setVisible(false);
            t->setEditTriggers(QAbstractItemView::NoEditTriggers);
            t->setSelectionBehavior(QAbstractItemView::SelectRows);
            // Filled with fixed columns, sized once afterwards (never ResizeToContents while filling).
            t->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
            t->horizontalHeader()->setStretchLastSection(true);
            t->setStyleSheet(table_ss());
            t->setMaximumHeight(qMin(static_cast<int>(rows.size()) * row_px(24) + row_px(32), row_px(280)));
            for (int i = 0; i < rows.size(); ++i) {
                const QJsonObject o = rows[i].toObject();
                auto* idx = num_item(QString::number(i + 1), i + 1);
                idx->setTextAlignment(Qt::AlignCenter);
                t->setItem(i, 0, idx);
                t->setItem(i, 1, new QTableWidgetItem(o.value(name_key).toString(name_fallback)));
                for (int c = 0; c < num_keys.size(); ++c) {
                    const QJsonValue v = o.value(num_keys[c]);
                    if (v.isNull() || v.isUndefined()) {
                        auto* na = new QTableWidgetItem(QString::fromUtf8("—"));
                        na->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                        t->setItem(i, 2 + c, na);
                    } else {
                        const double d = v.toDouble();
                        t->setItem(i, 2 + c, num_item(QString::number(d, 'f', num_keys[c] == QLatin1String("ic") ? 4 : 3), d,
                                                        gs_pos_neg_color(d)));
                    }
                }
                if (with_desc) {
                    const QString desc = o.value("description").toString();
                    auto* di = new QTableWidgetItem(desc);
                    di->setToolTip(desc);
                    t->setItem(i, 2 + num_keys.size(), di);
                }
                t->setRowHeight(i, row_px(24));
            }
            t->resizeColumnsToContents();
            finish_table(t);
            return t;
        };

        // ── RD-Agent: check_status ────────────────────────────────────────────────
        if (command == "check_status") {
            auto ver = payload["rdagent_version"].toString("?");
            auto avail = payload["availability"].toObject();
            QStringList flags;
            for (auto it = avail.begin(); it != avail.end(); ++it)
                if (it.value().toBool())
                    flags << it.key();
            rd_status(tr("rdagent %1 — %2").arg(ver, flags.join(", ")));
            return;
        }

        // ── RD-Agent: task started (factor_mining / model_optimization / quant_research) ──
        if (command == "start_factor_mining" || command == "start_model_optimization" ||
            command == "start_quant_research") {
            if (!payload["success"].toBool()) {
                rd_fail(payload["error"].toString(tr("Unknown error")));
                return;
            }
            auto task_id = payload["task_id"].toString();
            auto est = payload["estimated_time"].toString();
            rd_status(tr("Task %1 started").arg(task_id));
            // Follow it where it is shown: the Task Monitor (the last sub-tab).
            if (auto* sub = findChild<QTabWidget*>(QStringLiteral("rdSubTabs")))
                sub->setCurrentIndex(sub->count() - 1);
            // Populate the monitor right away (in-memory listing); its result arms
            // the auto-refresh while the task runs.
            AIQuantLabService::instance().rd_agent_list_tasks({});
            if (rd_agent_output_)
                rd_agent_output_->setPlainText(
                    tr("Task started: %1\nEstimated time: %2\n\nUse Task Monitor → REFRESH to track progress.")
                        .arg(task_id, est));
            return;
        }

        // ── RD-Agent: list_tasks ──────────────────────────────────────────────────
        if (command == "list_tasks" && rd_task_table_) {
            auto tasks = payload["tasks"].toArray();
            // Keep the user's selection across the rebuild — the monitor now
            // auto-refreshes every 5 s while a task runs.
            QString selected_id;
            if (const int cur = rd_task_table_->currentRow(); cur >= 0 && rd_task_table_->item(cur, 0))
                selected_id = rd_task_table_->item(cur, 0)->text();
            bool any_running = false;
            // No task this session: the designed empty state instead of a bare header.
            const bool none = tasks.isEmpty();
            rd_task_table_->setVisible(!none);
            if (rd_agent_output_)
                rd_agent_output_->setVisible(!none);
            if (auto* empty = findChild<QWidget*>(QStringLiteral("rdTasksEmpty")))
                empty->setVisible(none);
            rd_task_table_->setRowCount(0);
            for (const auto& t : tasks) {
                auto obj = t.toObject();
                int row = rd_task_table_->rowCount();
                rd_task_table_->insertRow(row);
                rd_task_table_->setItem(row, 0, new QTableWidgetItem(obj["task_id"].toString()));
                rd_task_table_->setItem(row, 1, new QTableWidgetItem(obj["type"].toString()));
                auto status_str = obj["status"].toString();
                any_running = any_running || status_str == QLatin1String("running");
                auto* status_item = new QTableWidgetItem(status_str);
                // A state (with its word): SUCCESS / DANGER, not the P&L direction colours.
                if (status_str == "running")
                    status_item->setForeground(QColor(ui::colors::WARNING()));
                else if (status_str == "completed")
                    status_item->setForeground(QColor(ui::colors::SUCCESS()));
                else if (status_str == "failed" || status_str == "stopped")
                    status_item->setForeground(QColor(ui::colors::DANGER()));
                rd_task_table_->setItem(row, 2, status_item);
                rd_task_table_->setItem(
                    row, 3, new QTableWidgetItem(QString::number(obj["progress"].toDouble() * 100, 'f', 0) + "%"));
                auto ic = obj["best_ic"];
                rd_task_table_->setItem(row, 4,
                                        new QTableWidgetItem(ic.isNull() ? "-" : QString::number(ic.toDouble(), 'f', 4)));
                rd_task_table_->setItem(row, 5, new QTableWidgetItem(obj["elapsed_time"].toString("-")));
                if (!selected_id.isEmpty() && obj["task_id"].toString() == selected_id)
                    rd_task_table_->setCurrentCell(row, 0);
            }
            if (auto* poll = findChild<QTimer*>(QStringLiteral("rdTaskPollTimer"))) {
                poll->setProperty("rdActive", any_running);
                if (any_running && rd_task_table_->isVisible()) {
                    if (!poll->isActive())
                        poll->start();
                } else {
                    poll->stop();
                }
            }
            rd_status(any_running ? tr("%1 task(s) · auto-refreshing every 5 s").arg(tasks.size())
                                  : tr("%1 task(s)").arg(tasks.size()));
            return;
        }

        // ── RD-Agent: get_task_status ─────────────────────────────────────────────
        if (command == "get_task_status") {
            if (!payload["success"].toBool()) {
                rd_fail(payload["error"].toString(tr("Unknown error")));
                return;
            }
            auto task_id = payload["task_id"].toString();
            auto progress = payload["progress"].toDouble() * 100;
            auto step = payload["current_step"].toString();
            auto ic = payload["best_ic"];
            rd_status(tr("Task %1 — %2% — %3").arg(task_id).arg(progress, 0, 'f', 0).arg(step));
            if (rd_agent_output_)
                rd_agent_output_->setPlainText(tr("Task %1 — status below.").arg(task_id));
            if (auto* vl = rd_results()) {
                const QString st = payload["status"].toString();
                const Tone st_tone = st == QLatin1String("completed") ? Tone::Positive
                                     : st == QLatin1String("running") ? Tone::Warning
                                     : (st == QLatin1String("failed") || st == QLatin1String("stopped")) ? Tone::Negative
                                                                                                         : Tone::Neutral;
                auto* strip = new ui::KpiStrip(this);
                rd_tile(strip, tr("TASK"), task_id);
                rd_tile(strip, tr("STATUS"), st.toUpper(), st_tone);
                rd_tile(strip, tr("PROGRESS"), QLocale().toString(progress, 'f', 0) + QLatin1Char('%'), Tone::Neutral);
                rd_tile(strip, tr("STEP"), step.isEmpty() ? QString::fromUtf8("—") : step);
                rd_tile(strip, tr("BEST IC"), rd_num(ic, 4),
                        ic.isNull() || ic.isUndefined() ? Tone::Neutral
                        : ic.toDouble() > 0.03         ? Tone::Positive
                        : ic.toDouble() < 0            ? Tone::Negative
                                                       : Tone::Neutral);
                rd_tile(strip, tr("ELAPSED"), payload["elapsed_time"].toString("-"));
                vl->addWidget(strip);
            }
            return;
        }

        // ── RD-Agent: get_discovered_factors ──────────────────────────────────────
        if (command == "get_discovered_factors" && rd_agent_output_) {
            if (!payload["success"].toBool()) {
                rd_fail(payload["error"].toString(tr("Unknown error")));
                return;
            }
            auto factors = payload["factors"].toArray();
            auto best_ic = payload["best_ic"];
            double best_sharpe = std::numeric_limits<double>::quiet_NaN();
            for (const auto& f : factors) {
                const QJsonValue s = f.toObject().value("sharpe");
                if (s.isDouble() && (!std::isfinite(best_sharpe) || s.toDouble() > best_sharpe))
                    best_sharpe = s.toDouble();
            }
            rd_agent_output_->setPlainText(tr("Discovered factors: %1 — sortable table below (click a header).")
                                               .arg(factors.size()));
            if (auto* vl = rd_results()) {
                auto* strip = new ui::KpiStrip(this);
                rd_tile(strip, tr("FACTORS"), QLocale().toString(static_cast<int>(factors.size())),
                        factors.isEmpty() ? Tone::Warning : Tone::Neutral);
                rd_tile(strip, tr("BEST IC"), rd_num(best_ic, 4),
                        !best_ic.isDouble()           ? Tone::Neutral
                        : best_ic.toDouble() > 0.03 ? Tone::Positive
                        : best_ic.toDouble() < 0    ? Tone::Negative
                                                    : Tone::Neutral,
                        tr("> 0.03 is useful"));
                rd_tile(strip, tr("BEST SHARPE"),
                        std::isfinite(best_sharpe) ? QLocale().toString(best_sharpe, 'f', 3) : QString::fromUtf8("—"),
                        !std::isfinite(best_sharpe) ? Tone::Neutral
                        : best_sharpe > 1.0         ? Tone::Positive
                        : best_sharpe < 0           ? Tone::Negative
                                                    : Tone::Neutral);
                vl->addWidget(strip);
                if (!factors.isEmpty())
                    vl->addWidget(rd_table(factors, QStringLiteral("name"), tr("Factor"),
                                           {QStringLiteral("ic"), QStringLiteral("sharpe")},
                                           {tr("#"), tr("Name"), tr("IC"), tr("Sharpe"), tr("Description")}, true));
            }
            rd_status(tr("Found %1 factor(s)").arg(factors.size()));
            return;
        }

        // ── RD-Agent: get_optimized_model ─────────────────────────────────────────
        if (command == "get_optimized_model" && rd_agent_output_) {
            if (!payload["success"].toBool()) {
                rd_fail(payload["error"].toString(tr("Unknown error")));
                return;
            }
            auto models = payload["models"].toArray();
            double best_sharpe = std::numeric_limits<double>::quiet_NaN();
            double best_ic = std::numeric_limits<double>::quiet_NaN();
            for (const auto& m : models) {
                const QJsonObject o = m.toObject();
                if (o.value("sharpe").isDouble() && (!std::isfinite(best_sharpe) || o.value("sharpe").toDouble() > best_sharpe))
                    best_sharpe = o.value("sharpe").toDouble();
                if (o.value("ic").isDouble() && (!std::isfinite(best_ic) || o.value("ic").toDouble() > best_ic))
                    best_ic = o.value("ic").toDouble();
            }
            rd_agent_output_->setPlainText(tr("Optimized models: %1 — sortable table below (click a header).")
                                               .arg(models.size()));
            if (auto* vl = rd_results()) {
                auto* strip = new ui::KpiStrip(this);
                rd_tile(strip, tr("MODELS"), QLocale().toString(static_cast<int>(models.size())),
                        models.isEmpty() ? Tone::Warning : Tone::Neutral);
                rd_tile(strip, tr("BEST SHARPE"),
                        std::isfinite(best_sharpe) ? QLocale().toString(best_sharpe, 'f', 3) : QString::fromUtf8("—"),
                        !std::isfinite(best_sharpe) ? Tone::Neutral
                        : best_sharpe > 1.0         ? Tone::Positive
                        : best_sharpe < 0           ? Tone::Negative
                                                    : Tone::Neutral);
                rd_tile(strip, tr("BEST IC"),
                        std::isfinite(best_ic) ? QLocale().toString(best_ic, 'f', 4) : QString::fromUtf8("—"),
                        !std::isfinite(best_ic) ? Tone::Neutral
                        : best_ic > 0.03        ? Tone::Positive
                        : best_ic < 0           ? Tone::Negative
                                                : Tone::Neutral);
                vl->addWidget(strip);
                if (!models.isEmpty())
                    vl->addWidget(rd_table(models, QStringLiteral("model_id"), tr("Model"),
                                           {QStringLiteral("sharpe"), QStringLiteral("ic")},
                                           {tr("#"), tr("Model"), tr("Sharpe"), tr("IC")}, false));
            }
            rd_status(tr("%1 optimized model(s)").arg(models.size()));
            return;
        }

        // ── RD-Agent: stop_task / resume_task ────────────────────────────────────
        if (command == "stop_task" || command == "resume_task") {
            // A refused stop / resume ("Task not found", "No checkpoint found") comes back as
            // success:false with the reason in "error", not in "message".
            if (!payload["success"].toBool(true)) {
                rd_fail(payload["error"].toString(tr("Unknown error")));
                return;
            }
            rd_status(payload["message"].toString(command));
            if (rd_agent_output_)
                rd_agent_output_->setPlainText(payload["message"].toString());
            return;
        }

        // ── RD-Agent: start_ui ───────────────────────────────────────────────────
        if (command == "start_ui") {
            auto url = payload["url"].toString();
            if (!url.isEmpty()) {
                rd_status(tr("Log viewer: %1").arg(url));
                QDesktopServices::openUrl(QUrl(url));
            } else {
                rd_fail(payload["error"].toString(tr("The log viewer did not report a URL.")));
            }
            return;
        }

        // ── RD-Agent: start_mcp_server ────────────────────────────────────────────
        if (command == "start_mcp_server") {
            if (!payload["success"].toBool()) {
                rd_status(tr("MCP server failed"));
                if (rd_agent_output_)
                    rd_agent_output_->setPlainText(
                        tr("MCP server failed to start:\n%1\n\nInstall: %2")
                            .arg(payload["error"].toString())
                            .arg(payload["install"].toString("pip install 'mcp[cli]' 'pydantic-ai[mcp]' yfinance")));
                return;
            }
            auto url = payload["url"].toString();
            auto port = payload["port"].toInt();
            auto tools = payload["tools"].toArray();
            QStringList tool_names;
            for (const auto& t : tools)
                tool_names << t.toString();
            rd_status(tr("MCP ready on port %1").arg(port));
            if (rd_agent_output_)
                rd_agent_output_->setPlainText(tr("MCP tool server running at %1\n\nAvailable tools:\n  %2\n\n"
                                                  "Tick 'Use Fincept MCP tools' before starting a task to give\n"
                                                  "RD-Agent loops access to these tools.")
                                                   .arg(url, tool_names.join("\n  ")));
            return;
        }

        // ── RD-Agent: mcp_status ─────────────────────────────────────────────────
        if (command == "mcp_status") {
            auto avail = payload["mcp_server_available"].toBool();
            auto pai_mcp = payload["pydantic_ai_mcp"].toBool();
            auto ports = payload["running_ports"].toArray();
            QString info = tr("MCP server available: %1\npydantic-ai MCP: %2\nRunning ports: %3")
                               .arg(avail ? tr("yes") : tr("no"))
                               .arg(pai_mcp ? tr("yes") : tr("no"))
                               .arg(ports.isEmpty() ? tr("none") : [&]() {
                                   QStringList ps;
                                   for (const auto& p : ports)
                                       ps << QString::number(p.toInt());
                                   return ps.join(", ");
                               }());
            rd_status(avail ? tr("MCP available") : tr("MCP not installed"));
            if (rd_agent_output_)
                rd_agent_output_->setPlainText(info);
            return;
        }
        return;
    }

    // ── Rolling Retraining: the live events of a retrain (the progress bar and log of the
    // Execute Retrain tab). Its outcome is the view's result (tagged); a run started from a
    // schedule card's Run Now goes through the same view.
    if (module_id == QLatin1String("rolling_retraining")) {
        // ── Streaming events from retrain command ────────────────────────────
        const QString event = payload["event"].toString();
        if (!event.isEmpty()) {
            auto* pb = this->findChild<QProgressBar*>("rr_progress");
            auto* log = this->findChild<QTextEdit*>("rr_log");

            if (event == "start") {
                const int total = payload["total_windows"].toInt();
                if (pb) {
                    pb->setRange(0, total);
                    pb->setValue(0);
                    pb->setFormat(tr("0 / %v windows"));
                }
                if (log)
                    log->append(
                        tr("Starting retrain: %1  |  %2 windows").arg(payload["model_id"].toString()).arg(total));
                status_label_->setText(
                    tr("Retraining %1 — 0/%2 windows").arg(payload["model_id"].toString()).arg(total));
            } else if (event == "window") {
                const int idx = payload["index"].toInt();
                const int total = payload["total"].toInt();
                if (pb) {
                    pb->setValue(idx);
                    pb->setFormat(tr("%1 / %2 windows").arg(idx).arg(total));
                }
                if (log)
                    log->append(tr("  Window %1/%2  train→%3  test %4→%5")
                                    .arg(idx)
                                    .arg(total)
                                    .arg(payload["train_end"].toString().left(10))
                                    .arg(payload["test_start"].toString().left(10))
                                    .arg(payload["test_end"].toString().left(10)));
                status_label_->setText(tr("Window %1/%2").arg(idx).arg(total));
            } else if (event == "ensemble") {
                if (log)
                    log->append(QString("  %1").arg(payload["message"].toString()));
                status_label_->setText(tr("Combining rolling results..."));
            } else if (event == "done") {
                const int total = payload["windows_trained"].toInt();
                if (pb) {
                    pb->setValue(total);
                    pb->setFormat(tr("Complete"));
                }
                if (log)
                    log->append(tr("\nDone — %1 windows in %2s  |  Experiment: %3")
                                    .arg(total)
                                    .arg(payload["elapsed_sec"].toDouble(), 0, 'f', 1)
                                    .arg(payload["exp_name"].toString()));
                status_label_->setText(tr("Retrain complete — %1 windows in %2s")
                                           .arg(total)
                                           .arg(payload["elapsed_sec"].toDouble(), 0, 'f', 1));
                set_retrain_busy(this, false);
            } else if (event == "error") {
                set_retrain_busy(this, false);
                if (pb) {
                    pb->setFormat(tr("Failed"));
                }
                if (log)
                    log->append(tr("\nError: %1").arg(payload["error"].toString()));
                // (the Execute view's run fails with this message: its error page shows it)
            }
            return;
        }
        if (command == QLatin1String("delete") && payload.value("success").toBool()) {
            invalidate_view(QStringLiteral("configs"));
            return;
        }
    }
    LOG_DEBUG("AIQuantLab", QStringLiteral("[%1/%2] untagged reply not shown").arg(module_id, command));
}

void QuantModulePanel::render_result(const QString& command, const QJsonObject& payload) {
    // Every view's result, drawn into the result pane's content (results_layout_). Called by the
    // pane's render (a fresh or a remembered result), so it must not depend on panel state that
    // only exists after a run.
    const QString& module_id = module_.id;

    if (module_id == "rl_trading") {
        display_rl_trading_result(command, payload);
        return;
    }
    if (module_id == "online_learning") {
        display_online_learning_result(command, payload);
        return;
    }
    if (module_id == "meta_learning") {
        display_meta_learning_result(command, payload);
        return;
    }
    // CORE-category renderers (backtesting handled later via display_backtest_result)
    if (module_id == "factor_discovery") {
        display_factor_discovery_result(command, payload);
        return;
    }
    if (module_id == "model_library") {
        display_model_library_result(command, payload);
        return;
    }
    if (module_id == "live_signals") {
        display_live_signals_result(command, payload);
        return;
    }

    // ── CFA Quant: uniform result shape across all analyses ──────────────────
    if (module_id == "cfa_quant") {
        display_cfa_result(command, payload);
        return;
    }

    // ── GS Quant: rich card-based display per sub-tab ────────────────────────
    if (module_id == "gs_quant") {
        display_gs_result(command, payload);
        return;
    }

    // ── Functime: rich card-based display per sub-tab ────────────────────────
    if (module_id == "functime") {
        display_functime_result(command, payload);
        return;
    }

    // ── Statsmodels: rich card-based display per sub-tab ─────────────────────
    if (module_id == "statsmodels") {
        display_statsmodels_result(command, payload);
        return;
    }

    // ── Fortitudo: rich card-based display per sub-tab ───────────────────────
    if (module_id == "fortitudo") {
        display_fortitudo_result(command, payload);
        return;
    }

    // ── Quant Reporting: rich card-based display per sub-tab ─────────────────
    if (module_id == "quant_reporting") {
        display_quant_reporting_result(command, payload);
        return;
    }

    // ── GluonTS: rich card-based display per sub-tab ─────────────────────────
    if (module_id == "gluonts") {
        display_gluonts_result(command, payload);
        return;
    }

    // ── ADVANCED-category panels: card-based displays ────────────────────────
    // hft and rolling_retraining are intentionally NOT dispatched here — they
    // have bespoke per-tab UI (in-tab card grid + order-book tables for HFT;
    // progress bar + schedule cards for Rolling Retraining) handled later in
    // this function. Re-routing them to the generic results pane would create
    // a duplicate, disconnected display.
    if (module_id == "advanced_models") {
        display_advanced_models_result(command, payload);
        return;
    }
    if (module_id == "feature_engineering") {
        display_feature_engineering_result(command, payload);
        return;
    }
    if (module_id == "portfolio_opt") {
        display_portfolio_opt_result(command, payload);
        return;
    }
    if (module_id == "factor_evaluation") {
        display_factor_evaluation_result(command, payload);
        return;
    }
    if (module_id == "strategy_builder") {
        display_strategy_builder_result(command, payload);
        return;
    }
    if (module_id == "data_processors") {
        display_data_processors_result(command, payload);
        return;
    }

    // ── LangGraph deep analysis result ───────────────────────────────────────
    if (command == "execute_task") {
        clear_results();
        if (!payload["success"].toBool(true)) {
            display_error(payload["error"].toString(tr("Unknown error")));
            return;
        }
        // The report is LLM Markdown (headings, bullets, bold, tables): rendered into the result
        // pane, the source kept on the label so refresh_theme() re-renders it in a new theme.
        // No report text: the response laid out structurally instead.
        const QString text = payload["result"].toString();
        const QString tid = payload["thread_id"].toString();
        const QString accent = ui::colors::legible(module_.color);
        results_layout_->addWidget(
            gs_section_header(tid.isEmpty() ? tr("DEEP ANALYSIS REPORT")
                                            : tr("DEEP ANALYSIS REPORT  |  THREAD %1").arg(tid.left(24)),
                              accent));
        if (text.trimmed().isEmpty()) {
            display_result(payload);
            return;
        }
        auto* report = new QLabel(result_host_);
        report->setObjectName(QStringLiteral("quantMarkdownReport"));
        report->setProperty("quantMarkdown", text);
        report->setTextFormat(Qt::RichText);
        report->setWordWrap(true);
        report->setOpenExternalLinks(true);
        report->setTextInteractionFlags(Qt::TextBrowserInteraction);
        report->setStyleSheet(QString("QLabel#quantMarkdownReport { background:%1; border:1px solid %2; padding:12px; }")
                                  .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
        report->setText(ui::MarkdownRenderer::render(text));
        results_layout_->addWidget(report);
        return;
    }

    // ── Backtesting ───────────────────────────────────────────────────────────
    if (module_id == "backtesting") {
        if (!payload["success"].toBool()) {
            display_error(payload["error"].toString(tr("Unknown error")));
            return;
        }
        if (command == "run_backtest") {
            display_backtest_result(payload);
            return;
        }
        display_result(payload);
        return;
    }

    // factor_discovery, model_library, live_signals are now rendered by the
    // card-based dispatch at the top of this function (display_*_result in
    // QuantModulePanel_CoreDisplays.cpp).
    //
    // online_learning and meta_learning are also rendered by card-based
    // dispatches at the top (display_*_result in
    // QuantModulePanel_AIMLDisplays.cpp).

    // ── HFT ──────────────────────────────────────────────────────────────────
    if (module_id == "hft") {
        if (!payload["success"].toBool()) {
            display_error(payload["error"].toString());
            return;
        }
        // HFT paints into fixed in-tab cards; the result pane under the tabs carries the
        // snapshot's headline (analyze) or what the last section run computed.
        clear_results();
        if (command != QLatin1String("analyze")) {
            const QString what = command == QLatin1String("fetch_orderbook") ? tr("ORDER BOOK")
                                 : command == QLatin1String("market_making") ? tr("MARKET-MAKING QUOTES")
                                 : command == QLatin1String("toxic_flow")    ? tr("TOXIC FLOW")
                                                                             : tr("SLIPPAGE ESTIMATE");
            results_layout_->addWidget(gs_section_header(
                tr("%1  |  %2 — see the cards above").arg(what, payload["symbol"].toString()),
                ui::colors::legible(module_.color)));
        }

        // Helper: find a named child label in this panel and set its text
        auto set_card = [this](const QString& name, const QString& text, const QString& color = {}) {
            auto* lbl = this->findChild<QLabel*>(name);
            if (!lbl)
                return;
            lbl->setText(text);
            if (!color.isEmpty())
                lbl->setStyleSheet(
                    QString(
                        "color:%1; font-size:" + fincept::ui::fonts::px(-1) + "px; font-weight:700; background:transparent;")
                        .arg(color));
        };

        // Helper: populate a bid or ask QTableWidget from a JSON array [[price,size],...]
        auto fill_book_table = [this](const QString& name, const QJsonArray& levels, bool is_bid) {
            auto* tbl = this->findChild<QTableWidget*>(name);
            if (!tbl)
                return;
            const int n = qMin(levels.size(), 10);
            tbl->setRowCount(n);
            double cumulative = 0.0;
            const QString price_col = is_bid ? QString(ui::colors::POSITIVE()) : QString(ui::colors::NEGATIVE());
            for (int i = 0; i < n; ++i) {
                const auto row = levels[i].toArray();
                const double price = row.size() > 0 ? row[0].toDouble() : 0.0;
                const double size = row.size() > 1 ? row[1].toDouble() : 0.0;
                cumulative += size;
                auto* pi = new QTableWidgetItem(QString::number(price, 'f', 4));
                pi->setForeground(QColor(price_col));
                auto* si = new QTableWidgetItem(QString::number(size, 'g', 5));
                si->setForeground(QColor(QString(ui::colors::TEXT_PRIMARY())));
                auto* ci = new QTableWidgetItem(QString::number(cumulative, 'g', 6));
                ci->setForeground(QColor(QString(ui::colors::TEXT_SECONDARY())));
                tbl->setItem(i, 0, pi);
                tbl->setItem(i, 1, si);
                tbl->setItem(i, 2, ci);
                tbl->setRowHeight(i, row_px(22));
            }
        };

        // Helper: the slippage FILLS table (single estimate and full analysis both fill it;
        // an empty array clears it so a failed estimate does not leave the previous fills).
        auto fill_slip_table = [this](const QJsonArray& fills) {
            auto* tbl = this->findChild<QTableWidget*>("hft_slip_table");
            if (!tbl)
                return;
            tbl->setRowCount(fills.size());
            for (int i = 0; i < fills.size(); ++i) {
                const auto f = fills[i].toObject();
                tbl->setItem(i, 0, new QTableWidgetItem(QString::number(f["price"].toDouble(), 'f', 4)));
                tbl->setItem(i, 1, new QTableWidgetItem(QString::number(f["quantity"].toDouble(), 'g', 6)));
                tbl->setItem(i, 2, new QTableWidgetItem(QString::number(f["cost"].toDouble(), 'f', 4)));
                tbl->setRowHeight(i, row_px(20));
            }
        };

        // ── fetch_orderbook ────────────────────────────────────────────────
        if (command == "fetch_orderbook") {
            const auto bids = payload["bids"].toArray();
            const auto asks = payload["asks"].toArray();
            const double mid = payload["mid_price"].toDouble();
            const double spread_bps = payload["spread_bps"].toDouble();
            const double obi = payload["obi"].toDouble();
            const QString pres = payload["pressure"].toString();
            const double wmid = payload["weighted_mid"].toDouble();
            const double lat = payload["latency_ms"].toDouble();

            // Update latency badge
            if (auto* lbl = this->findChild<QLabel*>("hftLatency"))
                lbl->setText(tr("LATENCY  %1 ms").arg(lat, 0, 'f', 1));

            // Metric cards
            set_card("hft_mid_val", QString::number(mid, 'f', 4));
            set_card("hft_spread_val", QString::number(spread_bps, 'f', 3) + tr(" bps"));
            const QString obi_color = obi > 0.1    ? QString(ui::colors::POSITIVE())
                                      : obi < -0.1 ? QString(ui::colors::NEGATIVE())
                                                   : QString(ui::colors::TEXT_PRIMARY());
            set_card("hft_obi_val", QString::number(obi, 'f', 4), obi_color);
            const QString pres_color = pres == "BUY"    ? QString(ui::colors::POSITIVE())
                                       : pres == "SELL" ? QString(ui::colors::NEGATIVE())
                                                        : QString(ui::colors::TEXT_PRIMARY());
            set_card("hft_pressure_val", pres, pres_color);
            set_card("hft_wmid_val", QString::number(wmid, 'f', 4));

            fill_book_table("hft_bid_table", bids, true);
            fill_book_table("hft_ask_table", asks, false);

            status_label_->setText(tr("%1  mid: %2  spread: %3 bps  OBI: %4")
                                       .arg(payload["symbol"].toString())
                                       .arg(mid, 0, 'f', 4)
                                       .arg(spread_bps, 0, 'f', 3)
                                       .arg(obi, 0, 'f', 4));
            return;
        }

        // ── market_making ─────────────────────────────────────────────────
        if (command == "market_making") {
            const auto mm = payload["market_making"].toObject();
            // Also update book metrics if present
            if (payload.contains("book_metrics")) {
                const auto bm = payload["book_metrics"].toObject();
                set_card("hft_mid_val", QString::number(bm["mid_price"].toDouble(), 'f', 4));
                set_card("hft_spread_val", QString::number(bm["spread_bps"].toDouble(), 'f', 3) + tr(" bps"));
            }
            set_card("hft_mm_bid", QString::number(mm["bid_price"].toDouble(), 'f', 4),
                     QString(ui::colors::POSITIVE()));
            set_card("hft_mm_ask", QString::number(mm["ask_price"].toDouble(), 'f', 4),
                     QString(ui::colors::NEGATIVE()));
            set_card("hft_mm_qspread", QString::number(mm["quoted_spread_bps"].toDouble(), 'f', 3) + tr(" bps"));
            set_card("hft_mm_edge", QString::number(mm["edge_per_side_bps"].toDouble(), 'f', 3) + tr(" bps"));
            const QString rec = mm["recommendation"].toString();
            const QString r_col = rec == "WIDEN"     ? QString(ui::colors::WARNING())
                                  : rec == "TIGHTEN" ? QString(ui::colors::POSITIVE())
                                                     : QString(ui::colors::TEXT_PRIMARY());
            set_card("hft_mm_rec", rec, r_col);
            status_label_->setText(tr("Bid: %1  Ask: %2  Edge: %3 bps/side")
                                       .arg(mm["bid_price"].toDouble(), 0, 'f', 4)
                                       .arg(mm["ask_price"].toDouble(), 0, 'f', 4)
                                       .arg(mm["edge_per_side_bps"].toDouble(), 0, 'f', 3));
            return;
        }

        // ── toxic_flow ────────────────────────────────────────────────────
        if (command == "toxic_flow") {
            const auto tf = payload["toxic_flow"].toObject();
            const bool is_toxic = tf["is_toxic"].toBool();
            const double score = tf["toxicity_score"].toDouble();
            const QString cls = tf["classification"].toString();
            const QString action = tf["action"].toString();

            const QString score_col = score > 60   ? QString(ui::colors::NEGATIVE())
                                      : score > 30 ? QString(ui::colors::WARNING())
                                                   : QString(ui::colors::POSITIVE());
            set_card("hft_tox_pin", QString::number(score, 'f', 1), score_col);
            set_card("hft_tox_vol", QString::number(tf["vol_imbalance"].toDouble(), 'f', 4));
            set_card("hft_tox_impact", QString::number(tf["price_impact_bps"].toDouble(), 'f', 2) + tr(" bps"));
            const QString cls_col = cls == "HIGH_RISK"  ? QString(ui::colors::NEGATIVE())
                                    : cls == "ELEVATED" ? QString(ui::colors::WARNING())
                                                        : QString(ui::colors::POSITIVE());
            set_card("hft_tox_class", cls, cls_col);
            const QString act_col = action == "WIDEN_SPREADS" || action == "STEP_BACK"
                                        ? QString(ui::colors::WARNING())
                                        : QString(ui::colors::POSITIVE());
            set_card("hft_tox_action", QString(action).replace('_', ' '), act_col);
            status_label_->setText(is_toxic ? tr("TOXIC FLOW DETECTED — score: %1 — %2").arg(score, 0, 'f', 1).arg(cls)
                                            : tr("Flow clean — score: %1 — %2").arg(score, 0, 'f', 1).arg(cls));
            return;
        }

        // ── slippage ──────────────────────────────────────────────────────
        if (command == "slippage") {
            const auto sl = payload; // flat: average_price, slippage_bps, fills, etc at top level
            const double avg_p = sl["average_price"].toDouble();
            const double sl_bps = sl["slippage_bps"].toDouble();
            const double cost = sl["total_cost"].toDouble();
            const int n_fills = sl["fill_count"].toInt();
            const bool viable = sl["viable"].toBool();

            set_card("hft_slip_avgp", QString::number(avg_p, 'f', 4));
            const QString bps_col = sl_bps < 5    ? QString(ui::colors::POSITIVE())
                                    : sl_bps < 20 ? QString(ui::colors::WARNING())
                                                  : QString(ui::colors::NEGATIVE());
            set_card("hft_slip_bps", QString::number(sl_bps, 'f', 4) + tr(" bps"), bps_col);
            set_card("hft_slip_cost", QString::number(cost, 'f', 4));
            set_card("hft_slip_fills", QString::number(n_fills));
            set_card("hft_slip_viable", viable ? tr("VIABLE") : tr("HIGH SLIP"),
                     viable ? QString(ui::colors::POSITIVE()) : QString(ui::colors::NEGATIVE()));

            fill_slip_table(sl["fills"].toArray());
            status_label_->setText(tr("Avg fill: %1  |  Slippage: %2 bps  |  %3")
                                       .arg(avg_p, 0, 'f', 4)
                                       .arg(sl_bps, 0, 'f', 4)
                                       .arg(viable ? tr("VIABLE") : tr("HIGH SLIPPAGE")));
            return;
        }

        // ── analyze (full analysis) ───────────────────────────────────────
        if (command == "analyze") {
            // Sub-analyses report their own failure as {"error": "..."} (too few trades,
            // zero spread, thin book). Their cards used to render as 0.0000 and the toxic-flow
            // card as a CLEAN reading; collect the messages and show them instead.
            QStringList partial_errors;
            auto sub_error = [&partial_errors, &set_card](const QJsonObject& sub, const QString& what,
                                                          const QStringList& cards) {
                const QString err = sub.value("error").toString();
                if (err.isEmpty())
                    return false;
                partial_errors << QString("%1: %2").arg(what, err);
                for (const auto& card : cards) // do not leave the previous run's values on screen
                    set_card(card, QStringLiteral("—"));
                return true;
            };
            // Dispatch sub-results to each section
            if (payload.contains("bids")) {
                const auto bids = payload["bids"].toArray();
                const auto asks = payload["asks"].toArray();
                fill_book_table("hft_bid_table", bids, true);
                fill_book_table("hft_ask_table", asks, false);
            }
            if (payload.contains("book_metrics")) {
                const auto bm = payload["book_metrics"].toObject();
                set_card("hft_mid_val", QString::number(bm["mid_price"].toDouble(), 'f', 4));
                set_card("hft_spread_val", QString::number(bm["spread_bps"].toDouble(), 'f', 3) + tr(" bps"));
                const double obi = bm["obi"].toDouble();
                const QString obi_col = obi > 0.1    ? QString(ui::colors::POSITIVE())
                                        : obi < -0.1 ? QString(ui::colors::NEGATIVE())
                                                     : QString(ui::colors::TEXT_PRIMARY());
                set_card("hft_obi_val", QString::number(obi, 'f', 4), obi_col);
                const QString pres = bm["pressure"].toString();
                const QString p_col = pres == "BUY"    ? QString(ui::colors::POSITIVE())
                                      : pres == "SELL" ? QString(ui::colors::NEGATIVE())
                                                       : QString(ui::colors::TEXT_PRIMARY());
                set_card("hft_pressure_val", pres, p_col);
                set_card("hft_wmid_val", QString::number(bm["weighted_mid"].toDouble(), 'f', 4));
            }
            if (payload.contains("market_making") && !sub_error(payload["market_making"].toObject(), tr("Quotes"),
                                                                {"hft_mm_bid", "hft_mm_ask", "hft_mm_qspread",
                                                                 "hft_mm_edge", "hft_mm_rec"})) {
                const auto mm = payload["market_making"].toObject();
                set_card("hft_mm_bid", QString::number(mm["bid_price"].toDouble(), 'f', 4),
                         QString(ui::colors::POSITIVE()));
                set_card("hft_mm_ask", QString::number(mm["ask_price"].toDouble(), 'f', 4),
                         QString(ui::colors::NEGATIVE()));
                set_card("hft_mm_qspread", QString::number(mm["quoted_spread_bps"].toDouble(), 'f', 3) + tr(" bps"));
                set_card("hft_mm_edge", QString::number(mm["edge_per_side_bps"].toDouble(), 'f', 3) + tr(" bps"));
                set_card("hft_mm_rec", mm["recommendation"].toString());
            }
            if (payload.contains("toxic_flow") && !sub_error(payload["toxic_flow"].toObject(), tr("Toxic flow"),
                                                              {"hft_tox_pin", "hft_tox_vol", "hft_tox_impact",
                                                               "hft_tox_class", "hft_tox_action"})) {
                const auto tf = payload["toxic_flow"].toObject();
                const double sc = tf["toxicity_score"].toDouble();
                const QString col = sc > 60   ? QString(ui::colors::NEGATIVE())
                                    : sc > 30 ? QString(ui::colors::WARNING())
                                              : QString(ui::colors::POSITIVE());
                set_card("hft_tox_pin", QString::number(sc, 'f', 1), col);
                set_card("hft_tox_vol", QString::number(tf["vol_imbalance"].toDouble(), 'f', 4));
                set_card("hft_tox_impact", QString::number(tf["price_impact_bps"].toDouble(), 'f', 2) + tr(" bps"));
                set_card("hft_tox_class", tf["classification"].toString());
                set_card("hft_tox_action", QString(tf["action"].toString()).replace('_', ' '));
            }
            if (payload.contains("slippage")) {
                if (sub_error(payload["slippage"].toObject(), tr("Slippage"),
                              {"hft_slip_avgp", "hft_slip_bps", "hft_slip_cost", "hft_slip_fills",
                               "hft_slip_viable"})) {
                    fill_slip_table(QJsonArray()); // no fills for this run: drop the previous estimate's rows
                } else {
                    const auto sl = payload["slippage"].toObject();
                    set_card("hft_slip_avgp", QString::number(sl["average_price"].toDouble(), 'f', 4));
                    set_card("hft_slip_bps", QString::number(sl["slippage_bps"].toDouble(), 'f', 4) + tr(" bps"));
                    set_card("hft_slip_cost", QString::number(sl["total_cost"].toDouble(), 'f', 4));
                    set_card("hft_slip_fills", QString::number(sl["fill_count"].toInt()));
                    const bool v = sl["viable"].toBool();
                    set_card("hft_slip_viable", v ? tr("VIABLE") : tr("HIGH SLIP"),
                             v ? QString(ui::colors::POSITIVE()) : QString(ui::colors::NEGATIVE()));
                    fill_slip_table(sl["fills"].toArray());
                }
            }
            if (auto* lat_lbl = this->findChild<QLabel*>("hftLatency"))
                lat_lbl->setText(tr("LATENCY  %1 ms").arg(payload["latency_ms"].toDouble(), 0, 'f', 1));

            status_label_->setText(
                partial_errors.isEmpty()
                    ? tr("Full analysis complete — %1 @ %2")
                          .arg(payload["symbol"].toString(), payload["timestamp"].toString().left(19))
                    : tr("Analysis incomplete — %1").arg(partial_errors.join("  |  ")));

            // The headline of the snapshot under the tabs (the tabs hold the detail): what was
            // read, from where, and what could not be computed.
            const QJsonObject bm = payload["book_metrics"].toObject();
            const QJsonObject tf = payload["toxic_flow"].toObject();
            const QJsonObject sl = payload["slippage"].toObject();
            results_layout_->addWidget(gs_section_header(
                tr("SNAPSHOT  |  %1 ON %2").arg(payload["symbol"].toString(), payload["exchange"].toString().toUpper()),
                ui::colors::legible(module_.color)));
            auto* strip = new ui::KpiStrip(result_host_);
            using HTone = ui::StatTile::Tone;
            auto tile = [strip](const QString& label, const QString& value, HTone tone) {
                auto* t = strip->add_tile(label);
                t->set_value(value);
                t->set_tone(tone);
            };
            const QString dash = QString::fromUtf8("—");
            tile(tr("MID"), bm.contains("mid_price") ? QString::number(bm["mid_price"].toDouble(), 'f', 4) : dash,
                 HTone::Neutral);
            tile(tr("SPREAD"), bm.contains("spread_bps") ? tr("%1 bps").arg(bm["spread_bps"].toDouble(), 0, 'f', 3) : dash,
                 HTone::Neutral);
            const QString pres = bm["pressure"].toString();
            tile(tr("PRESSURE"), pres.isEmpty() ? dash : pres,
                 pres == QLatin1String("BUY") ? HTone::Positive : pres == QLatin1String("SELL") ? HTone::Negative
                                                                                                : HTone::Neutral);
            tile(tr("TOXICITY"),
                 tf.contains("toxicity_score") && !tf.contains("error")
                     ? tr("%1 · %2").arg(tf["toxicity_score"].toDouble(), 0, 'f', 1).arg(tf["classification"].toString())
                     : dash,
                 tf["toxicity_score"].toDouble() > 60 ? HTone::Warning : HTone::Neutral);
            tile(tr("SLIPPAGE"),
                 sl.contains("slippage_bps") && !sl.contains("error")
                     ? tr("%1 bps").arg(sl["slippage_bps"].toDouble(), 0, 'f', 2)
                     : dash,
                 HTone::Neutral);
            tile(tr("LATENCY"), tr("%1 ms").arg(payload["latency_ms"].toDouble(), 0, 'f', 0), HTone::Neutral);
            results_layout_->addWidget(strip);
            if (!partial_errors.isEmpty()) {
                auto* note = new QLabel(tr("Not computed: %1").arg(partial_errors.join(QStringLiteral(" · "))),
                                        result_host_);
                note->setWordWrap(true);
                note->setStyleSheet(QString("color:%1; font-size:%2px; background:transparent;")
                                        .arg(ui::colors::TEXT_SECONDARY(), ui::fonts::px(-3)));
                results_layout_->addWidget(note);
            }
            return;
        }

        display_result(payload);
        return;
    }

    // ── Rolling Retraining ───────────────────────────────────────────────────
    if (module_id == "rolling_retraining") {
        // The outcome of a retrain (its "done" event): what was trained, in how long.
        if (command == QLatin1String("retrain") && payload.value("event").toString() == QLatin1String("done")) {
            clear_results();
            const QString accent = ui::colors::legible(module_.color);
            results_layout_->addWidget(gs_section_header(
                tr("ROLLING RETRAIN  |  %1").arg(payload["model_id"].toString(payload["exp_name"].toString())), accent));
            results_layout_->addWidget(gs_card_row(
                {gs_make_card(tr("WINDOWS TRAINED"), QString::number(payload["windows_trained"].toInt()), this,
                              ui::colors::SUCCESS()),
                 gs_make_card(tr("TIME"), tr("%1 s").arg(payload["elapsed_sec"].toDouble(), 0, 'f', 1), this),
                 gs_make_card(tr("EXPERIMENT"), payload["exp_name"].toString(QStringLiteral("—")), this)},
                this));
            return;
        }
        // ── Non-streaming responses ──────────────────────────────────────────
        if (!payload["success"].toBool()) {
            if (command == "retrain") {
                // A retrain refused before it started (unknown model id, Qlib missing,
                // config gone) is terminal too: release the launch buttons, or they stay
                // disabled for the rest of the session.
                set_retrain_busy(this, false);
                if (auto* pb = this->findChild<QProgressBar*>("rr_progress"))
                    pb->setFormat(tr("Failed"));
            }
            display_error(payload["error"].toString());
            return;
        }
        clear_results();

        if (command == "list") {
            auto schedules = payload["schedules"].toObject();
            if (schedules.isEmpty()) {
                // Nothing saved yet: say what a config is and where to make one.
                auto* es = ui::EmptyState::create(
                    ui::EmptyState::no_data_spec(tr("No retrain configs yet"),
                                                 tr("A config names the model and the rolling windows it is "
                                                    "retrained on. Create one, then run it from here."),
                                                 tr("CREATE CONFIG")),
                    result_host_);
                es->set_compact(true);
                es->set_alignment(Qt::AlignTop);
                connect(es, &ui::EmptyState::primary_clicked, this, [this]() {
                    if (current_view() != QLatin1String("create")) {
                        open_view(QStringLiteral("create"));
                        return;
                    }
                    for (auto* b : findChildren<QPushButton*>())
                        if (b->property("rrCreateButton").toBool() && b->isEnabled()) {
                            b->click();
                            break;
                        }
                });
                results_layout_->addWidget(es);
                status_label_->setText(tr("No retrain configs"));
                return;
            }
            results_layout_->addWidget(gs_section_header(
                tr("RETRAIN CONFIGS  |  %n saved", "", static_cast<int>(schedules.size())),
                ui::colors::legible(module_.color)));
            QWidget* cards_w = result_host_;
            {
                {
                    int i = 0;
                    for (auto it = schedules.begin(); it != schedules.end(); ++it, ++i) {
                        const QString mid = it.key();
                        auto obj = it.value().toObject();

                        // Card frame
                        auto* card = new QWidget(cards_w);
                        card->setStyleSheet(
                            QString("QWidget{background:%1;border:1px solid %2;border-radius:6px;}"
                                    "QWidget:hover{border-color:%3;}")
                                .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_MED(), ui::colors::legible(module_.color)));

                        auto* cvl = new QVBoxLayout(card);
                        cvl->setContentsMargins(12, 10, 12, 10);
                        cvl->setSpacing(4);

                        // Top row: model ID + status badge
                        auto* top = new QWidget(card);
                        top->setStyleSheet("QWidget{background:transparent;border:none;}");
                        auto* thl = new QHBoxLayout(top);
                        thl->setContentsMargins(0, 0, 0, 0);

                        auto* id_lbl = new QLabel(mid, top);
                        id_lbl->setStyleSheet(QString("color:%1;font-weight:700;font-size:" + fincept::ui::fonts::px(-1) + "px;"
                                                      "background:transparent;border:none;")
                                                  .arg(ui::colors::TEXT_PRIMARY()));
                        thl->addWidget(id_lbl);
                        thl->addStretch();

                        const QString status = obj["last_status"].toString(tr("pending"));
                        // A state, not a direction: SUCCESS / DANGER, with the word beside it.
                        const QString badge_color = (status == "completed") ? ui::colors::SUCCESS()
                                                    : (status == "failed")  ? ui::colors::DANGER()
                                                                            : ui::colors::WARNING();
                        auto* badge = new QLabel(status.toUpper(), top);
                        badge->setStyleSheet(QString("color:%1;background:transparent;border:none;"
                                                     "font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:700;")
                                                 .arg(badge_color));
                        thl->addWidget(badge);
                        cvl->addWidget(top);

                        // Details row. No frequency / next run: nothing runs a config on a
                        // timer (retrains start only from Run Now / EXECUTE RETRAIN NOW).
                        QString window = tr("from config");
                        if (obj.contains("train_start")) { // created since the window is applied
                            if (obj["window"].isDouble())
                                window = tr("%1 trading days").arg(obj["window"].toInt());
                            else if (!obj["train_start"].toString().isEmpty())
                                window = tr("template, from %1").arg(obj["train_start"].toString());
                        } else if (obj["template_generated"].toBool()) {
                            window = tr("template, from 2010-01-01"); // older configs ignored the window
                        }
                        const QString last = obj["last_run"].isNull()
                                                 ? tr("Never")
                                                 : obj["last_run"].toString().left(16).replace("T", "  ");
                        auto* detail = new QLabel(tr("Window: %1  |  Step: %2  |  Last: %3")
                                                      .arg(window, QString::number(obj["step"].toInt(20)), last),
                                                  card);
                        detail->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-3) + "px;"
                                                      "background:transparent;border:none;")
                                                  .arg(ui::colors::TEXT_SECONDARY()));
                        cvl->addWidget(detail);

                        // Action buttons row
                        auto* acts = new QWidget(card);
                        acts->setStyleSheet("QWidget{background:transparent;border:none;}");
                        auto* ahl = new QHBoxLayout(acts);
                        ahl->setContentsMargins(0, 4, 0, 0);
                        ahl->setSpacing(6);

                        auto* run_btn = new QPushButton(tr("Run Now"), acts);
                        run_btn->setProperty("rrRetrain", true);
                        run_btn->setEnabled(!this->property("rrBusy").toBool());
                        const QString run_shape = QStringLiteral(
                            "border:1px solid transparent; border-radius:3px; font-weight:700; padding:3px 10px;");
                        run_btn->setProperty(kAccentShapeProp, run_shape);
                        run_btn->setProperty(kAccentFontProp, -4);
                        run_btn->setStyleSheet(accent_button_ss(module_.color, run_shape, -4));
                        connect(run_btn, &QPushButton::clicked, this, [this, mid]() {
                            // The Execute Retrain view runs it: its progress bar, log and result.
                            open_view(QStringLiteral("execute"));
                            if (QLineEdit* id = text_inputs_.value(QStringLiteral("rr_exec_id")))
                                id->setText(mid);
                            for (auto* b : findChildren<QPushButton*>())
                                if (b->property("rrExecButton").toBool() && b->isEnabled()) {
                                    b->click();
                                    break;
                                }
                        });
                        ahl->addWidget(run_btn);

                        auto* del_btn = new QPushButton(tr("Delete"), acts);
                        del_btn->setProperty("fincept_agent_risk", "delete");
                        del_btn->setStyleSheet(
                            QString("QPushButton{background:transparent;color:%1;border:1px solid %1;"
                                    "border-radius:3px;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:700;padding:3px 10px;}"
                                    "QPushButton:hover{background:%2;}")
                                .arg(ui::colors::DANGER(), ui::colors::NEGATIVE_BG()));
                        connect(del_btn, &QPushButton::clicked, this, [this, mid]() {
                            // Removed from the saved configs at once: ask first, naming it.
                            auto* box = new QMessageBox(
                                QMessageBox::Warning, tr("Delete retrain config"),
                                tr("Delete the retrain config \"%1\"?\n\nIts saved settings (window, step, last "
                                   "run) are removed, with the YAML config this lab generated for it. Your own "
                                   "config files and trained models are not touched.")
                                    .arg(mid),
                                QMessageBox::NoButton, this);
                            box->setAttribute(Qt::WA_DeleteOnClose);
                            auto* confirm = box->addButton(tr("Delete config"), QMessageBox::DestructiveRole);
                            confirm->setProperty(fincept::core::agent::kAgentRiskProperty, "delete"); // as Delete
                            QPushButton* cancel = box->addButton(QMessageBox::Cancel);
                            box->setDefaultButton(cancel);
                            box->setEscapeButton(cancel);
                            connect(box, &QMessageBox::buttonClicked, this, [this, mid, confirm](QAbstractButton* b) {
                                if (b != confirm)
                                    return;
                                status_label_->setText(tr("Deleting %1...").arg(mid));
                                QJsonObject p;
                                p["model_id"] = mid;
                                AIQuantLabService::instance().rolling_delete_schedule(p);
                            });
                            box->open();
                        });
                        ahl->addWidget(del_btn);
                        ahl->addStretch();
                        cvl->addWidget(acts);

                        results_layout_->addWidget(card);
                    }
                }
            }
            status_label_->setText(tr("%1 schedule(s)").arg(schedules.size()));
            return;
        }

        if (command == "create") {
            auto sched = payload["schedule"].toObject();
            const QString tmpl =
                payload["template_generated"].toBool()
                    ? tr("\nBuilt-in LightGBM+Alpha158 template generated at:\n%1").arg(payload["conf_path"].toString())
                    : "";
            clear_results();
            QString window = tr("from config");
            if (sched["window"].isDouble())
                window = tr("%1 trading days (training from %2)")
                             .arg(sched["window"].toInt())
                             .arg(sched["train_start"].toString());
            else if (!sched["train_start"].toString().isEmpty())
                window = tr("template default (training from %1)").arg(sched["train_start"].toString());
            QString note = payload["note"].toString();
            if (!note.isEmpty())
                note.prepend(QLatin1Char('\n'));
            // The generated template's segments follow the chosen dataset's calendar.
            const QJsonObject seg = payload["segments"].toObject();
            if (!seg.isEmpty()) {
                auto span = [&seg](const char* key) {
                    const QJsonArray a = seg.value(QLatin1String(key)).toArray();
                    return QStringLiteral("%1 → %2").arg(a.at(0).toString(), a.at(1).toString());
                };
                note += tr("\nDataset %1 (%2)  |  train %3  |  valid %4  |  test %5")
                            .arg(payload["dataset_id"].toString(), sched["market"].toString(), span("train"),
                                 span("valid"), span("test"));
            }
            auto* lbl = new QLabel(tr("Retrain config created: %1\nWindow: %2  |  Step: %3%4%5")
                                       .arg(payload["model_id"].toString(), window,
                                            QString::number(sched["step"].toInt(20)), tmpl, note),
                                   this);
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1;").arg(ui::colors::TEXT_PRIMARY()));
            results_layout_->addWidget(lbl);
            status_label_->setText(tr("Retrain config created — switch to the Retrain Configs tab to view"));
            return;
        }

        if (command == "preview") {
            clear_results();
            const int total = payload["total_windows"].toInt();
            auto windows = payload["windows"].toArray();

            auto* hdr = new QLabel(tr("Preview: %1 rolling windows  (step=%2, horizon=%3)")
                                       .arg(total)
                                       .arg(payload["step"].toInt())
                                       .arg(payload["horizon"].toInt()),
                                   this);
            hdr->setStyleSheet(QString("color:%1;font-weight:700;font-size:" + fincept::ui::fonts::px(-1) + "px;").arg(ui::colors::TEXT_PRIMARY()));
            results_layout_->addWidget(hdr);

            // Table of windows. Sized from the returned array, not from
            // total_windows — the Python side caps the preview list, so a larger
            // `total` produced silently blank trailing rows (and a smaller one
            // dropped windows because setItem() past rowCount is a no-op).
            auto* tbl = new QTableWidget(int(windows.size()), 4, this);
            tbl->setStyleSheet(table_ss());
            tbl->setHorizontalHeaderLabels({"#", tr("Train Start"), tr("Train End"), tr("Test Period")});
            tbl->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            tbl->verticalHeader()->setVisible(false);
            tbl->setEditTriggers(QAbstractItemView::NoEditTriggers);
            tbl->setSelectionBehavior(QAbstractItemView::SelectRows);
            tbl->setAlternatingRowColors(true);
            int row = 0;
            for (const auto& v : windows) {
                auto obj = v.toObject();
                // Dates are cut to yyyy-MM-dd; the composed "start → end" cell must not be,
                // or the test END date was cut off.
                auto item = [&](const QString& s, bool truncate = true) {
                    auto* it = new QTableWidgetItem(truncate ? s.left(10) : s);
                    it->setTextAlignment(Qt::AlignCenter);
                    return it;
                };
                tbl->setItem(row, 0, item(QString::number(row + 1)));
                tbl->setItem(row, 1, item(obj["train_start"].toString()));
                tbl->setItem(row, 2, item(obj["train_end"].toString()));
                tbl->setItem(row, 3,
                             item(QString("%1 → %2").arg(obj["test_start"].toString().left(10),
                                                         obj["test_end"].toString().left(10)),
                                  false));
                ++row;
            }
            tbl->setMaximumHeight(220);
            results_layout_->addWidget(tbl);
            status_label_->setText(tr("Preview: %1 windows").arg(total));
            return;
        }

        if (command == "delete") {
            status_label_->setText(tr("Deleted: %1").arg(payload["model_id"].toString()));
            // Auto-refresh the schedule list
            AIQuantLabService::instance().rolling_list_schedules();
            return;
        }

        display_result(payload);
        return;
    }

    display_result(payload);
}

} // namespace fincept::screens
