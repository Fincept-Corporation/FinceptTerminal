#include "core/layout/LayoutTemplates.h"

#include "core/identity/Uuid.h"

#include <QDateTime>

namespace fincept::layout {

QList<LayoutTemplates::Persona> LayoutTemplates::personas() {
    return {
        {QStringLiteral("equity_trader"),     QStringLiteral("Equity Trader"),
         QStringLiteral("Stock trading with watchlist, ticker chart, and order book.")},
        {QStringLiteral("crypto_trader"),     QStringLiteral("Crypto Trader"),
         QStringLiteral("Live order book and chart for spot and perpetual markets.")},
        {QStringLiteral("portfolio_manager"), QStringLiteral("Portfolio Manager"),
         QStringLiteral("Holdings, allocation, risk and performance dashboard.")},
        {QStringLiteral("research_analyst"),  QStringLiteral("Research Analyst"),
         QStringLiteral("Equity research with fundamentals, news, and financials.")},
        {QStringLiteral("macro_economist"),   QStringLiteral("Macro Economist"),
         QStringLiteral("DBnomics series, economic calendar, world maps.")},
    };
}

Workspace LayoutTemplates::make(const QString& persona_id) {
    Workspace w;
    w.id = LayoutId::generate();
    w.kind = QStringLiteral("builtin");
    w.created_at_unix = QDateTime::currentSecsSinceEpoch();
    w.updated_at_unix = w.created_at_unix;

    // Build a single frame with one representative panel for the persona.
    FrameLayout fl;
    fl.window_id = WindowId::generate();
    fl.name = QStringLiteral("Main");

    PanelState p;
    p.instance_id = PanelInstanceId::generate();

    if (persona_id == QStringLiteral("equity_trader")) {
        w.name = QStringLiteral("Equity Trader");
        p.type_id = QStringLiteral("equity_trading");
        p.title = QStringLiteral("Equity Trading");
    } else if (persona_id == QStringLiteral("crypto_trader")) {
        w.name = QStringLiteral("Crypto Trader");
        p.type_id = QStringLiteral("crypto_trading");
        p.title = QStringLiteral("Crypto Trading");
    } else if (persona_id == QStringLiteral("portfolio_manager")) {
        w.name = QStringLiteral("Portfolio Manager");
        p.type_id = QStringLiteral("portfolio");
        p.title = QStringLiteral("Portfolio");
    } else if (persona_id == QStringLiteral("research_analyst")) {
        w.name = QStringLiteral("Research Analyst");
        p.type_id = QStringLiteral("equity_research");
        p.title = QStringLiteral("Equity Research");
    } else if (persona_id == QStringLiteral("macro_economist")) {
        w.name = QStringLiteral("Macro Economist");
        p.type_id = QStringLiteral("economics");
        p.title = QStringLiteral("Economics");
    } else {
        // Unknown persona — fall through to a blank dashboard frame so the
        // user always gets a usable window.
        w.name = QStringLiteral("New Layout");
        w.kind = QStringLiteral("user");
        p.type_id = QStringLiteral("dashboard");
        p.title = QStringLiteral("Dashboard");
    }

    fl.panels.append(p);
    fl.active_panel = p.instance_id;
    w.frames.append(fl);
    return w;
}

} // namespace fincept::layout
