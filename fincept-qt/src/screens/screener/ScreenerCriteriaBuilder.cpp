#include "screens/screener/ScreenerCriteriaBuilder.h"

#include <QComboBox>
#include <QEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStandardItemModel>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

namespace {

const QString kDefaultRegion = QStringLiteral("us");
const QString kDefaultExchanges = QStringLiteral("NMS,NGM,NCM,NYQ,ASE"); // US majors, OTC excluded
const QString kDefaultField = QStringLiteral("market_cap");
const QString kBetweenOp = QStringLiteral("btwn");

/// QComboBox has no native groups: add a bold, non-selectable category header item.
void add_combo_header(QComboBox* combo, const QString& text) {
    combo->addItem(text);
    auto* model = qobject_cast<QStandardItemModel*>(combo->model());
    if (!model)
        return;
    QStandardItem* item = model->item(combo->count() - 1);
    item->setFlags(item->flags() & ~(Qt::ItemIsSelectable | Qt::ItemIsEnabled));
    QFont f = item->font();
    f.setBold(true);
    item->setFont(f);
}

void select_data(QComboBox* combo, const QVariant& data) {
    const int i = combo->findData(data);
    combo->setCurrentIndex(i >= 0 ? i : 0);
}

/// First element of a criteria list ("us" or ["us"]) — the form edits one value per picker.
QString first_value(const QJsonValue& v) {
    if (v.isArray())
        return v.toArray().isEmpty() ? QString() : v.toArray().first().toString();
    return v.toString();
}

QString sorted_codes(const QStringList& codes) {
    QStringList c = codes;
    std::sort(c.begin(), c.end());
    return c.join(QLatin1Char(','));
}

void tune_combo(QComboBox* combo) {
    // Long catalogue entries must not widen the side panel.
    combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    combo->setMinimumContentsLength(8);
}

} // namespace

ScreenerCriteriaBuilder::ScreenerCriteriaBuilder(QWidget* parent) : QWidget(parent) {
    setObjectName("screenerBuilder");
    build_ui();
    retranslate();
}

void ScreenerCriteriaBuilder::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 8, 12, 12);
    root->setSpacing(8);

    universe_lbl_ = new QLabel(this);
    universe_lbl_->setObjectName("screenerSectionLabel");
    root->addWidget(universe_lbl_);

    universe_form_ = new QFormLayout;
    universe_form_->setContentsMargins(0, 0, 0, 0);
    universe_form_->setHorizontalSpacing(8);
    universe_form_->setVerticalSpacing(6);
    universe_form_->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    region_ = new QComboBox(this);
    exchange_ = new QComboBox(this);
    sector_ = new QComboBox(this);
    industry_ = new QComboBox(this);
    peer_ = new QComboBox(this);
    for (QComboBox* c : {region_, exchange_, sector_, industry_, peer_}) {
        tune_combo(c);
        universe_form_->addRow(QString(), c);
    }
    root->addLayout(universe_form_);

    criteria_lbl_ = new QLabel(this);
    criteria_lbl_->setObjectName("screenerSectionLabel");
    root->addWidget(criteria_lbl_);
    rows_layout_ = new QVBoxLayout;
    rows_layout_->setContentsMargins(0, 0, 0, 0);
    rows_layout_->setSpacing(6);
    root->addLayout(rows_layout_);

    add_btn_ = new QPushButton(this);
    add_btn_->setObjectName("screenerGhostBtn");
    add_btn_->setCursor(Qt::PointingHandCursor);
    connect(add_btn_, &QPushButton::clicked, this, [this]() {
        add_criterion(kDefaultField, QStringLiteral("gte"), QString(), QString());
        if (!rows_.isEmpty())
            rows_.last().field->setFocus();
        emit edited();
    });
    root->addWidget(add_btn_);

    hint_lbl_ = new QLabel(this);
    hint_lbl_->setObjectName("screenerHint");
    hint_lbl_->setWordWrap(true);
    root->addWidget(hint_lbl_);

    output_lbl_ = new QLabel(this);
    output_lbl_->setObjectName("screenerSectionLabel");
    root->addWidget(output_lbl_);
    output_form_ = new QFormLayout;
    output_form_->setContentsMargins(0, 0, 0, 0);
    output_form_->setHorizontalSpacing(8);
    output_form_->setVerticalSpacing(6);
    output_form_->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    sort_field_ = new QComboBox(this);
    sort_dir_ = new QComboBox(this);
    for (QComboBox* c : {sort_field_, sort_dir_}) {
        tune_combo(c);
        output_form_->addRow(QString(), c);
    }
    sort_dir_->addItem(QString(), QStringLiteral("desc"));
    sort_dir_->addItem(QString(), QStringLiteral("asc"));
    root->addLayout(output_form_);
    root->addStretch(1);

    auto notify = [this]() {
        if (!loading_)
            emit edited();
    };
    connect(region_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, notify](int) {
        populate_exchanges();
        notify();
    });
    connect(sector_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, notify](int) {
        populate_industries();
        notify();
    });
    for (QComboBox* c : {exchange_, industry_, peer_, sort_field_, sort_dir_})
        connect(c, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [notify](int) { notify(); });
}

