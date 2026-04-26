"""실시간 시장 데이터 MCP 툴 (yfinance 기반)"""

from mcp.server.fastmcp import FastMCP
import yfinance as yf


def register(mcp: FastMCP) -> None:

    @mcp.tool()
    def get_quote(symbol: str) -> dict:
        """종목의 현재 주가 및 기본 시장 정보 조회

        Args:
            symbol: 종목 코드 (예: AAPL, 005930.KS, BTC-USD)
        """
        ticker = yf.Ticker(symbol)
        info = ticker.fast_info
        return {
            "symbol": symbol.upper(),
            "price": info.last_price,
            "previous_close": info.previous_close,
            "open": info.open,
            "day_high": info.day_high,
            "day_low": info.day_low,
            "volume": info.three_month_average_volume,
            "market_cap": info.market_cap,
            "currency": info.currency,
        }

    @mcp.tool()
    def get_stock_info(symbol: str) -> dict:
        """종목의 상세 재무 정보 조회 (PER, PBR, 배당, 섹터 등)

        Args:
            symbol: 종목 코드 (예: AAPL, MSFT)
        """
        info = yf.Ticker(symbol).info
        keys = [
            "longName", "sector", "industry", "country", "marketCap",
            "trailingPE", "forwardPE", "priceToBook", "dividendYield",
            "beta", "fiftyTwoWeekHigh", "fiftyTwoWeekLow",
            "revenueGrowth", "earningsGrowth", "returnOnEquity",
            "totalDebt", "totalCash", "operatingMargins",
            "longBusinessSummary",
        ]
        return {k: info.get(k) for k in keys}

    @mcp.tool()
    def get_price_history(symbol: str, period: str = "1mo", interval: str = "1d") -> list[dict]:
        """종목의 과거 주가 이력 조회

        Args:
            symbol: 종목 코드
            period: 기간. 1d|5d|1mo|3mo|6mo|1y|2y|5y|10y|ytd|max 중 하나 (기본 1mo)
            interval: 주기. 1m|5m|15m|30m|1h|1d|1wk|1mo 중 하나 (기본 1d)
        """
        df = yf.Ticker(symbol).history(period=period, interval=interval)
        if df.empty:
            return []
        df = df.reset_index()
        return [
            {
                "date": str(row["Date"]) if "Date" in row else str(row["Datetime"]),
                "open": round(row["Open"], 4),
                "high": round(row["High"], 4),
                "low": round(row["Low"], 4),
                "close": round(row["Close"], 4),
                "volume": int(row["Volume"]),
            }
            for _, row in df.iterrows()
        ]

    @mcp.tool()
    def get_multiple_quotes(symbols: list[str]) -> list[dict]:
        """여러 종목의 현재 주가 일괄 조회

        Args:
            symbols: 종목 코드 목록 (예: ["AAPL", "MSFT", "GOOGL"])
        """
        results = []
        for symbol in symbols:
            try:
                info = yf.Ticker(symbol).fast_info
                results.append({
                    "symbol": symbol.upper(),
                    "price": info.last_price,
                    "previous_close": info.previous_close,
                    "change_pct": round(
                        (info.last_price - info.previous_close) / info.previous_close * 100, 2
                    ) if info.previous_close else None,
                    "currency": info.currency,
                })
            except Exception as e:
                results.append({"symbol": symbol.upper(), "error": str(e)})
        return results

    @mcp.tool()
    def analyze_portfolio_performance(symbols: list[str], weights: list[float] | None = None) -> dict:
        """보유 종목들의 수익률 및 포트폴리오 성과 분석

        Args:
            symbols: 종목 코드 목록
            weights: 각 종목 비중 (합계 1.0). None이면 동일 비중.
        """
        import pandas as pd

        if not symbols:
            return {"error": "symbols가 비어있습니다"}

        if weights is None:
            weights = [1.0 / len(symbols)] * len(symbols)

        if len(weights) != len(symbols):
            return {"error": "symbols와 weights 길이가 다릅니다"}

        prices = yf.download(symbols, period="1y", interval="1d", progress=False)["Close"]
        if isinstance(prices, pd.Series):
            prices = prices.to_frame(name=symbols[0])

        returns = prices.pct_change().dropna()
        portfolio_return = (returns * weights).sum(axis=1)

        result = {
            "period": "1year",
            "portfolio": {
                "total_return_pct": round(portfolio_return.add(1).prod() - 1, 4) * 100,
                "annualized_volatility_pct": round(portfolio_return.std() * (252 ** 0.5), 4) * 100,
                "sharpe_ratio": round(
                    portfolio_return.mean() / portfolio_return.std() * (252 ** 0.5), 2
                ) if portfolio_return.std() > 0 else None,
            },
            "individual": {},
        }

        for symbol in symbols:
            if symbol in returns.columns:
                s = returns[symbol].add(1).prod() - 1
                result["individual"][symbol] = {
                    "total_return_pct": round(s, 4) * 100,
                }

        return result

    @mcp.tool()
    def get_financials(symbol: str) -> dict:
        """종목의 최근 재무제표 요약 (손익계산서, 대차대조표)

        Args:
            symbol: 종목 코드 (예: AAPL)
        """
        ticker = yf.Ticker(symbol)
        try:
            income = ticker.income_stmt
            balance = ticker.balance_sheet
            return {
                "income_statement": income.iloc[:, 0].to_dict() if not income.empty else {},
                "balance_sheet": balance.iloc[:, 0].to_dict() if not balance.empty else {},
            }
        except Exception as e:
            return {"error": str(e)}
