#include "screens/ma_analytics/MaDealGrid.h"

#include "core/logging/Logger.h"
#include "services/ma_analytics/MAAnalyticsService.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/DataGridEditor.h"

#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <cmath>

namespace fincept::screens {

namespace {

using fincept::ui::DataGridColumn;

const QString kMaGridSample = QStringLiteral("Sample (illustrative)");

QVector<DataGridColumn> ma_grid_columns(MaDealGrid::Kind kind) {
    auto pct = [](const QString& key, const QString& title) {
        auto c = DataGridColumn::number(key, title, 1, false);
        c.tooltip = QCoreApplication::translate("MaDealGrid", "Percent: 45.3 means 45.3%");
        return c;
    };
    auto mult = [](const QString& key, const QString& title) { return DataGridColumn::number(key, title, 2, false); };
    const auto source = DataGridColumn::text(QStringLiteral("source"),
                                             QCoreApplication::translate("MaDealGrid", "SOURCE"), false);
    switch (kind) {
        case MaDealGrid::Kind::Comparison: {
            auto value = DataGridColumn::number(QStringLiteral("deal_value"),
                                                QCoreApplication::translate("MaDealGrid", "VALUE (MILLIONS)"), 0);
            value.tooltip = QCoreApplication::translate("MaDealGrid", "Deal value in millions (68700 = 68.7 billion)");
            return {DataGridColumn::text(QStringLiteral("acquirer"), QCoreApplication::translate("MaDealGrid", "ACQUIRER"), true),
                    DataGridColumn::text(QStringLiteral("target"), QCoreApplication::translate("MaDealGrid", "TARGET"), true),
                    value,
                    pct(QStringLiteral("premium"), QCoreApplication::translate("MaDealGrid", "PREMIUM %")),
                    mult(QStringLiteral("ev_revenue"), QCoreApplication::translate("MaDealGrid", "EV/REV")),
                    mult(QStringLiteral("ev_ebitda"), QCoreApplication::translate("MaDealGrid", "EV/EBITDA")),
                    pct(QStringLiteral("cash_pct"), QCoreApplication::translate("MaDealGrid", "CASH %")),
                    pct(QStringLiteral("stock_pct"), QCoreApplication::translate("MaDealGrid", "STOCK %")),
                    DataGridColumn::text(QStringLiteral("industry"), QCoreApplication::translate("MaDealGrid", "INDUSTRY"), false),
                    source};
        }
        case MaDealGrid::Kind::Precedent: {
            // Amount columns read like every money field (the user's separators, K / M / B, symbols).
            // No fixed currency: only ratios of one deal's own figures are used. The Comparison grid's
            // "VALUE (MILLIONS)" stays a plain number — a K / M / B suffix would be read in full units.
            auto value = DataGridColumn::amount(QStringLiteral("deal_value"),
                                                QCoreApplication::translate("MaDealGrid", "DEAL VALUE"), QString(), 0);
            value.tooltip = QCoreApplication::translate("MaDealGrid", "Enterprise / deal value in full units "
                                                                      "(68.7B or 68,700,000,000)");
            auto rev = DataGridColumn::amount(QStringLiteral("revenue"),
                                              QCoreApplication::translate("MaDealGrid", "TARGET REVENUE"), QString(), 0,
                                              false);
            auto ebitda = DataGridColumn::amount(QStringLiteral("ebitda"),
                                                 QCoreApplication::translate("MaDealGrid", "TARGET EBITDA"), QString(), 0,
                                                 false);
            rev.tooltip = ebitda.tooltip = QCoreApplication::translate(
                "MaDealGrid", "Used to derive the multiple when EV/Revenue or EV/EBITDA is left empty");
            return {DataGridColumn::date(QStringLiteral("announcement_date"),
                                         QCoreApplication::translate("MaDealGrid", "ANNOUNCED"), false),
                    DataGridColumn::text(QStringLiteral("acquirer"), QCoreApplication::translate("MaDealGrid", "ACQUIRER"), true),
                    DataGridColumn::text(QStringLiteral("target"), QCoreApplication::translate("MaDealGrid", "TARGET"), true),
                    value,
                    mult(QStringLiteral("ev_revenue"), QCoreApplication::translate("MaDealGrid", "EV/REV")),
                    mult(QStringLiteral("ev_ebitda"), QCoreApplication::translate("MaDealGrid", "EV/EBITDA")),
                    rev,
                    ebitda,
                    pct(QStringLiteral("premium"), QCoreApplication::translate("MaDealGrid", "PREMIUM %")),
                    source};
        }
        case MaDealGrid::Kind::Regression: {
            auto g = pct(QStringLiteral("growth"), QCoreApplication::translate("MaDealGrid", "GROWTH %"));
            g.required = true;
            return {DataGridColumn::text(QStringLiteral("name"), QCoreApplication::translate("MaDealGrid", "COMPANY"), true),
                    DataGridColumn::amount(QStringLiteral("ev"), QCoreApplication::translate("MaDealGrid", "EV"), QString(), 0),
                    DataGridColumn::amount(QStringLiteral("revenue"), QCoreApplication::translate("MaDealGrid", "REVENUE"),
                                           QString(), 0),
                    DataGridColumn::amount(QStringLiteral("ebitda"), QCoreApplication::translate("MaDealGrid", "EBITDA"),
                                           QString(), 0),
                    g,
                    source};
        }
    }
    return {};
}

QJsonArray ma_grid_sample(MaDealGrid::Kind kind) {
    // Well-known deals / round-number peers with illustrative, rounded figures; the SOURCE column says so.
    auto row = [](std::initializer_list<QPair<const char*, QJsonValue>> kv) {
        QJsonObject o;
        for (const auto& p : kv)
            o.insert(QString::fromLatin1(p.first), p.second);
        o.insert(QStringLiteral("source"), kMaGridSample);
        return o;
    };
    switch (kind) {
        case MaDealGrid::Kind::Comparison:
            return {row({{"acquirer", "Microsoft"}, {"target", "Activision Blizzard"}, {"deal_value", 68700}, {"premium", 45.3},
                         {"ev_revenue", 8.7}, {"ev_ebitda", 23.1}, {"cash_pct", 100}, {"stock_pct", 0}, {"industry", "Technology"}}),
                    row({{"acquirer", "Broadcom"}, {"target", "VMware"}, {"deal_value", 69000}, {"premium", 44.0},
                         {"ev_revenue", 5.3}, {"ev_ebitda", 16.8}, {"cash_pct", 50}, {"stock_pct", 50}, {"industry", "Technology"}}),
                    row({{"acquirer", "Cisco"}, {"target", "Splunk"}, {"deal_value", 28000}, {"premium", 31.0},
                         {"ev_revenue", 7.2}, {"cash_pct", 100}, {"stock_pct", 0}, {"industry", "Technology"}}),
                    row({{"acquirer", "Pfizer"}, {"target", "Seagen"}, {"deal_value", 43000}, {"premium", 32.7},
                         {"ev_revenue", 19.5}, {"cash_pct", 100}, {"stock_pct", 0}, {"industry", "Healthcare"}}),
                    row({{"acquirer", "Amgen"}, {"target", "Horizon Therapeutics"}, {"deal_value", 27800}, {"premium", 47.9},
                         {"ev_revenue", 7.7}, {"ev_ebitda", 17.4}, {"cash_pct", 100}, {"stock_pct", 0}, {"industry", "Healthcare"}}),
                    row({{"acquirer", "Exxon Mobil"}, {"target", "Pioneer Natural Resources"}, {"deal_value", 64500},
                         {"premium", 18.0}, {"ev_revenue", 2.7}, {"ev_ebitda", 5.6}, {"cash_pct", 0}, {"stock_pct", 100},
                         {"industry", "Energy"}})};
        case MaDealGrid::Kind::Precedent:
            return {row({{"acquirer", "Microsoft"}, {"target", "Activision Blizzard"}, {"deal_value", 68.7e9},
                         {"revenue", 7.9e9}, {"ebitda", 2.97e9}, {"premium", 45.3}}),
                    row({{"acquirer", "Broadcom"}, {"target", "VMware"}, {"deal_value", 69.0e9}, {"revenue", 13.0e9},
                         {"ebitda", 4.1e9}, {"premium", 44.0}}),
                    row({{"acquirer", "Amgen"}, {"target", "Horizon Therapeutics"}, {"deal_value", 27.8e9},
                         {"revenue", 3.6e9}, {"ebitda", 1.6e9}, {"premium", 47.9}}),
                    row({{"acquirer", "Exxon Mobil"}, {"target", "Pioneer Natural Resources"}, {"deal_value", 64.5e9},
                         {"revenue", 24.0e9}, {"ebitda", 11.5e9}, {"premium", 18.0}}),
                    row({{"acquirer", "Cisco"}, {"target", "Splunk"}, {"deal_value", 28.0e9}, {"revenue", 3.9e9},
                         {"ebitda", 0.6e9}, {"premium", 31.0}})};
        case MaDealGrid::Kind::Regression:
            return {row({{"name", "Sample peer 1"}, {"ev", 5.2e9}, {"revenue", 1.1e9}, {"ebitda", 2.4e8}, {"growth", 12}}),
                    row({{"name", "Sample peer 2"}, {"ev", 8.9e9}, {"revenue", 1.6e9}, {"ebitda", 3.9e8}, {"growth", 18}}),
                    row({{"name", "Sample peer 3"}, {"ev", 3.1e9}, {"revenue", 0.9e9}, {"ebitda", 1.6e8}, {"growth", 6}}),
                    row({{"name", "Sample peer 4"}, {"ev", 12.4e9}, {"revenue", 2.2e9}, {"ebitda", 5.5e8}, {"growth", 21}}),
                    row({{"name", "Sample peer 5"}, {"ev", 6.0e9}, {"revenue", 1.4e9}, {"ebitda", 2.9e8}, {"growth", 9}}),
                    row({{"name", "Sample peer 6"}, {"ev", 9.8e9}, {"revenue", 2.0e9}, {"ebitda", 4.4e8}, {"growth", 14}})};
    }
    return {};
}

double ma_grid_num(const QJsonObject& o, const char* key) {
    const QJsonValue v = o.value(QLatin1String(key));
    if (v.isDouble())
        return v.toDouble();
    if (v.isString()) {
        bool ok = false;
        const double d = v.toString().toDouble(&ok);
        if (ok)
            return d;
    }
    return std::nan("");
}

} // namespace

MaDealGrid::MaDealGrid(Kind kind, const QString& memory_name, QWidget* parent)
    : QWidget(parent), kind_(kind), memory_name_(memory_name) {
    setObjectName(QStringLiteral("maDealGrid"));
    setStyleSheet(QString("QLabel#maGridNote { color:%1; font-size:%2px; background:transparent; }"
                          "QPushButton#maGridBtn { background:%3; color:%4; border:1px solid %5; padding:3px 10px;"
                          " font-size:%6px; font-weight:600; }"
                          "QPushButton#maGridBtn:hover { color:%7; border-color:%8; }"
                          "QPushButton#maGridBtn:disabled { color:%1; }")
                      .arg(ui::colors::TEXT_TERTIARY())
                      .arg(ui::fonts::px(-2))
                      .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED())
                      .arg(ui::fonts::px(-2))
                      .arg(ui::colors::TEXT_PRIMARY(), ui::colors::AMBER()));
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(4);