void ScreenerCriteriaBuilder::retranslate() {
    universe_lbl_->setText(tr("UNIVERSE"));
    criteria_lbl_->setText(tr("CRITERIA"));
    output_lbl_->setText(tr("SORT"));
    add_btn_->setText(tr("+ ADD CRITERION"));
    hint_lbl_->setText(tr("Values accept K / M / B / T (10B = 10 billion). Percent fields take percent "
                          "points (25 = 25%). Price and market cap compare in the listing's quote currency."));

    const auto set_form_label = [](QFormLayout* form, QWidget* field, const QString& text) {
        if (auto* lbl = qobject_cast<QLabel*>(form->labelForField(field)))
            lbl->setText(text);
    };
    set_form_label(universe_form_, region_, tr("Region"));
    set_form_label(universe_form_, exchange_, tr("Exchange"));
    set_form_label(universe_form_, sector_, tr("Sector"));
    set_form_label(universe_form_, industry_, tr("Industry"));
    set_form_label(universe_form_, peer_, tr("Peer group"));
    set_form_label(output_form_, sort_field_, tr("Sort by"));
    set_form_label(output_form_, sort_dir_, tr("Order"));

    industry_->setToolTip(tr("Yahoo cannot filter some industries (banks, insurers, REITs, utilities, software, "
                             "drug makers, beverages) - use Peer group for those."));
    peer_->setToolTip(tr("Industry peer group (e.g. Banks, Insurance, Utilities, Software & Services)."));
    sort_field_->setToolTip(tr("Yahoo sorts the whole matching universe by this field before paging."));

    if (sort_dir_->count() >= 2) {
        sort_dir_->setItemText(0, tr("High to low"));
        sort_dir_->setItemText(1, tr("Low to high"));
    }
    if (region_->count() > 0)
        region_->setItemText(0, tr("Any region"));
    if (exchange_->count() > 0)
        exchange_->setItemText(0, tr("Any exchange"));
    if (sector_->count() > 0)
        sector_->setItemText(0, tr("Any sector"));
    if (industry_->count() > 0)
        industry_->setItemText(0, tr("Any industry"));
    if (peer_->count() > 0)
        peer_->setItemText(0, tr("Any peer group"));
    for (const auto& row : std::as_const(rows_)) {
        const int b = row.op->findData(kBetweenOp);
        if (b >= 0)
            row.op->setItemText(b, tr("between"));
        row.remove->setToolTip(tr("Remove this criterion"));
        update_row(row);
    }
}

void ScreenerCriteriaBuilder::changeEvent(QEvent* e) {
    QWidget::changeEvent(e);
    if (e && e->type() == QEvent::LanguageChange)
        retranslate();
}

// ── Catalogue ─────────────────────────────────────────────────────────────────

