// ConnectorParamForm.cpp — see ConnectorParamForm.h.

#include "screens/data_sources/ConnectorParamForm.h"

#include "screens/data_sources/DataWorkspace.h"
#include "services/connectors/ConnectorService.h"
#include "ui/widgets/InputMemory.h"
#include "ui/widgets/symbol/SymbolSearchBox.h"

#include <QAbstractItemView>
#include <QCalendarWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDateEdit>
#include <QDir>
#include <QDoubleSpinBox>
#include <QDoubleValidator>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QStandardPaths>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <climits>
#include <limits>

namespace fincept::screens::datasources {

namespace conn = fincept::services::connectors;
using conn::ConnectorParam;
using conn::ParamInput;
using conn::ParamKind;

namespace {

const QDate kPfmUnsetDate(1800, 1, 1);
const char* const kPfmCountrySep = " — ";

struct PfmCountry {
    const char* iso2;
    const char* iso3;
    const char* name;
};
// The main economies and aggregates the catalogue's connectors ask for; anything else is typed.
constexpr PfmCountry kPfmCountries[] = {
    {"US", "USA", "United States"},  {"CN", "CHN", "China"},          {"JP", "JPN", "Japan"},
    {"DE", "DEU", "Germany"},        {"IN", "IND", "India"},          {"GB", "GBR", "United Kingdom"},
    {"FR", "FRA", "France"},         {"IT", "ITA", "Italy"},          {"BR", "BRA", "Brazil"},
    {"CA", "CAN", "Canada"},         {"RU", "RUS", "Russia"},         {"KR", "KOR", "South Korea"},
    {"AU", "AUS", "Australia"},      {"ES", "ESP", "Spain"},          {"MX", "MEX", "Mexico"},
    {"ID", "IDN", "Indonesia"},      {"NL", "NLD", "Netherlands"},    {"SA", "SAU", "Saudi Arabia"},
    {"TR", "TUR", "Turkey"},         {"CH", "CHE", "Switzerland"},    {"TW", "TWN", "Taiwan"},
    {"PL", "POL", "Poland"},         {"SE", "SWE", "Sweden"},         {"BE", "BEL", "Belgium"},
    {"AR", "ARG", "Argentina"},      {"NO", "NOR", "Norway"},         {"AT", "AUT", "Austria"},
    {"IE", "IRL", "Ireland"},        {"IL", "ISR", "Israel"},         {"AE", "ARE", "United Arab Emirates"},
    {"TH", "THA", "Thailand"},       {"SG", "SGP", "Singapore"},      {"MY", "MYS", "Malaysia"},
    {"PH", "PHL", "Philippines"},    {"VN", "VNM", "Vietnam"},        {"BD", "BGD", "Bangladesh"},
    {"PK", "PAK", "Pakistan"},       {"EG", "EGY", "Egypt"},          {"NG", "NGA", "Nigeria"},
    {"ZA", "ZAF", "South Africa"},   {"KE", "KEN", "Kenya"},          {"CL", "CHL", "Chile"},
    {"CO", "COL", "Colombia"},       {"PE", "PER", "Peru"},           {"NZ", "NZL", "New Zealand"},
    {"DK", "DNK", "Denmark"},        {"FI", "FIN", "Finland"},        {"PT", "PRT", "Portugal"},
    {"GR", "GRC", "Greece"},         {"CZ", "CZE", "Czechia"},        {"HU", "HUN", "Hungary"},
    {"RO", "ROU", "Romania"},        {"UA", "UKR", "Ukraine"},        {"HK", "HKG", "Hong Kong"},
    {"QA", "QAT", "Qatar"},          {"KW", "KWT", "Kuwait"},         {"MA", "MAR", "Morocco"},
    {"ET", "ETH", "Ethiopia"},       {"GH", "GHA", "Ghana"},          {"LK", "LKA", "Sri Lanka"},
};

enum class PfmCountryFormat { Iso2, Iso3, Name, Snake };

/// The code system a country param uses, from its default and help (ISO3 when nothing says).
PfmCountryFormat pfm_country_format(const ConnectorParam& p) {
    static const QRegularExpression iso2(QStringLiteral("^[A-Z]{2}$"));
    static const QRegularExpression iso3(QStringLiteral("^[A-Z]{3}$"));
    const QString d = p.default_value.trimmed();
    const QString h = p.help.toLower();
    if (iso2.match(d).hasMatch() || h.contains(QLatin1String("iso2")) || h.contains(QLatin1String("2-letter")))
        return PfmCountryFormat::Iso2;
    if (iso3.match(d).hasMatch() || h.contains(QLatin1String("iso3")) || h.contains(QLatin1String("3-letter")))
        return PfmCountryFormat::Iso3;
    if (d.contains(QLatin1Char('_')))
        return PfmCountryFormat::Snake;
    if (!d.isEmpty() && d.contains(QRegularExpression(QStringLiteral("[a-z ]"))))
        return PfmCountryFormat::Name;
    return PfmCountryFormat::Iso3;
}

QString pfm_country_code(const PfmCountry& c, PfmCountryFormat f) {
    switch (f) {
        case PfmCountryFormat::Iso2:
            return QString::fromLatin1(c.iso2);
        case PfmCountryFormat::Name:
            return QString::fromUtf8(c.name);
        case PfmCountryFormat::Snake:
            return QString::fromUtf8(c.name).toLower().replace(QLatin1Char(' '), QLatin1Char('_'));
        case PfmCountryFormat::Iso3:
            break;
    }
    return QString::fromLatin1(c.iso3);
}

/// The symbol to send for a picked instrument: its Yahoo market-data symbol, except that an index
/// symbol loses its "^" when the param's example (help "e.g. SPX") is written without one.
QString pfm_symbol_for(const fincept::core::instrument::Instrument& inst, const ConnectorParam& p) {
    QString s = inst.symbol.isEmpty() ? inst.display : inst.symbol;
    if (s.startsWith(QLatin1Char('^'))) {
        static const QRegularExpression ex(QStringLiteral("e\\.g\\.\\s*([^\\s,;)]+)"));
        const QRegularExpressionMatch m = ex.match(p.help);
        if (m.hasMatch() && !m.captured(1).startsWith(QLatin1Char('^')))
            s = s.mid(1);
    }
    return s;
}

/// "Data files (*.csv *.parquet …)" from the extensions a path param's help lists.
QString pfm_file_filter(const QString& help) {
    static const QRegularExpression ext(QStringLiteral("\\.([A-Za-z0-9]{1,8})\\b"));
    QStringList pats;
    auto it = ext.globalMatch(help);
    while (it.hasNext()) {
        const QString e = QStringLiteral("*.") + it.next().captured(1).toLower();
        if (!pats.contains(e))
            pats << e;
    }
    const QString all = QObject::tr("All files (*)");
    return pats.isEmpty() ? all : QObject::tr("Data files (%1)").arg(pats.join(QLatin1Char(' '))) + QStringLiteral(";;") + all;
}

bool pfm_is_path_param(const ConnectorParam& p) {
    const QString n = p.name.toLower();
    return p.kind == ParamKind::String &&
           (n == QLatin1String("path") || n == QLatin1String("file") || n == QLatin1String("filepath") ||
            n == QLatin1String("file_path") || p.help.contains(QLatin1String("path of"), Qt::CaseInsensitive));
}

/// "1-100000" in a help text → the range.
bool pfm_help_range(const QString& help, qint64* lo, qint64* hi) {
    static const QRegularExpression re(QStringLiteral("(-?\\d+)\\s*(?:-|–|to)\\s*(\\d+)"));
    const QRegularExpressionMatch m = re.match(help);
    if (!m.hasMatch())
        return false;
    *lo = m.captured(1).toLongLong();
    *hi = m.captured(2).toLongLong();
    return *lo < *hi;
}

} // namespace

ConnectorParamForm::ConnectorParamForm(QWidget* parent) : QWidget(parent) {
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);
}