    auto* bar = new QWidget(this);
    auto* bl = new QHBoxLayout(bar);
    bl->setContentsMargins(0, 0, 0, 0);
    bl->setSpacing(6);
    auto make_btn = [bar](const QString& text, const QString& tip) {
        auto* b = new QPushButton(text, bar);
        b->setObjectName(QStringLiteral("maGridBtn"));
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(tip);
        return b;
    };
    if (kind_ != Kind::Regression) {
        load_verified_btn_ = make_btn(tr("LOAD VERIFIED DEALS"),
                                      tr("Replace the rows with the deals you verified in the Deal Database"));
        load_all_btn_ = make_btn(tr("LOAD ALL DEALS"), tr("Replace the rows with every deal in the Deal Database, "
                                                          "auto-extracted (unverified) ones included"));
        connect(load_verified_btn_, &QPushButton::clicked, this, [this]() { load_from_database(true); });
        connect(load_all_btn_, &QPushButton::clicked, this, [this]() { load_from_database(false); });
        bl->addWidget(load_verified_btn_);
        bl->addWidget(load_all_btn_);
    }
    auto* sample_btn = make_btn(tr("SAMPLE"), tr("Replace the rows with the illustrative sample set"));
    connect(sample_btn, &QPushButton::clicked, this, &MaDealGrid::load_sample);
    auto* json_btn = make_btn(tr("PASTE JSON"), tr("Replace the rows with a JSON array on the clipboard "
                                                   "([{\"acquirer\": ..., \"target\": ..., ...}])"));
    connect(json_btn, &QPushButton::clicked, this, &MaDealGrid::paste_json);
    bl->addWidget(sample_btn);
    bl->addWidget(json_btn);
    bl->addStretch(1);
    vl->addWidget(bar);

