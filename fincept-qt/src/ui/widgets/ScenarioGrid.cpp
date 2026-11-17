#include "ui/widgets/ScenarioGrid.h"

#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QAbstractTableModel>
#include <QCoreApplication>
#include <QEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

#include <cmath>

namespace fincept::ui {

namespace {

QString sg_style() {
    return QStringLiteral(
               "#scenarioGridTable { background: %1; color: %2; border: 1px solid %3; gridline-color: %3; "
               "  font-size: %6px; selection-background-color: %4; selection-color: %2; }"
               "#scenarioGridTable QHeaderView::section { background: %5; color: %7; border: none; "
               "  border-right: 1px solid %3; border-bottom: 1px solid %3; padding: 3px 6px; font-size: %8px; "
               "  font-weight: 700; }"
               "#scenarioGridBtn { background: transparent; color: %7; border: 1px solid %3; padding: 3px 9px; "
               "  font-size: %8px; font-weight: 700; }"
               "#scenarioGridBtn:hover, #scenarioGridBtn:focus { color: %2; border-color: %9; }"
               "#scenarioGridBtn:disabled { color: %10; }")
        .arg(colors::BG_BASE(), colors::TEXT_PRIMARY(), colors::BORDER_DIM(), colors::BG_HOVER(), colors::BG_RAISED(),
             fonts::px(-4), colors::TEXT_SECONDARY(), fonts::px(-5), colors::BORDER_BRIGHT())
        .arg(colors::TEXT_TERTIARY());
}

} // namespace

/// Rows × (name, shocks…, metrics…). No Q_OBJECT: edits are reported through `on_edit`.
class ScenarioGridModel : public QAbstractTableModel {
  public:
    explicit ScenarioGridModel(QObject* parent) : QAbstractTableModel(parent) {}

    QVector<ScenarioShockColumn> shocks;
    QVector<ScenarioMetricColumn> metrics;
    QVector<ScenarioRow> data_rows;
    bool editable = true;
    std::function<void()> on_edit;

    void reset_all() {
        beginResetModel();
        for (ScenarioRow& r : data_rows) {
            r.shocks.resize(shocks.size());
            const int old = static_cast<int>(r.metrics.size());
            r.metrics.resize(metrics.size());
            for (int i = old; i < r.metrics.size(); ++i)
                r.metrics[i] = std::nan(""); // new metric columns start "not computed"
        }
        endResetModel();
    }
    void insert_row(int at, const ScenarioRow& row) {
        beginInsertRows({}, at, at);
        ScenarioRow r = row;
        r.shocks.resize(shocks.size());
        r.metrics.fill(std::nan(""), metrics.size());
        data_rows.insert(at, r);
        endInsertRows();
    }
    void remove_row(int at) {
        beginRemoveRows({}, at, at);
        data_rows.removeAt(at);
        endRemoveRows();
    }
    void set_metrics(int row, const QVector<double>& v) {
        if (row < 0 || row >= data_rows.size())
            return;
        data_rows[row].metrics = v;
        data_rows[row].metrics.resize(metrics.size());
        emit dataChanged(index(row, 1 + shocks.size()), index(row, columnCount() - 1));
    }

    QString metric_text(int m, double v) const {
        if (!std::isfinite(v))
            return QStringLiteral("—");
        if (metrics[m].format)
            return metrics[m].format(v);
        return formatting::number(v, 2);
    }

    int rowCount(const QModelIndex& p = {}) const override { return p.isValid() ? 0 : data_rows.size(); }
    int columnCount(const QModelIndex& p = {}) const override {
        return p.isValid() ? 0 : 1 + shocks.size() + metrics.size();
    }

    Qt::ItemFlags flags(const QModelIndex& i) const override {
        Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
        if (editable && i.isValid() && i.column() <= shocks.size() && !(data_rows[i.row()].base && i.column() > 0))
            f |= Qt::ItemIsEditable;
        return f;
    }