void ConnectorParamForm::build(const QString& connector, const QString& command, const QVector<ConnectorParam>& params,
                               bool has_sheets_command) {
    if (memory_)
        delete memory_; // flushes an unsaved edit of the previous command
    connector_ = connector;
    command_ = command;
    params_ = params;
    controls_.clear();
    sheet_index_ = -1;
    path_index_ = -1;
    sheets_request_.clear();
    if (body_) {
        body_->hide();
        body_->deleteLater();
    }
    body_ = new QWidget(this);
    grid_ = new QGridLayout(body_);
    grid_->setContentsMargins(0, 0, 0, 0);
    grid_->setHorizontalSpacing(8);
    grid_->setVerticalSpacing(6);
    grid_->setColumnStretch(1, 1);
    grid_->setColumnStretch(3, 1);
    for (int i = 0; i < params_.size(); ++i) {
        const ConnectorParam& p = params_[i];
        if (pfm_is_path_param(p) && path_index_ < 0)
            path_index_ = i;
        if (has_sheets_command && p.name == QLatin1String("sheet"))
            sheet_index_ = i;
    }
    // Long inputs (paths, symbol lists, free text) get a whole row; the rest two per row.
    int row = 0;
    int col = 0;
    for (int i = 0; i < params_.size(); ++i) {
        const ConnectorParam& p = params_[i];
        QString label = p.name;
        if (p.required)
            label += QStringLiteral(" *");
        auto* lbl = new QLabel(amp_safe(label), body_);
        set_look(lbl, "field");
        QWidget* cell = i == path_index_ ? make_path_control(p, i) : make_control(p, i);
        QString tip = p.help;
        const QString kind = conn::param_kind_name(p.kind);
        tip += (tip.isEmpty() ? QString() : QStringLiteral("\n")) +
               (p.required ? tr("Required · %1").arg(kind)
                           : p.has_default && !p.default_value.isEmpty() ? tr("Optional · %1 · default %2").arg(kind, p.default_value)
                                                                         : tr("Optional · %1").arg(kind));
        if (p.named)
            tip += tr(" · sent as --%1=…").arg(p.name);
        lbl->setToolTip(tip);
        controls_[i]->setToolTip(tip);
        lbl->setBuddy(controls_[i]);
        const bool wide = i == path_index_ || p.is_list() || p.variadic || p.kind == ParamKind::String;
        if (wide && col != 0) {
            ++row;
            col = 0;
        }
        grid_->addWidget(lbl, row, col);
        grid_->addWidget(cell, row, col + 1, 1, wide ? 3 : 1);
        if (wide || col == 2) {
            ++row;
            col = 0;
        } else {
            col = 2;
        }
    }
    layout()->addWidget(body_);
}

