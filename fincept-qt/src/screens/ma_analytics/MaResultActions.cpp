// MaResultActions.cpp — MA-9: COPY / CSV / PNG / ADD TO REPORT above every M&A result. Everything is read from
// the result already on screen (the pane's payload, formatted by the same rules as the view, plus the sensitivity
// grids and charts drawn for it); nothing is recomputed. The report payload carries the KPIs, the tables, images
// of the grids / charts and an echo of the inputs behind the result.
#include "screens/ma_analytics/MAModulePanel.h"

#include "core/logging/Logger.h"
#include "core/report/ReportBlocks.h"
#include "screens/ma_analytics/MaDealGrid.h"
#include "screens/ma_analytics/MaResultExport.h"
#include "screens/ma_analytics/MaResultFormat.h"
#include "ui/charts/ChartExport.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/ResultView.h"
#include "ui/widgets/SensitivityTable.h"
#include "ui/widgets/report/AddToReportButton.h"
#include "ui/widgets/symbol/SymbolSearchBox.h"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QTabWidget>

#include <algorithm>

namespace fincept::screens {

namespace {

QByteArray ma_ra_png(const QPixmap& pm) {
    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    pm.save(&buf, "PNG");
    return bytes;
}

QString ma_ra_file_name(const QString& title) {
    QString s = title.toLower();
    static const QRegularExpression kNonWord(QStringLiteral("[^a-z0-9]+"));
    s.replace(kNonWord, QStringLiteral("_"));
    return QStringLiteral("ma_") + s.left(60).trimmed();
}

} // namespace

void MAModulePanel::build_result_actions(QWidget* parent) {
    actions_bar_ = new QWidget(parent);
    actions_bar_->setObjectName(QStringLiteral("maResultActions"));
    actions_bar_->setStyleSheet(QString("QLabel#maResultTitle { color:%1; font-size:%2px; font-weight:700;"
                                        " background:transparent; }"
                                        "QPushButton#maActionBtn { background:%3; color:%4; border:1px solid %5;"
                                        " padding:3px 10px; font-size:%6px; font-weight:600; }"
                                        "QPushButton#maActionBtn:hover { color:%1; border-color:%7; }"
                                        "QPushButton#maActionBtn:disabled { color:%8; border-color:%5; }")
                                    .arg(ui::colors::TEXT_PRIMARY())
                                    .arg(ui::fonts::px(-1))
                                    .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED())
                                    .arg(ui::fonts::px(-2))
                                    .arg(ui::colors::AMBER(), ui::colors::TEXT_TERTIARY()));
    auto* hl = new QHBoxLayout(actions_bar_);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(6);
    result_title_label_ = new QLabel(actions_bar_);
    result_title_label_->setObjectName(QStringLiteral("maResultTitle"));
    result_title_label_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    hl->addWidget(result_title_label_, 1);
    auto make_btn = [this](const QString& text, const QString& tip) {
        auto* b = new QPushButton(text, actions_bar_);
        b->setObjectName(QStringLiteral("maActionBtn"));
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(tip);
        b->setEnabled(false);
        return b;
    };
    copy_btn_ = make_btn(tr("COPY"), tr("Copy the key figures and tables as tab-separated text"));
    csv_btn_ = make_btn(tr("CSV"), tr("Save the key figures and every table as one CSV file"));
    png_btn_ = make_btn(tr("PNG"), tr("Save the result page as a PNG image"));
    connect(copy_btn_, &QPushButton::clicked, this, &MAModulePanel::copy_result);
    connect(csv_btn_, &QPushButton::clicked, this, &MAModulePanel::export_csv);
    connect(png_btn_, &QPushButton::clicked, this, &MAModulePanel::export_png);
    hl->addWidget(copy_btn_);
    hl->addWidget(csv_btn_);
    hl->addWidget(png_btn_);
    report_btn_ = new ui::AddToReportButton(QStringLiteral("M&A Analytics"), actions_bar_);
    report_btn_->set_payload_provider([this]() { return report_payload(); });
    report_btn_->setEnabled(false);
    hl->addWidget(report_btn_);
}

QString MAModulePanel::result_title() const {
    const QString tab = (sub_tabs_ && pane_) ? tab_title(sub_tabs_->currentIndex()) : QString();
    return tab.isEmpty() ? module_.label : module_.label + QStringLiteral(" · ") + tab;
}

void MAModulePanel::update_result_actions() {
    if (!actions_bar_ || !pane_)
        return;
    const bool has = pane_->has_result() && pane_->state() == ui::AutoLoadPane::State::Content &&
                     !pane_->payload().toObject().contains(QStringLiteral("needs_data"));
    for (QPushButton* b : {copy_btn_, csv_btn_, png_btn_})
        if (b)
            b->setEnabled(has);
    if (report_btn_)
        report_btn_->setEnabled(has);
    if (result_title_label_)
        result_title_label_->setText(result_title().toUpper());
}

