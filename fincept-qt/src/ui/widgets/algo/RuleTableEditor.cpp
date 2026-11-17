// src/ui/widgets/algo/RuleTableEditor.cpp
#include "ui/widgets/algo/RuleTableEditor.h"

#include "services/algo_trading/AlgoTradingTypes.h"

#include <QAbstractItemView>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::ui::algo {

namespace svc = fincept::services::algo;
namespace rules = fincept::services::algo::rules;

namespace {

const char* const kRteValueKey = "#value";

enum RteCol { kRteColIndex, kRteColLhs, kRteColLhsParams, kRteColOp, kRteColRhs, kRteColRhsValue, kRteColAgo,
              kRteColIssue, kRteColRemove, kRteColCount };

const svc::IndicatorDef* rte_find(const QString& id) {
    static const QVector<svc::IndicatorDef> kCatalog = svc::algo_indicators();
    for (const auto& d : kCatalog)
        if (d.id == id)
            return &d;
    return nullptr;
}

QString rte_number(double v) {
    QString s = QString::number(v, 'f', 6);
    while (s.contains(QLatin1Char('.')) && (s.endsWith(QLatin1Char('0')) || s.endsWith(QLatin1Char('.'))))
        s.chop(1);
    return s == QLatin1String("-0") ? QStringLiteral("0") : s;
}

/// "50", "12, 26, 9" from the JSON params (catalogue defaults for missing ones).
QString rte_params_text(const svc::IndicatorDef* def, const QJsonObject& p) {
    if (!def)
        return {};
    QStringList parts;
    for (const auto& spec : def->params) {
        const QJsonValue v = p.value(spec.name);
        parts << rte_number(v.isDouble() ? v.toDouble() : spec.def);
    }
    return parts.join(QStringLiteral(", "));
}

/// Parse "12, 26, 9" (space / comma / semicolon separated, "." or "," decimals when unambiguous).
/// Missing values take the defaults; out-of-range ones are clamped and reported in `problem`.
QJsonObject rte_parse_params(const svc::IndicatorDef* def, const QString& text, QString* problem) {
    QJsonObject out;
    if (!def)
        return out;
    QString t = text;
    const QStringList raw = t.split(QRegularExpression(QStringLiteral("[\\s;,/]+")), Qt::SkipEmptyParts);
    for (int i = 0; i < def->params.size(); ++i) {
        const auto& spec = def->params[i];
        double v = spec.def;
        if (i < raw.size()) {
            bool ok = false;
            const double parsed = raw[i].toDouble(&ok);
            if (!ok) {
                if (problem && problem->isEmpty())
                    *problem = QCoreApplication::translate("fincept::ui::algo::RuleTableEditor",
                                                           "“%1” is not a number (%2).")
                                   .arg(raw[i], spec.name);
            } else if (parsed < spec.min || parsed > spec.max) {
                if (problem && problem->isEmpty())
                    *problem = QCoreApplication::translate("fincept::ui::algo::RuleTableEditor",
                                                           "%1 must be between %2 and %3.")
                                   .arg(spec.name, rte_number(spec.min), rte_number(spec.max));
                v = std::clamp(parsed, spec.min, spec.max);
            } else {
                v = parsed;
            }
        }
        if (spec.decimals == 0)
            out[spec.name] = static_cast<int>(std::lround(v));
        else
            out[spec.name] = v;
    }
    if (raw.size() > def->params.size() && problem && problem->isEmpty())
        *problem = QCoreApplication::translate("fincept::ui::algo::RuleTableEditor", "%1 takes %n parameter(s).",
                                               nullptr, static_cast<int>(def->params.size()))
                       .arg(def->label);
    return out;
}

bool rte_parse_number(const QString& text, double* out) {
    QString t = text.trimmed();
    if (t.isEmpty())
        return false;
    t.remove(QLatin1Char(' '));
    if (t.count(QLatin1Char(',')) == 1 && !t.contains(QLatin1Char('.')))
        t.replace(QLatin1Char(','), QLatin1Char('.'));
    else
        t.remove(QLatin1Char(','));
    bool ok = false;
    const double v = t.toDouble(&ok);
    if (ok && std::isfinite(v)) {
        *out = v;
        return true;
    }
    return false;
}

QString rte_category_title(const QString& cat) {
    if (cat == QLatin1String("stock"))
        return QCoreApplication::translate("fincept::ui::algo::RuleTableEditor", "PRICE & VOLUME");
    if (cat == QLatin1String("ma"))
        return QCoreApplication::translate("fincept::ui::algo::RuleTableEditor", "MOVING AVERAGES");
    if (cat == QLatin1String("momentum"))
        return QCoreApplication::translate("fincept::ui::algo::RuleTableEditor", "MOMENTUM");
    if (cat == QLatin1String("trend"))
        return QCoreApplication::translate("fincept::ui::algo::RuleTableEditor", "TREND");
    if (cat == QLatin1String("volatility"))
        return QCoreApplication::translate("fincept::ui::algo::RuleTableEditor", "VOLATILITY");
    if (cat == QLatin1String("volume"))
        return QCoreApplication::translate("fincept::ui::algo::RuleTableEditor", "VOLUME");
    return cat.toUpper();
}

/// Item text of one indicator output: "Close", "SMA", "MACD signal line", "Bollinger Bands lower".
QString rte_output_text(const svc::IndicatorDef& d, const QString& field) {
    QString label = d.label;
    if (d.category == QLatin1String("stock") && label.endsWith(QLatin1String(" Price")))
        label.chop(6);
    if (d.fields.size() <= 1 || field == QLatin1String("value") ||
        (d.id == QLatin1String("MACD") && field == QLatin1String("line")))
        return label;
    QString f = field;
    f.replace(QLatin1Char('_'), QLatin1Char(' '));
    if (f == QLatin1String("pct b"))
        f = QStringLiteral("%B");
    return label + QLatin1Char(' ') + f;
}

void rte_set_prop(QWidget* w, const char* name, bool on) {
    if (!w || w->property(name).toBool() == on)
        return;
    w->setProperty(name, on);
    w->style()->unpolish(w);
    w->style()->polish(w);
}

QLineEdit* rte_line(QWidget* parent, int chars, const QString& name, const QString& tip) {
    auto* e = new QLineEdit(parent);
    e->setObjectName(QStringLiteral("algoField"));
    e->setAccessibleName(name);
    e->setToolTip(tip);
    e->setMinimumWidth(QFontMetrics(e->font()).averageCharWidth() * chars + 12);
    e->setMaximumWidth(QFontMetrics(e->font()).averageCharWidth() * (chars + 6) + 12);
    return e;
}

QSpinBox* rte_ago(QWidget* parent, const QString& name, const QString& tip) {
    auto* s = new QSpinBox(parent);
    s->setObjectName(QStringLiteral("algoField"));
    s->setRange(0, 500);
    s->setButtonSymbols(QAbstractSpinBox::NoButtons);
    s->setAccessibleName(name);
    s->setToolTip(tip);
    s->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    s->setMinimumWidth(QFontMetrics(s->font()).averageCharWidth() * 3 + 14);
    s->setMaximumWidth(QFontMetrics(s->font()).averageCharWidth() * 4 + 16);
    return s;
}

} // namespace