QWidget* ConnectorParamForm::make_control(const ConnectorParam& p, int index) {
    QWidget* w = nullptr;
    const bool real_default = p.has_default && !p.default_value.isEmpty();
    switch (p.kind) {
        case ParamKind::Bool: {
            auto* c = new QCheckBox(tr("yes"), body_);
            c->setChecked(p.default_value == QLatin1String("true"));
            w = c;
            break;
        }
        case ParamKind::Enum: {
            auto* c = new QComboBox(body_);
            if (!real_default)
                c->addItem(p.required ? tr("(choose)") : tr("(not set)"), QString());
            for (const QString& ch : p.choices)
                c->addItem(ch, ch);
            c->setCurrentIndex(std::max(0, c->findData(p.default_value)));
            w = c;
            break;
        }
        case ParamKind::Date: {
            auto* row = new QWidget(body_);
            auto* hl = new QHBoxLayout(row);
            hl->setContentsMargins(0, 0, 0, 0);
            hl->setSpacing(4);
            auto* d = new QDateEdit(row);
            d->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
            d->setCalendarPopup(true);
            d->setDateRange(kPfmUnsetDate, QDate(2200, 12, 31));
            d->setSpecialValueText(p.required ? tr("(choose a date)") : tr("(not set)"));
            const QDate def = QDate::fromString(p.default_value, QStringLiteral("yyyy-MM-dd"));
            d->setDate(def.isValid() ? def : kPfmUnsetDate);
            d->calendarWidget()->installEventFilter(this);
            d->calendarWidget()->setProperty("pfmDateEdit", QVariant::fromValue<QObject*>(d));
            hl->addWidget(d, 1);
            if (!p.required) {
                auto* clear = new QToolButton(row);
                set_look(clear, "button");
                clear->setText(tr("CLEAR"));
                clear->setToolTip(tr("Leave this date out"));
                connect(clear, &QToolButton::clicked, d, [d]() { d->setDate(kPfmUnsetDate); });
                hl->addWidget(clear);
            }
            controls_.append(d);
            return row;
        }
        case ParamKind::Int: {
            bool ok = false;
            const qint64 def = p.default_value.toLongLong(&ok);
            if (real_default && ok) {
                auto* s = new QSpinBox(body_);
                qint64 lo = -999999999;
                qint64 hi = 999999999;
                pfm_help_range(p.help, &lo, &hi);
                s->setRange(int(std::clamp<qint64>(std::min(lo, def), INT_MIN + 1, INT_MAX)),
                            int(std::clamp<qint64>(std::max(hi, def), INT_MIN + 1, INT_MAX)));
                s->setValue(int(def));
                s->setAccelerated(true);
                w = s;
            } else {
                auto* e = new QLineEdit(body_);
                e->setValidator(new QIntValidator(e));
                e->setPlaceholderText(p.required ? tr("whole number") : tr("(not set)"));
                connect(e, &QLineEdit::returnPressed, this, &ConnectorParamForm::submitted);
                w = e;
            }
            break;
        }
        case ParamKind::Float: {
            bool ok = false;
            const double def = p.default_value.toDouble(&ok);
            if (real_default && ok) {
                auto* s = new QDoubleSpinBox(body_);
                const int dot = int(p.default_value.indexOf(QLatin1Char('.')));
                const int decimals = dot < 0 ? 2 : std::clamp(int(p.default_value.size()) - dot - 1, 2, 8);
                s->setDecimals(decimals);
                s->setRange(-1e12, 1e12);
                s->setValue(def);
                s->setAccelerated(true);
                w = s;
            } else {
                auto* e = new QLineEdit(body_);
                auto* v = new QDoubleValidator(e);
                v->setLocale(QLocale::c());
                v->setNotation(QDoubleValidator::StandardNotation);
                e->setValidator(v);
                e->setPlaceholderText(p.required ? tr("number, e.g. 2.5") : tr("(not set)"));
                connect(e, &QLineEdit::returnPressed, this, &ConnectorParamForm::submitted);
                w = e;
            }
            break;
        }
        case ParamKind::Symbol: {
            auto* box = new fincept::ui::SymbolSearchBox(body_);
            box->set_clear_on_pick(false);
            box->set_raw_text_allowed(true);
            box->set_compact(true);
            box->set_multi(p.is_list() || p.variadic);
            box->set_recent_key(QStringLiteral("data_sources"));
            box->set_placeholder(p.help.isEmpty() ? tr("ticker, name or ISIN") : p.help);
            if (real_default)
                box->set_text(p.default_value);
            const ConnectorParam param = p;
            QPointer<fincept::ui::SymbolSearchBox> guard(box);
            connect(box, &fincept::ui::SymbolSearchBox::instrument_picked, this,
                    [guard, param](const fincept::core::instrument::Instrument& inst) {
                        if (guard && !(param.is_list() || param.variadic))
                            guard->set_text(pfm_symbol_for(inst, param));
                    });
            connect(box, &fincept::ui::SymbolSearchBox::instruments_picked, this,
                    [guard, param](const QVector<fincept::core::instrument::Instrument>& list) {
                        if (!guard)
                            return;
                        QStringList syms;
                        for (const auto& inst : list)
                            syms << pfm_symbol_for(inst, param);
                        guard->set_text(syms.join(QLatin1Char(',')));
                    });
            w = box;
            break;
        }
        case ParamKind::Country: {
            if (p.is_list() || p.variadic) {
                auto* e = new QLineEdit(body_);
                e->setPlaceholderText(p.help.isEmpty() ? tr("codes separated by commas, e.g. USA,CHN") : p.help);
                e->setText(p.default_value);
                connect(e, &QLineEdit::returnPressed, this, &ConnectorParamForm::submitted);
                w = e;
                break;
            }
            auto* c = new QComboBox(body_);
            c->setEditable(true);
            c->setInsertPolicy(QComboBox::NoInsert);
            const PfmCountryFormat fmt = pfm_country_format(p);
            for (const PfmCountry& k : kPfmCountries) {
                const QString code = pfm_country_code(k, fmt);
                c->addItem(code == QString::fromUtf8(k.name) ? code : code + QLatin1String(kPfmCountrySep) + QString::fromUtf8(k.name),
                           code);
            }
            c->completer()->setFilterMode(Qt::MatchContains);
            c->completer()->setCompletionMode(QCompleter::PopupCompletion);
            c->setEditText(p.default_value);
            c->lineEdit()->setPlaceholderText(p.help.isEmpty() ? tr("country") : p.help);
            // Picking "USA — United States" puts the code alone in the box.
            QPointer<QComboBox> guard(c);
            auto to_code = [guard](const QString&) {
                if (!guard)
                    return;
                const QString t = guard->currentText();
                const int cut = int(t.indexOf(QLatin1String(kPfmCountrySep)));
                if (cut > 0)
                    guard->setEditText(t.left(cut));
            };
            connect(c, &QComboBox::textActivated, this, to_code);
            connect(c->completer(), qOverload<const QString&>(&QCompleter::activated), this, to_code);
            connect(c->lineEdit(), &QLineEdit::returnPressed, this, &ConnectorParamForm::submitted);
            w = c;
            break;
        }
        case ParamKind::String: {
            if (index == sheet_index_) {
                auto* c = new QComboBox(body_);
                c->setEditable(true);
                c->setInsertPolicy(QComboBox::NoInsert);
                c->setEditText(p.default_value);
                c->lineEdit()->setPlaceholderText(tr("first sheet"));
                w = c;
                break;
            }
            auto* e = new QLineEdit(body_);
            e->setPlaceholderText(p.help.isEmpty() ? (p.required ? tr("required") : tr("(not set)")) : p.help);
            e->setText(p.default_value);
            e->setClearButtonEnabled(true);
            connect(e, &QLineEdit::returnPressed, this, &ConnectorParamForm::submitted);
            w = e;
            break;
        }
    }
    controls_.append(w);
    return w;
}

