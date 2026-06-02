// src/screens/economics/panels/EconPanelBase.cpp
#include "screens/economics/panels/EconPanelBase.h"

#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QColor>
#include <QFile>
#include <algorithm>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QListWidget>
#include <QTextStream>
#include <QVBoxLayout>

namespace fincept::screens {

// ── Stylesheet (token-based) ──────────────────────────────────────────────────

QString EconPanelBase::panel_style() const {
    using namespace ui::colors;
    int r = QColor(color_).red();
    int g = QColor(color_).green();
    int b = QColor(color_).blue();
    return QString("#econToolbar { background:%5; border-bottom:1px solid %6; }"
                   "#econFetchBtn { background:%1; color:%7; border:none;"
                   "  font-size:10px; font-weight:700; padding:4px 14px; }"
                   "#econFetchBtn:hover { background:rgba(%2,%3,%4,0.75); }"
                   "#econFetchBtn:disabled { background:%6; color:%8; }"
                   "#econCsvBtn { background:transparent; color:%9; border:1px solid %6;"
                   "  font-size:10px; font-weight:700; padding:4px 10px; }"
                   "#econCsvBtn:hover { color:%10; background:%11; }"
                   "QComboBox, QSpinBox, QDateEdit, QLineEdit {"
                   "  background:%7; color:%10; border:1px solid %6;"
                   "  font-size:11px; padding:2px 6px; }"
                   "QComboBox::drop-down, QDateEdit::drop-down { border:none; }"
                   "QComboBox QAbstractItemView { background:%5; color:%10;"
                   "  border:1px solid %6; }"
                   "QTableWidget { background:%7; color:%10; border:none;"
                   "  gridline-color:%6; font-size:11px; alternate-background-color:%12; }"
                   "QTableWidget::item { padding:5px 8px; border-bottom:1px solid %6; }"
                   "QTableWidget::item:selected { background:rgba(%2,%3,%4,0.1); color:%1; }"
                   "QHeaderView::section { background:%5; color:%9; border:none;"
                   "  border-bottom:2px solid %6; border-right:1px solid %6;"
                   "  padding:5px 8px; font-size:10px; font-weight:700; letter-spacing:0.5px; }"
                   "#econCardsRow  { background:%7; border-bottom:1px solid %6; }"
                   "#econTitleBar  { background:%12; border-bottom:1px solid %6; }"
                   "#econStatCard { background:%12; border:1px solid %6; }"
                   "#econStatCard:hover { border-color:%13; }"
                   "#econStatLabel { color:%9; font-size:8px; font-weight:700;"
                   "  letter-spacing:1px; background:transparent; }"
                   "#econStatVal  { color:%1;  font-size:15px; font-weight:700; background:transparent; }"
                   "#econStatPos  { color:%16; font-size:15px; font-weight:700; background:transparent; }"
                   "#econStatNeg  { color:%15; font-size:15px; font-weight:700; background:transparent; }"
                   "#econStatSub  { color:%14; font-size:9px;  background:transparent; }"
                   "#econEmptyPage { background:%7; }"
                   "#econEmptyMsg   { color:%9;  font-size:13px; background:transparent; }"
                   "#econLoadingMsg { color:%1;  font-size:13px; background:transparent; }"
                   "#econErrMsg     { color:%15; font-size:12px; background:transparent; }"
                   "#econTitleLbl { color:%10; font-size:11px; font-weight:700;"
                   "  background:transparent; }"
                   "#econRowCount { color:%14; font-size:9px; background:transparent; }"
                   "QScrollBar:vertical { background:%7; width:5px; }"
                   "QScrollBar::handle:vertical { background:%6; min-height:20px; }"
                   "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }"
                   "QScrollBar:horizontal { background:%7; height:5px; }"
                   "QScrollBar::handle:horizontal { background:%6; min-width:20px; }"
                   "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width:0; }")
        .arg(color_) // %1: source accent
        .arg(r)
        .arg(g)
        .arg(b)                // %2,%3,%4: accent RGB
        .arg(BG_RAISED())      // %5
        .arg(BORDER_DIM())     // %6
        .arg(BG_BASE())        // %7
        .arg(TEXT_DIM())       // %8
        .arg(TEXT_SECONDARY()) // %9
        .arg(TEXT_PRIMARY())   // %10
        .arg(BG_HOVER())       // %11
        .arg(BG_SURFACE())     // %12
        .arg(BORDER_BRIGHT())  // %13
        .arg(TEXT_TERTIARY())  // %14
        .arg(NEGATIVE())       // %15
        .arg(POSITIVE());      // %16
}

// ── Reusable token-based style helpers ────────────────────────────────────────

QString EconPanelBase::sidebar_style() {
    using namespace ui::colors;
    return QString("background:%1; border-right:1px solid %2;").arg(BG_SURFACE(), BORDER_DIM());
}

QString EconPanelBase::section_hdr_style() {
    using namespace ui::colors;
    return QString("background:%1; border-bottom:1px solid %2;").arg(BG_RAISED(), BORDER_DIM());
}

QString EconPanelBase::section_lbl_style() {
    using namespace ui::colors;
    return QString("color:%1; font-size:8px; font-weight:700;"
                   " letter-spacing:1px; background:transparent; padding:4px 10px;")
        .arg(TEXT_TERTIARY());
}

QString EconPanelBase::search_input_style() {
    using namespace ui::colors;
    return QString("background:%1; color:%2; border:none;"
                   " border-bottom:1px solid %3; padding:4px 10px; font-size:11px;")
        .arg(BG_BASE(), TEXT_PRIMARY(), BORDER_DIM());
}

QString EconPanelBase::ctrl_label_style() {
    using namespace ui::colors;
    return QString("color:%1; font-size:9px; font-weight:700; background:transparent;").arg(TEXT_TERTIARY());
}

QString EconPanelBase::list_style() const {
    using namespace ui::colors;
    QColor c(color_);
    QString rgba = QString("%1,%2,%3").arg(c.red()).arg(c.green()).arg(c.blue());
    return QString("QListWidget { background:transparent; border:none; outline:none; }"
                   "QListWidget::item { color:%1; padding:5px 10px;"
                   "  border-bottom:1px solid %2; font-size:11px; }"
                   "QListWidget::item:hover { color:%3; background:%4; }"
                   "QListWidget::item:selected { color:%5; background:rgba(%6,0.1);"
                   "  border-left:2px solid %5; font-weight:700; }")
        .arg(TEXT_SECONDARY(), BG_RAISED(), TEXT_PRIMARY(), BG_HOVER())
        .arg(color_, rgba);
}

QString EconPanelBase::notice_style() {
    using namespace ui::colors;
    return QString("color:%1; font-size:9px; background:transparent;").arg(WARNING());
}

// ── Constructor ───────────────────────────────────────────────────────────────

EconPanelBase::EconPanelBase(const QString& source_id, const QString& color, QWidget* parent)
    : QWidget(parent), source_id_(source_id), color_(color) {
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this, &EconPanelBase::refresh_panel_theme);
}