// ── Rows ────────────────────────────────────────────────────────────────────

struct RuleTableEditor::Row {
    int group = -1; ///< index into the group nodes (stable while the row lives); -1 = top level
    QLabel* index = nullptr;
    QComboBox* lhs = nullptr;
    QLineEdit* lhs_params = nullptr;
    QComboBox* op = nullptr;
    QComboBox* rhs = nullptr;
    QWidget* rhs_box = nullptr; ///< value / and / value2 / rhs params
    QLineEdit* value = nullptr;
    QLabel* and_label = nullptr;
    QLineEdit* value2 = nullptr;
    QLineEdit* rhs_params = nullptr;
    QWidget* ago_box = nullptr;
    QSpinBox* lhs_ago = nullptr;
    QLabel* ago_dot = nullptr;
    QSpinBox* rhs_ago = nullptr;
    QLabel* issue = nullptr;
    QPushButton* remove = nullptr;
    QJsonObject extra;         ///< keys this editor does not show (kept on save)
    QString param_problem;     ///< local parse problem (blocking)
};

struct RuleTableEditor::Group {
    QWidget* head = nullptr; ///< spans the operand columns
    QLabel* title = nullptr;
    QComboBox* logic = nullptr;
    QCheckBox* negate = nullptr;
    QPushButton* add = nullptr;
    QPushButton* remove = nullptr;
    QLabel* issue = nullptr;
    QVector<Row*> rows;
    QJsonObject extra;
};

struct RuleTableEditor::Node {
    Row* row = nullptr;
    Group* group = nullptr;
};

RuleTableEditor::RuleTableEditor(Kind kind, QWidget* parent) : QWidget(parent), kind_(kind) {
    setObjectName(kind == Kind::Entry   ? QStringLiteral("ruleTableEntry")
                  : kind == Kind::Exit  ? QStringLiteral("ruleTableExit")
                  : kind == Kind::Scan  ? QStringLiteral("ruleTableScan")
                                        : QStringLiteral("ruleTableAlert"));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(4);

    // Header controls (the host moves them into its panel header).
    header_controls_ = new QWidget(this);
    auto* hc = new QHBoxLayout(header_controls_);
    hc->setContentsMargins(0, 0, 0, 0);
    hc->setSpacing(6);
    logic_seg_ = new QWidget(header_controls_);
    auto* seg = new QHBoxLayout(logic_seg_);
    seg->setContentsMargins(0, 0, 0, 0);
    seg->setSpacing(0);
    auto* group = new QButtonGroup(logic_seg_);
    for (int i = 0; i < 2; ++i) {
        auto* b = new QPushButton(logic_seg_);
        b->setObjectName(QStringLiteral("algoSeg"));
        b->setCheckable(true);
        b->setChecked(i == 0);
        b->setCursor(Qt::PointingHandCursor);
        group->addButton(b, i);
        seg->addWidget(b);
    }
    connect(group, &QButtonGroup::idClicked, this, [this](int id) {
        const QString l = id == 1 ? QStringLiteral("OR") : QStringLiteral("AND");
        if (l == logic_)
            return;
        logic_ = l;
        schedule_changed();
    });
    hc->addWidget(logic_seg_);
    add_rule_btn_ = new QPushButton(header_controls_);
    add_rule_btn_->setObjectName(QStringLiteral("algoToolButton"));
    add_rule_btn_->setCursor(Qt::PointingHandCursor);
    connect(add_rule_btn_, &QPushButton::clicked, this, &RuleTableEditor::add_rule);
    hc->addWidget(add_rule_btn_);
    add_group_btn_ = new QPushButton(header_controls_);
    add_group_btn_->setObjectName(QStringLiteral("algoToolButton"));
    add_group_btn_->setCursor(Qt::PointingHandCursor);
    connect(add_group_btn_, &QPushButton::clicked, this, &RuleTableEditor::add_group);
    hc->addWidget(add_group_btn_);
    // Shown above the rows until a host moves it into its panel header (the layout lets go of
    // a re-parented child by itself).
    root->addWidget(header_controls_, 0, Qt::AlignRight);

    grid_host_ = new QWidget(this);
    grid_ = new QGridLayout(grid_host_);
    grid_->setContentsMargins(0, 0, 0, 0);
    grid_->setHorizontalSpacing(4);
    grid_->setVerticalSpacing(2);
    grid_->setColumnStretch(kRteColLhs, 3);
    grid_->setColumnStretch(kRteColRhs, 3);
    grid_->setColumnStretch(kRteColRhsValue, 2);
    for (int c = 0; c < kRteColCount; ++c) {
        auto* cap = new QLabel(grid_host_);
        cap->setObjectName(QStringLiteral("algoFieldLabel"));
        captions_.append(cap);
    }
    root->addWidget(grid_host_);

    empty_ = new QLabel(this);
    empty_->setObjectName(QStringLiteral("algoNote"));
    empty_->setWordWrap(true);
    root->addWidget(empty_);
    sentence_ = new QLabel(this);
    sentence_->setObjectName(QStringLiteral("algoSentence"));
    sentence_->setWordWrap(true);
    sentence_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(sentence_);
    problems_ = new QLabel(this);
    problems_->setObjectName(QStringLiteral("algoNote"));
    problems_->setProperty("tone", QStringLiteral("danger"));
    problems_->setWordWrap(true);
    problems_->setVisible(false);
    root->addWidget(problems_);

    retranslate();
    relayout();
    update_feedback();
}

