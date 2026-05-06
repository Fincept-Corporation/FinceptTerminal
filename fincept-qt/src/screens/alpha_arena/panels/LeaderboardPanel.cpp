#include "screens/alpha_arena/panels/LeaderboardPanel.h"

#include "services/alpha_arena/AlphaArenaRepo.h"
#include "ui/theme/Theme.h"

#include <QColor>
#include <QCryptographicHash>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QString>
#include <QStringList>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace fincept::screens::alpha_arena {

using namespace fincept::ui;
using fincept::services::alpha_arena::AlphaArenaRepo;
using fincept::services::alpha_arena::LeaderboardRow;

namespace {

const QStringList kPalette = {
    "#FF8800", "#00E5FF", "#9D4EDD", "#00D66F", "#FF3B3B",
    "#FFD700", "#0088FF", "#E91E63", "#4CAF50", "#FF5722",
};

QString fmt_dollars(double v) {
    return QStringLiteral("$%1").arg(v, 0, 'f', 2);
}
QString fmt_signed_pct(double frac) {
    const QString sign = (frac >= 0.0) ? QStringLiteral("+") : QString();
    return sign + QString::number(frac * 100.0, 'f', 2) + QStringLiteral("%");
}

} // namespace

LeaderboardPanel::LeaderboardPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("aaLeaderboardPanel");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* hdr = new QWidget;
    hdr->setObjectName("aaLeaderboardHeader");
    hdr->setFixedHeight(34);
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(12, 0, 12, 0);
    auto* title = new QLabel("LEADERBOARD");
    title->setObjectName("aaLeaderboardTitle");
    hl->addWidget(title);
    hl->addStretch(1);
    root->addWidget(hdr);

    table_ = new QTableWidget(this);
    table_->setColumnCount(8);
    table_->setHorizontalHeaderLabels(
        {"RANK", "MODEL", "EQUITY", "RETURN", "SHARPE", "MAX DD", "FEES", "LEV"});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setShowGrid(false);
    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        auto* item = table_->item(row, 1);
        if (!item) return;
        emit agent_double_clicked(item->data(Qt::UserRole).toString());
    });
    root->addWidget(table_, 1);
}

void LeaderboardPanel::set_competition(const QString& competition_id) {
    competition_id_ = competition_id;
    table_->setRowCount(0);
}

QString LeaderboardPanel::stable_colour_for(const QString& model_id) const {
    if (model_id.isEmpty()) return colors::TEXT_SECONDARY();
    const auto hash = QCryptographicHash::hash(model_id.toUtf8(), QCryptographicHash::Md5);
    const int idx = static_cast<unsigned char>(hash.at(0)) % kPalette.size();
    return kPalette[idx];
}

void LeaderboardPanel::refresh() {
    if (competition_id_.isEmpty()) { table_->setRowCount(0); return; }
    auto r = AlphaArenaRepo::instance().leaderboard(competition_id_);
    if (r.is_err()) { table_->setRowCount(0); return; }
    const auto rows = r.value();

    table_->setSortingEnabled(false);
    table_->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const LeaderboardRow& lr = rows[i];

        auto* rank_item = new QTableWidgetItem(QString::number(lr.rank));
        rank_item->setTextAlignment(Qt::AlignCenter);
        if (lr.rank == 1)      rank_item->setForeground(QColor("#FFD700"));
        else if (lr.rank == 2) rank_item->setForeground(QColor("#C0C0C0"));
        else if (lr.rank == 3) rank_item->setForeground(QColor("#CD7F32"));
        table_->setItem(i, 0, rank_item);

        auto* model_item = new QTableWidgetItem(lr.display_name);
        model_item->setData(Qt::UserRole, lr.agent_id);
        const QString colour = !lr.color_hex.isEmpty() ? lr.color_hex
                                                        : stable_colour_for(lr.display_name);
        model_item->setForeground(QColor(colour));
        table_->setItem(i, 1, model_item);

        auto* equity = new QTableWidgetItem(fmt_dollars(lr.equity));
        equity->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        equity->setForeground(QColor(colors::CYAN()));
        table_->setItem(i, 2, equity);

        auto* ret = new QTableWidgetItem(fmt_signed_pct(lr.return_pct));
        ret->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        ret->setForeground(QColor(lr.return_pct >= 0 ? colors::POSITIVE() : colors::NEGATIVE()));
        table_->setItem(i, 3, ret);

        auto* sharpe = new QTableWidgetItem(QString::number(lr.sharpe, 'f', 3));
        sharpe->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table_->setItem(i, 4, sharpe);

        auto* dd = new QTableWidgetItem(fmt_signed_pct(-std::fabs(lr.max_drawdown)));
        dd->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        dd->setForeground(QColor(colors::NEGATIVE()));
        table_->setItem(i, 5, dd);

        auto* fees = new QTableWidgetItem(fmt_dollars(lr.fees_paid));
        fees->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table_->setItem(i, 6, fees);

        auto* lev = new QTableWidgetItem(QString::number(lr.avg_leverage, 'f', 2) + QStringLiteral("x"));
        lev->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table_->setItem(i, 7, lev);
    }
    table_->resizeColumnsToContents();
    table_->setSortingEnabled(true);
}

} // namespace fincept::screens::alpha_arena