void ScreenerCriteriaBuilder::set_catalog(const QJsonObject& catalog) {
    catalog_ = catalog;
    fields_by_id_.clear();
    for (const auto& v : catalog.value("fields").toArray()) {
        const QJsonObject f = v.toObject();
        fields_by_id_.insert(f.value("id").toString(), f);
    }

    loading_ = true;
    {
        const QSignalBlocker b1(region_);
        const QSignalBlocker b2(sector_);
        const QSignalBlocker b3(peer_);
        const QSignalBlocker b4(sort_field_);
        region_->clear();
        region_->addItem(tr("Any region"), QString());
        for (const auto& v : catalog.value("regions").toArray()) {
            const QJsonObject r = v.toObject();
            region_->addItem(r.value("name").toString(), r.value("code").toString());
        }
        sector_->clear();
        sector_->addItem(tr("Any sector"), QString());
        for (const auto& v : catalog.value("sectors").toArray())
            sector_->addItem(v.toString(), v.toString());
        peer_->clear();
        peer_->addItem(tr("Any peer group"), QString());
        for (const auto& v : catalog.value("peer_groups").toArray())
            peer_->addItem(v.toString(), v.toString());
        populate_field_combo(sort_field_);
    }
    loading_ = false;
    reset_to_default();
}

QString ScreenerCriteriaBuilder::field_label(const QString& id) const {
    const auto it = fields_by_id_.constFind(id);
    return it == fields_by_id_.constEnd() ? id : it->value("label").toString(id);
}

void ScreenerCriteriaBuilder::populate_field_combo(QComboBox* combo) const {
    const QSignalBlocker block(combo);
    combo->clear();
    const QJsonArray fields = catalog_.value("fields").toArray();
    for (const auto& cat : catalog_.value("categories").toArray()) {
        const QString category = cat.toString();
        add_combo_header(combo, category.toUpper());
        for (const auto& v : fields) {
            const QJsonObject f = v.toObject();
            if (f.value("category").toString() != category)
                continue;
            combo->addItem(QStringLiteral("  ") + f.value("label").toString(), f.value("id").toString());
            combo->setItemData(combo->count() - 1, f.value("description").toString(), Qt::ToolTipRole);
        }
    }
}

void ScreenerCriteriaBuilder::populate_operator_combo(QComboBox* combo) const {
    combo->addItem(QStringLiteral(">"), QStringLiteral("gt"));
    combo->addItem(QString(QChar(0x2265)), QStringLiteral("gte")); // ≥
    combo->addItem(QStringLiteral("<"), QStringLiteral("lt"));
    combo->addItem(QString(QChar(0x2264)), QStringLiteral("lte")); // ≤
    combo->addItem(tr("between"), kBetweenOp);
}

void ScreenerCriteriaBuilder::populate_exchanges() {
    const QString keep = exchange_->currentData().toString();
    const QString region = region_->currentData().toString();
    const QSignalBlocker block(exchange_);
    exchange_->clear();
    exchange_->addItem(tr("Any exchange"), QString());
    const QJsonArray groups = catalog_.value("exchanges").toObject().value(region).toArray();
    for (const auto& v : groups) {
        const QJsonObject g = v.toObject();
        QStringList codes;
        for (const auto& c : g.value("codes").toArray())
            codes.append(c.toString());
        exchange_->addItem(g.value("label").toString(), codes.join(QLatin1Char(',')));
        exchange_->setItemData(exchange_->count() - 1, codes.join(QStringLiteral(", ")), Qt::ToolTipRole);
    }
    exchange_->setEnabled(!region.isEmpty() && !groups.isEmpty());
    select_data(exchange_, keep);
}