RuleTableEditor::~RuleTableEditor() {
    for (Node* n : nodes_) {
        if (n->group) {
            for (Row* r : n->group->rows)
                delete r;
            delete n->group;
        }
        delete n->row;
        delete n;
    }
}

void RuleTableEditor::retranslate() {
    const QStringList seg = {tr("ALL"), tr("ANY")};
    const auto buttons = logic_seg_->findChildren<QPushButton*>(QString(), Qt::FindDirectChildrenOnly);
    for (int i = 0; i < buttons.size() && i < seg.size(); ++i) {
        buttons[i]->setText(seg[i]);
        buttons[i]->setAccessibleName(i == 0 ? tr("Match all rules (AND)") : tr("Match any rule (OR)"));
        buttons[i]->setToolTip(i == 0 ? tr("Every rule must hold (AND)") : tr("One rule is enough (OR)"));
    }
    add_rule_btn_->setText(tr("+ RULE"));
    add_rule_btn_->setToolTip(tr("Add a rule (Enter in a rule adds one below it)"));
    add_rule_btn_->setAccessibleName(tr("Add rule"));
    add_group_btn_->setText(tr("+ GROUP"));
    add_group_btn_->setToolTip(tr("Add a group of rules that combine with their own ALL / ANY"));
    add_group_btn_->setAccessibleName(tr("Add group"));
    const QStringList caps = {tr("#"), tr("IF"), tr("PARAMS"), tr("IS"), tr("THAN"), tr("VALUE / PARAMS"),
                              tr("BARS AGO"), QString(), QString()};
    for (int i = 0; i < captions_.size() && i < caps.size(); ++i)
        captions_[i]->setText(caps[i]);
    captions_[kRteColAgo]->setToolTip(tr("Compare with an earlier bar: left · right operand (0 = the current bar)"));
    captions_[kRteColLhsParams]->setToolTip(tr("Indicator parameters, comma separated (hover a field for their names)"));
    switch (kind_) {
        case Kind::Entry:
            empty_->setText(tr("No entry rule yet — click a TEMPLATE above, or + RULE."));
            break;
        case Kind::Exit:
            empty_->setText(tr("No exit rule — the position closes only by stop-loss, take-profit or trailing "
                               "stop. + RULE adds one."));
            break;
        case Kind::Scan:
        case Kind::Alert:
            empty_->setText(tr("No rule yet — pick a preset, or + RULE."));
            break;
    }
}

void RuleTableEditor::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslate();
        update_feedback();
    }
    QWidget::changeEvent(event);
}

void RuleTableEditor::populate_operand_combo(QComboBox* c, bool with_value) const {
    auto* model = qobject_cast<QStandardItemModel*>(c->model());
    if (with_value) {
        c->addItem(tr("Value"), QString::fromLatin1(kRteValueKey));
        c->setItemData(0, tr("Compare with a number"), Qt::ToolTipRole);
    }
    QString last_cat;
    for (const auto& d : svc::algo_indicators()) {
        if (d.category != last_cat) {
            last_cat = d.category;
            c->addItem(QStringLiteral("— %1 —").arg(rte_category_title(d.category)), QString());
            if (model)
                if (QStandardItem* it = model->item(c->count() - 1))
                    it->setEnabled(false);
        }
        const QStringList fields = d.fields.isEmpty() ? QStringList{QStringLiteral("value")} : d.fields;
        for (const QString& f : fields) {
            c->addItem(rte_output_text(d, f), d.id + QLatin1Char('|') + f);
            c->setItemData(c->count() - 1, rte_output_text(d, f), Qt::ToolTipRole);
        }
    }
}

void RuleTableEditor::set_operand(QComboBox* c, QLineEdit* params, const QString& indicator, const QString& field,
                                  const QJsonObject& p) const {
    const svc::IndicatorDef* def = rte_find(indicator);
    QString f = field;
    if (def && !def->fields.isEmpty() && !def->fields.contains(f))
        f = def->fields.first();
    const int idx = c->findData(indicator + QLatin1Char('|') + f);
    if (idx >= 0)
        c->setCurrentIndex(idx);
    if (params)
        params->setText(rte_params_text(def, p));
}

QJsonObject RuleTableEditor::default_rule() const {
    const bool exit = kind_ == Kind::Exit;
    return QJsonObject{{"indicator", "SMA"},
                       {"params", QJsonObject{{"period", 20}}},
                       {"field", "value"},
                       {"offset", 0},
                       {"operator", exit ? "crosses_below" : "crosses_above"},
                       {"compare_mode", "indicator"},
                       {"compare_indicator", "SMA"},
                       {"compare_params", QJsonObject{{"period", 50}}},
                       {"compare_field", "value"},
                       {"compare_offset", 0}};
}