    grid_ = new ui::DataGridEditor(ma_grid_columns(kind_), this);
    grid_->set_visible_rows(6);
    grid_->set_max_rows(1000);
    grid_->set_hint(kind_ == Kind::Regression
                        ? tr("Type or paste rows from a spreadsheet (company, EV, revenue, EBITDA, growth %)")
                        : tr("Type or paste rows from a spreadsheet, or load the deals you verified"));
    vl->addWidget(grid_);

    note_ = new QLabel(this);
    note_->setObjectName(QStringLiteral("maGridNote"));
    note_->setWordWrap(true);
    note_->setVisible(false);
    vl->addWidget(note_);

    // The hidden mirror InputMemory saves and restores (named, so it survives layout changes).
    mirror_ = new QPlainTextEdit(this);
    mirror_->setObjectName(memory_name_);
    mirror_->setVisible(false);
    connect(mirror_, &QPlainTextEdit::textChanged, this, &MaDealGrid::apply_mirror);

    grid_->from_json(ma_grid_sample(kind_));
    sync_mirror();
    connect(grid_, &ui::DataGridEditor::changed, this, [this]() {
        sync_mirror();
        emit changed();
    });
    connect(&services::ma::MAAnalyticsService::instance(), &services::ma::MAAnalyticsService::deals_loaded, this,
            &MaDealGrid::on_deals_loaded);
    connect(&services::ma::MAAnalyticsService::instance(), &services::ma::MAAnalyticsService::deal_op_failed, this,
            [this](quint64 id, const QString& message, const QStringList&) {
                if (id != load_request_)
                    return;
                load_request_ = 0;
                if (load_verified_btn_)
                    load_verified_btn_->setEnabled(true);
                if (load_all_btn_)
                    load_all_btn_->setEnabled(true);
                set_note(tr("Could not read the Deal Database: %1").arg(message.left(200)));
            });
}