QWidget* ConnectorParamForm::make_path_control(const ConnectorParam& p, int index) {
    Q_UNUSED(index);
    auto* row = new QWidget(body_);
    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(6);
    auto* e = new QLineEdit(row);
    e->setPlaceholderText(p.help.isEmpty() ? tr("path of a file on this computer") : p.help);
    e->setText(p.default_value);
    e->setClearButtonEnabled(true);
    hl->addWidget(e, 1);
    auto* browse = new QPushButton(tr("BROWSE…"), row);
    hl->addWidget(browse);
    const QString filter = pfm_file_filter(p.help);
    QPointer<QLineEdit> guard(e);
    connect(browse, &QPushButton::clicked, this, [this, guard, filter]() {
        if (!guard)
            return;
        const QString cur = guard->text().trimmed();
        QString start = cur.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                                      : QFileInfo(cur).absolutePath();
        const QString picked = QFileDialog::getOpenFileName(this, tr("Choose a data file"), start, filter);
        if (picked.isEmpty())
            return;
        guard->setText(QDir::toNativeSeparators(picked));
        refresh_sheets(guard->text());
    });
    connect(e, &QLineEdit::editingFinished, this, [this, guard]() {
        if (guard)
            refresh_sheets(guard->text());
    });
    connect(e, &QLineEdit::returnPressed, this, &ConnectorParamForm::submitted);
    controls_.append(e);
    return row;
}