RuleTableEditor::Row* RuleTableEditor::make_row(QWidget* parent, const QJsonObject& c, int group) {
    auto* r = new Row;
    r->group = group;
    r->index = new QLabel(parent);
    r->index->setObjectName(QStringLiteral("algoNote"));
    r->index->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    r->lhs = new QComboBox(parent);
    r->lhs->setObjectName(QStringLiteral("algoField"));
    r->lhs->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    r->lhs->setMinimumContentsLength(9);
    populate_operand_combo(r->lhs, false);
    r->lhs->setMaxVisibleItems(18);
    r->lhs_params = rte_line(parent, 5, tr("Left indicator parameters"), QString());

    r->op = new QComboBox(parent);
    r->op->setObjectName(QStringLiteral("algoField"));
    r->op->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    r->op->setMinimumContentsLength(7);
    const QStringList ops = svc::algo_operators();
    const QStringList texts = {QStringLiteral(">"),  QStringLiteral("<"), QStringLiteral("≥"), QStringLiteral("≤"),
                               tr("touches"),        tr("crosses above"),  tr("crosses below"), tr("rising"),
                               tr("falling"),        tr("between")};
    const QStringList tips = {tr("greater than"), tr("less than"), tr("greater than or equal"),
                              tr("less than or equal"), tr("meets or passes through the level between two bars"),
                              tr("was at or below on the previous bar, above now"),
                              tr("was at or above on the previous bar, below now"), tr("higher than on the previous bar"),
                              tr("lower than on the previous bar"), tr("inside a range (both ends included)")};
    for (int i = 0; i < ops.size() && i < texts.size(); ++i) {
        r->op->addItem(texts[i], ops[i]);
        r->op->setItemData(i, tips[i], Qt::ToolTipRole);
    }

    r->rhs = new QComboBox(parent);
    r->rhs->setObjectName(QStringLiteral("algoField"));
    r->rhs->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    r->rhs->setMinimumContentsLength(9);
    populate_operand_combo(r->rhs, true);
    r->rhs->setMaxVisibleItems(18);

    r->rhs_box = new QWidget(parent);
    auto* rb = new QHBoxLayout(r->rhs_box);
    rb->setContentsMargins(0, 0, 0, 0);
    rb->setSpacing(4);
    r->value = rte_line(r->rhs_box, 7, tr("Value"), tr("A number (price, level, threshold)"));
    r->and_label = new QLabel(tr("and"), r->rhs_box);
    r->and_label->setObjectName(QStringLiteral("algoNote"));
    r->value2 = rte_line(r->rhs_box, 7, tr("Upper bound"), tr("Upper end of the range"));
    r->rhs_params = rte_line(r->rhs_box, 5, tr("Right indicator parameters"), QString());
    rb->addWidget(r->value, 1);
    rb->addWidget(r->and_label);
    rb->addWidget(r->value2, 1);
    rb->addWidget(r->rhs_params, 1);

    r->ago_box = new QWidget(parent);
    auto* ab = new QHBoxLayout(r->ago_box);
    ab->setContentsMargins(0, 0, 0, 0);
    ab->setSpacing(2);
    r->lhs_ago = rte_ago(r->ago_box, tr("Left operand bars ago"), tr("Left operand: bars ago (0 = the current bar)"));
    r->ago_dot = new QLabel(QStringLiteral("·"), r->ago_box);
    r->ago_dot->setObjectName(QStringLiteral("algoNote"));
    r->rhs_ago = rte_ago(r->ago_box, tr("Right operand bars ago"), tr("Right operand: bars ago (0 = the current bar)"));
    ab->addWidget(r->lhs_ago);
    ab->addWidget(r->ago_dot);
    ab->addWidget(r->rhs_ago);
    ab->addStretch();

    r->issue = new QLabel(QStringLiteral("!"), parent);
    r->issue->setObjectName(QStringLiteral("algoChip"));
    r->issue->setProperty("tone", QStringLiteral("danger"));
    r->issue->setAlignment(Qt::AlignCenter);
    r->issue->setVisible(false);

    r->remove = new QPushButton(QStringLiteral("✕"), parent);
    r->remove->setObjectName(QStringLiteral("algoToolButton"));
    r->remove->setFocusPolicy(Qt::NoFocus); // Delete / Ctrl+Delete remove a row from the keyboard
    r->remove->setCursor(Qt::PointingHandCursor);
    r->remove->setToolTip(tr("Remove this rule (Delete on its combo, Ctrl+Delete anywhere in the row)"));

    // Values first, then signals (no change storm while loading).
    const QString ind = c.value(QStringLiteral("indicator")).toString(QStringLiteral("SMA"));
    set_operand(r->lhs, r->lhs_params, ind, c.value(QStringLiteral("field")).toString(QStringLiteral("value")),
                c.value(QStringLiteral("params")).toObject());
    const QString op = c.value(QStringLiteral("operator")).toString(QStringLiteral(">"));
    const int oi = r->op->findData(op);
    r->op->setCurrentIndex(oi >= 0 ? oi : 0);
    if (c.value(QStringLiteral("compare_mode")).toString(QStringLiteral("value")) == QLatin1String("indicator")) {
        set_operand(r->rhs, r->rhs_params, c.value(QStringLiteral("compare_indicator")).toString(),
                    c.value(QStringLiteral("compare_field")).toString(QStringLiteral("value")),
                    c.value(QStringLiteral("compare_params")).toObject());
    } else {
        r->rhs->setCurrentIndex(0);
    }
    r->value->setText(rte_number(c.value(QStringLiteral("value")).toDouble(0)));
    r->value2->setText(rte_number(c.value(QStringLiteral("value2")).toDouble(op == QLatin1String("between") ? 0 : 100)));
    r->lhs_ago->setValue(std::max(0, c.value(QStringLiteral("offset")).toInt(0)));
    r->rhs_ago->setValue(std::max(0, c.value(QStringLiteral("compare_offset")).toInt(0)));
    static const QStringList kShown = {"indicator", "params", "field", "offset", "operator", "compare_mode", "value",
                                       "value2", "compare_indicator", "compare_params", "compare_field",
                                       "compare_offset"};
    for (auto it = c.begin(); it != c.end(); ++it)
        if (!kShown.contains(it.key()))
            r->extra.insert(it.key(), it.value());

    auto edited = [this, r]() {
        if (loading_)
            return;
        refresh_row_widgets(r);
        schedule_changed();
    };
    connect(r->lhs, &QComboBox::currentIndexChanged, this, [this, r, edited](int) {
        if (!loading_) {
            const QStringList key = r->lhs->currentData().toString().split(QLatin1Char('|'));
            r->lhs_params->setText(rte_params_text(rte_find(key.value(0)), {}));
        }
        edited();
    });
    connect(r->rhs, &QComboBox::currentIndexChanged, this, [this, r, edited](int) {
        if (!loading_) {
            const QStringList key = r->rhs->currentData().toString().split(QLatin1Char('|'));
            r->rhs_params->setText(rte_params_text(rte_find(key.value(0)), {}));
        }
        edited();
    });
    connect(r->op, &QComboBox::currentIndexChanged, this, [edited](int) { edited(); });
    for (QLineEdit* e : {r->lhs_params, r->rhs_params, r->value, r->value2})
        connect(e, &QLineEdit::textEdited, this, [edited](const QString&) { edited(); });
    for (QSpinBox* s : {r->lhs_ago, r->rhs_ago})
        connect(s, &QSpinBox::valueChanged, this, [edited](int) { edited(); });
    connect(r->remove, &QPushButton::clicked, this, [this, r]() { remove_row(r); });

    for (QWidget* w : std::initializer_list<QWidget*>{r->lhs, r->lhs_params, r->op, r->rhs, r->value, r->value2,
                                                      r->rhs_params, r->lhs_ago, r->rhs_ago})
        w->installEventFilter(this);
    refresh_row_widgets(r);
    return r;
}