// ── Build ─────────────────────────────────────────────────────────────────────

void EconPanelBase::build_base_ui(QWidget* container) {
    container_ = container;
    container->setStyleSheet(panel_style());

    auto* root = new QVBoxLayout(container);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Toolbar
    auto* toolbar = new QWidget(this);
    toolbar->setObjectName("econToolbar");
    toolbar->setFixedHeight(40);
    auto* thl = new QHBoxLayout(toolbar);
    thl->setContentsMargins(10, 0, 10, 0);
    thl->setSpacing(6);
    build_controls(thl);
    thl->addStretch(1);

    fetch_btn_ = new QPushButton(tr("FETCH"));
    fetch_btn_->setObjectName("econFetchBtn");
    fetch_btn_->setCursor(Qt::PointingHandCursor);
    connect(fetch_btn_, &QPushButton::clicked, this, &EconPanelBase::on_fetch);

    export_btn_ = new QPushButton(tr("CSV"));
    export_btn_->setObjectName("econCsvBtn");
    export_btn_->setCursor(Qt::PointingHandCursor);
    connect(export_btn_, &QPushButton::clicked, this, &EconPanelBase::export_csv);

    thl->addWidget(fetch_btn_);
    thl->addWidget(export_btn_);
    root->addWidget(toolbar);

    // Stat cards row
    cards_row_ = new QWidget(this);
    cards_row_->setObjectName("econCardsRow");
    auto* crhl = new QHBoxLayout(cards_row_);
    crhl->setContentsMargins(10, 6, 10, 6);
    crhl->setSpacing(6);

    auto make_card = [&](const QString& label, QLabel*& out, QLabel*& title_out) {
        auto* card = new QWidget(this);
        card->setObjectName("econStatCard");
        card->setMinimumWidth(90);
        auto* vl = new QVBoxLayout(card);
        vl->setContentsMargins(10, 6, 10, 6);
        vl->setSpacing(1);
        auto* lbl = new QLabel(label);
        lbl->setObjectName("econStatLabel");
        title_out = lbl;
        out = new QLabel("—");
        out->setObjectName("econStatVal");
        vl->addWidget(lbl);
        vl->addWidget(out);
        crhl->addWidget(card);
    };
    make_card(tr("LATEST"), stat_latest_, stat_latest_lbl_);
    make_card(tr("CHANGE"), stat_change_, stat_change_lbl_);
    make_card(tr("MIN"), stat_min_, stat_min_lbl_);
    make_card(tr("MAX"), stat_max_, stat_max_lbl_);
    make_card(tr("AVG"), stat_avg_, stat_avg_lbl_);
    make_card(tr("POINTS"), stat_count_, stat_count_lbl_);
    crhl->addStretch(1);
    root->addWidget(cards_row_);

    // Title bar
    title_bar_ = new QWidget(this);
    title_bar_->setObjectName("econTitleBar");
    auto* tbhl = new QHBoxLayout(title_bar_);
    tbhl->setContentsMargins(12, 4, 12, 4);
    title_lbl_ = new QLabel;
    title_lbl_->setObjectName("econTitleLbl");
    row_count_ = new QLabel;
    row_count_->setObjectName("econRowCount");
    tbhl->addWidget(title_lbl_);
    tbhl->addStretch(1);
    tbhl->addWidget(row_count_);
    root->addWidget(title_bar_);

    // Content stack
    stack_ = new QStackedWidget;

    auto* empty_pg = new QWidget(this);
    empty_pg->setObjectName("econEmptyPage");
    auto* evl = new QVBoxLayout(empty_pg);
    evl->setAlignment(Qt::AlignCenter);
    empty_lbl_ = new QLabel(tr("Select parameters and click FETCH"));
    empty_lbl_->setObjectName("econEmptyMsg");
    empty_lbl_->setAlignment(Qt::AlignCenter);
    empty_lbl_->setWordWrap(true);
    status_kind_ = StatusKind::Empty;
    status_msg_ = tr("Select parameters and click FETCH");
    evl->addWidget(empty_lbl_);
    stack_->addWidget(empty_pg); // index 0

    auto* table_page = new QWidget(this);
    auto* tpl = new QVBoxLayout(table_page);
    tpl->setContentsMargins(0, 0, 0, 0);
    tpl->setSpacing(0);

    table_ = new QTableWidget;
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->verticalHeader()->setVisible(false);
    table_->setAlternatingRowColors(true);
    table_->setShowGrid(true);
    tpl->addWidget(table_, 1);

    pager_ = new ui::PaginationBar(this);
    connect(pager_, &ui::PaginationBar::page_changed, this, [this]() { render_page(); });
    tpl->addWidget(pager_);

    stack_->addWidget(table_page); // index 1

    root->addWidget(stack_, 1);

    // Apply token-based inline styles for non-QSS-covered widgets
    refresh_panel_theme();
}

