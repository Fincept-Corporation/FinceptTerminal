#pragma once
// AltDdTemplates.h — the due-diligence checklist templates of the Alternative Investments tab (A10):
// operational, legal and investment questions an allocator works through before committing, per
// kind of vehicle. Data only (Qt Core), so a test checks that ids are unique and stable: a saved
// checklist stores each item's status and note by item id, never by its text, so wording can be
// improved or translated without losing anyone's answers.
//
// The questions are the common ground of public due-diligence questionnaires (the ILPA DDQ for
// private funds, the AIMA DDQ for hedge funds) written as short checks; they are a working list,
// not legal advice, and the screen says so.

#include <QCoreApplication>
#include <QString>
#include <QVector>

namespace fincept::screens::alt_dd {

struct Item {
    const char* id;
    const char* text; ///< QT_TRANSLATE_NOOP("AltDueDiligence", ...)
};

struct Section {
    const char* title;
    QVector<Item> items;
};

struct Template {
    const char* id;
    const char* name;
    QVector<Section> sections;
};

/// Status values stored per item.
inline QStringList statuses() {
    return {QStringLiteral("open"), QStringLiteral("done"), QStringLiteral("issue"), QStringLiteral("na")};
}

inline QString tr_dd(const char* s) {
    return QCoreApplication::translate("AltDueDiligence", s);
}

namespace detail {

inline Section firm() {
    return {QT_TRANSLATE_NOOP("AltDueDiligence", "Firm and team"),
            {{"firm.ownership", QT_TRANSLATE_NOOP("AltDueDiligence", "Ownership and economics of the manager are documented")},
             {"firm.key_persons", QT_TRANSLATE_NOOP("AltDueDiligence", "Key persons named, time commitment and key-person clause reviewed")},
             {"firm.turnover", QT_TRANSLATE_NOOP("AltDueDiligence", "Senior staff turnover over the last five years explained")},
             {"firm.references", QT_TRANSLATE_NOOP("AltDueDiligence", "References taken (investors, portfolio companies, former staff)")},
             {"firm.background", QT_TRANSLATE_NOOP("AltDueDiligence", "Background checks on principals (regulatory, litigation, press)")}}};
}

inline Section terms() {
    return {QT_TRANSLATE_NOOP("AltDueDiligence", "Terms and alignment"),
            {{"terms.fees", QT_TRANSLATE_NOOP("AltDueDiligence", "Management fee basis and step-downs understood")},
             {"terms.carry", QT_TRANSLATE_NOOP("AltDueDiligence", "Carry / performance fee, hurdle, catch-up and waterfall modelled")},
             {"terms.gp_commit", QT_TRANSLATE_NOOP("AltDueDiligence", "Manager's own commitment (amount and form) confirmed")},
             {"terms.expenses", QT_TRANSLATE_NOOP("AltDueDiligence", "Fund expenses and offsets (transaction, monitoring fees) listed")},
             {"terms.mfn", QT_TRANSLATE_NOOP("AltDueDiligence", "Side letters and most-favoured-nation rights reviewed")}}};
}

inline Section operations() {
    return {QT_TRANSLATE_NOOP("AltDueDiligence", "Operations and controls"),
            {{"ops.admin", QT_TRANSLATE_NOOP("AltDueDiligence", "Independent administrator and its scope confirmed")},
             {"ops.auditor", QT_TRANSLATE_NOOP("AltDueDiligence", "Auditor named; last audited statements and opinion read")},
             {"ops.valuation", QT_TRANSLATE_NOOP("AltDueDiligence", "Valuation policy, frequency and independence reviewed")},
             {"ops.cash", QT_TRANSLATE_NOOP("AltDueDiligence", "Cash controls: dual signatures, segregation of duties")},
             {"ops.cyber", QT_TRANSLATE_NOOP("AltDueDiligence", "Cyber security and business continuity plans seen")}}};
}

inline Section legal() {
    return {QT_TRANSLATE_NOOP("AltDueDiligence", "Legal, tax and compliance"),
            {{"legal.docs", QT_TRANSLATE_NOOP("AltDueDiligence", "Offering documents and agreement read by counsel")},
             {"legal.regulator", QT_TRANSLATE_NOOP("AltDueDiligence", "Manager's registration and regulatory history checked")},
             {"legal.conflicts", QT_TRANSLATE_NOOP("AltDueDiligence", "Conflicts of interest and allocation policy reviewed")},
             {"legal.tax", QT_TRANSLATE_NOOP("AltDueDiligence", "Tax treatment for this investor (structure, withholding, reporting) confirmed")},
             {"legal.aml", QT_TRANSLATE_NOOP("AltDueDiligence", "AML / KYC and sanctions procedures in place")}}};
}

inline Section reporting() {
    return {QT_TRANSLATE_NOOP("AltDueDiligence", "Reporting and liquidity"),
            {{"rep.frequency", QT_TRANSLATE_NOOP("AltDueDiligence", "Reporting frequency, content and timeliness agreed")},
             {"rep.transparency", QT_TRANSLATE_NOOP("AltDueDiligence", "Position-level transparency available")},
             {"rep.liquidity", QT_TRANSLATE_NOOP("AltDueDiligence", "Redemption / transfer terms, gates and lock-ups understood")}}};
}

} // namespace detail

inline const QVector<Template>& templates() {
    using namespace detail;
    static const QVector<Template> kTemplates = {
        {"pe", QT_TRANSLATE_NOOP("AltDueDiligence", "Private equity fund"),
         {firm(),
          {QT_TRANSLATE_NOOP("AltDueDiligence", "Strategy and track record"),
           {{"pe.track_record", QT_TRANSLATE_NOOP("AltDueDiligence", "Deal-level track record with gross and net IRR / TVPI / DPI")},
            {"pe.attribution", QT_TRANSLATE_NOOP("AltDueDiligence", "Value creation split: earnings growth, multiple, leverage")},
            {"pe.pme", QT_TRANSLATE_NOOP("AltDueDiligence", "Prior funds compared with a public index (PME)")},
            {"pe.losses", QT_TRANSLATE_NOOP("AltDueDiligence", "Loss ratio and the worst deals explained")},
            {"pe.pipeline", QT_TRANSLATE_NOOP("AltDueDiligence", "Pipeline and sourcing edge for this fund")},
            {"pe.leverage", QT_TRANSLATE_NOOP("AltDueDiligence", "Use of fund-level leverage (subscription lines, NAV loans)")}}},
          terms(), operations(), legal(), reporting()}},
        {"vc", QT_TRANSLATE_NOOP("AltDueDiligence", "Venture capital fund"),
         {firm(),
          {QT_TRANSLATE_NOOP("AltDueDiligence", "Strategy and track record"),
           {{"vc.stage", QT_TRANSLATE_NOOP("AltDueDiligence", "Stage, sector focus and ownership targets consistent with past funds")},
            {"vc.reserves", QT_TRANSLATE_NOOP("AltDueDiligence", "Reserve policy for follow-on rounds")},
            {"vc.concentration", QT_TRANSLATE_NOOP("AltDueDiligence", "Returns concentration: share of value from the top deals")},
            {"vc.marks", QT_TRANSLATE_NOOP("AltDueDiligence", "Unrealised marks checked against last round terms and preferences")},
            {"vc.access", QT_TRANSLATE_NOOP("AltDueDiligence", "Access to follow-on and co-investment rights")}}},
          terms(), operations(), legal(), reporting()}},
        {"private_credit", QT_TRANSLATE_NOOP("AltDueDiligence", "Private credit fund"),
         {firm(),
          {QT_TRANSLATE_NOOP("AltDueDiligence", "Credit process and portfolio"),
           {{"pc.underwriting", QT_TRANSLATE_NOOP("AltDueDiligence", "Underwriting standards: leverage, coverage, covenants")},
            {"pc.defaults", QT_TRANSLATE_NOOP("AltDueDiligence", "Default, recovery and non-accrual history through a downturn")},
            {"pc.workout", QT_TRANSLATE_NOOP("AltDueDiligence", "Workout team and restructuring experience")},
            {"pc.concentration", QT_TRANSLATE_NOOP("AltDueDiligence", "Borrower, sector and sponsor concentration limits")},
            {"pc.rates", QT_TRANSLATE_NOOP("AltDueDiligence", "Floating-rate exposure, floors and borrower interest cover under higher rates")},
            {"pc.fund_leverage", QT_TRANSLATE_NOOP("AltDueDiligence", "Fund-level leverage and its terms")}}},
          terms(), operations(), legal(), reporting()}},
        {"real_estate", QT_TRANSLATE_NOOP("AltDueDiligence", "Real estate"),
         {firm(),
          {QT_TRANSLATE_NOOP("AltDueDiligence", "Property and market"),
           {{"re.title", QT_TRANSLATE_NOOP("AltDueDiligence", "Title, zoning and environmental reports")},
            {"re.condition", QT_TRANSLATE_NOOP("AltDueDiligence", "Building condition survey and capex plan")},
            {"re.leases", QT_TRANSLATE_NOOP("AltDueDiligence", "Rent roll, lease expiries and tenant credit")},
            {"re.market", QT_TRANSLATE_NOOP("AltDueDiligence", "Market rents, vacancy and new supply checked")},
            {"re.debt", QT_TRANSLATE_NOOP("AltDueDiligence", "Debt terms: DSCR / LTV covenants, maturity and hedging")},
            {"re.exit", QT_TRANSLATE_NOOP("AltDueDiligence", "Exit cap rate assumption tested against sales evidence")}}},
          terms(), operations(), legal(), reporting()}},
        {"hedge_fund", QT_TRANSLATE_NOOP("AltDueDiligence", "Hedge fund"),
         {firm(),
          {QT_TRANSLATE_NOOP("AltDueDiligence", "Strategy and risk"),
           {{"hf.returns", QT_TRANSLATE_NOOP("AltDueDiligence", "Monthly net returns since inception, audited where possible")},
            {"hf.drawdown", QT_TRANSLATE_NOOP("AltDueDiligence", "Worst drawdowns and recovery explained")},
            {"hf.risk", QT_TRANSLATE_NOOP("AltDueDiligence", "Risk limits: gross / net exposure, VaR, stop-losses")},
            {"hf.capacity", QT_TRANSLATE_NOOP("AltDueDiligence", "Strategy capacity and current assets")},
            {"hf.prime", QT_TRANSLATE_NOOP("AltDueDiligence", "Prime brokers, counterparties and custody of assets")},
            {"hf.illiquid", QT_TRANSLATE_NOOP("AltDueDiligence", "Share of hard-to-value positions and side pockets")}}},
          terms(), operations(), legal(), reporting()}},
        {"digital_asset", QT_TRANSLATE_NOOP("AltDueDiligence", "Digital asset fund"),
         {firm(),
          {QT_TRANSLATE_NOOP("AltDueDiligence", "Custody and market risk"),
           {{"da.custody", QT_TRANSLATE_NOOP("AltDueDiligence", "Custodian, key management and proof of reserves")},
            {"da.venues", QT_TRANSLATE_NOOP("AltDueDiligence", "Trading venues and counterparty limits")},
            {"da.staking", QT_TRANSLATE_NOOP("AltDueDiligence", "Staking, lending and DeFi exposure and their risks")},
            {"da.valuation", QT_TRANSLATE_NOOP("AltDueDiligence", "Pricing sources for illiquid tokens")},
            {"da.regulation", QT_TRANSLATE_NOOP("AltDueDiligence", "Regulatory status of the assets and the manager in each jurisdiction")}}},
          terms(), operations(), legal(), reporting()}},
    };
    return kTemplates;
}

inline const Template* find_template(const QString& id) {
    for (const Template& t : templates())
        if (id == QLatin1String(t.id))
            return &t;
    return nullptr;
}

inline int item_count(const Template& t) {
    int n = 0;
    for (const Section& s : t.sections)
        n += static_cast<int>(s.items.size());
    return n;
}

} // namespace fincept::screens::alt_dd