void RuleTableEditor::refresh_row_widgets(Row* r) {
    const QString op = r->op->currentData().toString();
    const bool unary = op == QLatin1String("rising") || op == QLatin1String("falling");
    const bool between = op == QLatin1String("between");
    const bool rhs_value = r->rhs->currentData().toString() == QLatin1String(kRteValueKey);
    const QStringList lkey = r->lhs->currentData().toString().split(QLatin1Char('|'));
    const QStringList rkey = r->rhs->currentData().toString().split(QLatin1Char('|'));
    const svc::IndicatorDef* ldef = rte_find(lkey.value(0));
    const svc::IndicatorDef* rdef = rhs_value ? nullptr : rte_find(rkey.value(0));
    auto param_tip = [](const svc::IndicatorDef* d) {
        if (!d || d->params.isEmpty())
            return QString();
        QStringList names;
        for (const auto& p : d->params)
            names << QStringLiteral("%1 (%2–%3)").arg(p.name, rte_number(p.min), rte_number(p.max));
        return tr("%1 parameters: %2").arg(d->label, names.join(QStringLiteral(", ")));
    };
    r->lhs_params->setVisible(ldef && !ldef->params.isEmpty());
    r->lhs_params->setToolTip(param_tip(ldef));
    r->rhs->setVisible(!unary && !between);
    r->value->setVisible(!unary && (between || rhs_value));
    r->and_label->setVisible(between);
    r->value2->setVisible(between);
    r->rhs_params->setVisible(!unary && !between && rdef && !rdef->params.isEmpty());
    r->rhs_params->setToolTip(param_tip(rdef));
    r->value->setToolTip(between ? tr("Lower end of the range") : tr("A number (price, level, threshold)"));
    const bool rhs_ind = !unary && !between && !rhs_value;
    r->ago_dot->setVisible(rhs_ind);
    r->rhs_ago->setVisible(rhs_ind);
    // Local parse problems of the parameter / value fields.
    QString problem;
    rte_parse_params(ldef, r->lhs_params->text(), &problem);
    if (rhs_ind)
        rte_parse_params(rdef, r->rhs_params->text(), &problem);
    double dummy = 0;
    if (problem.isEmpty() && r->value->isVisibleTo(r->rhs_box) && !rte_parse_number(r->value->text(), &dummy))
        problem = tr("Enter a number.");
    if (problem.isEmpty() && between && !rte_parse_number(r->value2->text(), &dummy))
        problem = tr("Enter the upper bound.");
    r->param_problem = problem;
}

QJsonObject RuleTableEditor::row_json(const Row* r) const {
    QJsonObject o = r->extra;
    const QStringList lkey = r->lhs->currentData().toString().split(QLatin1Char('|'));
    const svc::IndicatorDef* ldef = rte_find(lkey.value(0));
    o[QStringLiteral("indicator")] = lkey.value(0);
    o[QStringLiteral("params")] = rte_parse_params(ldef, r->lhs_params->text(), nullptr);
    o[QStringLiteral("field")] = lkey.value(1, QStringLiteral("value"));
    o[QStringLiteral("offset")] = r->lhs_ago->value();
    const QString op = r->op->currentData().toString();
    o[QStringLiteral("operator")] = op;
    double v = 0;
    double v2 = 0;
    rte_parse_number(r->value->text(), &v);
    rte_parse_number(r->value2->text(), &v2);
    if (op == QLatin1String("between")) {
        o[QStringLiteral("compare_mode")] = QStringLiteral("value");
        o[QStringLiteral("value")] = v;
        o[QStringLiteral("value2")] = v2;
    } else if (op == QLatin1String("rising") || op == QLatin1String("falling")) {
        o[QStringLiteral("compare_mode")] = QStringLiteral("value");
        o[QStringLiteral("value")] = 0;
    } else if (r->rhs->currentData().toString() == QLatin1String(kRteValueKey)) {
        o[QStringLiteral("compare_mode")] = QStringLiteral("value");
        o[QStringLiteral("value")] = v;
    } else {
        const QStringList rkey = r->rhs->currentData().toString().split(QLatin1Char('|'));
        o[QStringLiteral("compare_mode")] = QStringLiteral("indicator");
        o[QStringLiteral("compare_indicator")] = rkey.value(0);
        o[QStringLiteral("compare_params")] = rte_parse_params(rte_find(rkey.value(0)), r->rhs_params->text(), nullptr);
        o[QStringLiteral("compare_field")] = rkey.value(1, QStringLiteral("value"));
        o[QStringLiteral("compare_offset")] = r->rhs_ago->value();
    }
    return o;
}

void RuleTableEditor::destroy_row(Row* r) {
    // No late signal / key event may reach a lambda holding `r` once it is gone.
    for (QWidget* w : std::initializer_list<QWidget*>{r->lhs, r->lhs_params, r->op, r->rhs, r->value, r->value2,
                                                      r->rhs_params, r->lhs_ago, r->rhs_ago, r->remove}) {
        if (!w)
            continue;
        w->removeEventFilter(this);
        w->disconnect(this);
    }
    for (QWidget* w : std::initializer_list<QWidget*>{r->index, r->lhs, r->lhs_params, r->op, r->rhs, r->rhs_box,
                                                      r->ago_box, r->issue, r->remove}) {
        if (!w)
            continue;
        grid_->removeWidget(w);
        w->hide();
        w->deleteLater();
    }
    delete r;
}