// ── Theme refresh ─────────────────────────────────────────────────────────────

void EconPanelBase::refresh_panel_theme() {
    if (container_)
        container_->setStyleSheet(panel_style());
}

// ── State ─────────────────────────────────────────────────────────────────────

void EconPanelBase::show_loading(const QString& msg) {
    if (!empty_lbl_)
        return;
    status_kind_ = StatusKind::Loading;
    status_msg_ = msg.isEmpty() ? tr("Fetching data…") : msg;
    empty_lbl_->setObjectName("econLoadingMsg");
    empty_lbl_->style()->unpolish(empty_lbl_);
    empty_lbl_->style()->polish(empty_lbl_);
    empty_lbl_->setText(status_msg_);
    stack_->setCurrentIndex(0);
    if (fetch_btn_)
        fetch_btn_->setEnabled(false);
}

void EconPanelBase::show_error(const QString& msg) {
    if (!empty_lbl_)
        return;
    status_kind_ = StatusKind::Error;
    status_msg_ = msg;
    empty_lbl_->setObjectName("econErrMsg");
    empty_lbl_->style()->unpolish(empty_lbl_);
    empty_lbl_->style()->polish(empty_lbl_);
    empty_lbl_->setText(tr("Error: %1").arg(msg));
    stack_->setCurrentIndex(0);
    if (fetch_btn_)
        fetch_btn_->setEnabled(true);
}

