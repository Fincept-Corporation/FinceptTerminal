#pragma once
// FiiDiiTypes — daily institutional flows.
//
// One row per trading day. Source: NSE cash-market FII/DII end-of-day
// publication (scripts/fii_dii_scraper.py). All values in Indian Rupees,
// Crore units (1 Crore = 10⁷ ₹).

#include <QMetaType>
#include <QString>
#include <QVector>

namespace fincept::services::options {

struct FiiDiiDay {
    QString date_iso;       // "YYYY-MM-DD"
    double fii_buy = 0;
    double fii_sell = 0;
    double fii_net = 0;     // = fii_buy − fii_sell (kept explicit for null-safe display)
    double dii_buy = 0;
    double dii_sell = 0;
    double dii_net = 0;
};

} // namespace fincept::services::options

Q_DECLARE_METATYPE(fincept::services::options::FiiDiiDay)
Q_DECLARE_METATYPE(QVector<fincept::services::options::FiiDiiDay>)