RuleTableEditor::Group* RuleTableEditor::make_group(const QJsonObject& g) {
    auto* grp = new Group;
    grp->head = new QWidget(grid_host_);
    grp->head->setObjectName(QStringLiteral("algoRuleGroup"));
    grp->head->setAttribute(Qt::WA_StyledBackground, true);
    auto* h = new QHBoxLayout(grp->head);
    h->setContentsMargins(6, 0, 0, 0);
    h->setSpacing(6);
    grp->title = new QLabel(grp->head);
    grp->title->setObjectName(QStringLiteral("algoFieldLabel"));
    grp->logic = new QComboBox(grp->head);
    grp->logic->setObjectName(QStringLiteral("algoField"));
    grp->logic->addItem(tr("ALL of"), QStringLiteral("AND"));
    grp->logic->addItem(tr("ANY of"), QStringLiteral("OR"));
    grp->logic->setAccessibleName(tr("Group logic"));
    grp->logic->setToolTip(tr("How the rules inside this group combine"));
    grp->negate = new QCheckBox(tr("NOT"), grp->head);
    grp->negate->setToolTip(tr("The group holds when its rules do NOT"));
    grp->add = new QPushButton(tr("+ RULE"), grp->head);
    grp->add->setObjectName(QStringLiteral("algoToolButton"));
    grp->add->setAccessibleName(tr("Add rule to group"));
    grp->add->setCursor(Qt::PointingHandCursor);
    h->addWidget(grp->title);
    h->addWidget(grp->logic);
    h->addWidget(grp->negate);
    h->addStretch();
    h->addWidget(grp->add);
    grp->issue = new QLabel(QStringLiteral("!"), grid_host_);
    grp->issue->setObjectName(QStringLiteral("algoChip"));
    grp->issue->setProperty("tone", QStringLiteral("danger"));
    grp->issue->setVisible(false);
    grp->remove = new QPushButton(QStringLiteral("✕"), grid_host_);
    grp->remove->setObjectName(QStringLiteral("algoToolButton"));
    grp->remove->setFocusPolicy(Qt::NoFocus);
    grp->remove->setCursor(Qt::PointingHandCursor);
    grp->remove->setToolTip(tr("Remove this group and its rules"));
    const QString logic = g.value(QStringLiteral("logic")).toString(g.value(QStringLiteral("op")).toString(QStringLiteral("AND")));
    grp->logic->setCurrentIndex(logic.compare(QLatin1String("OR"), Qt::CaseInsensitive) == 0 ? 1 : 0);
    grp->negate->setChecked(g.value(QStringLiteral("negate")).toBool(false));
    for (auto it = g.begin(); it != g.end(); ++it)
        if (it.key() != QLatin1String("children") && it.key() != QLatin1String("logic") &&
            it.key() != QLatin1String("op") && it.key() != QLatin1String("negate"))
            grp->extra.insert(it.key(), it.value());
    connect(grp->logic, &QComboBox::currentIndexChanged, this, [this](int) {
        if (!loading_)
            schedule_changed();
    });
    connect(grp->negate, &QCheckBox::toggled, this, [this](bool) {
        if (!loading_)
            schedule_changed();
    });
    return grp;
}

void RuleTableEditor::relayout() {
    // Take every item out (widgets stay alive), then lay the rows out again in order.
    while (QLayoutItem* it = grid_->takeAt(0))
        delete it;
    const bool any = !nodes_.isEmpty();
    for (int c = 0; c < captions_.size(); ++c) {
        captions_[c]->setVisible(any && !captions_[c]->text().isEmpty());
        if (any)
            grid_->addWidget(captions_[c], 0, c);
    }
    int line = 1;
    QWidget* prev_focus = nullptr;
    auto chain = [&prev_focus](QWidget* w) {
        if (!w)
            return;
        if (prev_focus)
            QWidget::setTabOrder(prev_focus, w);
        prev_focus = w;
    };
    auto place_row = [&](Row* r, const QString& label, int indent) {
        r->index->setText(label);
        r->index->setContentsMargins(indent, 0, 0, 0);
        grid_->addWidget(r->index, line, kRteColIndex);
        grid_->addWidget(r->lhs, line, kRteColLhs);
        grid_->addWidget(r->lhs_params, line, kRteColLhsParams);
        grid_->addWidget(r->op, line, kRteColOp);
        grid_->addWidget(r->rhs, line, kRteColRhs);
        grid_->addWidget(r->rhs_box, line, kRteColRhsValue);
        grid_->addWidget(r->ago_box, line, kRteColAgo);
        grid_->addWidget(r->issue, line, kRteColIssue);
        grid_->addWidget(r->remove, line, kRteColRemove);
        for (QWidget* w : std::initializer_list<QWidget*>{r->index, r->lhs, r->op, r->rhs_box, r->ago_box, r->remove})
            w->show();
        const QString who = tr("Rule %1").arg(label);
        r->lhs->setAccessibleName(tr("%1: left operand").arg(who));
        r->op->setAccessibleName(tr("%1: operator").arg(who));
        r->rhs->setAccessibleName(tr("%1: right operand").arg(who));
        r->remove->setAccessibleName(tr("Remove %1").arg(who.toLower()));
        for (QWidget* w : std::initializer_list<QWidget*>{r->lhs, r->lhs_params, r->op, r->rhs, r->value, r->value2,
                                                          r->rhs_params, r->lhs_ago, r->rhs_ago})
            chain(w);
        refresh_row_widgets(r);
        ++line;
    };
    for (int i = 0; i < nodes_.size(); ++i) {
        Node* n = nodes_[i];
        if (n->row) {
            place_row(n->row, QString::number(i + 1), 0);
            continue;
        }
        Group* g = n->group;
        g->title->setText(tr("GROUP %1").arg(i + 1));
        grid_->addWidget(g->head, line, kRteColLhs, 1, kRteColAgo - kRteColLhs + 1);
        grid_->addWidget(g->issue, line, kRteColIssue);
        grid_->addWidget(g->remove, line, kRteColRemove);
        g->head->show();
        g->remove->show();
        chain(g->logic);
        chain(g->negate);
        chain(g->add);
        ++line;
        for (int k = 0; k < g->rows.size(); ++k)
            place_row(g->rows[k], QStringLiteral("%1.%2").arg(i + 1).arg(k + 1), 10);
    }
    empty_->setVisible(!any);
}

void RuleTableEditor::insert_rule(int top_index, int group_child_index, int group, const QJsonObject& c, bool focus) {
    Row* r = nullptr;
    if (group >= 0 && group < nodes_.size() && nodes_[group]->group) {
        Group* g = nodes_[group]->group;
        r = make_row(grid_host_, c, group);
        const int at = group_child_index < 0 ? g->rows.size() : std::min<int>(group_child_index, g->rows.size());
        g->rows.insert(at, r);
    } else {
        r = make_row(grid_host_, c, -1);
        auto* n = new Node;
        n->row = r;
        const int at = top_index < 0 ? nodes_.size() : std::min<int>(top_index, nodes_.size());
        nodes_.insert(at, n);
    }
    relayout();
    schedule_changed();
    if (focus && r)
        QTimer::singleShot(0, r->lhs, [lhs = r->lhs]() { lhs->setFocus(Qt::TabFocusReason); });
}

