from langchain_core.messages import HumanMessage
from fincept_terminal.FinceptTerminalAIPwrResModule.ai_hedge_fund.data.state import AgentState, show_agent_reasoning
from fincept_terminal.FinceptTerminalAIPwrResModule.ai_hedge_fund.utils.progress import progress
import json

from fincept_terminal.FinceptTerminalAIPwrResModule.ai_hedge_fund.data.data_acquisition import get_financial_metrics

def fundamentals_agent(state: AgentState):
    data = state["data"]
    end_date = data["end_date"]
    tickers = data["tickers"]
    fundamental_analysis = {}

    for ticker in tickers:
        progress.update_status("fundamentals_agent", ticker, "Fetching financial metrics")
        financial_metrics = get_financial_metrics(ticker=ticker, end_date=end_date, period="ttm", limit=10)

        if not financial_metrics:
            progress.update_status("fundamentals_agent", ticker, "Failed: No financial metrics found")
            continue

        metrics = financial_metrics[0]
        signals = []
        reasoning = {}

        progress.update_status("fundamentals_agent", ticker, "Analyzing profitability")
        roe = metrics.return_on_equity
        net_margin = metrics.net_margin
        op_margin = metrics.operating_margin
        thresholds = [
            (roe, 0.15),
            (net_margin, 0.20),
            (op_margin, 0.15),
        ]
        profitability_score = sum(x is not None and x > y for x, y in thresholds)
        s1 = "bullish" if profitability_score >= 2 else "bearish" if profitability_score == 0 else "neutral"
        signals.append(s1)
        reasoning["profitability_signal"] = {
            "signal": s1,
            "details": (
                (f"ROE: {roe:.2%}" if roe else "ROE: N/A")
                + ", "
                + (f"Net Margin: {net_margin:.2%}" if net_margin else "Net Margin: N/A")
                + ", "
                + (f"Op Margin: {op_margin:.2%}" if op_margin else "Op Margin: N/A")
            ),
        }

        progress.update_status("fundamentals_agent", ticker, "Analyzing growth")
        rev_growth = metrics.revenue_growth
        earn_growth = metrics.earnings_growth
        bv_growth = metrics.book_value_growth
        thresholds = [
            (rev_growth, 0.10),
            (earn_growth, 0.10),
            (bv_growth, 0.10),
        ]
        growth_score = sum(x is not None and x > y for x, y in thresholds)
        s2 = "bullish" if growth_score >= 2 else "bearish" if growth_score == 0 else "neutral"
        signals.append(s2)
        reasoning["growth_signal"] = {
            "signal": s2,
            "details": (
                (f"Revenue Growth: {rev_growth:.2%}" if rev_growth else "Revenue Growth: N/A")
                + ", "
                + (f"Earnings Growth: {earn_growth:.2%}" if earn_growth else "Earnings Growth: N/A")
            ),
        }

        progress.update_status("fundamentals_agent", ticker, "Analyzing financial health")
        curr_ratio = metrics.current_ratio
        dte = metrics.debt_to_equity
        fcf_sh = metrics.free_cash_flow_per_share
        eps = metrics.earnings_per_share
        health_score = 0
        if curr_ratio and curr_ratio > 1.5:
            health_score += 1
        if dte and dte < 0.5:
            health_score += 1
        if fcf_sh and eps and fcf_sh > eps * 0.8:
            health_score += 1
        s3 = "bullish" if health_score >= 2 else "bearish" if health_score == 0 else "neutral"
        signals.append(s3)
        reasoning["financial_health_signal"] = {
            "signal": s3,
            "details": (
                (f"Current Ratio: {curr_ratio:.2f}" if curr_ratio else "Current Ratio: N/A")
                + ", "
                + (f"D/E: {dte:.2f}" if dte else "D/E: N/A")
            ),
        }

        progress.update_status("fundamentals_agent", ticker, "Analyzing valuation ratios")
        pe = metrics.price_to_earnings_ratio
        pb = metrics.price_to_book_ratio
        ps = metrics.price_to_sales_ratio
        thresholds = [
            (pe, 25),
            (pb, 3),
            (ps, 5),
        ]
        ratio_score = sum(x is not None and x > y for x, y in thresholds)
        s4 = "bullish" if ratio_score >= 2 else "bearish" if ratio_score == 0 else "neutral"
        signals.append(s4)
        reasoning["price_ratios_signal"] = {
            "signal": s4,
            "details": (
                (f"P/E: {pe:.2f}" if pe else "P/E: N/A")
                + ", "
                + (f"P/B: {pb:.2f}" if pb else "P/B: N/A")
                + ", "
                + (f"P/S: {ps:.2f}" if ps else "P/S: N/A")
            ),
        }

        progress.update_status("fundamentals_agent", ticker, "Calculating final signal")
        b_signals = signals.count("bullish")
        bear_signals = signals.count("bearish")
        if b_signals > bear_signals:
            overall = "bullish"
        elif bear_signals > b_signals:
            overall = "bearish"
        else:
            overall = "neutral"
        confidence = round(max(b_signals, bear_signals) / len(signals), 2) * 100

        fundamental_analysis[ticker] = {
            "signal": overall,
            "confidence": confidence,
            "reasoning": reasoning,
        }

        progress.update_status("fundamentals_agent", ticker, "Done")

    message = HumanMessage(content=json.dumps(fundamental_analysis), name="fundamentals_agent")
    if state["metadata"]["show_reasoning"]:
        show_agent_reasoning(fundamental_analysis, "Fundamental Analysis Agent")
    state["data"]["analyst_signals"]["fundamentals_agent"] = fundamental_analysis
    return {"messages": [message], "data": data}