void ScreenerCriteriaBuilder::populate_industries() {
    const QString keep = industry_->currentData().toString();
    const QString sector = sector_->currentData().toString();
    const QJsonObject by_sector = catalog_.value("industries").toObject();
    QStringList names;
    if (sector.isEmpty()) {
        for (auto it = by_sector.constBegin(); it != by_sector.constEnd(); ++it)
            for (const auto& v : it.value().toArray())
                names.append(v.toString());
        names.sort(Qt::CaseInsensitive);
    } else {
        for (const auto& v : by_sector.value(sector).toArray())
            names.append(v.toString());
    }
    const QSignalBlocker block(industry_);
    industry_->clear();
    industry_->addItem(tr("Any industry"), QString());
    for (const auto& n : std::as_const(names))
        industry_->addItem(n, n);
    industry_->setEnabled(!names.isEmpty());
    select_data(industry_, keep);
}

// ── Criteria rows ─────────────────────────────────────────────────────────────

void ScreenerCriteriaBuilder::add_criterion(const QString& field_id, const QString& op, const QString& lo,
                                            const QString& hi) {
    CriterionRow row;
    row.box = new QWidget(this);
    row.box->setObjectName("screenerCriterionRow");
    auto* v = new QVBoxLayout(row.box);
    v->setContentsMargins(6, 6, 6, 6);
    v->setSpacing(4);

    auto* top = new QHBoxLayout;
    top->setSpacing(4);
    row.field = new QComboBox(row.box);
    tune_combo(row.field);
    populate_field_combo(row.field);
    row.remove = new QToolButton(row.box);
    row.remove->setObjectName("screenerRemoveBtn");
    row.remove->setText(QString(QChar(0x2715))); // ✕
    row.remove->setToolTip(tr("Remove this criterion"));
    row.remove->setCursor(Qt::PointingHandCursor);
    top->addWidget(row.field, 1);
    top->addWidget(row.remove);

    auto* bottom = new QHBoxLayout;
    bottom->setSpacing(4);
    row.op = new QComboBox(row.box);
    populate_operator_combo(row.op);
    row.op->setFixedWidth(84);
    row.lo = new QLineEdit(row.box);
    row.hi = new QLineEdit(row.box);
    row.unit = new QLabel(row.box);
    row.unit->setObjectName("screenerUnit");
    bottom->addWidget(row.op);
    bottom->addWidget(row.lo, 1);
    bottom->addWidget(row.hi, 1);
    bottom->addWidget(row.unit);

    v->addLayout(top);
    v->addLayout(bottom);

    select_data(row.field, field_id.isEmpty() ? kDefaultField : field_id);
    if (row.field->currentData().toString().isEmpty()) // landed on a category header
        select_data(row.field, kDefaultField);
    select_data(row.op, op);
    row.lo->setText(lo);
    row.hi->setText(hi);

    QWidget* box = row.box;
    auto refresh = [this, box]() {
        for (const auto& r : std::as_const(rows_)) {
            if (r.box == box) {
                update_row(r);
                break;
            }
        }
        if (!loading_)
            emit edited();
    };
    connect(row.field, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [refresh](int) { refresh(); });
    connect(row.op, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [refresh](int) { refresh(); });
    for (QLineEdit* edit : {row.lo, row.hi}) {
        connect(edit, &QLineEdit::textEdited, this, [this, edit](const QString&) {
            set_invalid(edit, false);
            emit edited();
        });
        connect(edit, &QLineEdit::returnPressed, this, &ScreenerCriteriaBuilder::run_requested);
    }
    connect(row.remove, &QToolButton::clicked, this, [this, box]() { remove_criterion(box); });

    rows_layout_->addWidget(row.box);
    rows_.append(row);
    update_row(row);
}

void ScreenerCriteriaBuilder::remove_criterion(QWidget* box) {
    for (int i = 0; i < rows_.size(); ++i) {
        if (rows_[i].box == box) {
            rows_.removeAt(i);
            box->hide();
            box->deleteLater(); // we may be inside the remove button's clicked() handler
            emit edited();
            return;
        }
    }
}

void ScreenerCriteriaBuilder::clear_criteria() {
    for (const auto& r : std::as_const(rows_)) {
        r.box->hide();
        r.box->deleteLater();
    }
    rows_.clear();
}

