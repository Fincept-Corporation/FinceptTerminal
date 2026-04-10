#include "screens/markets/MarketPanel.h"

#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QPointer>
#include <QResizeEvent>
#include <QVBoxLayout>

namespace fincept::screens {

MarketPanel::MarketPanel(const MarketPanelConfig& config, QWidget* parent)
    : QWidget(parent), config_(config) {
    if (config_.column_order.isEmpty())
        config_.column_order = default_market_columns();
    setMinimumWidth(180);
    setMinimumHeight(kHeaderH + kColHeaderH + kRowH * 4);  // always room for at least 4 rows
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    build_ui();
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { refresh_theme(); });
    refresh_theme();
}

// ---------------------------------------------------------------------------
// UI construction
// ---------------------------------------------------------------------------

void MarketPanel::build_ui() {
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // --- Header (28px) ---
    header_ = new QWidget(this);
    header_->setFixedHeight(kHeaderH);
    auto* hhl = new QHBoxLayout(header_);
    hhl->setContentsMargins(8, 0, 4, 0);
    hhl->setSpacing(2);

    title_label_ = new QLabel(config_.title.toUpper());
    hhl->addWidget(title_label_);
    hhl->addStretch();

    auto make_hdr_btn = [](const QString& text) -> QPushButton* {
        auto* b = new QPushButton(text);
        b->setFixedHeight(20);
        b->setCursor(Qt::PointingHandCursor);
        b->setFlat(true);
        return b;
    };

    cols_btn_   = make_hdr_btn("[COLS]");
    edit_btn_   = make_hdr_btn("[EDIT]");
    delete_btn_ = make_hdr_btn("[DEL]");

    hhl->addWidget(cols_btn_);
    hhl->addWidget(edit_btn_);
    hhl->addWidget(delete_btn_);

    connect(cols_btn_,   &QPushButton::clicked, this, &MarketPanel::open_cols_dropdown);
    connect(edit_btn_,   &QPushButton::clicked, this, [this]() { emit edit_requested(config_.id); });
    connect(delete_btn_, &QPushButton::clicked, this, [this]() { emit delete_requested(config_.id); });

    vl->addWidget(header_);

    // --- Body (fills remaining height) ---
    body_ = new QWidget(this);
    auto* bl = new QVBoxLayout(body_);
    bl->setContentsMargins(0, 0, 0, 0);
    bl->setSpacing(0);

    // Data table
    table_ = new QTableWidget(body_);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setShowGrid(false);
    table_->setFocusPolicy(Qt::NoFocus);
    table_->verticalHeader()->setVisible(false);
    table_->verticalHeader()->setDefaultSectionSize(kRowH);
    table_->horizontalHeader()->setFixedHeight(kColHeaderH);
    table_->horizontalHeader()->setHighlightSections(false);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setup_table_columns();
    bl->addWidget(table_);

    // Error state (hidden by default)
    error_widget_ = new QWidget(body_);
    auto* el = new QVBoxLayout(error_widget_);
    el->setAlignment(Qt::AlignCenter);
    error_label_ = new QLabel;
    error_label_->setAlignment(Qt::AlignCenter);
    retry_btn_ = new QPushButton("[RETRY]");
    retry_btn_->setCursor(Qt::PointingHandCursor);
    retry_btn_->setFlat(true);
    connect(retry_btn_, &QPushButton::clicked, this, &MarketPanel::refresh);
    el->addWidget(error_label_);
    el->addWidget(retry_btn_);
    error_widget_->setVisible(false);
    bl->addWidget(error_widget_);

    vl->addWidget(body_, 1);
}

void MarketPanel::setup_table_columns() {
    const QStringList& cols = config_.column_order;
    table_->setColumnCount(cols.size());
    table_->setHorizontalHeaderLabels(cols);

    for (int i = 0; i < cols.size(); ++i) {
        const QString& c = cols[i];
        if (c == "SYMBOL" || c == "NAME")
            table_->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Stretch);
        else
            table_->horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    }
}

// ---------------------------------------------------------------------------
// Column configurator
// ---------------------------------------------------------------------------