void RuleTableEditor::add_rule() {
    insert_rule(-1, -1, -1, default_rule(), true);
}

void RuleTableEditor::add_group() {
    auto* n = new Node;
    n->group = make_group(QJsonObject{{"logic", "OR"}});
    nodes_.append(n);
    const int gi = nodes_.size() - 1;
    connect(n->group->add, &QPushButton::clicked, this, [this, g = n->group]() {
        for (int i = 0; i < nodes_.size(); ++i)
            if (nodes_[i]->group == g) {
                insert_rule(-1, -1, i, default_rule(), true);
                return;
            }
    });
    connect(n->group->remove, &QPushButton::clicked, this, [this, g = n->group]() {
        for (int i = 0; i < nodes_.size(); ++i)
            if (nodes_[i]->group == g) {
                remove_group(i);
                return;
            }
    });
    insert_rule(-1, -1, gi, default_rule(), true);
}

void RuleTableEditor::remove_row(Row* r) {
    QWidget* next_focus = nullptr;
    for (int i = 0; i < nodes_.size(); ++i) {
        Node* n = nodes_[i];
        if (n->row == r) {
            nodes_.removeAt(i);
            delete n;
            if (i < nodes_.size() && nodes_[i]->row)
                next_focus = nodes_[i]->row->lhs;
            else if (i > 0 && nodes_[i - 1]->row)
                next_focus = nodes_[i - 1]->row->lhs;
            break;
        }
        if (n->group) {
            const int k = n->group->rows.indexOf(r);
            if (k >= 0) {
                n->group->rows.removeAt(k);
                if (k < n->group->rows.size())
                    next_focus = n->group->rows[k]->lhs;
                break;
            }
        }
    }
    destroy_row(r);
    relayout();
    schedule_changed();
    if (next_focus)
        next_focus->setFocus(Qt::OtherFocusReason);
    else
        add_rule_btn_->setFocus(Qt::OtherFocusReason);
}

void RuleTableEditor::remove_group(int group) {
    if (group < 0 || group >= nodes_.size() || !nodes_[group]->group)
        return;
    Node* n = nodes_.takeAt(group);
    for (Row* r : n->group->rows)
        destroy_row(r);
    for (QWidget* w : std::initializer_list<QWidget*>{n->group->head, n->group->issue, n->group->remove}) {
        grid_->removeWidget(w);
        w->hide();
        w->deleteLater();
    }
    delete n->group;
    delete n;
    relayout();
    schedule_changed();
    add_rule_btn_->setFocus(Qt::OtherFocusReason);
}

RuleTableEditor::Row* RuleTableEditor::row_of(QObject* o) const {
    auto owns = [o](const Row* r) {
        for (QObject* w : std::initializer_list<QObject*>{r->lhs, r->lhs_params, r->op, r->rhs, r->value, r->value2,
                                                          r->rhs_params, r->lhs_ago, r->rhs_ago})
            if (w == o)
                return true;
        return false;
    };
    for (Node* n : nodes_) {
        if (n->row && owns(n->row))
            return n->row;
        if (n->group)
            for (Row* r : n->group->rows)
                if (owns(r))
                    return r;
    }
    return nullptr;
}