void EconPanelBase::show_empty(const QString& msg) {
    if (!empty_lbl_)
        return;
    status_kind_ = StatusKind::Empty;
    status_msg_ = msg.isEmpty() ? tr("Select parameters and click FETCH") : msg;
    empty_lbl_->setObjectName("econEmptyMsg");
    empty_lbl_->style()->unpolish(empty_lbl_);
    empty_lbl_->style()->polish(empty_lbl_);
    empty_lbl_->setText(status_msg_);
    stack_->setCurrentIndex(0);
    if (fetch_btn_)
        fetch_btn_->setEnabled(true);
}

void EconPanelBase::show_table() {
    stack_->setCurrentIndex(1);
    if (fetch_btn_)
        fetch_btn_->setEnabled(true);
}

// ── Display ───────────────────────────────────────────────────────────────────

void EconPanelBase::display(const QJsonArray& rows, const QString& title) {
    if (rows.isEmpty()) {
        show_empty(tr("No data returned for this selection"));
        return;
    }

    all_rows_ = rows;

    columns_.clear();
    for (const auto& v : rows) {
        for (const auto& k : v.toObject().keys())
            if (!columns_.contains(k))
                columns_ << k;
    }
    static const QStringList kDateKeys = {"date",   "period",      "year",        "time",    "Date",
                                          "Period", "TIME_PERIOD", "record_date", "ref_date"};
    for (const auto& dk : kDateKeys) {
        int idx = columns_.indexOf(dk);
        if (idx > 0) {
            columns_.move(idx, 0);
            break;
        }
    }
    static const QStringList kValKeys = {"value", "Value", "val", "OBS_VALUE", "obs_value", "amount", "rate"};
    for (const auto& vk : kValKeys) {
        int idx = columns_.indexOf(vk);
        if (idx > 1) {
            columns_.move(idx, 1);
            break;
        }
    }

    pager_->set_total(rows.size());
    render_page();

    if (title_lbl_)
        title_lbl_->setText(title);
    if (row_count_)
        row_count_->setText(tr("%1 records").arg(rows.size()));

    update_stats(rows);
    show_table();
}

void EconPanelBase::render_page() {
    const int start = pager_->offset();
    const int count = std::min<int>(pager_->page_size(), all_rows_.size() - start);

    table_->setColumnCount(columns_.size());
    table_->setHorizontalHeaderLabels(columns_);
    table_->setRowCount(count);

    for (int r = 0; r < count; ++r) {
        const auto obj = all_rows_[start + r].toObject();
        for (int c = 0; c < columns_.size(); ++c) {
            const auto jv = obj[columns_[c]];
            QString text;
            if (jv.isDouble())
                text = QString::number(jv.toDouble(), 'g', 8);
            else
                text = jv.toString();
            auto* item = new QTableWidgetItem(text);
            if (c == 0)
                item->setForeground(QColor(color_));
            table_->setItem(r, c, item);
        }
    }

    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    if (columns_.size() > 1)
        table_->horizontalHeader()->setSectionResizeMode(columns_.size() - 1, QHeaderView::Stretch);
}