void ConnectorParamForm::refresh_sheets(const QString& path) {
    if (sheet_index_ < 0 || sheet_index_ >= controls_.size())
        return;
    auto* combo = qobject_cast<QComboBox*>(controls_[sheet_index_]);
    const QString p = path.trimmed();
    if (!combo || !(p.endsWith(QLatin1String(".xlsx"), Qt::CaseInsensitive) || p.endsWith(QLatin1String(".xlsm"), Qt::CaseInsensitive) ||
                    p.endsWith(QLatin1String(".xls"), Qt::CaseInsensitive))) {
        if (combo)
            combo->setEnabled(!p.isEmpty() ? false : true);
        return;
    }
    combo->setEnabled(true);
    if (!sheets_request_.isEmpty())
        conn::ConnectorService::instance().cancel(sheets_request_);
    QPointer<QComboBox> guard(combo);
    QPointer<ConnectorParamForm> self(this);
    sheets_request_ = conn::ConnectorService::instance().run_with(
        connector_, QStringLiteral("sheets"), QStringList{p}, conn::ConnectorService::RunOptions{},
        [self, guard](const conn::ConnectorResult& r) {
            if (!self || !guard)
                return;
            self->sheets_request_.clear();
            if (!r.success || r.tabular.tables.isEmpty())
                return;
            const auto& t = r.tabular.tables.first();
            int col = t.column_index(QStringLiteral("sheet"));
            if (col < 0)
                col = 0;
            const QString keep = guard->currentText();
            guard->clear();
            for (int i = 0; i < t.row_count(); ++i)
                guard->addItem(fincept::services::datasources::cell_text(t.cell(i, col)));
            guard->setEditText(keep);
        });
}

