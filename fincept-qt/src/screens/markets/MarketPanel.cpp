#include "screens/markets/MarketPanel.h"

#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QPointer>
#include <QResizeEvent>
#include <QVBoxLayout>

namespace fincept::screens {

MarketPanel::MarketPanel(const QString& title, const QStringList& symbols, bool show_name, QWidget* parent)
    : QWidget(parent), title_(title), symbols_(symbols), show_name_(show_name) {
    setMinimumWidth(280);
    setMaximumWidth(520);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(322);

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Header
    auto* header = new QWidget(this);
    header->setFixedHeight(32);
    auto* hhl = new QHBoxLayout(header);
    hhl->setContentsMargins(10, 0, 8, 0);

    title_label_ = new QLabel(title.toUpper());
    title_label_->setStyleSheet(
        QString("color:%1;font-weight:700;letter-spacing:1.2px;background:transparent;").arg(ui::colors::AMBER()));
    hhl->addWidget(title_label_);
    hhl->addStretch();

    status_label_ = new QLabel;
    status_label_->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_DIM()));
    hhl->addWidget(status_label_);
    vl->addWidget(header);

    // Table
    table_ = new QTableWidget;
    table_->setAlternatingRowColors(true);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setShowGrid(false);
    table_->setFocusPolicy(Qt::NoFocus);
    table_->verticalHeader()->setVisible(false);
    table_->verticalHeader()->setDefaultSectionSize(26);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setHighlightSections(false);
    table_->horizontalHeader()->setFixedHeight(18);

    if (show_name_) {
        table_->setColumnCount(6);
        table_->setHorizontalHeaderLabels({"SYM", "NAME", "LAST", "CHG", "CHG%", "VOL"});
        table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        for (int i = 2; i < 6; i++)
            table_->horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    } else {
        table_->setColumnCount(7);
        table_->setHorizontalHeaderLabels({"SYMBOL", "LAST", "CHG", "CHG%", "HIGH", "LOW", "VOL"});
        table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        for (int i = 1; i < 7; i++)
            table_->horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    }
    vl->addWidget(table_, 1);

    loading_overlay_ = new ui::LoadingOverlay(this);

    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { refresh_theme(); });
    refresh_theme();
}

void MarketPanel::refresh_theme() {
    setStyleSheet(
        QString("background:%1;border:1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));

    // Header
    if (title_label_) {
        title_label_->parentWidget()->setStyleSheet(
            QString("background:%1;border-bottom:1px solid %2;border-left:2px solid %3;")
                .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM(), ui::colors::AMBER()));
        title_label_->setStyleSheet(
            QString("color:%1;font-weight:700;letter-spacing:1.2px;background:transparent;").arg(ui::colors::AMBER()));
        status_label_->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_DIM()));
    }

    // Table
    if (table_)
        table_->setStyleSheet(QString("QTableWidget{background:%1;alternate-background-color:%2;border:none;}"
                                      "QTableWidget::item{padding:0 6px;border-bottom:1px solid %3;}"
                                      "QTableWidget::item:selected{background:%4;}"
                                      "QHeaderView::section{background:%5;color:%6;border:none;"
                                      "border-bottom:1px solid %3;font-weight:600;letter-spacing:0.5px;padding:0 6px;}")
                                  .arg(ui::colors::BG_BASE(), ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(),
                                       ui::colors::BG_HOVER(), ui::colors::BG_RAISED(), ui::colors::TEXT_DIM()));
}

void MarketPanel::set_symbols(const QStringList& s) {
    symbols_ = s;
    refresh();
}

void MarketPanel::refresh() {
    status_label_->setText("LOADING");
    status_label_->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::AMBER()));
    loading_overlay_->show_loading("LOADING\xe2\x80\xa6");

    QPointer<MarketPanel> self = this;
    services::MarketDataService::instance().fetch_quotes(symbols_, [self](bool ok, QVector<services::QuoteData> q) {
        if (!self)
            return;
        self->loading_overlay_->hide_loading();
        if (!ok) {
            self->status_label_->setText("FAIL");
            self->status_label_->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::NEGATIVE()));
            emit self->refresh_finished();
            return;
        }
        self->status_label_->setText(QString::number(q.size()));
        self->status_label_->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_DIM()));
        self->populate(q);
        emit self->refresh_finished();
    });
}

void MarketPanel::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (loading_overlay_)
        loading_overlay_->setGeometry(rect());
}

void MarketPanel::populate(const QVector<services::QuoteData>& quotes) {
    table_->setRowCount(0);
    for (const auto& q : quotes) {
        int row = table_->rowCount();
        table_->insertRow(row);
        auto mk = [](const QString& s, const QString& c, Qt::Alignment a = Qt::AlignRight | Qt::AlignVCenter) {
            auto* i = new QTableWidgetItem(s);
            i->setForeground(QColor(c));
            i->setTextAlignment(a);
            return i;
        };
        bool pos = q.change >= 0;
        const QString cc = pos ? ui::colors::POSITIVE() : ui::colors::NEGATIVE();
        const QString arr = pos ? "▲" : "▼";
        int prec = q.price > 100 ? 2 : (q.price > 1 ? 2 : 4);
        int col = 0;
        table_->setItem(row, col++, mk(q.symbol, ui::colors::TEXT_PRIMARY(), Qt::AlignLeft | Qt::AlignVCenter));
        if (show_name_)
            table_->setItem(row, col++, mk(q.name, ui::colors::TEXT_TERTIARY(), Qt::AlignLeft | Qt::AlignVCenter));
        table_->setItem(row, col++, mk(QString::number(q.price, 'f', prec), ui::colors::AMBER()));
        table_->setItem(row, col++, mk(QString("%1 %2").arg(arr).arg(std::abs(q.change), 0, 'f', 2), cc));
        table_->setItem(row, col++, mk(QString("%1%2%").arg(arr).arg(std::abs(q.change_pct), 0, 'f', 2), cc));
        if (!show_name_) {
            table_->setItem(row, col++, mk(QString::number(q.high, 'f', 2), ui::colors::TEXT_SECONDARY()));
            table_->setItem(row, col++, mk(QString::number(q.low, 'f', 2), ui::colors::TEXT_SECONDARY()));
        }
        table_->setItem(row, col++, mk("--", ui::colors::TEXT_DIM()));
        table_->setRowHeight(row, 26);
    }
}

} // namespace fincept::screens
