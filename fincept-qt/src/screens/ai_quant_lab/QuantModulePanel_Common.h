// src/screens/ai_quant_lab/QuantModulePanel_Common.h
//
// Private header -- included only by QuantModulePanel*.cpp translation units.
// Provides cross-panel helpers that were originally in file-static anonymous
// namespaces inside QuantModulePanel.cpp. Several panel families (GS Quant,
// Functime, Statsmodels, Fortitudo, Gluonts, Quant Reporting) share the same
// numeric-input parsing and synthetic-sample generators, so they live here.
#pragma once

#include <QCoreApplication>
#include <QDate>
#include <QJsonArray>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include <cmath>

namespace fincept::screens::quant_common {

// Parse a CSV / whitespace / newline string into a JSON array of doubles.
// Returns false on any non-numeric token (sets bad_token to the offender).
inline bool parse_doubles(const QString& text, QJsonArray& out, QString* bad_token = nullptr) {
    out = QJsonArray{};
    QString s = text;
    static const QRegularExpression seps(R"([,;\t\n\r])");
    s.replace(seps, " ");
    const QStringList parts = s.split(' ', Qt::SkipEmptyParts);
    bool ok = false;
    for (const QString& p : parts) {
        const double v = p.toDouble(&ok);
        if (!ok) {
            if (bad_token)
                *bad_token = p;
            return false;
        }
        out.append(v);
    }
    return true;
}

// Split a comma-separated string into a JSON array of trimmed, non-empty
// strings. Replaces the ad-hoc `for (auto& s : text.split(',')) arr.append(...)`
// loops that shipped `[""]` to Python when the field was left blank.
inline QJsonArray parse_csv_strings(const QString& text, bool lowercase = false) {
    QJsonArray out;
    for (const QString& part : text.split(',', Qt::SkipEmptyParts)) {
        const QString t = part.trimmed();
        if (!t.isEmpty())
            out.append(lowercase ? t.toLower() : t);
    }
    return out;
}

// Validate an OPTIONAL free-text YYYY-MM-DD date range. Blank fields are
// allowed (the Python side then picks its own defaults) but garbage is not --
// those used to be forwarded verbatim and blew up inside Qlib.
// Returns an empty QString on success; otherwise a user-facing message.
// iso_start / iso_end receive the normalised values (empty when left blank).
inline QString check_iso_range(const QString& start_text, const QString& end_text, QString& iso_start,
                               QString& iso_end) {
    const auto tr_ = [](const char* s) { return QCoreApplication::translate("QuantModulePanel", s); };
    iso_start.clear();
    iso_end.clear();

    const QString s = start_text.trimmed();
    const QString e = end_text.trimmed();
    QDate ds, de;
    if (!s.isEmpty()) {
        ds = QDate::fromString(s, QStringLiteral("yyyy-MM-dd"));
        if (!ds.isValid())
            return tr_("Start date must be YYYY-MM-DD (got '%1').").arg(s);
        iso_start = ds.toString(QStringLiteral("yyyy-MM-dd"));
    }
    if (!e.isEmpty()) {
        de = QDate::fromString(e, QStringLiteral("yyyy-MM-dd"));
        if (!de.isValid())
            return tr_("End date must be YYYY-MM-DD (got '%1').").arg(e);
        iso_end = de.toString(QStringLiteral("yyyy-MM-dd"));
    }
    if (ds.isValid() && de.isValid() && ds > de)
        return tr_("Start date (%1) is after end date (%2).").arg(iso_start, iso_end);
    return {};
}

// Generate a small synthetic returns sample so the user can try the panel
// without typing 252 numbers. Produces ~1y of plausible daily returns.
inline QString sample_returns(unsigned seed = 1, int n = 252, double mu = 0.0005, double sigma = 0.012) {
    quint32 state = seed ? seed : 1u;
    auto rand01 = [&]() -> double {
        state = state * 1664525u + 1013904223u;
        return double(state >> 8) / double(1u << 24);
    };
    QStringList out;
    out.reserve(n);
    for (int i = 0; i < n; ++i) {
        const double u1 = std::max(rand01(), 1e-12);
        const double u2 = rand01();
        constexpr double kTwoPi = 6.283185307179586;
        const double z = std::sqrt(-2.0 * std::log(u1)) * std::cos(kTwoPi * u2);
        out << QString::number(mu + sigma * z, 'f', 6);
    }
    return out.join(',');
}

// Synthetic time series sample with trend + seasonality + noise.
// Useful for the LOAD SAMPLE buttons on the Functime / Statsmodels / Gluonts panels.
inline QString sample_series(unsigned seed = 1, int n = 200, double base = 50.0, double trend_per_step = 0.05,
                             double season_amp = 5.0, int season_period = 30, double noise_sigma = 1.5) {
    quint32 state = seed ? seed : 1u;
    auto rand01 = [&]() -> double {
        state = state * 1664525u + 1013904223u;
        return double(state >> 8) / double(1u << 24);
    };
    QStringList out;
    out.reserve(n);
    for (int i = 0; i < n; ++i) {
        const double u1 = std::max(rand01(), 1e-12);
        const double u2 = rand01();
        constexpr double kTwoPi = 6.283185307179586;
        const double z = std::sqrt(-2.0 * std::log(u1)) * std::cos(kTwoPi * u2);
        const double v = base + trend_per_step * i +
                         season_amp * std::sin(kTwoPi * double(i) / double(std::max(2, season_period))) +
                         noise_sigma * z;
        out << QString::number(v, 'f', 4);
    }
    return out.join(',');
}

// Trending random walk -- useful for stationarity testing where d=1 should win.
inline QString sample_random_walk(unsigned seed = 1, int n = 150, double drift = 0.05, double sigma = 1.0) {
    quint32 state = seed ? seed : 1u;
    auto rand01 = [&]() -> double {
        state = state * 1664525u + 1013904223u;
        return double(state >> 8) / double(1u << 24);
    };
    QStringList out;
    out.reserve(n);
    double level = 0.0;
    for (int i = 0; i < n; ++i) {
        const double u1 = std::max(rand01(), 1e-12);
        const double u2 = rand01();
        constexpr double kTwoPi = 6.283185307179586;
        const double z = std::sqrt(-2.0 * std::log(u1)) * std::cos(kTwoPi * u2);
        level += drift + sigma * z;
        out << QString::number(level, 'f', 4);
    }
    return out.join(',');
}

} // namespace fincept::screens::quant_common
