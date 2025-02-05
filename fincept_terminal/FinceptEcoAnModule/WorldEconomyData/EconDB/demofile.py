from textual.app import App, ComposeResult
from textual.widgets import DataTable, Header, Footer
from textual.containers import Grid, VerticalScroll
from textual_plotext import PlotextPlot
import httpx
from datetime import datetime
import asyncio

class EconomicDashboard(App):
    BINDINGS = [("q", "quit", "Quit"), ("d", "toggle_dark", "Toggle Dark")]

    def compose(self) -> ComposeResult:
        yield Header(show_clock=True)
        with Grid():
            yield VerticalScroll(DataTable(zebra_stripes=True, id="gdp-table"))
            yield PlotextPlot(id="gdp-plot")
            yield VerticalScroll(DataTable(zebra_stripes=True, id="trade-table"))
            yield PlotextPlot(id="trade-plot")
        yield Footer()

    async def on_mount(self) -> None:
        self.title = "Economic Dashboard"
        self.grid_style()
        await self.load_data()

    def grid_style(self):
        grid = self.query_one(Grid)
        grid.styles.grid_size_columns = 2
        grid.styles.grid_size_rows = 2
        grid.styles.grid_gutter = (1, 1)
        grid.styles.height = "100%"

        for plot in [self.query_one("#gdp-plot"), self.query_one("#trade-plot")]:
            plot.styles.min_width = 40
            plot.styles.min_height = 15

    async def load_data(self):
        await asyncio.gather(
            self.process_gdp_data(),
            self.process_trade_data()
        )

        async def process_gdp_data(self):
            try:
                async with httpx.AsyncClient() as client:
                    response = await client.get(
                        "https://www.econdb.com/widgets/real-gdp-growth/data/?country=IN&format=json"
                    )
                    data = response.json()
                    plot_data = data["plots"][0]

                    table = self.query_one("#gdp-table", DataTable)
                    table.add_columns("Quarter", "Growth (%)")

                    dates, values = [], []
                    for entry in plot_data["data"]:
                        date = datetime.strptime(entry["Date"].split("T")[0], "%Y-%m-%d").strftime("%b %Y")
                        growth = entry["RGDP"]
                        dates.append(date)
                        values.append(growth)
                        table.add_row(date, f"{growth:.2f}%")

                    plot = self.query_one("#gdp-plot", PlotextPlot)
                    plt = plot.plt
                    plt.clf()
                    plt.bar(dates, values, orientation="vertical", color="cyan")
                    plt.title("India Quarterly GDP Growth")
                    plt.xlabel("Quarter")
                    plt.ylabel("Growth (%)")
                    plot.refresh()

            except Exception as e:
                self.footer = f"⚠️ GDP Error: {str(e)}"

    async def process_trade_data(self):
        try:
            async with httpx.AsyncClient() as client:
                response = await client.get(
                    "https://www.econdb.com/widgets/imports-exports/data/?country=US&format=json"
                )
                data = response.json()
                plot_data = data["plots"][0]

                table = self.query_one("#trade-table", DataTable)
                table.add_columns("Date", "Exports (B)", "Imports (B)", "Exp% GDP", "Imp% GDP")

                trade_dates, exports, imports = [], [], []
                pct_dates, exp_pct, imp_pct = [], [], []

                for entry in plot_data["data"]:
                    date_str = datetime.strptime(entry["Date"], "%Y-%m-%d").strftime("%b %Y")

                    # Trade values (always present)
                    trade_dates.append(date_str)
                    exports.append(entry["EXPMON"] / 1000)
                    imports.append(abs(entry["IMPMON"]) / 1000)

                    # Percentages (handle nulls)
                    if entry["EXP_PCT"] is not None and entry["IMP_PCT"] is not None:
                        pct_dates.append(date_str)
                        exp_pct.append(entry["EXP_PCT"])
                        imp_pct.append(abs(entry["IMP_PCT"]))

                    table.add_row(
                        date_str,
                        f"${exports[-1]:,.1f}",
                        f"${imports[-1]:,.1f}",
                        f"{exp_pct[-1]:.1f}%" if entry["EXP_PCT"] else "N/A",
                        f"{imp_pct[-1]:.1f}%" if entry["IMP_PCT"] else "N/A"
                    )

                plot = self.query_one("#trade-plot", PlotextPlot)
                plt = plot.plt
                plt.clf()

                # Line chart for percentages
                plt.subplots(1, 1)
                plt.plot(pct_dates, exp_pct, label="Exports % GDP", color="green")
                plt.plot(pct_dates, imp_pct, label="Imports % GDP", color="red")
                plt.title("US Trade % of GDP (Monthly)")
                plt.xlabel("Month")
                plt.ylabel("Percentage")
                plt.legend()
                plot.refresh()

        except Exception as e:
            self.footer = f"⚠️ Trade Error: {str(e)}"

    def action_toggle_dark(self) -> None:
        self.dark = not self.dark


if __name__ == "__main__":
    EconomicDashboard().run()