QVector<QPair<QString, QString>> MAModulePanel::inputs_echo() const {
    QVector<QPair<QString, QString>> out;
    QWidget* page = sub_tabs_ ? sub_tabs_->currentWidget() : nullptr;
    // The company the inputs were loaded for, when there is one.
    for (auto it = company_boxes_.cbegin(); it != company_boxes_.cend(); ++it)
        if (it.value() && !it.value()->text().trimmed().isEmpty())
            out.append({it.key() == QLatin1String("acquirer") ? tr("Acquirer") : tr("Company"), it.value()->text().trimmed()});
    if (!page)
        return out;
    // In screen order: the row labels and the values as shown.
    QVector<QPair<int, QPair<QString, QString>>> rows;
    for (auto it = input_labels_.cbegin(); it != input_labels_.cend(); ++it) {
        QWidget* w = it.key();
        if (!w || !page->isAncestorOf(w))
            continue;
        QString value;
        if (auto* s = qobject_cast<QAbstractSpinBox*>(w))
            value = s->text();
        else if (auto* c = qobject_cast<QComboBox*>(w))
            value = c->currentText();
        else if (auto* l = qobject_cast<QLineEdit*>(w))
            value = l->text();
        else if (auto* b = qobject_cast<ui::SymbolSearchBox*>(w))
            value = b->text();
        if (value.trimmed().isEmpty())
            continue;
        rows.append({w->mapTo(page, QPoint(0, 0)).y(), {it.value(), value.trimmed()}});
    }
    std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    for (const auto& r : rows)
        out.append(r.second);
    for (MaDealGrid* g : page->findChildren<MaDealGrid*>())
        out.append({tr("Comparables"), tr("%n valid row(s)", nullptr, g->valid_count())});
    if (comparison_grid_ && module_.id == services::ma::ModuleId::Comparison)
        out.append({tr("Deals"), tr("%n valid row(s)", nullptr, comparison_grid_->valid_count())});
    return out;
}

void MAModulePanel::copy_result() {
    if (!pane_)
        return;
    const QString context = pane_->view();
    const QJsonObject flat = ma_fmt::ma_flatten_result(pane_->payload().toObject());
    const QString intrinsic = module_.id == services::ma::ModuleId::Deals ? QStringLiteral("USD") : QString();
    const ma_export::Export e = ma_export::build(flat, ma_fmt::ma_result_options(context, flat, intrinsic));
    QString text = ma_export::to_tsv(result_title(), e);
    for (const auto& g : shown_grids_)
        if (g && !g->is_empty())
            text += QStringLiteral("\n\n") + g->title() + QLatin1Char('\n') + g->to_tsv();
    if (auto* cb = QApplication::clipboard())
        cb->setText(text);
    status_label_->setText(tr("Copied to the clipboard"));
}

void MAModulePanel::export_csv() {
    if (!pane_)
        return;
    const QString context = pane_->view();
    const QJsonObject flat = ma_fmt::ma_flatten_result(pane_->payload().toObject());
    const QString intrinsic = module_.id == services::ma::ModuleId::Deals ? QStringLiteral("USD") : QString();
    const ma_export::Export e = ma_export::build(flat, ma_fmt::ma_result_options(context, flat, intrinsic));
    auto sheet = ma_export::to_sheet(e);
    for (const auto& g : shown_grids_) {
        if (!g || g->is_empty())
            continue;
        sheet.second.append(QStringList());
        sheet.second.append({g->title()});
        sheet.second.append(g->headers());
        sheet.second += g->rows();
    }
    ui::ChartExport::save_csv(sheet.first, sheet.second, ma_ra_file_name(result_title()), this);
}

void MAModulePanel::export_png() {
    if (result_view_ && result_view_->content_widget())
        ui::ChartExport::save_png(result_view_->content_widget(), ma_ra_file_name(result_title()));
}

report::ReportPayload MAModulePanel::report_payload() {
    report::ReportPayload p;
    if (!pane_)
        return p;
    const QString context = pane_->view();
    const QJsonObject flat = ma_fmt::ma_flatten_result(pane_->payload().toObject());
    const QString intrinsic = module_.id == services::ma::ModuleId::Deals ? QStringLiteral("USD") : QString();
    const ma_export::Export e = ma_export::build(flat, ma_fmt::ma_result_options(context, flat, intrinsic));
    p.title = result_title();
    p.source = QStringLiteral("M&A Analytics");
    p.as_of = pane_->as_of();
    for (const auto& kv : e.kpis)
        p.kpis.append(report::PayloadKpi{kv.first, kv.second, QString(), QString()});
    for (const ma_export::Table& t : e.tables) {
        report::PayloadTable rt;
        rt.title = t.title;
        rt.columns = t.headers;
        rt.rows = t.rows;
        p.tables.append(rt);
    }
    for (const auto& g : shown_grids_) {
        if (!g || g->is_empty())
            continue;
        report::PayloadTable rt;
        rt.title = g->title() + (g->subtitle().isEmpty() ? QString() : QStringLiteral(" (") + g->subtitle() + QLatin1Char(')'));
        rt.columns = g->headers();
        rt.rows = g->rows();
        rt.note = tr("Base case = the inputs of this result");
        p.tables.append(rt);
    }
    for (const auto& c : shown_charts_) {
        if (!c.second || !c.second->isVisible())
            continue;
        report::PayloadChart ch;
        ch.title = c.first;
        ch.kind = QStringLiteral("bar");
        ch.image = ma_ra_png(ui::ChartExport::grab(c.second.data()));
        p.charts.append(ch);
    }
    p.inputs = inputs_echo();
    if (context == QLatin1String("fairness_opinion"))
        p.notes = flat.value(QStringLiteral("not_a_fairness_opinion")).toString();
    LOG_INFO("MAAnalytics", QStringLiteral("Result sent to report module=%1 context=%2 tables=%3 charts=%4")
                                .arg(module_.key, context)
                                .arg(p.tables.size())
                                .arg(p.charts.size()));
    return p;
}

} // namespace fincept::screens