QJsonArray MaDealGrid::deals() const {
    QJsonArray out;
    for (const QJsonValue& v : grid_->to_json()) {
        QJsonObject o = v.toObject();
        o.remove(QStringLiteral("source"));
        if (kind_ == Kind::Regression && o.contains(QStringLiteral("growth")))
            o.insert(QStringLiteral("growth"), o.value(QStringLiteral("growth")).toDouble() / 100.0);
        out.append(o);
    }
    return out;
}

int MaDealGrid::valid_count() const {
    return grid_->valid_row_count();
}
int MaDealGrid::invalid_count() const {
    return grid_->invalid_row_count();
}
int MaDealGrid::row_count() const {
    return grid_->row_count();
}
QJsonArray MaDealGrid::raw_rows() const {
    return grid_->raw_json();
}

void MaDealGrid::set_rows(const QJsonArray& rows) {
    grid_->from_json(rows);
    sync_mirror();
    emit changed();
}

void MaDealGrid::load_sample() {
    set_rows(ma_grid_sample(kind_));
    set_note(tr("Sample deals loaded: illustrative, rounded figures - replace them with your own."));
}

QStringList MaDealGrid::headers() const {
    return grid_->headers();
}
QVector<QStringList> MaDealGrid::rows_text() const {
    return grid_->rows_text();
}

void MaDealGrid::load_from_database(bool verified_only) {
    load_verified_only_ = verified_only;
    if (load_verified_btn_)
        load_verified_btn_->setEnabled(false);
    if (load_all_btn_)
        load_all_btn_->setEnabled(false);
    set_note(tr("Reading the Deal Database..."));
    load_request_ = services::ma::MAAnalyticsService::instance().load_deals();
}

QJsonObject MaDealGrid::row_from_deal(const QJsonObject& d) const {
    QJsonObject o;
    const bool verified = d.value(QStringLiteral("verified")).toInt() == 1 || d.value(QStringLiteral("verified")).toBool();
    o.insert(QStringLiteral("source"), verified ? tr("Deal Database (verified)") : tr("Deal Database (unverified)"));
    o.insert(QStringLiteral("acquirer"), d.value(QStringLiteral("acquirer_name")).toString());
    o.insert(QStringLiteral("target"), d.value(QStringLiteral("target_name")).toString());
    auto put = [&o](const char* key, double v) {
        if (std::isfinite(v) && v != 0.0)
            o.insert(QString::fromLatin1(key), v);
    };
    const double value = ma_grid_num(d, "deal_value");
    put("ev_revenue", ma_grid_num(d, "ev_revenue"));
    put("ev_ebitda", ma_grid_num(d, "ev_ebitda"));
    put("premium", ma_grid_num(d, "premium_1day"));
    if (kind_ == Kind::Comparison) {
        put("deal_value", value / 1e6);
        const double cash = ma_grid_num(d, "cash_percentage");
        if (std::isfinite(cash)) {
            o.insert(QStringLiteral("cash_pct"), cash);
            o.insert(QStringLiteral("stock_pct"), 100.0 - cash);
        }
        const QString industry = d.value(QStringLiteral("industry")).toString();
        if (!industry.isEmpty())
            o.insert(QStringLiteral("industry"), industry);
    } else {
        put("deal_value", value);
        const QString date = d.value(QStringLiteral("announcement_date")).toString().left(10);
        if (!date.isEmpty())
            o.insert(QStringLiteral("announcement_date"), date);
    }
    return o;
}

