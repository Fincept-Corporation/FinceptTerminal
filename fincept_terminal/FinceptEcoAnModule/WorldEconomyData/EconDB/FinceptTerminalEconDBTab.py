from textual.widgets import Static, Button, DataTable, TabPane, TabbedContent, OptionList, Collapsible
from textual.app import ComposeResult
from textual.containers import Container, VerticalScroll, Horizontal
import requests
from textual_plotext import PlotextPlot
from datetime import datetime
import httpx
import asyncio
from fincept_terminal.FinceptUtilsModule.ChartingUtils.ChartingWidget import ChartWidget
# API URLs
COUNTRY_API_URL = "https://www.econdb.com/static/appdata/econdb-countries.json"
BASE_API_URL = "https://www.econdb.com/api/series/{ticker}/?token=e17da7710dbd6103c273625fcd9f2e11d6f3249a&format=json"
INDICATORS_API_URL = "https://www.econdb.com/trends/list_indicators/"

# List of Economic Indicators
INDICATORS = ["RGDP", "GDP", "UNRATE", "PPI", "CPI", "RETA", "IP", "GBAL", "GDEBT", "CA", "NIIP", "Y10YD", "HOU", "OILPROD", "POP"]


class MainIndicatorTab(VerticalScroll):
    """Tab for displaying economic indicators dynamically based on country selection."""

    ITEMS_PER_PAGE = 10  # Pagination limit

    def __init__(self):
        super().__init__()
        self.all_countries = []  # Holds the list of all countries
        self.all_indicators = []  # Holds the list of all indicators
        self.current_country_page = 0  # Country pagination tracker
        self.is_loaded = False  # Prevent redundant loading

    def compose(self) -> ComposeResult:
        """Compose the UI layout for the Economic Indicators tab."""
        yield Static("🌎 Select a Country for Economic Indicators")

        # Collapsible section for country selection
        with Collapsible(title="Select Country", collapsed_symbol=">"):
            yield OptionList(id="country_selector")
            with Horizontal(classes="horizontal_next_previous"):
                yield Button("Previous", id="previous_country_page", variant="default", disabled=True)
                yield Button("Next", id="next_country_page", variant="default", disabled=True)
        with TabbedContent():
            with TabPane("Latest Data"):
                # DataTable to display economic indicators
                yield DataTable(name="Economic Indicators", id="economic_data_table")
            with TabPane("Historical Data",classes="history_data_tab"):
                with Container(classes="history_data_container"):
                    yield DataTable(name="Monthly Data", id="monthly_data_table")
                    yield DataTable(name="Quarterly Data", id="quarterly_data_table")

    async def on_show(self):
        """Triggered when the tab becomes visible. Loads data only once."""
        if not self.is_loaded:
            self.app.notify("📡 Loading available data...")

            # Schedule background tasks without blocking the UI
            asyncio.create_task(self.populate_country_list())
            asyncio.create_task(self.populate_indicators_list())

    async def populate_country_list(self):
        """Fetch and populate the list of available countries dynamically."""
        try:
            self.all_countries = await self.fetch_available_countries()
            if not self.all_countries:
                self.app.notify("❌ No countries available.", severity="error")
                return

            self.current_country_page = 0
            self.display_countries()
        except Exception as e:
            self.app.notify(f"❌ Error loading countries: {e}", severity="error")

    async def populate_indicators_list(self):
        """Fetch and populate the list of available indicators dynamically."""
        try:
            self.all_indicators = await self.fetch_available_indicators()
            if not self.all_indicators:
                self.app.notify("❌ No indicators available.", severity="error")
                return
        except Exception as e:
            self.app.notify(f"❌ Error loading indicators: {e}", severity="error")


    def display_countries(self):
        """Display the current page of countries in the OptionList."""
        start, end = self._get_pagination_range(self.current_country_page, self.all_countries)
        country_list = self.query_one("#country_selector", OptionList)
        country_list.clear_options()

        for country in self.all_countries[start:end]:
            country_list.add_option(f"{country['verbose']} ({country['iso2']})")

        self._update_pagination_controls(
            "previous_country_page", "next_country_page",
            self.current_country_page, len(self.all_countries)
        )

    async def fetch_available_countries(self):
        """Fetch country list dynamically from EconDB."""
        try:
            response = requests.get(COUNTRY_API_URL)
            if response.status_code == 200:
                country_data = response.json()
                unique_countries = {country["iso2"]: country for country in country_data}.values()  # Remove duplicates
                return list(unique_countries)
            else:
                self.app.notify("⚠ Failed to fetch country list.", severity="warning")
                return []
        except requests.RequestException as e:
            self.app.notify(f"❌ Error fetching country list: {e}", severity="error")
            return []

    async def fetch_available_indicators(self):
        """Fetch the list of indicators dynamically from EconDB."""
        headers = {
            "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36",
            "Accept": "application/json",
        }

        try:
            response = requests.get(INDICATORS_API_URL, headers=headers, timeout=10)  # Add headers
            response.raise_for_status()  # Raise an exception for HTTP errors

            try:
                indicators = response.json()
                if indicators:
                    self.app.notify("✅ Successfully fetched indicators list.", severity="information")
                    return indicators
                else:
                    self.app.notify("⚠ No indicators found in the response.", severity="warning")
                    return []
            except ValueError:
                self.app.notify("❌ Error parsing JSON response for indicators.", severity="error")
                return []
        except requests.exceptions.Timeout:
            self.app.notify("❌ Request to fetch indicators timed out.", severity="error")
            return []
        except requests.exceptions.RequestException as e:
            self.app.notify(f"❌ Failed to fetch indicators: {e}", severity="error")
            return []

    async def fetch_economic_data(self, country_code):
        """Fetch economic indicator data dynamically based on the selected country code."""
        economic_data = []
        try:
            # Run HTTP requests concurrently for all indicators
            tasks = []
            async with httpx.AsyncClient() as client:
                for indicator in self.all_indicators:
                    ticker = f"{indicator['code']}{country_code}"
                    api_url = BASE_API_URL.format(ticker=ticker)
                    tasks.append(client.get(api_url))

                # Await all HTTP responses
                responses = await asyncio.gather(*tasks)

            # Process responses
            for response, indicator in zip(responses, self.all_indicators):
                if response.status_code == 200:
                    api_data = response.json()
                    if "data" in api_data and "values" in api_data["data"] and api_data["data"]["values"]:
                        economic_data.append({
                            "indicator_name": indicator["verbose"],
                            "ticker": f"{indicator['code']}{country_code}",
                            "frequency": api_data.get("frequency", "Unknown"),
                            "latest_value": api_data["data"]["values"][-1] if api_data["data"]["values"] else "N/A"
                        })

            self.display_economic_data(economic_data, country_code)
        except Exception as e:
            self.app.notify(f"❌ Error fetching economic data: {e}")
    def display_economic_data(self, economic_data, country_code):
        """Display the fetched economic indicator data in DataTable."""
        data_table = self.query_one("#economic_data_table", DataTable)
        data_table.clear()
        if not data_table.columns:
            data_table.add_column("Indicator Name")
            data_table.add_column("Ticker")
            data_table.add_column("Frequency")
            data_table.add_column("Latest Value")

        if not economic_data:
            self.app.notify(f"⚠ No data found for {country_code}.", severity="warning")
            return

        for indicator in economic_data:
            data_table.add_row(
                indicator["indicator_name"],
                indicator["ticker"],
                indicator["frequency"],
                str(indicator["latest_value"]),
            )

        self.app.notify(f"✅ Fetched {len(economic_data)} indicators for {country_code}.")

    async def fetch_historical_data(self, country_code):
        """Fetch historical data for economic indicators and separate it by frequency."""
        historical_monthly = {}
        historical_quarterly = {}
        try:
            # Run HTTP requests concurrently for all indicators
            tasks = []
            async with httpx.AsyncClient() as client:
                for indicator in self.all_indicators:
                    ticker = f"{indicator['code']}{country_code}"
                    api_url = BASE_API_URL.format(ticker=ticker)
                    tasks.append(client.get(api_url))

                # Await all HTTP responses
                responses = await asyncio.gather(*tasks)

            # Process responses
            for response, indicator in zip(responses, self.all_indicators):
                if response.status_code == 200:
                    api_data = response.json()
                    if "data" in api_data and "values" in api_data["data"] and api_data["data"]["values"]:
                        frequency = api_data.get("frequency", "Unknown")
                        for date, value in zip(api_data["data"]["dates"], api_data["data"]["values"]):
                            if frequency == "M":
                                historical_monthly.setdefault(date, {})[f"{indicator['code']}{country_code}"] = value
                            elif frequency == "Q":
                                historical_quarterly.setdefault(date, {})[f"{indicator['code']}{country_code}"] = value

            self.display_historical_data(historical_monthly, historical_quarterly)
        except Exception as e:
            self.app.notify(f"❌ Error fetching historical data: {e}")

    def display_historical_data(self, historical_monthly, historical_quarterly):
        """Display historical data in separate tables for monthly and quarterly data."""

        def populate_table(table, data, tickers):
            """Helper function to populate a DataTable with historical data."""
            # Clear the table entirely before repopulating
            table.clear()  # Clears rows and columns

            # Reset columns to ensure no duplicates
            table.columns.clear()  # Removes all existing columns
            table.add_column("Date")  # First column is always Date
            for ticker in tickers:
                table.add_column(ticker)  # Add a column for each ticker

            # Populate rows with the correct data
            for date in sorted(data.keys(), reverse=True):  # Sort dates in descending order
                row = [date]  # Start the row with the date
                for ticker in tickers:
                    row.append(str(data.get(date, {}).get(ticker, "N/A")))  # Fetch data or use "N/A"

                # Add row to the table
                table.add_row(*row)

        # Process Monthly Data
        monthly_table = self.query_one("#monthly_data_table", DataTable)
        monthly_tickers = sorted(set(ticker for date_values in historical_monthly.values() for ticker in date_values))
        populate_table(monthly_table, historical_monthly, monthly_tickers)

        # Process Quarterly Data
        quarterly_table = self.query_one("#quarterly_data_table", DataTable)
        quarterly_tickers = sorted(
            set(ticker for date_values in historical_quarterly.values() for ticker in date_values))
        populate_table(quarterly_table, historical_quarterly, quarterly_tickers)

        # Notify the user
        self.app.notify(
            f"✅ Displayed {len(historical_monthly)} monthly and {len(historical_quarterly)} quarterly data points."
        )

    async def on_option_list_option_selected(self, event: OptionList.OptionSelected):
        """Handle selection events for country selection."""
        if event.option_list.id == "country_selector":
            selected_country = event.option_list.get_option_at_index(event.option_index).prompt
            country_code = selected_country.split(" (")[-1].strip(")")
            self.app.notify(f"✅ Selected Country: {selected_country}")
            asyncio.create_task(self.fetch_all_data(country_code))

    async def fetch_all_data(self, country_code):
        """Fetch all data asynchronously."""
        try:
            # # Fetch economic and historical data concurrently
            # await asyncio.gather(
            #     asyncio.create_task(self.fetch_economic_data(country_code)),
            #     asyncio.create_task(self.fetch_historical_data(country_code))
            # )
            await self.fetch_economic_data(country_code)
            await self.fetch_historical_data(country_code)
            self.app.notify("✅ All data loaded successfully!")
        except Exception as e:
            self.app.notify(f"❌ Error loading data: {e}")

    async def on_button_pressed(self, event: Button.Pressed):
        """Handle button presses for pagination."""
        if event.button.id in ["next_country_page", "previous_country_page"]:
            self.current_country_page += 1 if "next" in event.button.id else -1
            self.display_countries()

    def _get_pagination_range(self, current_page, data_list):
        """Calculate start and end indices for pagination."""
        start = current_page * self.ITEMS_PER_PAGE
        end = start + self.ITEMS_PER_PAGE
        return start, min(end, len(data_list))

    def _update_pagination_controls(self, prev_button_id, next_button_id, current_page, total_items):
        """Enable/Disable pagination buttons based on available pages."""
        self.query_one(f"#{prev_button_id}", Button).disabled = current_page == 0
        self.query_one(f"#{next_button_id}", Button).disabled = (current_page + 1) * self.ITEMS_PER_PAGE >= total_items