    QVariant data(const QModelIndex& i, int role) const override {
        if (!i.isValid() || i.row() >= data_rows.size())
            return {};
        const ScenarioRow& r = data_rows[i.row()];
        const int c = i.column();
        const int nshock = shocks.size();
        if (role == Qt::FontRole && r.base) {
            QFont f;
            f.setBold(true);
            return f;
        }
        if (role == Qt::BackgroundRole && r.base)
            return QColor(colors::BG_RAISED());
        if (role == Qt::TextAlignmentRole)
            return QVariant::fromValue(c == 0 ? (Qt::AlignLeft | Qt::AlignVCenter) : (Qt::AlignRight | Qt::AlignVCenter));
        if (role == Qt::ForegroundRole && c > nshock) {
            const int m = c - 1 - nshock;
            const double v = m < r.metrics.size() ? r.metrics[m] : std::nan("");
            if (metrics[m].signed_money && std::isfinite(v) && v != 0)
                return QColor(v > 0 ? colors::POSITIVE() : colors::NEGATIVE());
            return {};
        }
        if (role == Qt::EditRole) {
            if (c == 0)
                return r.name;
            if (c <= nshock)
                return r.shocks.value(c - 1);
            return {};
        }
        if (role != Qt::DisplayRole)
            return {};
        if (c == 0)
            return r.name;
        if (c <= nshock)
            return formatting::number(r.shocks.value(c - 1), shocks[c - 1].decimals);
        const int m = c - 1 - nshock;
        return metric_text(m, m < r.metrics.size() ? r.metrics[m] : std::nan(""));
    }

    bool setData(const QModelIndex& i, const QVariant& v, int role) override {
        if (role != Qt::EditRole || !i.isValid() || i.row() >= data_rows.size())
            return false;
        ScenarioRow& r = data_rows[i.row()];
        if (i.column() == 0) {
            const QString name = v.toString().trimmed();
            if (name.isEmpty())
                return false;
            r.name = name;
        } else if (i.column() <= shocks.size()) {
            bool ok = false;
            const double d = v.toDouble(&ok);
            if (!ok || !std::isfinite(d))
                return false;
            r.shocks[i.column() - 1] = d;
        } else {
            return false;
        }
        emit dataChanged(i, i);
        if (on_edit)
            on_edit();
        return true;
    }

    QVariant headerData(int s, Qt::Orientation o, int role) const override {
        if (o != Qt::Horizontal)
            return {};
        const int nshock = shocks.size();
        if (role == Qt::ToolTipRole) {
            if (s >= 1 && s <= nshock)
                return shocks[s - 1].tooltip;
            if (s > nshock && s - 1 - nshock < metrics.size())
                return metrics[s - 1 - nshock].tooltip;
            return {};
        }
        if (role != Qt::DisplayRole)
            return {};
        if (s == 0)
            return QCoreApplication::translate("fincept::ui::ScenarioGrid", "SCENARIO");
        if (s <= nshock)
            return shocks[s - 1].title;
        return s - 1 - nshock < metrics.size() ? metrics[s - 1 - nshock].title : QString();
    }
};

ScenarioGrid::ScenarioGrid(QWidget* parent) : QWidget(parent) {
    setStyleSheet(sg_style()); // before any child exists
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(6);
    table_ = new QTableView(this);
    table_->setObjectName(QStringLiteral("scenarioGridTable"));
    model_ = new ScenarioGridModel(table_);
    model_->on_edit = [this] { emit scenarios_edited(); };
    table_->setModel(model_);
    table_->verticalHeader()->setVisible(false);
    table_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->setSelectionBehavior(QAbstractItemView::SelectItems);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed |
                            QAbstractItemView::AnyKeyPressed);
    connect(table_, &QTableView::activated, this, [this](const QModelIndex& i) { emit row_activated(i.row()); });
    vl->addWidget(table_, 1);

