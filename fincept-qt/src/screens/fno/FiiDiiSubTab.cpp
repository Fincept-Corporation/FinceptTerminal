#include "screens/fno/FiiDiiSubTab.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/fno/FiiDiiChart.h"
#include "ui/theme/Theme.h"

#include <QDate>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHideEvent>
#include <QLabel>
#include <QLocale>
#include <QPointer>
#include <QPushButton>
#include <QShowEvent>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace fincept::screens::fno {

using fincept::services::options::FiiDiiDay;
using namespace fincept::ui;

namespace {

const QString kTopic = QStringLiteral("fno:fii_dii:daily");

QString fmt_currency(double v) {
    if (v == 0)
        return QStringLiteral("—");
    QString s = QLocale(QLocale::English).toString(v, 'f', 2);
    return (v >= 0 ? "+" : "") + s;
}

QColor color_for(double v) {
    if (v > 0)
        return QColor(colors::POSITIVE());
    if (v < 0)
        return QColor(colors::NEGATIVE());
    return QColor(colors::TEXT_SECONDARY());
}

}  // namespace

FiiDiiSubTab::FiiDiiSubTab(QWidget* parent) : QWidget(parent) {
    setObjectName("fnoFiiDiiTab");
    setStyleSheet(QString("#fnoFiiDiiTab { background:%1; }"
                          "#fnoFiiHeader { background:%2; border-bottom:1px solid %3; }"
                          "#fnoFiiStatus { color:%4; font-size:10px; background:transparent; }"
                          "#fnoFiiRefresh { background:%5; color:%2; border:none; padding:5px 12px; "
                          "                  font-size:9px; font-weight:700; letter-spacing:0.5px; }"
                          "#fnoFiiRefresh:hover { background:%6; color:%2; }"
                          "QTableWidget { background:%1; color:%6; border:1px solid %3; "
                          "                gridline-color:%3; selection-background-color:%7; }"
                          "QHeaderView::section { background:%2; color:%4; border:none; "
                          "                       border-bottom:1px solid %3; padding:5px 8px; "
                          "                       font-size:9px; font-weight:700; letter-spacing:0.4px; }")
                      .arg(colors::BG_BASE(),         // %1
                           colors::BG_RAISED(),       // %2
                           colors::BORDER_DIM(),      // %3
                           colors::TEXT_SECONDARY(),  // %4
                           colors::AMBER(),           // %5
                           colors::TEXT_PRIMARY(),    // %6
                           colors::BG_HOVER()));      // %7

    setup_ui();
    connect(refresh_btn_, &QPushButton::clicked, this, &FiiDiiSubTab::on_refresh_clicked);
}

FiiDiiSubTab::~FiiDiiSubTab() {
    fincept::datahub::DataHub::instance().unsubscribe(this);
}

void FiiDiiSubTab::setup_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header strip ──────────────────────────────────────────────────────
    auto* header = new QWidget(this);
    header->setObjectName("fnoFiiHeader");
    auto* hlay = new QHBoxLayout(header);
    hlay->setContentsMargins(12, 8, 12, 8);
    hlay->setSpacing(8);
    lbl_status_ = new QLabel(QStringLiteral("FII / DII flows — fetching…"), header);
    lbl_status_->setObjectName("fnoFiiStatus");
    refresh_btn_ = new QPushButton(QStringLiteral("REFRESH"), header);
    refresh_btn_->setObjectName("fnoFiiRefresh");
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    hlay->addWidget(lbl_status_);
    hlay->addStretch(1);
    hlay->addWidget(refresh_btn_);
    root->addWidget(header);

    // ── Body: table | chart split ─────────────────────────────────────────
    auto* split = new QSplitter(Qt::Horizontal, this);
    split->setHandleWidth(1);
    split->setChildrenCollapsible(false);

    table_ = new QTableWidget(split);
    table_->setColumnCount(7);
    table_->setHorizontalHeaderLabels({"Date", "FII Buy", "FII Sell", "FII Net", "DII Buy", "DII Sell", "DII Net"});
    table_->verticalHeader()->setVisible(false);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setShowGrid(false);
    table_->setFocusPolicy(Qt::NoFocus);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_->verticalHeader()->setDefaultSectionSize(22);

    chart_ = new FiiDiiChart(split);

    split->addWidget(table_);
    split->addWidget(chart_);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 4);
    root->addWidget(split, 1);
}

QVariantMap FiiDiiSubTab::save_state() const { return {}; }
void FiiDiiSubTab::restore_state(const QVariantMap& /*state*/) {}

void FiiDiiSubTab::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (subscribed_)
        return;
    auto& hub = fincept::datahub::DataHub::instance();
    QPointer<FiiDiiSubTab> self = this;
    hub.subscribe(this, kTopic, [self](const QVariant& v) {
        if (!self) return;
        self->on_data_arrived(v);
    });
    hub.subscribe_errors(this, kTopic, [self](const QString& err) {
        if (!self) return;
        self->lbl_status_->setText(QStringLiteral("FII / DII flows — error: ") + err);
    });
    hub.request(kTopic, /*force*/ false);
    subscribed_ = true;
}

void FiiDiiSubTab::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    if (!subscribed_)
        return;
    fincept::datahub::DataHub::instance().unsubscribe(this, kTopic);
    fincept::datahub::DataHub::instance().unsubscribe_errors(this, kTopic);
    subscribed_ = false;
}

void FiiDiiSubTab::on_refresh_clicked() {
    fincept::datahub::DataHub::instance().request(kTopic, /*force*/ true);
    lbl_status_->setText(QStringLiteral("FII / DII flows — refreshing…"));
}

void FiiDiiSubTab::on_data_arrived(const QVariant& v) {
    if (!v.canConvert<QVector<FiiDiiDay>>())
        return;
    apply_data(v.value<QVector<FiiDiiDay>>());
}

void FiiDiiSubTab::apply_data(const QVector<FiiDiiDay>& rows) {
    chart_->set_data(rows);

    table_->setRowCount(rows.size());
    auto put = [&](int r, int c, const QString& txt, const QColor& fg = QColor()) {
        auto* item = new QTableWidgetItem(txt);
        item->setTextAlignment(c == 0 ? Qt::AlignCenter : (Qt::AlignRight | Qt::AlignVCenter));
        if (fg.isValid())
            item->setForeground(fg);
        table_->setItem(r, c, item);
    };
    // Render newest at top of the table — flip the ascending vector here.
    for (int i = 0; i < rows.size(); ++i) {
        const FiiDiiDay& d = rows.at(rows.size() - 1 - i);
        put(i, 0, d.date_iso);
        put(i, 1, fmt_currency(d.fii_buy));
        put(i, 2, fmt_currency(d.fii_sell));
        put(i, 3, fmt_currency(d.fii_net), color_for(d.fii_net));
        put(i, 4, fmt_currency(d.dii_buy));
        put(i, 5, fmt_currency(d.dii_sell));
        put(i, 6, fmt_currency(d.dii_net), color_for(d.dii_net));
    }

    if (rows.isEmpty()) {
        lbl_status_->setText(
            QStringLiteral("FII / DII flows — no data yet. Try refreshing after 6 PM IST."));
    } else {
        lbl_status_->setText(QStringLiteral("FII / DII flows — last update: %1   ·   %2 days cached")
                                  .arg(rows.last().date_iso)
                                  .arg(rows.size()));
    }
}

} // namespace fincept::screens::fno