QString ConnectorParamForm::value_of(int index) const {
    QWidget* w = controls_.value(index);
    if (auto* c = qobject_cast<QCheckBox*>(w))
        return c->isChecked() ? QStringLiteral("true") : QStringLiteral("false");
    if (auto* d = qobject_cast<QDateEdit*>(w))
        return d->date() == kPfmUnsetDate ? QString() : conn::date_arg(d->date());
    if (auto* s = qobject_cast<QSpinBox*>(w))
        return QString::number(s->value());
    if (auto* s = qobject_cast<QDoubleSpinBox*>(w))
        return conn::float_arg(s->value());
    if (auto* b = qobject_cast<fincept::ui::SymbolSearchBox*>(w))
        return b->text();
    if (auto* c = qobject_cast<QComboBox*>(w)) {
        if (!c->isEditable())
            return c->currentData().toString();
        const QString t = c->currentText();
        const int cut = int(t.indexOf(QLatin1String(kPfmCountrySep)));
        return cut > 0 ? t.left(cut) : t;
    }
    if (auto* e = qobject_cast<QLineEdit*>(w))
        return e->text();
    return QString();
}

QVector<ParamInput> ConnectorParamForm::inputs() const {
    QVector<ParamInput> out;
    out.reserve(params_.size());
    for (int i = 0; i < params_.size(); ++i)
        out << ParamInput::of(value_of(i));
    return out;
}

void ConnectorParamForm::set_inputs(const QVector<ParamInput>& inputs) {
    for (int i = 0; i < params_.size() && i < controls_.size(); ++i) {
        const ConnectorParam& p = params_[i];
        const ParamInput in = inputs.value(i);
        const QString v = in.given ? in.value.trimmed() : (p.has_default ? p.default_value : QString());
        QWidget* w = controls_[i];
        if (auto* c = qobject_cast<QCheckBox*>(w)) {
            const QString l = v.toLower();
            c->setChecked(l == QLatin1String("true") || l == QLatin1String("1") || l == QLatin1String("yes"));
        } else if (auto* d = qobject_cast<QDateEdit*>(w)) {
            const QDate date = QDate::fromString(v, QStringLiteral("yyyy-MM-dd"));
            d->setDate(date.isValid() ? date : kPfmUnsetDate);
        } else if (auto* s = qobject_cast<QSpinBox*>(w)) {
            bool ok = false;
            const int n = v.toInt(&ok);
            if (ok)
                s->setValue(n);
        } else if (auto* s2 = qobject_cast<QDoubleSpinBox*>(w)) {
            bool ok = false;
            const double n = v.toDouble(&ok);
            if (ok)
                s2->setValue(n);
        } else if (auto* b = qobject_cast<fincept::ui::SymbolSearchBox*>(w)) {
            b->set_text(v);
        } else if (auto* c2 = qobject_cast<QComboBox*>(w)) {
            if (c2->isEditable())
                c2->setEditText(v);
            else
                c2->setCurrentIndex(std::max(0, c2->findData(v)));
        } else if (auto* e = qobject_cast<QLineEdit*>(w)) {
            e->setText(v);
        }
    }
    if (path_index_ >= 0)
        refresh_sheets(value_of(path_index_));
}

bool ConnectorParamForm::attach_memory() {
    // Tracking starts only now: InputMemory counts every later change as an edit, so the defaults
    // and the sample put in before this call are never stored as "the user's inputs".
    if (memory_)
        delete memory_;
    memory_ = new fincept::ui::InputMemory(QStringLiteral("data_sources"),
                                           QStringLiteral("catalog/%1/%2").arg(connector_, command_), this);
    for (int i = 0; i < params_.size() && i < controls_.size(); ++i)
        memory_->track(params_[i].name, controls_[i]);
    const bool applied = memory_->restore();
    if (applied && path_index_ >= 0)
        refresh_sheets(value_of(path_index_));
    return applied;
}

void ConnectorParamForm::focus_param(int index) {
    if (QWidget* w = controls_.value(index)) {
        if (auto* b = qobject_cast<fincept::ui::SymbolSearchBox*>(w))
            b->focus_and_select();
        else
            w->setFocus(Qt::OtherFocusReason);
    }
}

bool ConnectorParamForm::eventFilter(QObject* watched, QEvent* event) {
    // An unset date opens its calendar on today, not on the "(not set)" placeholder date.
    if (event->type() == QEvent::Show) {
        if (auto* cal = qobject_cast<QCalendarWidget*>(watched)) {
            auto* d = qobject_cast<QDateEdit*>(cal->property("pfmDateEdit").value<QObject*>());
            if (d && d->date() == kPfmUnsetDate)
                cal->setSelectedDate(QDate::currentDate());
        }
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace fincept::screens::datasources