void ScreenerCriteriaBuilder::update_row(const CriterionRow& row) {
    const QJsonObject f = fields_by_id_.value(row.field->currentData().toString());
    const QString unit = f.value("unit").toString();
    const bool between = row.op->currentData().toString() == kBetweenOp;
    row.unit->setText(f.value("unit_label").toString());
    row.field->setToolTip(f.value("description").toString());
    row.hi->setVisible(between);
    const bool large = unit == QLatin1String("mcap") || unit == QLatin1String("money") ||
                       unit == QLatin1String("shares");
    if (between) {
        row.lo->setPlaceholderText(tr("min"));
        row.hi->setPlaceholderText(tr("max"));
    } else {
        row.lo->setPlaceholderText(large ? tr("e.g. 10B") : tr("value"));
    }
}

void ScreenerCriteriaBuilder::set_invalid(QWidget* w, bool invalid) {
    if (w->property("invalid").toBool() == invalid)
        return;
    w->setProperty("invalid", invalid);
    // Dynamic-property selectors only re-evaluate on a re-polish.
    w->style()->unpolish(w);
    w->style()->polish(w);
}

bool ScreenerCriteriaBuilder::parse_value(const QString& text, double* out) {
    QString t = text.trimmed();
    t.remove(QLatin1Char(','));
    t.remove(QLatin1Char(' '));
    t.remove(QLatin1Char('%'));
    t.remove(QLatin1Char('$'));
    if (t.isEmpty())
        return false;
    double mult = 1.0;
    const QChar suffix = t.back().toUpper();
    if (suffix == QLatin1Char('K') || suffix == QLatin1Char('M') || suffix == QLatin1Char('B') ||
        suffix == QLatin1Char('T')) {
        mult = suffix == QLatin1Char('K')   ? 1e3
               : suffix == QLatin1Char('M') ? 1e6
               : suffix == QLatin1Char('B') ? 1e9
                                            : 1e12;
        t.chop(1);
    }
    bool ok = false;
    const double v = QLocale::c().toDouble(t, &ok);
    if (!ok || !std::isfinite(v))
        return false;
    *out = v * mult;
    return true;
}

QString ScreenerCriteriaBuilder::format_value(double v) {
    const double a = std::fabs(v);
    if (a >= 1e12)
        return QString::number(v / 1e12, 'g', 12) + QLatin1Char('T');
    if (a >= 1e9)
        return QString::number(v / 1e9, 'g', 12) + QLatin1Char('B');
    if (a >= 1e6)
        return QString::number(v / 1e6, 'g', 12) + QLatin1Char('M');
    return QString::number(v, 'g', 12);
}

// ── Query <-> form ────────────────────────────────────────────────────────────

bool ScreenerCriteriaBuilder::build_query(services::ScreenerQuery* out, QString* error, bool mark_invalid) {
    QJsonObject crit;
    const auto put_single = [&crit](const char* key, const QString& value) {
        if (!value.isEmpty())
            crit[QLatin1String(key)] = QJsonArray{value};
    };
    put_single("region", region_->currentData().toString());
    put_single("sector", sector_->currentData().toString());
    put_single("industry", industry_->currentData().toString());
    put_single("peer_group", peer_->currentData().toString());
    const QString exchanges = exchange_->isEnabled() ? exchange_->currentData().toString() : QString();
    if (!exchanges.isEmpty()) {
        QJsonArray arr;
        for (const auto& code : exchanges.split(QLatin1Char(','), Qt::SkipEmptyParts))
            arr.append(code);
        crit["exchange"] = arr;
    }

    QJsonArray filters;
    for (int i = 0; i < rows_.size(); ++i) {
        const CriterionRow& row = rows_[i];
        const QString fid = row.field->currentData().toString();
        if (fid.isEmpty())
            continue;
        const QString label = field_label(fid);
        const bool between = row.op->currentData().toString() == kBetweenOp;
        double lo = 0.0;
        double hi = 0.0;
        const bool lo_ok = parse_value(row.lo->text(), &lo);
        const bool hi_ok = !between || parse_value(row.hi->text(), &hi);
        if (mark_invalid) {
            set_invalid(row.lo, !lo_ok);
            set_invalid(row.hi, !hi_ok);
        }
        if (!lo_ok || !hi_ok) {
            if (mark_invalid)
                (lo_ok ? row.hi : row.lo)->setFocus();
            *error = tr("Criterion %1 (%2): enter a number such as 15, 2.5, 500M or 10B.").arg(i + 1).arg(label);
            return false;
        }
        QJsonObject f;
        f["field"] = fid;
        f["op"] = row.op->currentData().toString();
        if (between)
            f["value"] = QJsonArray{std::min(lo, hi), std::max(lo, hi)};
        else
            f["value"] = lo;
        filters.append(f);
    }
    if (!filters.isEmpty())
        crit["filters"] = filters;
    if (crit.isEmpty()) {
        *error = tr("Add at least one criterion: a region, a sector or a numeric filter.");
        return false;
    }

    out->predefined_id.clear();
    out->criteria = crit;
    out->sort_field = sort_field_->currentData().toString();
    out->sort_asc = sort_dir_->currentData().toString() == QLatin1String("asc");
    return true;
}