    buttons_ = new QWidget(this);
    auto* hl = new QHBoxLayout(buttons_);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(6);
    auto mk = [this, hl]() {
        auto* b = new QPushButton(buttons_);
        b->setObjectName(QStringLiteral("scenarioGridBtn"));
        b->setCursor(Qt::PointingHandCursor);
        hl->addWidget(b);
        return b;
    };
    add_ = mk();
    dup_ = mk();
    del_ = mk();
    hl->addStretch(1);
    vl->addWidget(buttons_);
    connect(add_, &QPushButton::clicked, this, [this] {
        ScenarioRow r;
        r.name = tr("Scenario %1").arg(model_->data_rows.size() + 1);
        model_->insert_row(model_->data_rows.size(), r);
        table_->setCurrentIndex(model_->index(model_->data_rows.size() - 1, 0));
        emit scenarios_edited();
    });
    connect(dup_, &QPushButton::clicked, this, [this] {
        const int row = table_->currentIndex().row();
        if (row < 0 || row >= model_->data_rows.size())
            return;
        ScenarioRow r = model_->data_rows[row];
        r.base = false;
        r.name = tr("%1 (copy)").arg(r.name);
        model_->insert_row(row + 1, r);
        emit scenarios_edited();
    });
    connect(del_, &QPushButton::clicked, this, [this] {
        const int row = table_->currentIndex().row();
        if (row < 0 || row >= model_->data_rows.size() || model_->data_rows[row].base)
            return;
        model_->remove_row(row);
        emit scenarios_edited();
    });
    connect(&ThemeManager::instance(), &ThemeManager::appearance_settled, this, [this] { refit(); });
    retranslate();
    refit();
}

ScenarioGrid::~ScenarioGrid() = default;

void ScenarioGrid::set_columns(const QVector<ScenarioShockColumn>& shocks, const QVector<ScenarioMetricColumn>& metrics) {
    model_->shocks = shocks;
    model_->metrics = metrics;
    model_->reset_all();
}

void ScenarioGrid::set_rows(const QVector<ScenarioRow>& rows) {
    model_->data_rows = rows;
    for (ScenarioRow& r : model_->data_rows) {
        r.shocks.resize(model_->shocks.size());
        if (r.metrics.size() != model_->metrics.size())
            r.metrics.fill(std::nan(""), model_->metrics.size());
    }
    model_->reset_all();
}

QVector<ScenarioRow> ScenarioGrid::scenarios() const {
    return model_->data_rows;
}

void ScenarioGrid::set_metric_values(int row, const QVector<double>& values) {
    model_->set_metrics(row, values);
}

void ScenarioGrid::set_editable(bool on) {
    model_->editable = on;
    buttons_->setVisible(on);
}

QStringList ScenarioGrid::headers() const {
    QStringList h;
    for (int c = 0; c < model_->columnCount(); ++c)
        h << model_->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString();
    return h;
}

QVector<QStringList> ScenarioGrid::rows() const {
    QVector<QStringList> out;
    for (int r = 0; r < model_->rowCount(); ++r) {
        QStringList row;
        for (int c = 0; c < model_->columnCount(); ++c)
            row << model_->data(model_->index(r, c), Qt::DisplayRole).toString();
        out.append(row);
    }
    return out;
}

QString ScenarioGrid::to_tsv() const {
    QStringList lines;
    lines << headers().join(QLatin1Char('\t'));
    for (const QStringList& r : rows())
        lines << r.join(QLatin1Char('\t'));
    return lines.join(QLatin1Char('\n'));
}

void ScenarioGrid::retranslate() {
    add_->setText(tr("ADD"));
    add_->setToolTip(tr("Add a scenario row"));
    dup_->setText(tr("DUPLICATE"));
    dup_->setToolTip(tr("Copy the selected scenario"));
    del_->setText(tr("DELETE"));
    del_->setToolTip(tr("Remove the selected scenario (the base row stays)"));
    if (model_->rowCount() > 0)
        emit model_->headerDataChanged(Qt::Horizontal, 0, model_->columnCount() - 1);
}

void ScenarioGrid::refit() {
    table_->verticalHeader()->setDefaultSectionSize(QFontMetrics(table_->font()).height() + 8);
}

void ScenarioGrid::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(e);
}

} // namespace fincept::ui
