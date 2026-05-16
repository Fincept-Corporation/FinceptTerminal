// src/screens/report_builder/ReportBuilderScreen_Dialogs.cpp
//
// Modal dialogs (Recent / Template / Theme / Metadata) + the file-action
// slots (on_new / on_open / on_save / on_export_pdf / on_preview).
//
// Part of the partial-class split of ReportBuilderScreen.cpp.

#include "screens/report_builder/ReportBuilderScreen.h"

#include "core/session/ScreenStateManager.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "services/file_manager/FileManagerService.h"
#include "services/markets/MarketDataService.h"
#include "services/report_builder/ReportBuilderService.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPointer>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QShowEvent>
#include <QHideEvent>
#include <QTextDocument>
#include <QTextFrame>
#include <QVBoxLayout>

namespace fincept::screens {

namespace rep = ::fincept::report;
using Service = ::fincept::services::ReportBuilderService;

void ReportBuilderScreen::show_recent_dialog() {
    QStringList recent = Service::instance().recent_files();

    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("Recent Reports");
    dlg->setMinimumWidth(500);
    dlg->setStyleSheet(
        QString("QDialog { background: %1; color: %2; }"
                "QListWidget { background: %3; color: %2; border: 1px solid %4; }"
                "QListWidget::item { padding: 6px; }"
                "QListWidget::item:selected { background: %5; }"
                "QPushButton { background: %3; color: %2; border: 1px solid %4; padding: 6px 16px; }"
                "QPushButton:hover { background: %5; }")
            .arg(ui::colors::DARK(), ui::colors::WHITE(), ui::colors::PANEL(), ui::colors::BORDER(),
                 ui::colors::BG_RAISED()));

    auto* vl = new QVBoxLayout(dlg);
    auto* lbl = new QLabel("Select a report to open:");
    lbl->setStyleSheet(QString("color: %1;").arg(ui::colors::MUTED()));
    vl->addWidget(lbl);

    auto* list = new QListWidget;
    if (recent.isEmpty()) {
        list->addItem("(No recent reports)");
    } else {
        list->addItems(recent);
    }
    vl->addWidget(list, 1);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Open | QDialogButtonBox::Cancel);
    vl->addWidget(bb);

    auto open_selected = [list, recent, dlg, this]() {
        int row = list->currentRow();
        if (row >= 0 && row < recent.size()) {
            auto r = Service::instance().load_from(recent[row]);
            if (r.is_err())
                QMessageBox::warning(this, "Open Report",
                                     "Could not open file:\n" + recent[row] + "\n\n" +
                                         QString::fromStdString(r.error()));
        }
        dlg->accept();
    };

