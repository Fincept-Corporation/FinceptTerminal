#pragma once

#include <map>
#include <sstream>
#include <string>
#include <utility>

namespace fincept::datahub {

class FXMacroDataClient {
public:
    explicit FXMacroDataClient(std::string apiKey = {},
                               std::string baseUrl = "https://api.fxmacrodata.com/v1")
        : apiKey_(std::move(apiKey)), baseUrl_(std::move(baseUrl)) {}

    std::string dataCatalogue(const std::string& currency) const {
        return url("/data_catalogue/" + lower(currency));
    }

    std::string announcements(const std::string& currency, const std::string& indicator) const {
        return url("/announcements/" + lower(currency) + "/" + indicator);
    }

    std::string calendar(const std::string& currency) const {
        return url("/calendar/" + lower(currency));
    }

    std::string predictions(const std::string& currency, const std::string& indicator) const {
        return url("/predictions/" + lower(currency) + "/" + indicator);
    }

    std::string forex(const std::string& base, const std::string& quote) const {
        return url("/forex/" + lower(base) + "/" + lower(quote));
    }

    std::string cot(const std::string& currency) const {
        return url("/cot/" + lower(currency));
    }

    std::string commoditiesLatest() const {
        return url("/commodities/latest");
    }

    std::string commodity(const std::string& indicator) const {
        return url("/commodities/" + indicator);
    }

    std::string curves(const std::string& currency) const {
        return url("/curves/" + lower(currency));
    }

    std::string curveProxies(const std::string& currency) const {
        return url("/curve_proxies/" + lower(currency));
    }

    std::string forwardCurves(const std::string& currency) const {
        return url("/forward_curves/" + lower(currency));
    }

    std::string marketSessions() const {
        return url("/market_sessions");
    }

    std::string riskSentiment() const {
        return url("/risk_sentiment");
    }

    std::string news(const std::string& currency) const {
        return url("/news/" + lower(currency));
    }

    std::string pressReleases(const std::string& currency) const {
        return url("/press-releases/" + lower(currency));
    }

    std::string centralBankers(const std::string& currency) const {
        return url("/central_bankers/" + lower(currency));
    }

    std::string graphql(const std::string& query) const {
        return url("/graphql", {{"query", query}});
    }

private:
    std::string url(const std::string& path,
                    std::map<std::string, std::string> params = {}) const {
        if (!apiKey_.empty()) {
            params.emplace("api_key", apiKey_);
        }

        std::ostringstream out;
        out << baseUrl_ << path;
        char separator = '?';
        for (const auto& item : params) {
            out << separator << encode(item.first) << '=' << encode(item.second);
            separator = '&';
        }
        return out.str();
    }

    static std::string lower(std::string value) {
        for (char& c : value) {
            if (c >= 'A' && c <= 'Z') {
                c = static_cast<char>(c - 'A' + 'a');
            }
        }
        return value;
    }

    static std::string encode(const std::string& value) {
        static constexpr char hex[] = "0123456789ABCDEF";
        std::string encoded;
        for (unsigned char c : value) {
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
                encoded.push_back(static_cast<char>(c));
            } else {
                encoded.push_back('%');
                encoded.push_back(hex[c >> 4]);
                encoded.push_back(hex[c & 15]);
            }
        }
        return encoded;
    }

    std::string apiKey_;
    std::string baseUrl_;
};

}  // namespace fincept::datahub