bool RuleTableEditor::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        Row* r = row_of(watched);
        if (r) {
            const bool is_combo = qobject_cast<QComboBox*>(watched) != nullptr;
            if ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) && ke->modifiers() == Qt::NoModifier) {
                if (auto* cb = qobject_cast<QComboBox*>(watched); cb && cb->view() && cb->view()->isVisible())
                    return false; // the open list picks
                // Enter: a new rule below this one (in its group), focused.
                int top = -1, child = -1;
                for (int i = 0; i < nodes_.size(); ++i) {
                    if (nodes_[i]->row == r) {
                        top = i;
                        break;
                    }
                    if (nodes_[i]->group) {
                        const int k = nodes_[i]->group->rows.indexOf(r);
                        if (k >= 0) {
                            top = i;
                            child = k;
                            break;
                        }
                    }
                }
                if (child >= 0)
                    insert_rule(-1, child + 1, top, default_rule(), true);
                else
                    insert_rule(top + 1, -1, -1, default_rule(), true);
                return true;
            }
            if (ke->key() == Qt::Key_Delete && (is_combo || (ke->modifiers() & Qt::ControlModifier))) {
                QPointer<QComboBox> lhs = r->lhs;
                QTimer::singleShot(0, this, [this, lhs]() {
                    // Still there (a second key press may have removed it already)?
                    if (!lhs)
                        return;
                    if (Row* row = row_of(lhs))
                        remove_row(row);
                });
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void RuleTableEditor::schedule_changed() {
    if (loading_)
        return;
    if (change_pending_)
        return;
    change_pending_ = true;
    QTimer::singleShot(0, this, [this]() {
        change_pending_ = false;
        update_feedback();
        emit conditions_changed();
    });
}

void RuleTableEditor::update_feedback() {
    const QJsonArray conds = conditions();
    issues_ = rules::check_conditions(conds);
    // Local parse problems (parameters / numbers) are blocking too.
    int top = 0;
    for (Node* n : nodes_) {
        if (n->row && !n->row->param_problem.isEmpty())
            issues_.append({top, -1, n->row->param_problem, true});
        if (n->group)
            for (int k = 0; k < n->group->rows.size(); ++k)
                if (!n->group->rows[k]->param_problem.isEmpty())
                    issues_.append({top, k, n->group->rows[k]->param_problem, true});
        ++top;
    }
    std::sort(issues_.begin(), issues_.end(), [](const rules::RuleIssue& a, const rules::RuleIssue& b) {
        return a.index != b.index ? a.index < b.index : a.child < b.child;
    });
    auto mark = [](QLabel* issue, QWidget* field, const QString& reason) {
        const bool on = !reason.isEmpty();
        issue->setVisible(on);
        issue->setToolTip(reason);
        issue->setAccessibleName(on ? reason : QString());
        rte_set_prop(field, "error", on);
    };
    for (int i = 0; i < nodes_.size(); ++i) {
        Node* n = nodes_[i];
        auto reason_for = [this, i](int child) {
            QStringList rs;
            for (const auto& is : issues_)
                if (is.index == i && is.child == child)
                    rs << is.reason;
            return rs.join(QLatin1Char('\n'));
        };
        if (n->row) {
            mark(n->row->issue, n->row->lhs, reason_for(-1));
            n->row->lhs->setAccessibleDescription(reason_for(-1));
            continue;
        }
        const QString greason = reason_for(-1);
        n->group->issue->setVisible(!greason.isEmpty());
        n->group->issue->setToolTip(greason);
        for (int k = 0; k < n->group->rows.size(); ++k) {
            mark(n->group->rows[k]->issue, n->group->rows[k]->lhs, reason_for(k));
            n->group->rows[k]->lhs->setAccessibleDescription(reason_for(k));
        }
    }
    rules::Side side = rules::Side::Entry;
    switch (kind_) {
        case Kind::Entry:
            side = rules::Side::Entry;
            break;
        case Kind::Exit:
            side = rules::Side::Exit;
            break;
        case Kind::Scan:
            side = rules::Side::Scan;
            break;
        case Kind::Alert:
            side = rules::Side::Alert;
            break;
    }
    sentence_->setText(conds.isEmpty() ? QString() : rules::rules_sentence(conds, logic_, side));
    sentence_->setVisible(!conds.isEmpty());
    QStringList lines;
    for (const auto& is : issues_) {
        const QString where = is.child >= 0 ? QStringLiteral("%1.%2").arg(is.index + 1).arg(is.child + 1)
                                            : QString::number(is.index + 1);
        lines << tr("! Rule %1: %2").arg(where, is.reason);
    }
    problems_->setText(lines.join(QLatin1Char('\n')));
    problems_->setVisible(!lines.isEmpty());
    // The logic segments follow logic_ (set_conditions / user).
    const auto buttons = logic_seg_->findChildren<QPushButton*>(QString(), Qt::FindDirectChildrenOnly);
    if (buttons.size() == 2) {
        buttons[0]->setChecked(logic_ != QLatin1String("OR"));
        buttons[1]->setChecked(logic_ == QLatin1String("OR"));
    }
    logic_seg_->setEnabled(nodes_.size() > 1);
}

// ── Public API ──────────────────────────────────────────────────────────────

QJsonArray RuleTableEditor::conditions() const {
    QJsonArray out;
    for (Node* n : nodes_) {
        if (n->row) {
            out.append(row_json(n->row));
            continue;
        }
        QJsonObject g = n->group->extra;
        if (!g.contains(QStringLiteral("type")))
            g[QStringLiteral("type")] = QStringLiteral("group");
        g[QStringLiteral("logic")] = n->group->logic->currentData().toString();
        if (n->group->negate->isChecked())
            g[QStringLiteral("negate")] = true;
        QJsonArray kids;
        for (Row* r : n->group->rows)
            kids.append(row_json(r));
        g[QStringLiteral("children")] = kids;
        out.append(g);
    }
    return out;
}

QString RuleTableEditor::combined_logic() const {
    return logic_;
}

void RuleTableEditor::clear_all() {
    loading_ = true;
    for (Node* n : nodes_) {
        if (n->row)
            destroy_row(n->row);
        if (n->group) {
            for (Row* r : n->group->rows)
                destroy_row(r);
            for (QWidget* w : std::initializer_list<QWidget*>{n->group->head, n->group->issue, n->group->remove}) {
                grid_->removeWidget(w);
                w->hide();
                w->deleteLater();
            }
            delete n->group;
        }
        delete n;
    }
    nodes_.clear();
    loading_ = false;
    relayout();
    update_feedback();
}

void RuleTableEditor::set_conditions(const QJsonArray& conditions, const QString& logic) {
    clear_all();
    loading_ = true;
    logic_ = logic.compare(QLatin1String("OR"), Qt::CaseInsensitive) == 0 ? QStringLiteral("OR") : QStringLiteral("AND");
    for (const auto& v : conditions) {
        const QJsonObject o = v.toObject();
        auto* n = new Node;
        if (o.contains(QStringLiteral("children")) || o.value(QStringLiteral("type")).toString() == QLatin1String("group")) {
            n->group = make_group(o);
            nodes_.append(n);
            const int gi = nodes_.size() - 1;
            for (const auto& k : o.value(QStringLiteral("children")).toArray()) {
                const QJsonObject kid = k.toObject();
                // Nested groups are kept flat here: their rows join this group (the engine
                // semantics of a 2-level tree are what the Builder ever produced).
                if (kid.contains(QStringLiteral("children"))) {
                    for (const auto& kk : kid.value(QStringLiteral("children")).toArray())
                        n->group->rows.append(make_row(grid_host_, kk.toObject(), gi));
                } else {
                    n->group->rows.append(make_row(grid_host_, kid, gi));
                }
            }
            connect(n->group->add, &QPushButton::clicked, this, [this, g = n->group]() {
                for (int i = 0; i < nodes_.size(); ++i)
                    if (nodes_[i]->group == g) {
                        insert_rule(-1, -1, i, default_rule(), true);
                        return;
                    }
            });
            connect(n->group->remove, &QPushButton::clicked, this, [this, g = n->group]() {
                for (int i = 0; i < nodes_.size(); ++i)
                    if (nodes_[i]->group == g) {
                        remove_group(i);
                        return;
                    }
            });
        } else {
            n->row = make_row(grid_host_, o, -1);
            nodes_.append(n);
        }
    }
    loading_ = false;
    relayout();
    update_feedback();
    emit conditions_changed();
}

QString RuleTableEditor::sentence() const {
    return sentence_->text();
}

QVector<rules::RuleIssue> RuleTableEditor::issues() const {
    return issues_;
}

bool RuleTableEditor::has_blocking_issue() const {
    for (const auto& i : issues_)
        if (i.blocking)
            return true;
    return false;
}

int RuleTableEditor::row_count() const {
    int n = 0;
    for (Node* node : nodes_)
        n += node->row ? 1 : node->group->rows.size();
    return n;
}

void RuleTableEditor::focus_first() {
    for (Node* n : nodes_) {
        if (n->row) {
            n->row->lhs->setFocus(Qt::TabFocusReason);
            return;
        }
        if (n->group && !n->group->rows.isEmpty()) {
            n->group->rows.first()->lhs->setFocus(Qt::TabFocusReason);
            return;
        }
    }
    add_rule_btn_->setFocus(Qt::TabFocusReason);
}

} // namespace fincept::ui::algo
