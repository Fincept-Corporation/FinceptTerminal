// src/screens/economics/panels/UnescoPanel.cpp
// UNESCO UIS (Institute for Statistics) — No API key required.
//
// Primary command: fetch <indicator_code> <country_code> [start_year] [end_year]
//   Response: { "success": true,
//               "data": [{"date": "2022", "value": 95.2}],
//               "metadata": {"indicator": "GER.1", "indicator_name": "Gross Enrolment...",
//                            "country": "USA", "source": "UNESCO UIS"} }
//
// list_indicators command:
//   Response: { "success": true, "data": { "records": [{"id": "GER.1", "name": "...",
//               "theme": "EDUCATION"}] } }
//
// education_overview [country_codes...]:
//   Response: { "success": true, "data": { ... complex nested object ... } }
//   — we use fetch for simplicity, education_overview as fallback overview
#include "screens/economics/panels/UnescoPanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QSplitter>
#include <QVBoxLayout>

namespace fincept::screens {
namespace {

static constexpr const char* kUnescoScript = "unesco_data.py";
static constexpr const char* kUnescoSourceId = "unesco";
static constexpr const char* kUnescoColor = "#00ACC1"; // cyan
} // namespace

// Curated preset indicators per theme — used before live list loads
struct UnescoIndicatorDef {
    QString name;
    QString code;
};

static const QList<UnescoIndicatorDef> kEducationIndicators = {
    {"Gross Enrolment Ratio, Primary", "GER.1"},    {"Gross Enrolment Ratio, Secondary", "GER.2"},
    {"Gross Enrolment Ratio, Tertiary", "GER.3"},   {"Net Enrolment Rate, Primary", "NER.1"},
    {"Net Enrolment Rate, Secondary", "NER.2"},     {"Youth Literacy Rate (15-24)", "LR.AG15T24"},
    {"Adult Literacy Rate (15+)", "LR.AG15T99"},    {"Out-of-School Rate, Primary", "ROFST.H.1"},
    {"Out-of-School Rate, Secondary", "ROFST.H.2"}, {"Government Expenditure on Education (% GDP)", "XGDP.FSGOV"},
    {"Pupil-Teacher Ratio, Primary", "PTRHC.1"},    {"Trained Teachers, Primary (%)", "TRTP.1"},
    {"Completion Rate, Primary", "CR.1"},           {"Completion Rate, Secondary", "CR.2"},
};

static const QList<UnescoIndicatorDef> kScienceIndicators = {
    {"R&D Expenditure (% GDP)", "XGDP.GERD"},
    {"Researchers per million inhabitants", "RESEARCHERS.TOTAL.FTE"},
    {"Technicians per million inhabitants", "TECHN.TOTAL.FTE"},
    {"Patent Applications (residents)", "PAT.RESIDENT"},
    {"Patent Applications (non-residents)", "PAT.NONRESIDENT"},
    {"Scientific & Technical Journal Articles", "JA.SCITECHJ"},
    {"High-tech Exports (% manufactured exports)", "HTECH.EXP"},
};

static const QList<UnescoIndicatorDef> kCultureIndicators = {
    {"Cultural Employment (% total employment)", "CEMP.TOTAL"},
    {"Cultural & Creative Industries (% GDP)", "CCIGDP.TOTAL"},
    {"Museum Visitors per 1000 inhabitants", "MUS.VISIT"},
    {"Feature Films Produced", "FF.PROD"},
    {"Books Published (per million inhabitants)", "BOOKS.PUB"},
};

// ── Constructor ───────────────────────────────────────────────────────────────

UnescoPanel::UnescoPanel(QWidget* parent) : EconPanelBase(kUnescoSourceId, kUnescoColor, parent) {

    // Outer layout: left selector | right base UI
    auto* main = new QHBoxLayout(this);
    main->setContentsMargins(0, 0, 0, 0);
    main->setSpacing(0);

    // Left: theme + indicator selector
    auto* left = new QWidget(this);
    left->setFixedWidth(210);
    left->setStyleSheet(sidebar_style());
    auto* lvl = new QVBoxLayout(left);
    lvl->setContentsMargins(0, 0, 0, 0);
    lvl->setSpacing(0);

    // Theme header
    auto* theme_hdr = new QWidget(this);
    theme_hdr->setStyleSheet(section_hdr_style());
    theme_hdr->setFixedHeight(32);
    auto* thl = new QHBoxLayout(theme_hdr);
    thl->setContentsMargins(8, 0, 8, 0);
    theme_lbl_ = new QLabel(tr("THEME"));
    theme_lbl_->setStyleSheet(ctrl_label_style());
    theme_combo_ = new QComboBox;
    theme_combo_->addItem(tr("Education"), "education");
    theme_combo_->addItem(tr("Science & Tech"), "science");
    theme_combo_->addItem(tr("Culture"), "culture");
    theme_combo_->setFixedHeight(22);
    connect(theme_combo_, &QComboBox::currentIndexChanged, this, &UnescoPanel::on_theme_changed);
    thl->addWidget(theme_lbl_);
    thl->addWidget(theme_combo_, 1);
    lvl->addWidget(theme_hdr);

    // Indicator search
    indicator_search_ = new QLineEdit;
    indicator_search_->setPlaceholderText(tr("Filter indicators…"));
    indicator_search_->setStyleSheet(search_input_style());
    indicator_search_->setFixedHeight(28);
    connect(indicator_search_, &QLineEdit::textChanged, this, &UnescoPanel::on_indicator_filter);
    lvl->addWidget(indicator_search_);

    // Indicator list
    indicator_list_ = new QListWidget;
    indicator_list_->setStyleSheet(list_style());
    indicator_list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    lvl->addWidget(indicator_list_, 1);

    main->addWidget(left);

    // Right: base UI (toolbar + stat cards + table)
    auto* right = new QWidget(this);
    right->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    build_base_ui(right);
    main->addWidget(right, 1);

    // Populate initial indicator list
    on_theme_changed(0);

    connect(&services::EconomicsService::instance(), &services::EconomicsService::result_ready, this,
            &UnescoPanel::on_result);
}

void UnescoPanel::activate() {
    show_empty(tr("Select a theme, indicator and country code, then click FETCH\n"
                  "UNESCO UIS data is free — no API key required\n"
                  "Use 3-letter ISO country codes: USA, GBR, IND, CHN, BRA, DEU"));
}

// ── Controls ──────────────────────────────────────────────────────────────────

void UnescoPanel::build_controls(QHBoxLayout* thl) {
    auto make_lbl = [](const QString& text) {
        auto* l = new QLabel(text);
        l->setStyleSheet(ctrl_label_style());
        return l;
    };

    country_input_ = new QLineEdit;
    country_input_->setPlaceholderText(tr("Country (e.g. USA, GBR, IND)"));
    country_input_->setText("USA");
    country_input_->setFixedHeight(26);
    country_input_->setFixedWidth(140);

    start_input_ = new QLineEdit;
    start_input_->setPlaceholderText(tr("Start year"));
    start_input_->setFixedHeight(26);
    start_input_->setFixedWidth(70);

    end_input_ = new QLineEdit;
    end_input_->setPlaceholderText(tr("End year"));
    end_input_->setFixedHeight(26);
    end_input_->setFixedWidth(70);

    thl->addWidget(country_lbl_ = make_lbl(tr("COUNTRY")));
    thl->addWidget(country_input_);
    thl->addWidget(from_lbl_ = make_lbl(tr("FROM")));
    thl->addWidget(start_input_);
    thl->addWidget(to_lbl_ = make_lbl(tr("TO")));
    thl->addWidget(end_input_);
}

void UnescoPanel::on_theme_changed(int index) {
    current_indicators_.clear();
    indicator_list_->clear();
    indicator_search_->clear();

    QList<UnescoIndicatorDef> list;
    if (index == 0)
        list = kEducationIndicators;
    else if (index == 1)
        list = kScienceIndicators;
    else
        list = kCultureIndicators;

    for (const auto& ind : list) {
        current_indicators_.append({ind.name, ind.code});
        auto* item = new QListWidgetItem(ind.name);
        item->setData(Qt::UserRole, ind.code);
        item->setToolTip(ind.code);
        indicator_list_->addItem(item);
    }

    if (indicator_list_->count() > 0)
        indicator_list_->setCurrentRow(0);
}

void UnescoPanel::on_indicator_filter(const QString& text) {
    for (int i = 0; i < indicator_list_->count(); ++i) {
        auto* item = indicator_list_->item(i);
        const bool match = text.isEmpty() || item->text().contains(text, Qt::CaseInsensitive) ||
                           item->data(Qt::UserRole).toString().contains(text, Qt::CaseInsensitive);
        item->setHidden(!match);
    }
}

void UnescoPanel::load_indicators() {
    // Fetch live indicator list from API — fires once per session
    indicators_loaded_ = true;
    services::EconomicsService::instance().execute(kUnescoSourceId, kUnescoScript, "list_indicators", {},
                                                   "unesco_list_indicators");
}

// ── Fetch ─────────────────────────────────────────────────────────────────────

void UnescoPanel::on_fetch() {
    const auto* sel_item = indicator_list_->currentItem();
    if (!sel_item) {
        show_empty(tr("Select an indicator from the list"));
        return;
    }
    const QString indicator_code = sel_item->data(Qt::UserRole).toString();
    const QString country = country_input_->text().trimmed().toUpper();
    const QString start = start_input_->text().trimmed();
    const QString end = end_input_->text().trimmed();

    if (country.isEmpty()) {
        show_empty(tr("Enter a 3-letter country code (e.g. USA, GBR, IND)"));
        return;
    }

    QStringList args = {indicator_code, country};
    if (!start.isEmpty())
        args << start;
    if (!start.isEmpty() && !end.isEmpty())
        args << end;

    show_loading(tr("Fetching UNESCO: %1 for %2…").arg(sel_item->text(), country));
    services::EconomicsService::instance().execute(kUnescoSourceId, kUnescoScript, "fetch", args,
                                                   "unesco_fetch_" + indicator_code + "_" + country);
}

// ── Result ────────────────────────────────────────────────────────────────────

void UnescoPanel::on_result(const QString& request_id, const services::EconomicsResult& result) {
    if (result.source_id != kUnescoSourceId)
        return;

    // Handle live indicator list load
    if (request_id == "unesco_list_indicators") {
        // data: { "records": [{"id": "GER.1", "name": "...", "theme": "EDUCATION"}] }
        const QJsonArray records = result.data["data"].toObject()["records"].toArray();
        if (records.isEmpty())
            return;

        const QString current_theme = theme_combo_->currentData().toString().toUpper();
        indicator_list_->clear();
        current_indicators_.clear();

        for (const auto& v : records) {
            const auto obj = v.toObject();
            const QString theme = obj["theme"].toString();
            if (!current_theme.isEmpty() && !theme.contains(current_theme, Qt::CaseInsensitive))
                continue;
            const QString id = obj["id"].toString();
            const QString name = obj["name"].toString();
            current_indicators_.append({name, id});
            auto* item = new QListWidgetItem(name);
            item->setData(Qt::UserRole, id);
            item->setToolTip(id);
            indicator_list_->addItem(item);
        }
        return;
    }

    if (!request_id.startsWith("unesco_fetch_"))
        return;

    if (!result.success) {
        show_error(result.error);
        return;
    }

    // fetch command response: { "success": true,
    //   "data": [{"date": "2022", "value": 95.2}],
    //   "metadata": {"indicator_name": "Gross Enrolment...", "country": "USA"} }
    QJsonArray rows = result.data["data"].toArray();

    if (rows.isEmpty()) {
        show_empty(tr("No data found for this indicator and country\n"
                      "Try a different country code or check the indicator is available"));
        return;
    }

    // Build title from metadata
    const QJsonObject meta = result.data["metadata"].toObject();
    const QString ind_name = meta["indicator_name"].toString(
        indicator_list_->currentItem() ? indicator_list_->currentItem()->text() : tr("Indicator"));
    const QString country = meta["country"].toString(country_input_->text().toUpper());

    const QString title = "UNESCO: " + ind_name + " — " + country;
    display(rows, title);

    LOG_INFO("UnescoPanel", QString("Displayed %1 data points for %2").arg(rows.size()).arg(request_id));
}

// ── i18n ──────────────────────────────────────────────────────────────────────

void UnescoPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    EconPanelBase::changeEvent(event);
}

void UnescoPanel::retranslateUi() {
    if (theme_lbl_)
        theme_lbl_->setText(tr("THEME"));
    if (theme_combo_ && theme_combo_->count() >= 3) {
        theme_combo_->setItemText(0, tr("Education"));
        theme_combo_->setItemText(1, tr("Science & Tech"));
        theme_combo_->setItemText(2, tr("Culture"));
    }
    if (indicator_search_)
        indicator_search_->setPlaceholderText(tr("Filter indicators…"));
    if (country_lbl_)
        country_lbl_->setText(tr("COUNTRY"));
    if (from_lbl_)
        from_lbl_->setText(tr("FROM"));
    if (to_lbl_)
        to_lbl_->setText(tr("TO"));
    if (country_input_)
        country_input_->setPlaceholderText(tr("Country (e.g. USA, GBR, IND)"));
    if (start_input_)
        start_input_->setPlaceholderText(tr("Start year"));
    if (end_input_)
        end_input_->setPlaceholderText(tr("End year"));
    EconPanelBase::retranslateUi();
}

} // namespace fincept::screens