void MarketPanel::open_cols_dropdown() {
    auto* menu = new QMenu(this);
    menu->setStyleSheet(
        QString("QMenu{background:%1;border:1px solid %2;color:%3;font-size:11px;font-family:monospace;}"
                "QMenu::item{padding:4px 16px;}"
                "QMenu::item:selected{background:%4;}"
                "QMenu::indicator{width:12px;height:12px;}")
            .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_MED(),
                 ui::colors::TEXT_PRIMARY(), ui::colors::BG_HOVER()));

    const QStringList optional = {"CHG", "CHG%", "HIGH", "LOW", "VOL", "BID", "ASK", "OPEN", "NAME"};

    for (const QString& col : optional) {
        auto* act = menu->addAction(col);
        act->setCheckable(true);
        act->setChecked(config_.column_order.contains(col));
        // Enforce max 7 columns (SYMBOL + LAST are locked = 2, so max 5 optional)
        if (!act->isChecked() && config_.column_order.size() >= 7)
            act->setEnabled(false);
        connect(act, &QAction::toggled, this, [this, col](bool checked) {
            if (checked) {
                if (!config_.column_order.contains(col) && config_.column_order.size() < 7)
                    config_.column_order.append(col);
            } else {
                config_.column_order.removeAll(col);
                // Always keep SYMBOL and LAST
                if (!config_.column_order.contains("SYMBOL"))
                    config_.column_order.prepend("SYMBOL");
                if (!config_.column_order.contains("LAST")) {
                    int idx = config_.column_order.indexOf("SYMBOL");
                    config_.column_order.insert(idx + 1, "LAST");
                }
            }
            setup_table_columns();
            if (has_data_) populate(cached_quotes_);
            emit config_changed(config_);
        });
    }

    menu->exec(cols_btn_->mapToGlobal(QPoint(0, cols_btn_->height())));
    menu->deleteLater();
}

void MarketPanel::update_config(const MarketPanelConfig& cfg) {
    config_ = cfg;
    if (config_.column_order.isEmpty())
        config_.column_order = default_market_columns();
    title_label_->setText(cfg.title.toUpper());
    setup_table_columns();
    if (has_data_) populate(cached_quotes_);
}

// ---------------------------------------------------------------------------
// Data population
// ---------------------------------------------------------------------------

void MarketPanel::refresh() {
    fetch_failed_ = false;
    title_label_->setText(config_.title.toUpper() + "  ...");

    QPointer<MarketPanel> self = this;
    services::MarketDataService::instance().fetch_quotes(
        config_.symbols, [self](bool ok, QVector<services::QuoteData> q) {
            if (!self) return;
            if (!ok) {
                self->fetch_failed_ = true;
                if (self->has_data_) {
                    // Show stale data with STALE badge
                    self->title_label_->setText(self->config_.title.toUpper() + "  [STALE]");
                } else {
                    self->show_error("FETCH FAILED");
                }
                emit self->refresh_finished();
                return;
            }
            self->title_label_->setText(self->config_.title.toUpper());
            self->cached_quotes_ = q;
            self->has_data_      = true;
            self->show_data();
            emit self->refresh_finished();
        });
}

void MarketPanel::show_data() {
    error_widget_->setVisible(false);
    table_->setVisible(true);
    // Defer populate until after the splitter has performed layout so body_->height() is valid
    QPointer<MarketPanel> self = this;
    QMetaObject::invokeMethod(this, [self]() {
        if (self) self->populate(self->cached_quotes_);
    }, Qt::QueuedConnection);
}

void MarketPanel::show_error(const QString& msg) {
    table_->setVisible(false);
    error_widget_->setVisible(true);
    error_label_->setText(msg);
}

void MarketPanel::populate(const QVector<services::QuoteData>& quotes) {
    const int body_h  = body_->height();
    // Use actual visible rows if laid out, otherwise show all quotes (resizeEvent will trim)
    const int visible = (body_h > kColHeaderH + kRowH)
                            ? (body_h - kColHeaderH) / kRowH
                            : quotes.size();
    const int count   = qMin(quotes.size(), qMax(visible, 0));
    table_->setRowCount(count);

    for (int row = 0; row < count; ++row) {
        const auto& q  = quotes[row];
        bool pos          = q.change >= 0;
        const QString cc  = pos ? ui::colors::POSITIVE() : ui::colors::NEGATIVE();
        const QString arr = pos ? QString::fromUtf8("\xe2\x96\xb2") : QString::fromUtf8("\xe2\x96\xbc");
        int prec = q.price > 1.0 ? 2 : 4;

        auto mk = [](const QString& s, const QString& c,
                     Qt::Alignment a = Qt::AlignRight | Qt::AlignVCenter) -> QTableWidgetItem* {
            auto* item = new QTableWidgetItem(s);
            item->setForeground(QColor(c));
            item->setTextAlignment(a);
            return item;
        };

        const QStringList& cols = config_.column_order;
        for (int ci = 0; ci < cols.size(); ++ci) {
            const QString& col = cols[ci];
            if      (col == "SYMBOL") table_->setItem(row, ci, mk(q.symbol, ui::colors::TEXT_PRIMARY(), Qt::AlignLeft | Qt::AlignVCenter));
            else if (col == "NAME")   table_->setItem(row, ci, mk(q.name,   ui::colors::TEXT_DIM(),     Qt::AlignLeft | Qt::AlignVCenter));
            else if (col == "LAST")   table_->setItem(row, ci, mk(QString::number(q.price,  'f', prec), ui::colors::AMBER()));
            else if (col == "CHG")    table_->setItem(row, ci, mk(QString("%1 %2").arg(arr).arg(std::abs(q.change),     0, 'f', 2), cc));
            else if (col == "CHG%")   table_->setItem(row, ci, mk(QString("%1%2%").arg(arr).arg(std::abs(q.change_pct), 0, 'f', 2), cc));
            else if (col == "HIGH")   table_->setItem(row, ci, mk(QString::number(q.high, 'f', 2), ui::colors::TEXT_SECONDARY()));
            else if (col == "LOW")    table_->setItem(row, ci, mk(QString::number(q.low,  'f', 2), ui::colors::TEXT_SECONDARY()));
            else if (col == "VOL")    table_->setItem(row, ci, mk("--", ui::colors::TEXT_DIM()));  // TODO: wire volume from API response
            else if (col == "BID")    table_->setItem(row, ci, mk("--", ui::colors::TEXT_DIM()));
            else if (col == "ASK")    table_->setItem(row, ci, mk("--", ui::colors::TEXT_DIM()));
            else if (col == "OPEN")   table_->setItem(row, ci, mk("--", ui::colors::TEXT_DIM()));
        }
    }
}