class EconomyInDetailTab(VerticalScroll):
    """Class for the 'Economy in Detail' tab with country selection and plot widgets."""

    COUNTRY_API_URL = "https://www.econdb.com/static/appdata/econdb-countries.json"

    def __init__(self):
        super().__init__()
        self.selected_country_code = None  # Track the selected country code
        self.countries = []  # List of available countries
        self.plots = {}  # Dictionary to store PlotextPlot widgets

    def compose(self):
        """Compose the UI for the Economy in Detail tab."""
        # Add country selection
        yield Static("Select a Country:", id="country_header")
        with Collapsible():
            yield OptionList(id="country_selector")

        # Add sub-tabs for data visualization
        with TabbedContent():
            with TabPane("National Accounts"):
                with Container(classes="national_accounts"):
                    yield DataTable(id="gdp_growth_table")
                    yield DataTable(id="contribution_gdp_table")
                    yield DataTable(id="imports_exports_table")
                    yield DataTable(id="niip_table")
                    yield DataTable(id="government_finances_table")
            with TabPane("Price and Labour"):
                with Container(classes="price_labour"):
                    yield DataTable(id="cpi_table")
                    yield DataTable(id="supermarket_index_table")
                    yield DataTable(id="unemployment_rate_table")
            with TabPane("Interest and Money"):
                with Container(classes="interest_money"):
                    yield DataTable(id="money_supply_table")
                    yield DataTable(id="policy_rate_table")
            with TabPane("Port Activity"):
                with Container(classes="port_activity"):
                    yield DataTable(id="port_calls_table")
                    yield DataTable(id="port_waiting_times_table")
                    yield DataTable(id="container_trade_table")
            with TabPane("Oil and Gas"):
                with Container(classes="oil_gas"):
                    yield DataTable(id="oil_balance_table")
                    yield DataTable(id="crude_oil_stock_table")
                    yield DataTable(id="crude_oil_forward_cover_table")


    async def on_show(self):
        """Triggered when the tab becomes visible. Loads country data if not already loaded."""
        if not self.countries:
            await self.load_countries()

    async def load_countries(self):
        """Fetch and populate the list of available countries dynamically."""
        try:
            response = requests.get(self.COUNTRY_API_URL)
            response.raise_for_status()
            country_data = response.json()

            # Deduplicate and store country options
            self.countries = sorted(
                {country["iso2"]: country["verbose"] for country in country_data}.items()
            )
            country_selector = self.query_one("#country_selector", OptionList)
            for iso2, verbose in self.countries:
                country_selector.add_option(f"{verbose} ({iso2})")

            self.notify("✅ Loaded countries successfully.")
        except requests.RequestException as e:
            self.query_one("#country_header", Static).update(f"Error loading countries: {e}")

    async def on_option_list_option_selected(self, event: OptionList.OptionSelected):
        """Handle selection events for country selection."""
        if event.option_list.id == "country_selector":
            selected_country = event.option_list.get_option_at_index(event.option_index).prompt
            country_code = selected_country.split(" (")[-1].strip(")")
            self.selected_country_code = country_code
            self.notify(f"✅ Selected Country: {selected_country}")
            await self.load_plots()

    async def load_plots(self):
        await self.process_gdp_data()
        await self.process_contribution_to_gdp()
        await self.process_imports_exports()
        await self.process_niip_data()
        await self.process_government_finances()
        await self.process_cpi_data()
        await self.process_supermarket_index()
        await self.process_unemployment_rate()
        await self.process_money_supply_data()
        await self.process_port_calls_data()
        await self.process_port_waiting_times()
        await self.process_container_trade_data()
        await self.process_oil_balance_data()
        await self.process_crude_oil_stock_data()
        await self.process_crude_oil_forward_cover()

    async def process_gdp_data(self):
        """Fetch and display GDP growth data dynamically for the selected country (DataTable)."""
        if not self.selected_country_code:
            self.notify("⚠ No country selected! Please select a country.")
            return

        api_url = f"https://www.econdb.com/widgets/real-gdp-growth/data/?country={self.selected_country_code}&format=json"
        data_table = self.query_one("#gdp_growth_table", DataTable)

        try:
            async with httpx.AsyncClient() as client:
                response = await client.get(api_url)
                response.raise_for_status()
                data = response.json()

                # Extract GDP data
                plots = data.get("plots", [])
                if not plots:
                    self.notify(f"⚠ No data available for GDP growth in {self.selected_country_code}")
                    return

                plot_data = plots[0]
                series = plot_data.get("series", [])
                if not series:
                    self.notify(f"⚠ No GDP series data for {self.selected_country_code}")
                    return

                gdp_code = series[0].get("code")  # The GDP indicator code (e.g., "RGDP")
                gdp_name = series[0].get("name", "GDP Growth")

                # Clear the table and add columns
                data_table.clear()
                if not data_table.columns:
                    data_table.add_column("Date")
                    data_table.add_column("Growth (%)")

                chart_data = {"labels": [], "values": []}  # Store data for the bar chart

                # Populate the table with GDP data
                for entry in plot_data.get("data", []):
                    date = datetime.strptime(entry["Date"][:10], "%Y-%m-%d")
                    growth = entry.get(gdp_code, None)  # Extract the GDP growth value

                    if growth is not None:  # Ignore missing data
                        formatted_date = date.strftime("%b %Y")  # Format: "Jan 2020"
                        data_table.add_row(formatted_date, f"{growth:.2f}%")

                        # Collect chart data
                        chart_data["labels"].append(formatted_date)
                        chart_data["values"].append(growth)

                # ✅ Create and mount the ChartWidget instead of yielding it
                if chart_data["labels"] and chart_data["values"]:
                    chart_widget = ChartWidget(chart_type="bar", raw_data=chart_data, title="GDP Growth", classes="gdp_growth")
                    await self.mount(chart_widget)

                self.notify(f"✅ Displayed GDP growth data for {self.selected_country_code}")

        except Exception as e:
            self.notify(f"⚠️ GDP Error: {str(e)}")

    async def process_contribution_to_gdp(self):
        """Fetch and display Contributions to GDP data dynamically for the selected country (DataTable) and generate a Multi-Bar Chart."""
        if not self.selected_country_code:
            self.notify("⚠ No country selected! Please select a country.")
            return

        api_url = f"https://www.econdb.com/widgets/contributions-to-gdp/data/?country={self.selected_country_code}&format=json"
        data_table = self.query_one("#contribution_gdp_table", DataTable)

        try:
            async with httpx.AsyncClient() as client:
                response = await client.get(api_url)
                response.raise_for_status()
                data = response.json()

                # Extract Contribution to GDP data
                plots = data.get("plots", [])
                if not plots:
                    self.notify(f"⚠ No data available for Contribution to GDP in {self.selected_country_code}")
                    return

                plot_data = plots[0]
                series = plot_data.get("series", [])
                if not series:
                    self.notify(f"⚠ No Contribution to GDP series data for {self.selected_country_code}")
                    return

                # Extract sector names and their codes
                sector_columns = {serie["name"]: serie["code"] for serie in series if "code" in serie}

                # Clear the table and add columns
                data_table.clear()
                if not data_table.columns:
                    data_table.add_column("Date")
                    for sector_name in sector_columns.keys():
                        data_table.add_column(sector_name)

                # Store data for multi-bar chart
                chart_categories = []  # Dates
                series_data = {sector: [] for sector in sector_columns.keys()}  # Sector-wise values

                # Populate rows for the DataTable
                for entry in plot_data.get("data", []):
                    # Parse and format the date
                    date = datetime.strptime(entry["Date"][:10], "%Y-%m-%d").strftime("%b %Y")

                    # Extract values for all sectors
                    row = [date]
                    for sector_name, sector_code in sector_columns.items():
                        value = entry.get(sector_code, "N/A")
                        formatted_value = f"{value:.2f}" if isinstance(value, (float, int)) else "N/A"
                        row.append(formatted_value)

                        # Add values to the multi-bar chart dataset
                        if isinstance(value, (float, int)):
                            series_data[sector_name].append(value)
                        else:
                            series_data[sector_name].append(0)  # Handle missing values

                    # Add row to the DataTable
                    data_table.add_row(*row)
                    chart_categories.append(date)

                # ✅ Ensure valid data exists before creating the chart
                if chart_categories and any(len(v) > 0 for v in series_data.values()):
                    chart_widget = ChartWidget(
                        chart_type="multi_bar",
                        raw_data={"categories": chart_categories, "series_data": series_data},
                        title="Contribution to GDP",
                        classes="contribution_to_gdp"
                    )
                    await self.mount(chart_widget)
                    self.notify(f"✅ Displayed Contribution to GDP data for {self.selected_country_code}")
                else:
                    self.notify("⚠ No valid data available for Contribution to GDP chart.")

        except Exception as e:
            self.notify(f"⚠️ Contribution to GDP Error: {str(e)}")

    async def process_imports_exports(self):
        """Fetch and display the last 10 Imports and Exports data entries dynamically for the selected country (DataTable)."""
        if not self.selected_country_code:
            self.notify("⚠ No country selected! Please select a country.")
            return

        api_url = f"https://www.econdb.com/widgets/imports-exports/data/?country={self.selected_country_code}&format=json"
        data_table = self.query_one("#imports_exports_table", DataTable)

        try:
            async with httpx.AsyncClient() as client:
                response = await client.get(api_url)
                response.raise_for_status()
                data = response.json()

                # Extract Imports and Exports data
                plots = data.get("plots", [])
                if not plots:
                    self.notify(f"⚠ No data available for Imports and Exports in {self.selected_country_code}")
                    return

                plot_data = plots[0]
                trade_data = plot_data.get("data", [])

                # Show only the last 10 records
                trade_data = trade_data[-10:]

                # Prepare data for the DataTable
                data_table.clear()
                if not data_table.columns:
                    data_table.add_column("Date")
                    data_table.add_column("Exports (Millions USD)")
                    data_table.add_column("Imports (Millions USD)")
                    data_table.add_column("Exports (% of GDP)")
                    data_table.add_column("Imports (% of GDP)")

                # Prepare data for the Multi-Bar Chart
                categories = []
                series_data = {"Exports": [], "Imports": []}

                # Populate rows for the DataTable and Multi-Bar Chart
                for entry in trade_data:
                    # Parse and format the date
                    date = datetime.strptime(entry["Date"], "%Y-%m-%d").strftime("%b %Y")
                    categories.append(date)

                    # Extract values for exports, imports, and percentages
                    exports = entry.get("EXPMON", "N/A")
                    imports = entry.get("IMPMON", 0)  # Keep the sign of imports
                    exp_pct = entry.get("EXP_PCT", "N/A")
                    imp_pct = entry.get("IMP_PCT", "N/A")

                    # Add data for the multi-bar chart
                    series_data["Exports"].append(exports if isinstance(exports, (int, float)) else 0)
                    series_data["Imports"].append(imports if isinstance(imports, (int, float)) else 0)

                    # Format values for the table
                    exports = f"{exports:,.0f}" if isinstance(exports, (int, float)) else "N/A"
                    imports = f"{imports:,.0f}" if isinstance(imports, (int, float)) else "N/A"
                    exp_pct = f"{exp_pct:.2f}%" if isinstance(exp_pct, (int, float)) else "N/A"
                    imp_pct = f"{imp_pct:.2f}%" if isinstance(imp_pct, (int, float)) else "N/A"

                    # Add row to the DataTable
                    data_table.add_row(date, exports, imports, exp_pct, imp_pct)

                # Generate the Multi-Bar Chart
                chart_widget = ChartWidget(
                    chart_type="multi_bar",
                    raw_data={"categories": categories, "series_data": series_data},
                    title="Imports and Exports Multi-Bar Chart",
                    classes="import_export"
                )
                await self.mount(chart_widget)  # Mount the ChartWidget to display the chart

                self.notify(f"✅ Displayed last 10 Imports and Exports records for {self.selected_country_code}")

        except Exception as e:
            self.notify(f"⚠️ Imports and Exports Error: {str(e)}")

    async def process_niip_data(self):
        """Fetch and display Net International Investment Position (NIIP) data dynamically for the selected country (DataTable)."""
        if not self.selected_country_code:
            self.notify("⚠ No country selected! Please select a country.")
            return

        api_url = f"https://www.econdb.com/widgets/niip/data/?country={self.selected_country_code}&format=json"
        data_table = self.query_one("#niip_table", DataTable)

        try:
            async with httpx.AsyncClient() as client:
                response = await client.get(api_url)
                response.raise_for_status()
                data = response.json()

                # Extract NIIP data
                plots = data.get("plots", [])
                if not plots:
                    self.notify(f"⚠ No data available for NIIP in {self.selected_country_code}")
                    return

                plot_data = plots[0]
                niip_data = plot_data.get("data", [])

                # Show only the last 10 records
                niip_data = niip_data[-10:]
                # Clear the table and add columns
                data_table.clear()
                if not data_table.columns:
                    data_table.add_column("Date")
                    data_table.add_column("Net Position (Mn. ARS)")
                    data_table.add_column("% of GDP")

                # Populate rows for the DataTable
                for entry in niip_data:
                    # Parse and format the date
                    date = datetime.strptime(entry["Date"], "%Y-%m-%d").strftime("%b %Y")

                    # Extract values for NIIP and % of GDP
                    niip_value = entry.get("NIIP", "N/A")
                    pct_gdp = entry.get("PCT_GDP", "N/A")

                    # Format values for the table
                    niip_value = f"{niip_value:,.2f}" if isinstance(niip_value, (int, float)) else "N/A"
                    pct_gdp = f"{pct_gdp:.2f}%" if isinstance(pct_gdp, (int, float)) else "N/A"

                    # Add row to the DataTable
                    data_table.add_row(date, niip_value, pct_gdp)

                self.notify(f"✅ Displayed NIIP data for {self.selected_country_code}")

        except Exception as e:
            self.notify(f"⚠️ NIIP Error: {str(e)}")

    async def process_government_finances(self):
        """Fetch and display Government Balance and Debt data dynamically for the selected country (DataTable)."""
        if not self.selected_country_code:
            self.notify("⚠ No country selected! Please select a country.")
            return

        api_url = f"https://www.econdb.com/widgets/public-finances/data/?country={self.selected_country_code}&format=json"
        data_table = self.query_one("#government_finances_table", DataTable)

        try:
            async with httpx.AsyncClient() as client:
                response = await client.get(api_url)
                response.raise_for_status()
                data = response.json()

                # Extract Government Balance and Debt data
                plots = data.get("plots", [])
                if not plots:
                    self.notify(f"⚠ No data available for Government Balance and Debt in {self.selected_country_code}")
                    return

                plot_data = plots[0]
                finance_data = plot_data.get("data", [])

                # Show only the last 10 records
                finance_data = finance_data[-10:]
                # Clear the table and add columns
                data_table.clear()
                if not data_table.columns:
                    data_table.add_column("Date")
                    data_table.add_column("Spending (% GDP)")
                    data_table.add_column("Revenue (% GDP)")
                    data_table.add_column("Balance (% GDP)")
                    data_table.add_column("Debt (% GDP)")

                # Populate rows for the DataTable
                for entry in finance_data:
                    # Parse and format the date
                    date = datetime.strptime(entry["Date"], "%Y-%m-%d").strftime("%b %Y")

                    # Extract values for Spending, Revenue, Balance, and Debt
                    spending = entry.get("GSPE", "N/A")
                    revenue = entry.get("GREV", "N/A")
                    balance = entry.get("GBAL", "N/A")
                    debt = entry.get("GDEBT", "N/A")

                    # Format values for the table
                    spending = f"{spending:.2f}%" if isinstance(spending, (int, float)) else "N/A"
                    revenue = f"{revenue:.2f}%" if isinstance(revenue, (int, float)) else "N/A"
                    balance = f"{balance:.2f}%" if isinstance(balance, (int, float)) else "N/A"
                    debt = f"{debt:.2f}%" if isinstance(debt, (int, float)) else "N/A"

                    # Add row to the DataTable
                    data_table.add_row(date, spending, revenue, balance, debt)

                self.notify(f"✅ Displayed Government Balance and Debt data for {self.selected_country_code}")

        except Exception as e:
            self.notify(f"⚠️ Government Finances Error: {str(e)}")

    async def process_cpi_data(self):
        """Fetch and display the last 10 records of Consumer Price Index (CPI) data dynamically for the selected country (DataTable)."""
        if not self.selected_country_code:
            self.notify("⚠ No country selected! Please select a country.")
            return

        api_url = f"https://www.econdb.com/widgets/cpi-annual-growth/data/?country={self.selected_country_code}&format=json"
        data_table = self.query_one("#cpi_table", DataTable)

        try:
            async with httpx.AsyncClient() as client:
                response = await client.get(api_url)
                response.raise_for_status()
                data = response.json()

                # Extract CPI data
                plots = data.get("plots", [])
                if not plots:
                    self.notify(f"⚠ No data available for CPI in {self.selected_country_code}")
                    return

                plot_data = plots[0]
                cpi_data = plot_data.get("data", [])

                # Show only the last 10 records
                cpi_data = cpi_data[-10:]

                # Clear the table and add columns
                data_table.clear()
                if not data_table.columns:
                    data_table.add_column("Date")
                    data_table.add_column("All-items CPI (% Change)")
                    data_table.add_column("Core CPI (% Change)")

                # Populate rows for the DataTable (only last 10 entries)
                for entry in cpi_data:
                    # Parse and format the date
                    date = datetime.strptime(entry["Date"], "%Y-%m-%d").strftime("%b %Y")

                    # Extract values for CPI categories
                    all_items_cpi = entry.get("ALL", "N/A")
                    core_cpi = entry.get("CORE", "N/A")

                    # Format values for the table
                    all_items_cpi = f"{all_items_cpi:.2f}%" if isinstance(all_items_cpi, (int, float)) else "N/A"
                    core_cpi = f"{core_cpi:.2f}%" if isinstance(core_cpi, (int, float)) else "N/A"

                    # Add row to the DataTable
                    data_table.add_row(date, all_items_cpi, core_cpi)

                self.notify(f"✅ Displayed last 10 CPI records for {self.selected_country_code}")

        except Exception as e:
            self.notify(f"⚠️ CPI Error: {str(e)}")

    async def process_supermarket_index(self):
        """Fetch and display the last 10 records of the Supermarket Price Index dynamically for the selected country (DataTable)."""
        if not self.selected_country_code:
            self.notify("⚠ No country selected! Please select a country.")
            return

        api_url = f"https://www.econdb.com/widgets/supermarket-country-index/data/?country={self.selected_country_code}&format=json"
        data_table = self.query_one("#supermarket_index_table", DataTable)

        try:
            async with httpx.AsyncClient() as client:
                response = await client.get(api_url)
                response.raise_for_status()
                data = response.json()

                # Extract Supermarket Index data
                plots = data.get("plots", [])
                if not plots:
                    self.notify(f"⚠ No data available for Supermarket Price Index in {self.selected_country_code}")
                    return

                plot_data = plots[0]
                index_data = plot_data.get("data", [])

                # Show only the last 10 records
                index_data = index_data[-10:]

                # Clear the table and add columns
                data_table.clear()
                if not data_table.columns:
                    data_table.add_column("Date")
                    data_table.add_column("Real Time Index")
                    data_table.add_column("CPIAR")

                # Populate rows for the DataTable (only last 10 entries)
                for entry in index_data:
                    # Parse and format the date
                    date = datetime.strptime(entry["Date"], "%Y-%m-%d").strftime("%b %Y")

                    # Extract values for Supermarket Index and CPIAR
                    real_time_index = entry.get("Real time index", "N/A")
                    cpiar_value = entry.get("CPIAR", "N/A")

                    # Format values for the table
                    real_time_index = f"{real_time_index:.2f}" if isinstance(real_time_index, (int, float)) else "N/A"
                    cpiar_value = f"{cpiar_value:.2f}" if isinstance(cpiar_value, (int, float)) else "N/A"

                    # Add row to the DataTable
                    data_table.add_row(date, real_time_index, cpiar_value)

                self.notify(f"✅ Displayed last 10 Supermarket Price Index records for {self.selected_country_code}")

        except Exception as e:
            self.notify(f"⚠️ Supermarket Price Index Error: {str(e)}")

    async def process_unemployment_rate(self):
        """Fetch and display the last 10 records of the Unemployment Rate dynamically for the selected country (DataTable)."""
        if not self.selected_country_code:
            self.notify("⚠ No country selected! Please select a country.")
            return

        api_url = f"https://www.econdb.com/widgets/unemployment-rate/data/?country={self.selected_country_code}&format=json"
        data_table = self.query_one("#unemployment_rate_table", DataTable)

        try:
            async with httpx.AsyncClient() as client:
                response = await client.get(api_url)
                response.raise_for_status()
                data = response.json()

                # Extract Unemployment Rate data
                plots = data.get("plots", [])
                if not plots:
                    self.notify(f"⚠ No data available for Unemployment Rate in {self.selected_country_code}")
                    return

                plot_data = plots[0]
                unemployment_data = plot_data.get("data", [])

                # Show only the last 10 records
                unemployment_data = unemployment_data[-10:]

                # Clear the table and add columns
                data_table.clear()
                if not data_table.columns:
                    data_table.add_column("Date")
                    data_table.add_column("Unemployment Rate (%)")

                # Populate rows for the DataTable (only last 10 entries)
                for entry in unemployment_data:
                    # Parse and format the date
                    date = datetime.strptime(entry["Date"], "%Y-%m-%d").strftime("%b %Y")

                    # Extract values for the Unemployment Rate
                    unemployment_rate = entry.get("URATE", "N/A")

                    # Format values for the table
                    unemployment_rate = f"{unemployment_rate:.2f}%" if isinstance(unemployment_rate,
                                                                                  (int, float)) else "N/A"

                    # Add row to the DataTable
                    data_table.add_row(date, unemployment_rate)

                self.notify(f"✅ Displayed last 10 Unemployment Rate records for {self.selected_country_code}")

        except Exception as e:
            self.notify(f"⚠️ Unemployment Rate Error: {str(e)}")

    async def process_money_supply_data(self):
        """Fetch and display the last 10 records of Money Supply (M3) and Nominal GDP dynamically for the selected country (DataTable)."""
        if not self.selected_country_code:
            self.notify("⚠ No country selected! Please select a country.")
            return

        api_url = f"https://www.econdb.com/widgets/money-supply/data/?country={self.selected_country_code}&format=json"
        data_table = self.query_one("#money_supply_table", DataTable)

        try:
            async with httpx.AsyncClient() as client:
                response = await client.get(api_url)
                response.raise_for_status()
                data = response.json()

                # Extract Money Supply and GDP data
                plots = data.get("plots", [])
                if not plots:
                    self.notify(f"⚠ No data available for Money Supply and GDP in {self.selected_country_code}")
                    return

                plot_data = plots[0]
                money_supply_data = plot_data.get("data", [])

                # Show only the last 10 records
                money_supply_data = money_supply_data[-10:]

                # Clear the table and add columns
                data_table.clear()
                if not data_table.columns:
                    data_table.add_column("Date")
                    data_table.add_column("M3 Money Supply (% Change)")
                    data_table.add_column("Nominal GDP (% Change)")

                # Populate rows for the DataTable (only last 10 entries)
                for entry in money_supply_data:
                    # Parse and format the date
                    date = datetime.strptime(entry["Date"], "%Y-%m-%d").strftime("%b %Y")

                    # Extract values for M3 Money Supply and Nominal GDP
                    m3_value = entry.get("M3", "N/A")
                    gdp_value = entry.get("GDP", "N/A")

                    # Format values for the table
                    m3_value = f"{m3_value:.2f}%" if isinstance(m3_value, (int, float)) else "N/A"
                    gdp_value = f"{gdp_value:.2f}%" if isinstance(gdp_value, (int, float)) else "N/A"

                    # Add row to the DataTable
                    data_table.add_row(date, m3_value, gdp_value)

                self.notify(f"✅ Displayed last 10 Money Supply and GDP records for {self.selected_country_code}")

        except Exception as e:
            self.notify(f"⚠️ Money Supply Error: {str(e)}")

    async def process_policy_rate_data(self):
        """Fetch and display the last 10 records of the Monetary Policy Rate dynamically for the selected country (DataTable)."""
        if not self.selected_country_code:
            self.notify("⚠ No country selected! Please select a country.")
            return

        api_url = f"https://www.econdb.com/widgets/policy-rate/data/?country={self.selected_country_code}&format=json"
        data_table = self.query_one("#policy_rate_table", DataTable)

        try:
            async with httpx.AsyncClient() as client:
                response = await client.get(api_url)
                response.raise_for_status()
                data = response.json()

                # Extract Monetary Policy Rate data
                plots = data.get("plots", [])
                if not plots:
                    self.notify(f"⚠ No data available for Monetary Policy Rate in {self.selected_country_code}")
                    return

                plot_data = plots[0]
                policy_rate_data = plot_data.get("data", [])

                # Show only the last 10 records
                policy_rate_data = policy_rate_data[-10:]

                # Clear the table and add columns
                data_table.clear()
                if not data_table.columns:
                    data_table.add_column("Date")
                    data_table.add_column("Central Bank Policy Rate (%)")

                # Populate rows for the DataTable (only last 10 entries)
                for entry in policy_rate_data:
                    # Parse and format the date
                    date = datetime.strptime(entry["Date"], "%Y-%m-%d").strftime("%b %Y")

                    # Extract values for Monetary Policy Rate
                    policy_rate = entry.get("POLIR", "N/A")

                    # Format values for the table
                    policy_rate = f"{policy_rate:.2f}%" if isinstance(policy_rate, (int, float)) else "N/A"

                    # Add row to the DataTable
                    data_table.add_row(date, policy_rate)

                self.notify(f"✅ Displayed last 10 Monetary Policy Rate records for {self.selected_country_code}")

        except Exception as e:
            self.notify(f"⚠️ Monetary Policy Rate Error: {str(e)}")

    async def process_port_calls_data(self):
        """Fetch and display the last 10 records of Port Calls by Vessel Class (Throughput) dynamically for the selected country (DataTable)."""
        if not self.selected_country_code:
            self.notify("⚠ No country selected! Please select a country.")
            return

        api_url = f"https://www.econdb.com/widgets/port-time-series/data/?type=arrivals&unit=throughput&country={self.selected_country_code}&format=json"
        data_table = self.query_one("#port_calls_table", DataTable)

        try:
            async with httpx.AsyncClient() as client:
                response = await client.get(api_url)
                response.raise_for_status()
                data = response.json()

                # Extract Port Calls data
                plots = data.get("plots", [])
                if not plots:
                    self.notify(f"⚠ No data available for Port Calls in {self.selected_country_code}")
                    return

                plot_data = plots[0]
                port_calls_data = plot_data.get("data", [])

                # Show only the last 10 records
                port_calls_data = port_calls_data[-10:]

                # Extract vessel categories dynamically
                series = plot_data.get("series", [])
                vessel_classes = {serie["name"]: serie["code"] for serie in series if "code" in serie}

                # Clear the table and add columns
                data_table.clear()
                if not data_table.columns:
                    data_table.add_column("Date")
                    for vessel_class in vessel_classes.keys():
                        data_table.add_column(vessel_class)

                # Populate rows for the DataTable (only last 10 entries)
                for entry in port_calls_data:
                    # Parse and format the date
                    date = datetime.strptime(entry["Date"], "%Y-%m-%d").strftime("%b %Y")

                    # Extract values for each vessel class
                    row = [date]
                    for vessel_code in vessel_classes.values():
                        value = entry.get(vessel_code, "N/A")
                        row.append(f"{value:,}" if isinstance(value, (int, float)) else "N/A")

                    # Add row to the DataTable
                    data_table.add_row(*row)

                self.notify(f"✅ Displayed last 10 Port Calls records for {self.selected_country_code}")

        except Exception as e:
            self.notify(f"⚠️ Port Calls Error: {str(e)}")

    async def process_port_waiting_times(self):
        """Fetch and display the last 10 records of Port Waiting Times dynamically for the selected country (DataTable)."""
        if not self.selected_country_code:
            self.notify("⚠ No country selected! Please select a country.")
            return

        api_url = f"https://www.econdb.com/widgets/port-waiting-times/data/?country={self.selected_country_code}&format=json"
        data_table = self.query_one("#port_waiting_times_table", DataTable)

        try:
            async with httpx.AsyncClient() as client:
                response = await client.get(api_url)
                response.raise_for_status()
                data = response.json()

                # Extract Port Waiting Times data
                plots = data.get("plots", [])
                if not plots:
                    self.notify(f"⚠ No data available for Port Waiting Times in {self.selected_country_code}")
                    return

                plot_data = plots[0]
                waiting_times_data = plot_data.get("data", [])

                # Show only the last 10 records
                waiting_times_data = waiting_times_data[-10:]

                # Extract categories dynamically
                series = plot_data.get("series", [])
                categories = {serie["name"]: serie["code"] for serie in series if "code" in serie}

                # Clear the table and add columns
                data_table.clear()
                if not data_table.columns:
                    data_table.add_column("Date")
                    for category in categories.keys():
                        data_table.add_column(category)

                # Populate rows for the DataTable (only last 10 entries)
                for entry in waiting_times_data:
                    # Parse and format the date
                    date = datetime.strptime(entry["Date"], "%Y-%m-%d").strftime("%b %Y")

                    # Extract values for each category
                    row = [date]
                    for category_code in categories.values():
                        value = entry.get(category_code, "N/A")
                        row.append(f"{value:.2f}" if isinstance(value, (int, float)) else "N/A")

                    # Add row to the DataTable
                    data_table.add_row(*row)

                self.notify(f"✅ Displayed last 10 Port Waiting Times records for {self.selected_country_code}")

        except Exception as e:
            self.notify(f"⚠️ Port Waiting Times Error: {str(e)}")

    async def process_container_trade_data(self):
        """Fetch and display the last 10 records of Container Trade (TEU, full) dynamically for the selected country (DataTable)."""
        if not self.selected_country_code:
            self.notify("⚠ No country selected! Please select a country.")
            return

        api_url = f"https://www.econdb.com/widgets/sea-trade-by-week/data/?country={self.selected_country_code}&format=json"
        data_table = self.query_one("#container_trade_table", DataTable)

        try:
            async with httpx.AsyncClient() as client:
                response = await client.get(api_url)
                response.raise_for_status()
                data = response.json()

                # Extract Container Trade data
                plots = data.get("plots", [])
                if not plots:
                    self.notify(f"⚠ No data available for Container Trade in {self.selected_country_code}")
                    return

                plot_data = plots[0]
                container_trade_data = plot_data.get("data", [])

                # Show only the last 10 records
                container_trade_data = container_trade_data[-10:]

                # Clear the table and add columns
                data_table.clear()
                if not data_table.columns:
                    data_table.add_column("Date")
                    data_table.add_column("Exports (TEU)")
                    data_table.add_column("Imports (TEU)")

                # Populate rows for the DataTable (only last 10 entries)
                for entry in container_trade_data:
                    # Parse and format the date
                    date = datetime.strptime(entry["Date"], "%Y-%m-%d").strftime("%b %Y")

                    # Extract values for Exports and Imports
                    exports = entry.get("Exports", "N/A")
                    imports = entry.get("Imports", "N/A")

                    # Format values for the table
                    exports = f"{exports:,}" if isinstance(exports, (int, float)) else "N/A"
                    imports = f"{imports:,}" if isinstance(imports, (int, float)) else "N/A"

                    # Add row to the DataTable
                    data_table.add_row(date, exports, imports)

                self.notify(f"✅ Displayed last 10 Container Trade records for {self.selected_country_code}")

        except Exception as e:
            self.notify(f"⚠️ Container Trade Error: {str(e)}")

    async def process_oil_balance_data(self):
        """Fetch and display the last 10 records of Oil Production and Demand dynamically for the selected country (DataTable)."""
        if not self.selected_country_code:
            self.notify("⚠ No country selected! Please select a country.")
            return

        api_url = f"https://www.econdb.com/widgets/oil-balance/data/?country={self.selected_country_code}&format=json"
        data_table = self.query_one("#oil_balance_table", DataTable)

        try:
            async with httpx.AsyncClient() as client:
                response = await client.get(api_url)
                response.raise_for_status()
                data = response.json()

                # Extract Oil Balance data
                plots = data.get("plots", [])
                if not plots:
                    self.notify(f"⚠ No data available for Oil Balance in {self.selected_country_code}")
                    return

                plot_data = plots[0]
                oil_data = plot_data.get("data", [])

                # Show only the last 10 records
                oil_data = oil_data[-10:]

                # Extract categories dynamically
                series = plot_data.get("series", [])
                oil_categories = {serie["name"]: serie["code"] for serie in series if "code" in serie}

                # Clear the table and add columns
                data_table.clear()
                if not data_table.columns:
                    data_table.add_column("Date")
                    for category in oil_categories.keys():
                        data_table.add_column(category)

                # Populate rows for the DataTable (only last 10 entries)
                for entry in oil_data:
                    # Parse and format the date
                    date = datetime.strptime(entry["Date"], "%Y-%m-%d").strftime("%b %Y")

                    # Extract values for each oil category
                    row = [date]
                    for category_code in oil_categories.values():
                        value = entry.get(category_code, "N/A")
                        row.append(f"{value:,.2f}" if isinstance(value, (int, float)) else "N/A")

                    # Add row to the DataTable
                    data_table.add_row(*row)

                self.notify(f"✅ Displayed last 10 Oil Balance records for {self.selected_country_code}")

        except Exception as e:
            self.notify(f"⚠️ Oil Balance Error: {str(e)}")

    async def process_crude_oil_stock_data(self):
        """Fetch and display the last 10 records of Crude Oil Stocks dynamically for the selected country (DataTable)."""
        if not self.selected_country_code:
            self.notify("⚠ No country selected! Please select a country.")
            return

        api_url = f"https://www.econdb.com/widgets/energy-storage/data/?commodity=CRUDEOIL&country={self.selected_country_code}&format=json"
        data_table = self.query_one("#crude_oil_stock_table", DataTable)

        try:
            async with httpx.AsyncClient() as client:
                response = await client.get(api_url)
                response.raise_for_status()
                data = response.json()

                # Extract Crude Oil Stock data
                plots = data.get("plots", [])
                if not plots:
                    self.notify(f"⚠ No data available for Crude Oil Stocks in {self.selected_country_code}")
                    return

                plot_data = plots[0]
                crude_oil_data = plot_data.get("data", [])

                # Show only the last 10 records
                crude_oil_data = crude_oil_data[-10:]

                # Clear the table and add columns
                data_table.clear()
                if not data_table.columns:
                    data_table.add_column("Date")
                    data_table.add_column("2020 Stock (M Barrels)")
                    data_table.add_column("5-Year Avg (M Barrels)")
                    data_table.add_column("5-Year Range Min (M Barrels)")
                    data_table.add_column("5-Year Range Max (M Barrels)")

                # Populate rows for the DataTable (only last 10 entries)
                for entry in crude_oil_data:
                    # Parse and format the date
                    date = datetime.strptime(entry["Date"], "%Y-%m-%d").strftime("%b %Y")

                    # Extract values for Crude Oil Stocks
                    stock_2020 = entry.get("2020", "N/A")
                    mean_stock = entry.get("mean", "N/A")
                    range_stock = entry.get("range", ["N/A", "N/A"])

                    # Extract range min and max
                    range_min, range_max = range_stock if isinstance(range_stock, list) and len(range_stock) == 2 else [
                        "N/A", "N/A"]

                    # Format values for the table
                    stock_2020 = f"{stock_2020:.3f}" if isinstance(stock_2020, (int, float)) else "N/A"
                    mean_stock = f"{mean_stock:.3f}" if isinstance(mean_stock, (int, float)) else "N/A"
                    range_min = f"{range_min:.3f}" if isinstance(range_min, (int, float)) else "N/A"
                    range_max = f"{range_max:.3f}" if isinstance(range_max, (int, float)) else "N/A"

                    # Add row to the DataTable
                    data_table.add_row(date, stock_2020, mean_stock, range_min, range_max)

                self.notify(f"✅ Displayed last 10 Crude Oil Stock records for {self.selected_country_code}")

        except Exception as e:
            self.notify(f"⚠️ Crude Oil Stock Error: {str(e)}")

    async def process_crude_oil_forward_cover(self):
        """Fetch and display the last 10 records of Crude Oil Forward Cover dynamically for the selected country (DataTable)."""
        if not self.selected_country_code:
            self.notify("⚠ No country selected! Please select a country.")
            return

        api_url = f"https://www.econdb.com/widgets/energy-forward-cover/data/?commodity=CRUDEOIL&country={self.selected_country_code}&format=json"
        data_table = self.query_one("#crude_oil_forward_cover_table", DataTable)

        try:
            async with httpx.AsyncClient() as client:
                response = await client.get(api_url)
                response.raise_for_status()
                data = response.json()

                # Extract Crude Oil Forward Cover data
                plots = data.get("plots", [])
                if not plots:
                    self.notify(f"⚠ No data available for Crude Oil Forward Cover in {self.selected_country_code}")
                    return

                plot_data = plots[0]
                forward_cover_data = plot_data.get("data", [])

                # Show only the last 10 records
                forward_cover_data = forward_cover_data[-10:]

                # Clear the table and add columns
                data_table.clear()
                if not data_table.columns:
                    data_table.add_column("Date")
                    data_table.add_column("Forward Cover (Days)")

                # Populate rows for the DataTable (only last 10 entries)
                for entry in forward_cover_data:
                    # Parse and format the date
                    date = datetime.strptime(entry["Date"][:10], "%Y-%m-%d").strftime("%b %Y")

                    # Extract values for Forward Cover in Days
                    forward_cover_days = entry.get("Days", "N/A")

                    # Format values for the table
                    forward_cover_days = f"{forward_cover_days:.2f}" if isinstance(forward_cover_days,
                                                                                   (int, float)) else "N/A"

                    # Add row to the DataTable
                    data_table.add_row(date, forward_cover_days)

                self.notify(f"✅ Displayed last 10 Crude Oil Forward Cover records for {self.selected_country_code}")

        except Exception as e:
            self.notify(f"⚠️ Crude Oil Forward Cover Error: {str(e)}")


class EconDBTab(Container):
    """Main class for the EconDB Tab under the Economy Analysis tab."""

    def compose(self) -> ComposeResult:
        """Compose the UI for the EconDB Tab."""
        # Default static content or introduction
        yield Static("Welcome to the EconDB Tab")

        # Tabbed content for Main Indicators and Economy in Detail
        with TabbedContent():
            with TabPane("Main Indicators"):
                yield MainIndicatorTab()

            with TabPane("Economy in Detail"):
                yield EconomyInDetailTab()