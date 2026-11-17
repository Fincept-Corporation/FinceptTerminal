// AdapterSupport.cpp — helpers shared by the argv-prompt harness adapters (see
// AdapterSupport.h). QtCore only.

#include "services/harness/adapters/AdapterSupport.h"

#include "services/coding_agents/CodingAgentTypes.h"

namespace fincept::harness::adapter_support {

bool valid_model_id(const QString& model) {
    static const QRegularExpression rx(QStringLiteral(R"(^[A-Za-z0-9][A-Za-z0-9._:/@\[\]-]{0,99}$)"));
    return rx.match(model).hasMatch();
}

bool valid_session_id(const QString& id) {
    static const QRegularExpression rx(QStringLiteral("^[A-Za-z0-9_][A-Za-z0-9_-]{0,127}$"));
    return rx.match(id).hasMatch();
}

bool valid_uuid(const QString& id) {
    static const QRegularExpression rx(
        QStringLiteral("^[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}$"));
    return rx.match(id).hasMatch();
}

bool valid_mcp_endpoint(const McpEndpoint& endpoint) {
    // A name starting with '-' would be read as a flag by `<cli> mcp get <name>`.
    static const QRegularExpression name_rx(QStringLiteral("^[A-Za-z0-9_][A-Za-z0-9_-]{0,63}$"));
    static const QRegularExpression env_rx(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]{0,63}$"));
    static const QRegularExpression url_rx(QStringLiteral(R"(^https?://[^\s"'\\]{1,500}$)"));
    return name_rx.match(endpoint.name).hasMatch() && url_rx.match(endpoint.url).hasMatch() &&
           env_rx.match(endpoint.token_env_var).hasMatch();
}

QString argv_text(const QString& text) {
    return text.startsWith(QLatin1Char('-')) ? QStringLiteral(" ") + text : text;
}

QString placeholder_bearer_header() {
    return QStringLiteral("Authorization: Bearer ") + QString::fromLatin1(kMcpTokenPlaceholder);
}

QString named_or_bare_version(const QString& line, const QString& tool, const QStringList& foreign) {
    const QString t = line.trimmed();
    for (const QString& other : foreign) {
        if (t.contains(other, Qt::CaseInsensitive))
            return {};
    }
    static const QRegularExpression bare_rx(QStringLiteral(R"(^v?(\d+\.\d+\.\d+)(?:[-+][0-9A-Za-z.+-]*)?$)"));
    const auto m = bare_rx.match(t);
    if (m.hasMatch())
        return m.captured(1);
    if (!t.contains(tool, Qt::CaseInsensitive))
        return {};
    return services::coding_agents::parse_cli_version(t);
}

FailureKind classify(int exit_code, const services::coding_agents::StreamFacts& facts, const QStringList& stderr_tail,
                     const QRegularExpression& auth_rx, bool require_result) {
    const bool produced = facts.saw_result || (!require_result && !facts.final_text.trimmed().isEmpty());
    if (exit_code == 0 && !facts.result_is_error && produced)
        return FailureKind::None;
    const QString text = (facts.result_error + QLatin1Char('\n') + facts.last_error + QLatin1Char('\n') +
                          stderr_tail.join(QLatin1Char('\n')))
                             .toLower();
    static const QRegularExpression rate_rx(
        QStringLiteral(R"(rate.?limit|\b429\b|usage limit|quota|too many requests|resource.?exhausted)"));
    static const QRegularExpression model_rx(QStringLiteral(
        R"(model[^\n]{0,80}(not found|not supported|does not exist|is not available|unavailable)|unknown model|invalid model)"));
    if (auth_rx.match(text).hasMatch())
        return FailureKind::Auth;
    if (rate_rx.match(text).hasMatch() || facts.rate_limit_status == QLatin1String("rejected"))
        return FailureKind::RateLimit;
    if (model_rx.match(text).hasMatch())
        return FailureKind::Model;
    return FailureKind::Other;
}

} // namespace fincept::harness::adapter_support