void MarketPanel::update_visible_rows() {
    if (!table_ || !body_) return;
    const int visible = (body_->height() - kColHeaderH) / kRowH;
    table_->setRowCount(qMax(visible, 0));
}

// ---------------------------------------------------------------------------
// Resize
// ---------------------------------------------------------------------------

void MarketPanel::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (has_data_)
        populate(cached_quotes_);
    else
        update_visible_rows();
}

// ---------------------------------------------------------------------------
// Theme
// ---------------------------------------------------------------------------

void MarketPanel::refresh_theme() {
    const QString ff   = ui::fonts::DATA_FAMILY;
    const int    fdata = ui::fonts::font_px(-2);   // table cell font size (base-2, e.g. 12px at base 14)
    const int    fhdr  = ui::fonts::font_px(-2);   // column header font size
    const int    fbtn  = ui::fonts::font_px(-3);   // button font size (base-3, e.g. 11px)
    const int    ftitle = ui::fonts::font_px(-1);  // panel title font size (base-1, e.g. 13px)

    setStyleSheet(
        QString("background:%1;border:1px solid %2;")
            .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));

    if (header_) {
        header_->setStyleSheet(
            QString("background:%1;border-bottom:1px solid %2;")
                .arg(ui::colors::BG_RAISED(), ui::colors::AMBER()));
    }

    const QString btn_ss =
        QString("QPushButton{background:transparent;color:%1;border:none;"
                "font-size:%2px;font-family:'%3';padding:0 4px;}"
                "QPushButton:hover{color:%4;}")
            .arg(ui::colors::TEXT_DIM()).arg(fbtn).arg(ff).arg(ui::colors::TEXT_PRIMARY());
    if (cols_btn_)   cols_btn_->setStyleSheet(btn_ss);
    if (edit_btn_)   edit_btn_->setStyleSheet(btn_ss);
    if (delete_btn_) delete_btn_->setStyleSheet(btn_ss);

    if (title_label_)
        title_label_->setStyleSheet(
            QString("color:%1;font-size:%2px;font-weight:700;font-family:'%3';background:transparent;")
                .arg(ui::colors::TEXT_PRIMARY()).arg(ftitle).arg(ff));

    if (error_label_)
        error_label_->setStyleSheet(
            QString("color:%1;font-size:%2px;font-family:'%3';background:transparent;")
                .arg(ui::colors::NEGATIVE()).arg(fdata).arg(ff));

    if (retry_btn_)
        retry_btn_->setStyleSheet(
            QString("QPushButton{background:transparent;color:%1;border:none;"
                    "font-size:%2px;font-family:'%3';padding:0 4px;}"
                    "QPushButton:hover{color:%4;}")
                .arg(ui::colors::TEXT_DIM()).arg(fbtn).arg(ff).arg(ui::colors::TEXT_PRIMARY()));

    if (table_) {
        table_->setStyleSheet(
            QString("QTableWidget{background:%1;alternate-background-color:%2;border:none;"
                    "font-size:%6px;font-family:'%7';}"
                    "QTableWidget::item{padding:0 4px;}"
                    "QTableWidget::item:selected{background:transparent;}"
                    "QHeaderView::section{background:%3;color:%4;border:none;"
                    "border-bottom:1px solid %5;font-size:%6px;font-weight:600;"
                    "font-family:'%7';padding:0 4px;}")
                .arg(ui::colors::BG_BASE(), ui::colors::BG_SURFACE(), ui::colors::BG_BASE(),
                     ui::colors::TEXT_DIM(), ui::colors::BORDER_DIM())
                .arg(fhdr).arg(ff));
    }
}

} // namespace fincept::screens
