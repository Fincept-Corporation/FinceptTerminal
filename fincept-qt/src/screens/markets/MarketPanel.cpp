#include "screens/markets/MarketPanel.h"

#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QEnterEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPointer>
#include <QResizeEvent>
#include <QVBoxLayout>

namespace fincept::screens {

MarketPanel::MarketPanel(const MarketPanelConfig& config, QWidget* parent)
    : QWidget(parent), config_(config) {
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
    hhl->setContentsMargins(10, 0, 6, 0);
    hhl->setSpacing(4);

    title_label_ = new QLabel(config_.title.toUpper());
    title_label_->setStyleSheet(
        QString("color:%1;font-weight:700;letter-spacing:1.2px;background:transparent;").arg(ui::colors::AMBER()));
    hhl->addWidget(title_label_);
    hhl->addStretch();

    status_label_ = new QLabel;
    status_label_->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_DIM()));
    hhl->addWidget(status_label_);

    // Edit / delete buttons — hidden until hover
    edit_btn_ = new QPushButton("✎");
    edit_btn_->setFixedSize(20, 20);
    edit_btn_->setCursor(Qt::PointingHandCursor);
    edit_btn_->setToolTip("Edit panel");
    edit_btn_->setVisible(false);
    edit_btn_->setStyleSheet(
        QString("QPushButton{background:transparent;color:%1;border:none;font-size:13px;}"
                "QPushButton:hover{color:%2;}")
            .arg(ui::colors::TEXT_TERTIARY(), ui::colors::AMBER()));
    connect(edit_btn_, &QPushButton::clicked, this, [this]() {
        emit edit_requested(config_.id);
    });
    hhl->addWidget(edit_btn_);

    delete_btn_ = new QPushButton("✕");
    delete_btn_->setFixedSize(20, 20);
    delete_btn_->setCursor(Qt::PointingHandCursor);
    delete_btn_->setToolTip("Remove panel");
    delete_btn_->setVisible(false);
    delete_btn_->setStyleSheet(
        QString("QPushButton{background:transparent;color:%1;border:none;font-size:11px;font-weight:bold;}"
                "QPushButton:hover{color:%2;}")
            .arg(ui::colors::TEXT_TERTIARY(), ui::colors::NEGATIVE()));
    connect(delete_btn_, &QPushButton::clicked, this, [this]() {
        emit delete_requested(config_.id);
    });
    hhl->addWidget(delete_btn_);

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

    if (config_.show_name) {
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

    if (title_label_) {
        title_label_->parentWidget()->setStyleSheet(
            QString("background:%1;border-bottom:1px solid %2;border-left:2px solid %3;")
                .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM(), ui::colors::AMBER()));
        title_label_->setStyleSheet(
            QString("color:%1;font-weight:700;letter-spacing:1.2px;background:transparent;").arg(ui::colors::AMBER()));
        status_label_->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_DIM()));
    }

    if (table_)
        table_->setStyleSheet(QString("QTableWidget{background:%1;alternate-background-color:%2;border:none;}"
                                      "QTableWidget::item{padding:0 6px;border-bottom:1px solid %3;}"
                                      "QTableWidget::item:selected{background:%4;}"
                                      "QHeaderView::section{background:%5;color:%6;border:none;"
                                      "border-bottom:1px solid %3;font-weight:600;letter-spacing:0.5px;padding:0 6px;}")
                                  .arg(ui::colors::BG_BASE(), ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(),
                                       ui::colors::BG_HOVER(), ui::colors::BG_RAISED(), ui::colors::TEXT_DIM()));
}

void MarketPanel::enterEvent(QEnterEvent* event) {
    QWidget::enterEvent(event);
    if (edit_btn_)   edit_btn_->setVisible(true);
    if (delete_btn_) delete_btn_->setVisible(true);
}

void MarketPanel::leaveEvent(QEvent* event) {
    QWidget::leaveEvent(event);
    if (edit_btn_)   edit_btn_->setVisible(false);
    if (delete_btn_) delete_btn_->setVisible(false);
}

void MarketPanel::refresh() {
    status_label_->setText("LOADING");
    status_label_->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::AMBER()));
    loading_overlay_->show_loading("LOADING\xe2\x80\xa6");

    QPointer<MarketPanel> self = this;
    services::MarketDataService::instance().fetch_quotes(config_.symbols, [self](bool ok, QVector<services::QuoteData> q) {
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
        const QString cc  = pos ? ui::colors::POSITIVE() : ui::colors::NEGATIVE();
        const QString arr = pos ? "▲" : "▼";
        int prec = q.price > 1 ? 2 : 4;
        int col  = 0;
        table_->setItem(row, col++, mk(q.symbol, ui::colors::TEXT_PRIMARY(), Qt::AlignLeft | Qt::AlignVCenter));
        if (config_.show_name)
            table_->setItem(row, col++, mk(q.name, ui::colors::TEXT_TERTIARY(), Qt::AlignLeft | Qt::AlignVCenter));
        table_->setItem(row, col++, mk(QString::number(q.price, 'f', prec), ui::colors::AMBER()));
        table_->setItem(row, col++, mk(QString("%1 %2").arg(arr).arg(std::abs(q.change), 0, 'f', 2), cc));
        table_->setItem(row, col++, mk(QString("%1%2%").arg(arr).arg(std::abs(q.change_pct), 0, 'f', 2), cc));
        if (!config_.show_name) {
            table_->setItem(row, col++, mk(QString::number(q.high, 'f', 2), ui::colors::TEXT_SECONDARY()));
            table_->setItem(row, col++, mk(QString::number(q.low, 'f', 2),  ui::colors::TEXT_SECONDARY()));
        }
        table_->setItem(row, col++, mk("--", ui::colors::TEXT_DIM()));
        table_->setRowHeight(row, 26);
    }
}

} // namespace fincept::screens