    connect(bb, &QDialogButtonBox::accepted, dlg, open_selected);
    connect(bb, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    connect(list, &QListWidget::itemDoubleClicked, dlg, [open_selected](QListWidgetItem*) { open_selected(); });

    dlg->exec();
    dlg->deleteLater();
}

void ReportBuilderScreen::show_template_dialog() {
    struct TemplateEntry {
        QString name;
        QString description;
    };
    struct Category {
        QString label;
        QVector<TemplateEntry> entries;
    };

    const QVector<Category> categories = {
        {"General Purpose",
         {{"Blank Report", "Empty document with title and date — start from scratch."},
          {"Meeting Notes", "Agenda, attendees, discussion points, action items, next steps."},
          {"Investment Memo", "Executive summary, thesis, opportunity, risks, recommendation."}}},
        {"Retail Investor",
         {{"Stock Research", "Company overview, financials, valuation, thesis, and risks."},
          {"Portfolio Review", "Holdings, performance, allocation pie, risk metrics, notes."},
          {"Watchlist Report", "Tracked tickers with price, change, target, and notes columns."},
          {"Dividend Income Report", "Dividend stocks table, yield chart, income projection, calendar."}}},
        {"Trader",
         {{"Daily Market Brief", "Indices snapshot, top movers, news highlights, watchlist."},
          {"Trade Journal", "Trade log table, P&L chart, win rate stats, lessons learned."},
          {"Technical Analysis", "Price chart placeholder, indicators table, S/R levels, setup."},
          {"Pre-Market Checklist", "Macro events, key levels, planned trades, risk per trade."}}},
        {"Institutional / Analyst",
         {{"Equity Research Report", "Full buy-side template: overview, financials, DCF, comparables."},
          {"Earnings Review", "Revenue/EPS vs estimates, guidance, segment breakdown, outlook."},
          {"M&A Deal Summary", "Deal overview, rationale, comparables, synergies, valuation."},
          {"Sector Deep Dive", "Sector overview, sub-industries, key players, trends, risks."}}},
        {"Economist / Macro",
         {{"Macro Economic Summary", "GDP, inflation, unemployment, central bank policy, outlook."},
          {"Country Risk Report", "Political, economic, financial, market risk tables and charts."},
          {"Central Bank Monitor", "Rate decisions, forward guidance, balance sheet, implications."}}},
        {"Crypto / Digital Assets",
         {{"Crypto Research Report", "Project overview, tokenomics, on-chain metrics, price chart."},
          {"DeFi Protocol Analysis", "Protocol overview, TVL, revenue, risks, token valuation."},
          {"Crypto Portfolio Review", "Holdings, allocation, cost basis table, P&L, risk exposure."}}},
        {"Fixed Income",
         {{"Bond Research Report", "Issuer overview, credit profile, yield analysis, recommendation."},
          {"Yield Curve Analysis", "Curve chart, spread table, duration/convexity, macro drivers."}}},
        {"Quant / Risk",
         {{"Quant Strategy Report", "Strategy description, backtest results, stats, drawdown chart."},
          {"Risk Management Report", "VaR, CVaR, stress tests, correlation matrix, recommendations."}}},
        {"Corporate / Business",
         {{"Business Performance", "KPI dashboard, revenue chart, cost breakdown, targets vs actual."},
          {"Project Status Report", "Objectives, milestones table, status, risks, next actions."},
          {"Financial Statement", "Income statement, balance sheet, cash flow tables."}}},
    };

    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("Report Templates");
    dlg->setMinimumSize(680, 520);
    dlg->setStyleSheet(QString("QDialog    { background: %1; color: %2; }"
                               "QListWidget { background: %3; color: %2; border: 1px solid %4; }"
                               "QListWidget::item { padding: 6px 8px; font-size: 12px; }"
                               "QListWidget::item:selected { background: %5; color: %6; }"
                               "QPushButton { background: %3; color: %2; border: 1px solid %4; padding: 6px 16px; }"
                               "QPushButton:hover { background: %5; }"
                               "QLabel { background: transparent; }")
                           .arg(ui::colors::DARK(), ui::colors::WHITE(), ui::colors::PANEL(), ui::colors::BORDER(),
                                ui::colors::AMBER_DIM(), ui::colors::AMBER()));

    auto* hl = new QHBoxLayout(dlg);
    hl->setSpacing(0);
    hl->setContentsMargins(0, 0, 0, 0);

    auto* left = new QWidget(this);
    left->setMinimumWidth(280);
    auto* lvl = new QVBoxLayout(left);
    lvl->setContentsMargins(12, 12, 8, 12);
    lvl->setSpacing(6);

    auto* pick_lbl = new QLabel("Choose a template:");
    pick_lbl->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: bold;").arg(ui::colors::MUTED()));
    lvl->addWidget(pick_lbl);

    auto* list = new QListWidget;
    list->setSpacing(1);
    for (const auto& cat : categories) {
        auto* cat_item = new QListWidgetItem("  " + cat.label.toUpper());
        cat_item->setFlags(Qt::NoItemFlags);
        cat_item->setForeground(QColor(ui::colors::AMBER()));
        QFont f = cat_item->font();
        f.setBold(true);
        f.setPointSize(10);
        cat_item->setFont(f);
        cat_item->setBackground(QColor(ui::colors::BG_RAISED()));
        list->addItem(cat_item);
        for (const auto& tmpl : cat.entries) {
            auto* item = new QListWidgetItem("    " + tmpl.name);
            item->setData(Qt::UserRole, tmpl.name);
            item->setData(Qt::UserRole + 1, tmpl.description);
            list->addItem(item);
        }
    }
    lvl->addWidget(list, 1);
    hl->addWidget(left);

    auto* sep = new QFrame;
    sep->setFixedWidth(1);
    sep->setStyleSheet(QString("background: %1;").arg(ui::colors::BORDER()));
    hl->addWidget(sep);

    auto* right = new QWidget(this);
    right->setMinimumWidth(280);
    auto* rvl = new QVBoxLayout(right);
    rvl->setContentsMargins(12, 12, 12, 12);
    rvl->setSpacing(8);

    auto* tmpl_name_lbl = new QLabel("Select a template");
    tmpl_name_lbl->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: bold;").arg(ui::colors::AMBER()));
    tmpl_name_lbl->setWordWrap(true);
    rvl->addWidget(tmpl_name_lbl);

    auto* desc_lbl = new QLabel("Pick a template from the list to see a description.");
    desc_lbl->setWordWrap(true);
    desc_lbl->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    desc_lbl->setStyleSheet(QString("color: %1; font-size: 12px; line-height: 160%;").arg(ui::colors::GRAY()));
    rvl->addWidget(desc_lbl, 1);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    rvl->addWidget(bb);
    hl->addWidget(right);

    connect(list, &QListWidget::currentItemChanged, this,
            [tmpl_name_lbl, desc_lbl](QListWidgetItem* cur, QListWidgetItem*) {
                if (!cur || !(cur->flags() & Qt::ItemIsSelectable))
                    return;
                tmpl_name_lbl->setText(cur->data(Qt::UserRole).toString());
                desc_lbl->setText(cur->data(Qt::UserRole + 1).toString());
            });

    auto apply_selected = [list, dlg]() {
        auto* cur = list->currentItem();
        if (!cur || !(cur->flags() & Qt::ItemIsSelectable))
            return;
        QString tname = cur->data(Qt::UserRole).toString();
        if (!tname.isEmpty())
            Service::instance().apply_template(tname);
        dlg->accept();
    };

    connect(list, &QListWidget::itemDoubleClicked, dlg, [apply_selected](QListWidgetItem*) { apply_selected(); });
    connect(bb, &QDialogButtonBox::accepted, dlg, apply_selected);
    connect(bb, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

    dlg->exec();
    dlg->deleteLater();
}

void ReportBuilderScreen::show_theme_dialog() {
    const QStringList theme_names = rep::themes::all_names();

    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("Report Theme");
    dlg->setMinimumWidth(380);
    dlg->setStyleSheet(
        QString("QDialog { background: %1; color: %2; }"
                "QComboBox, QPushButton { background: %3; color: %2; border: 1px solid %4; padding: 4px 8px; }"
                "QLabel { background: transparent; color: %2; }")
            .arg(ui::colors::DARK(), ui::colors::WHITE(), ui::colors::PANEL(), ui::colors::BORDER()));

    auto* vl = new QVBoxLayout(dlg);
    vl->addWidget(new QLabel("Select a color theme for your report:"));

    auto* combo = new QComboBox;
    for (const auto& n : theme_names)
        combo->addItem(n);
    const QString current = Service::instance().theme().name;
    int cur_idx = theme_names.indexOf(current);
    if (cur_idx >= 0)
        combo->setCurrentIndex(cur_idx);
    vl->addWidget(combo);

    auto* preview_lbl = new QLabel;
    preview_lbl->setFixedHeight(40);
    preview_lbl->setAlignment(Qt::AlignCenter);
    auto update_preview = [preview_lbl, theme_names](int idx) {
        const auto t = rep::themes::by_name(theme_names.value(idx));
        preview_lbl->setStyleSheet(QString("background: %1; color: %2; border: 2px solid %3; font-weight: bold;")
                                       .arg(t.page_bg, t.heading_color, t.accent_color));
        preview_lbl->setText(t.name + " — Sample Text");
    };
    connect(combo, &QComboBox::currentIndexChanged, this, update_preview);
    update_preview(combo->currentIndex());
    vl->addWidget(preview_lbl);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    vl->addWidget(bb);

    connect(bb, &QDialogButtonBox::accepted, dlg, [combo, theme_names, dlg]() {
        Service::instance().set_theme(rep::themes::by_name(theme_names.value(combo->currentIndex())));
        dlg->accept();
    });
    connect(bb, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

    dlg->exec();
    dlg->deleteLater();
}

void ReportBuilderScreen::show_metadata_dialog() {
    auto m = Service::instance().metadata();

    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("Report Metadata");
    dlg->setMinimumWidth(420);
    dlg->setStyleSheet(
        QString("QDialog { background: %1; color: %2; }"
                "QLineEdit { background: %3; color: %2; border: 1px solid %4; padding: 4px; }"
                "QLabel { background: transparent; color: %2; }"
                "QPushButton { background: %3; color: %2; border: 1px solid %4; padding: 6px 16px; }"
                "QPushButton:hover { background: %5; }")
            .arg(ui::colors::DARK(), ui::colors::WHITE(), ui::colors::PANEL(), ui::colors::BORDER(),
                 ui::colors::BG_RAISED()));

    auto* vl = new QVBoxLayout(dlg);
    auto* form = new QFormLayout;
    form->setSpacing(8);

    auto* title_edit = new QLineEdit(m.title);
    auto* author_edit = new QLineEdit(m.author);
    auto* company_edit = new QLineEdit(m.company);
    auto* date_edit = new QLineEdit(m.date);
    date_edit->setPlaceholderText("yyyy-MM-dd");

    auto lbl_style = QString("color: %1;").arg(ui::colors::GRAY());
    auto add_row = [&](const char* text, QLineEdit* edit) {
        auto* lbl = new QLabel(text);
        lbl->setStyleSheet(lbl_style);
        form->addRow(lbl, edit);
    };
    add_row("Title:", title_edit);
    add_row("Author:", author_edit);
    add_row("Company:", company_edit);
    add_row("Date:", date_edit);
    vl->addLayout(form);

    auto* hf_lbl = new QLabel("HEADER / FOOTER");
    hf_lbl->setStyleSheet(
        QString("color: %1; font-size: 11px; font-weight: bold; padding-top: 10px;").arg(ui::colors::MUTED()));
    vl->addWidget(hf_lbl);

    auto* hf_form = new QFormLayout;
    hf_form->setSpacing(8);

    auto* hl_edit = new QLineEdit(m.header_left);
    auto* hc_edit = new QLineEdit(m.header_center);
    auto* hr_edit = new QLineEdit(m.header_right);
    auto* fl_edit = new QLineEdit(m.footer_left);
    auto* fc_edit = new QLineEdit(m.footer_center);
    auto* fr_edit = new QLineEdit(m.footer_right);

    hl_edit->setPlaceholderText("Left");
    hc_edit->setPlaceholderText("Center");
    hr_edit->setPlaceholderText("Right");
    fl_edit->setPlaceholderText("Left");
    fc_edit->setPlaceholderText("Center (use {page})");
    fr_edit->setPlaceholderText("Right");

    auto f_row = [&](const char* left_text, QLineEdit* l, QLineEdit* c, QLineEdit* r) {
        auto* row = new QWidget(this);
        row->setStyleSheet("background: transparent;");
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->addWidget(l);
        rl->addWidget(c);
        rl->addWidget(r);
        auto* lbl2 = new QLabel(left_text);
        lbl2->setStyleSheet(lbl_style);
        hf_form->addRow(lbl2, row);
    };
    f_row("Header:", hl_edit, hc_edit, hr_edit);
    f_row("Footer:", fl_edit, fc_edit, fr_edit);
    vl->addLayout(hf_form);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    vl->addWidget(bb);

    connect(bb, &QDialogButtonBox::accepted, dlg, [=]() {
        rep::ReportMetadata nm = m;
        nm.title = title_edit->text();
        nm.author = author_edit->text();
        nm.company = company_edit->text();
        nm.date = date_edit->text();
        nm.header_left = hl_edit->text();
        nm.header_center = hc_edit->text();
        nm.header_right = hr_edit->text();
        nm.footer_left = fl_edit->text();
        nm.footer_center = fc_edit->text();
        nm.footer_right = fr_edit->text();
        Service::instance().set_metadata(nm);
        dlg->accept();
    });
    connect(bb, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

    dlg->exec();
    dlg->deleteLater();
}

// ── File operations ──────────────────────────────────────────────────────────

void ReportBuilderScreen::on_new() {
    auto answer = QMessageBox::question(this, "New Report", "Create a new report? Unsaved changes will be lost.",
                                        QMessageBox::Yes | QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;
    Service::instance().clear_document();
    selected_id_ = 0;
    properties_->show_empty();
}

void ReportBuilderScreen::on_open() {
    QString path =
        QFileDialog::getOpenFileName(this, "Open Report", "", "Fincept Report (*.fincept);;JSON (*.json)");
    if (path.isEmpty())
        return;
    auto r = Service::instance().load_from(path);
    if (r.is_err())
        QMessageBox::warning(this, "Open Report", "Could not open file:\n" + path + "\n\n" +
                                                       QString::fromStdString(r.error()));
}

void ReportBuilderScreen::on_save() {
    auto& svc = Service::instance();
    QString path = svc.current_file();
    if (path.isEmpty()) {
        path = QFileDialog::getSaveFileName(this, "Save Report", svc.metadata().title,
                                            "Fincept Report (*.fincept);;JSON (*.json)");
        if (path.isEmpty())
            return;
    }
    auto r = svc.save_to(path);
    if (r.is_err()) {
        QMessageBox::warning(this, "Save Report", "Could not save to:\n" + path + "\n\n" +
                                                      QString::fromStdString(r.error()));
        return;
    }
    services::FileManagerService::instance().import_file(path, "report_builder");
}

void ReportBuilderScreen::on_export_pdf() {
    auto m = Service::instance().metadata();
    QString path = QFileDialog::getSaveFileName(this, "Export PDF", m.title, "PDF (*.pdf)");
    if (path.isEmpty())
        return;

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(path);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageMargins(QMarginsF(15, 20, 15, 20), QPageLayout::Millimeter);

    bool has_header = !m.header_left.isEmpty() || !m.header_center.isEmpty() || !m.header_right.isEmpty();
    bool has_footer = !m.footer_left.isEmpty() || !m.footer_center.isEmpty() || !m.footer_right.isEmpty() ||
                      m.show_page_numbers;

    if (!has_header && !has_footer) {
        canvas_->text_edit()->document()->print(&printer);
    } else {
        QTextDocument* doc = canvas_->text_edit()->document()->clone(this);

        QTextFrameFormat root_fmt = doc->rootFrame()->frameFormat();
        root_fmt.setTopMargin(has_header ? 24 : root_fmt.topMargin());
        root_fmt.setBottomMargin(has_footer ? 24 : root_fmt.bottomMargin());
        doc->rootFrame()->setFrameFormat(root_fmt);

        QPainter painter;
        if (!painter.begin(&printer)) {
            QMessageBox::warning(this, "Export PDF", "Failed to start PDF painter.");
            doc->deleteLater();
            return;
        }

        QRectF page_rect = printer.pageRect(QPrinter::DevicePixel);
        doc->setPageSize(page_rect.size());
        int total_pages = doc->pageCount();

        auto draw_hf_line = [&](int page, bool is_header) {
            QString left = is_header ? m.header_left : m.footer_left;
            QString center = is_header ? m.header_center : m.footer_center;
            QString right = is_header ? m.header_right : m.footer_right;

            auto replace_tokens = [&](QString s) {
                s.replace("{page}", QString::number(page));
                s.replace("{total}", QString::number(total_pages));
                return s;
            };
            left = replace_tokens(left);
            center = replace_tokens(center);
            right = replace_tokens(right);

            double y = is_header ? 4 : page_rect.height() - 16;
            QFont hf_font;
            hf_font.setPointSizeF(7);
            painter.setFont(hf_font);
            painter.setPen(QColor("#888888"));

            QRectF r(page_rect.left() + 8, y, page_rect.width() - 16, 14);
            if (!left.isEmpty())
                painter.drawText(r, Qt::AlignLeft | Qt::AlignVCenter, left);
            if (!center.isEmpty())
                painter.drawText(r, Qt::AlignHCenter | Qt::AlignVCenter, center);
            if (!right.isEmpty())
                painter.drawText(r, Qt::AlignRight | Qt::AlignVCenter, right);

            painter.setPen(QPen(QColor("#cccccc"), 0.5));
            double rule_y = is_header ? y + 14 : y - 2;
            painter.drawLine(QPointF(page_rect.left() + 8, rule_y), QPointF(page_rect.right() - 8, rule_y));
        };

        for (int page = 1; page <= total_pages; ++page) {
            if (page > 1)
                printer.newPage();
            painter.save();
            double content_top = has_header ? 20 : 0;
            double content_bottom = has_footer ? page_rect.height() - 20 : page_rect.height();
            painter.setClipRect(
                QRectF(page_rect.left(), content_top, page_rect.width(), content_bottom - content_top));
            painter.translate(0, -(page - 1) * page_rect.height() + content_top);
            doc->drawContents(&painter);
            painter.restore();
            if (has_header)
                draw_hf_line(page, true);
            if (has_footer)
                draw_hf_line(page, false);
        }

        painter.end();
        doc->deleteLater();
    }

    QMessageBox::information(this, "Export PDF", "Report exported successfully to:\n" + path);
    services::FileManagerService::instance().import_file(path, "report_builder");
}

void ReportBuilderScreen::on_preview() {
    QPrinter printer(QPrinter::HighResolution);
    printer.setPageSize(QPageSize(QPageSize::A4));
    QPrintPreviewDialog preview(&printer, this);
    connect(&preview, &QPrintPreviewDialog::paintRequested, this,
            [this](QPrinter* p) { canvas_->text_edit()->document()->print(p); });
    preview.exec();
}

// ── Show / Hide ──────────────────────────────────────────────────────────────

} // namespace fincept::screens