void MaDealGrid::on_deals_loaded(quint64 request_id, const QJsonArray& deals) {
    if (request_id != load_request_ || load_request_ == 0)
        return; // another panel's request, or a superseded one
    load_request_ = 0;
    if (load_verified_btn_)
        load_verified_btn_->setEnabled(true);
    if (load_all_btn_)
        load_all_btn_->setEnabled(true);
    QJsonArray rows;
    int skipped_unverified = 0;
    for (const QJsonValue& v : deals) {
        const QJsonObject d = v.toObject();
        const bool verified = d.value(QStringLiteral("verified")).toInt() == 1 || d.value(QStringLiteral("verified")).toBool();
        if (load_verified_only_ && !verified) {
            ++skipped_unverified;
            continue;
        }
        rows.append(row_from_deal(d));
    }
    if (rows.isEmpty()) {
        set_note(load_verified_only_
                     ? tr("No verified deals yet (%1 unverified in the database). Verify deals in Deal Database > "
                          "Deals, or type them here.")
                           .arg(skipped_unverified)
                     : tr("The Deal Database is empty. Scan SEC filings or add deals in Deal Database > Deals."));
        return;
    }
    grid_->from_json(rows);
    sync_mirror();
    emit changed();
    set_note(load_verified_only_ ? tr("%n verified deal(s) loaded from the Deal Database.", nullptr, int(rows.size()))
                                 : tr("%n deal(s) loaded, unverified ones included - check them before relying on "
                                      "the result.", nullptr, int(rows.size())));
    LOG_INFO("MAAnalytics", QStringLiteral("Deal grid loaded from the database rows=%1 verified_only=%2")
                                .arg(rows.size())
                                .arg(load_verified_only_ ? 1 : 0));
}

void MaDealGrid::paste_json() {
    const QString text = QApplication::clipboard() ? QApplication::clipboard()->text().trimmed() : QString();
    QJsonParseError perr{};
    const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isArray()) {
        set_note(tr("The clipboard does not hold a JSON array of deals (%1). Copy cells from a spreadsheet and use "
                    "PASTE instead.")
                     .arg(perr.error != QJsonParseError::NoError ? perr.errorString() : tr("not an array")));
        return;
    }
    QJsonArray rows = doc.array();
    // The old box format: regression growth as a fraction (0.12) -> the grid's percent (12).
    if (kind_ == Kind::Regression) {
        for (int i = 0; i < rows.size(); ++i) {
            QJsonObject o = rows.at(i).toObject();
            const double g = ma_grid_num(o, "growth");
            if (std::isfinite(g) && std::fabs(g) <= 1.5)
                o.insert(QStringLiteral("growth"), g * 100.0);
            rows[i] = o;
        }
    }
    set_rows(rows);
    set_note(tr("%n row(s) pasted from JSON.", nullptr, int(rows.size())));
}

void MaDealGrid::sync_mirror() {
    if (syncing_)
        return;
    syncing_ = true;
    mirror_->setPlainText(QString::fromUtf8(QJsonDocument(grid_->raw_json()).toJson(QJsonDocument::Compact)));
    syncing_ = false;
}

void MaDealGrid::apply_mirror() {
    // A restore by ui::InputMemory (or a saved case) put rows into the mirror: show them.
    if (syncing_)
        return;
    const QJsonDocument doc = QJsonDocument::fromJson(mirror_->toPlainText().toUtf8());
    if (!doc.isArray())
        return;
    syncing_ = true;
    grid_->from_json(doc.array());
    syncing_ = false;
    emit changed();
}

void MaDealGrid::set_note(const QString& text) {
    note_->setText(text);
    note_->setVisible(!text.isEmpty());
}

} // namespace fincept::screens