void EconPanelBase::update_stats(const QJsonArray& rows) {
    QVector<double> vals;
    for (const auto& v : rows) {
        const auto obj = v.toObject();
        QJsonValue jv = obj["value"];
        if (jv.isUndefined())
            jv = obj["Value"];
        if (jv.isUndefined())
            jv = obj["OBS_VALUE"];
        if (jv.isUndefined())
            jv = obj["obs_value"];
        if (jv.isUndefined())
            jv = obj["amount"];
        if (jv.isUndefined())
            jv = obj["rate"];
        if (jv.isUndefined()) {
            auto keys = obj.keys();
            if (keys.size() > 1)
                jv = obj[keys[1]];
        }
        bool ok = false;
        double d = jv.toString().toDouble(&ok);
        if (!ok && jv.isDouble()) {
            d = jv.toDouble();
            ok = true;
        }
        if (ok)
            vals << d;
    }

    if (vals.isEmpty()) {
        for (auto* l : {stat_latest_, stat_change_, stat_min_, stat_max_, stat_avg_, stat_count_})
            if (l)
                l->setText("—");
        if (stat_count_)
            stat_count_->setText(QString::number(rows.size()));
        return;
    }

    double latest = vals.last();
    double prev = vals.size() > 1 ? vals[vals.size() - 2] : latest;
    double change = (prev != 0.0) ? ((latest - prev) / qAbs(prev)) * 100.0 : 0.0;
    double mn = *std::min_element(vals.begin(), vals.end());
    double mx = *std::max_element(vals.begin(), vals.end());
    double sum = 0.0;
    for (double x : vals)
        sum += x;
    double avg = sum / vals.size();

    auto fmt = [](double v) { return QString::number(v, 'g', 6); };

    if (stat_latest_)
        stat_latest_->setText(fmt(latest));
    if (stat_change_) {
        stat_change_->setText((change >= 0 ? "+" : "") + QString::number(change, 'f', 2) + "%");
        stat_change_->setObjectName(change >= 0 ? "econStatPos" : "econStatNeg");
        stat_change_->style()->unpolish(stat_change_);
        stat_change_->style()->polish(stat_change_);
    }
    if (stat_min_)
        stat_min_->setText(fmt(mn));
    if (stat_max_)
        stat_max_->setText(fmt(mx));
    if (stat_avg_)
        stat_avg_->setText(fmt(avg));
    if (stat_count_)
        stat_count_->setText(QString::number(rows.size()));
}

// ── CSV ───────────────────────────────────────────────────────────────────────

void EconPanelBase::export_csv() {
    if (!table_ || table_->rowCount() == 0)
        return;

    QString path =
        QFileDialog::getSaveFileName(this, tr("Export CSV"), source_id_ + "_data.csv", tr("CSV Files (*.csv)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;
    QTextStream out(&file);

    QStringList hdrs;
    for (int c = 0; c < table_->columnCount(); ++c) {
        auto* h = table_->horizontalHeaderItem(c);
        hdrs << (h ? h->text() : "");
    }
    out << hdrs.join(",") << "\n";

    for (int r = 0; r < table_->rowCount(); ++r) {
        QStringList row;
        for (int c = 0; c < table_->columnCount(); ++c) {
            auto* item = table_->item(r, c);
            QString val = item ? item->text() : "";
            if (val.contains(',') || val.contains('"'))
                val = "\"" + val.replace("\"", "\"\"") + "\"";
            row << val;
        }
        out << row.join(",") << "\n";
    }
}

// ── Shared utilities ──────────────────────────────────────────────────────────

void EconPanelBase::filter_list(QListWidget* list, const QString& text) {
    if (!list)
        return;
    for (int i = 0; i < list->count(); ++i) {
        auto* item = list->item(i);
        item->setHidden(!item->text().contains(text, Qt::CaseInsensitive));
    }
}

// ── i18n ──────────────────────────────────────────────────────────────────────

void EconPanelBase::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void EconPanelBase::retranslateUi() {
    // Toolbar buttons
    if (fetch_btn_)
        fetch_btn_->setText(tr("FETCH"));
    if (export_btn_)
        export_btn_->setText(tr("CSV"));

    // Stat-card titles
    if (stat_latest_lbl_)
        stat_latest_lbl_->setText(tr("LATEST"));
    if (stat_change_lbl_)
        stat_change_lbl_->setText(tr("CHANGE"));
    if (stat_min_lbl_)
        stat_min_lbl_->setText(tr("MIN"));
    if (stat_max_lbl_)
        stat_max_lbl_->setText(tr("MAX"));
    if (stat_avg_lbl_)
        stat_avg_lbl_->setText(tr("AVG"));
    if (stat_count_lbl_)
        stat_count_lbl_->setText(tr("POINTS"));

    // Record count (re-apply count with translated suffix)
    if (row_count_ && !all_rows_.isEmpty())
        row_count_->setText(tr("%1 records").arg(all_rows_.size()));

    // Current status message. The empty default re-translates; loading/error
    // messages keep the message that was last shown (data-derived prefixes are
    // re-translated on the next fetch).
    if (empty_lbl_ && stack_ && stack_->currentIndex() == 0) {
        switch (status_kind_) {
        case StatusKind::Empty:
            empty_lbl_->setText(status_msg_);
            break;
        case StatusKind::Loading:
            empty_lbl_->setText(status_msg_);
            break;
        case StatusKind::Error:
            empty_lbl_->setText(tr("Error: %1").arg(status_msg_));
            break;
        }
    }
}

} // namespace fincept::screens