void ScreenerCriteriaBuilder::load_query(const services::ScreenerQuery& q) {
    if (!has_catalog() || q.is_predefined())
        return;
    loading_ = true;
    const QJsonObject& c = q.criteria;
    select_data(region_, first_value(c.value("region")));
    populate_exchanges(); // region_'s signal is not guaranteed to fire (same index)
    QStringList codes;
    if (c.value("exchange").isArray()) {
        for (const auto& v : c.value("exchange").toArray())
            codes.append(v.toString());
    } else if (!c.value("exchange").toString().isEmpty()) {
        codes.append(c.value("exchange").toString());
    }
    int exchange_index = 0;
    const QString wanted = sorted_codes(codes);
    for (int i = 1; !codes.isEmpty() && i < exchange_->count(); ++i) {
        if (sorted_codes(exchange_->itemData(i).toString().split(QLatin1Char(','))) == wanted) {
            exchange_index = i;
            break;
        }
    }
    exchange_->setCurrentIndex(exchange_index);
    select_data(sector_, first_value(c.value("sector")));
    populate_industries();
    select_data(industry_, first_value(c.value("industry")));
    select_data(peer_, first_value(c.value("peer_group")));

    clear_criteria();
    for (const auto& v : c.value("filters").toArray()) {
        const QJsonObject f = v.toObject();
        const QJsonValue value = f.value("value");
        QString lo;
        QString hi;
        if (value.isArray() && value.toArray().size() == 2) {
            lo = format_value(value.toArray().at(0).toDouble());
            hi = format_value(value.toArray().at(1).toDouble());
        } else if (value.isDouble()) {
            lo = format_value(value.toDouble());
        }
        add_criterion(f.value("field").toString(), f.value("op").toString().toLower(), lo, hi);
    }
    select_data(sort_field_, q.sort_field.isEmpty() ? kDefaultField : q.sort_field);
    select_data(sort_dir_, q.sort_asc ? QStringLiteral("asc") : QStringLiteral("desc"));
    loading_ = false;
}

void ScreenerCriteriaBuilder::reset_to_default() {
    if (!has_catalog())
        return;
    services::ScreenerQuery q;
    q.criteria["region"] = QJsonArray{kDefaultRegion};
    QJsonArray exchanges;
    for (const auto& code : kDefaultExchanges.split(QLatin1Char(',')))
        exchanges.append(code);
    q.criteria["exchange"] = exchanges;
    QJsonObject f;
    f["field"] = kDefaultField;
    f["op"] = QStringLiteral("gte");
    f["value"] = 2e9;
    q.criteria["filters"] = QJsonArray{f};
    q.sort_field = kDefaultField;
    q.sort_asc = false;
    load_query(q);
}

} // namespace fincept::screens
