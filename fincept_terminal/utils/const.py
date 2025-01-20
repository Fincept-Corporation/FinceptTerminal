# Function to display lists in columns with max 7 rows per column (no "Column 1" heading)
def display_in_columns(title, items):
    """Display the items in a table with multiple columns if they exceed 7 rows."""
    from rich.table import Table
    table = Table(title=title, header_style="bold green on #282828",
                  show_lines=True)  # show_lines=True adds spacing between rows
    max_rows = 7  # Maximum number of rows per column
    num_columns = (len(items) + max_rows - 1) // max_rows  # Calculate the required number of columns

    # Add the columns (empty headers to remove column titles)
    for _ in range(num_columns):
        table.add_column("", justify="left")

    # Add rows in columns
    rows = [[] for _ in range(max_rows)]  # Empty rows to hold the items
    for index, item in enumerate(items):
        row_index = index % max_rows
        rows[row_index].append(f"{index + 1}. {item}")

    # Fill the table
    for row in rows:
        # If the row has fewer elements than the number of columns, fill the rest with empty strings
        row += [""] * (num_columns - len(row))
        table.add_row(*row)

    from fincept_terminal.utils.themes import console
    console.print(table)


def display_info_in_three_columns(info):
    """Display key-value pairs in three columns, skipping long values."""
    from rich.table import Table
    table = Table(show_lines=True, style="info", header_style="bold white on #282828")

    # Add columns for three attributes and values
    table.add_column("Attribute 1", style="cyan on #282828", width=25)
    table.add_column("Value 1", style="green on #282828", width=35)
    table.add_column("Attribute 2", style="cyan on #282828", width=25)
    table.add_column("Value 2", style="green on #282828", width=35)
    table.add_column("Attribute 3", style="cyan on #282828", width=25)
    table.add_column("Value 3", style="green on #282828", width=35)

    max_value_length = 40  # Set a maximum length for displayed values
    keys = list(info.keys())
    values = list(info.values())

    for i in range(0, len(keys), 3):
        row_data = []
        for j in range(3):
            if i + j < len(keys):
                key = keys[i + j]
                value = values[i + j]
                # Skip long values and add a placeholder
                if isinstance(value, str) and len(value) > max_value_length:
                    row_data.append(str(key))
                    row_data.append("[value too long]")
                else:
                    row_data.append(str(key))
                    row_data.append(str(value))
            else:
                row_data.append("")
                row_data.append("")
        table.add_row(*row_data)

    from fincept_terminal.utils.themes import console
    console.print(table)


from fuzzywuzzy import process

def find_closest_ticker(user_input, stock_data, threshold=70):
    """Find the closest matching ticker or company name from the displayed stocks."""
    stock_symbols = stock_data['symbol'].tolist()
    stock_names = stock_data['name'].tolist()

    # Combine symbols and names into a dictionary for easy reference
    stock_dict = {symbol: name for symbol, name in zip(stock_symbols, stock_names)}

    # Use fuzzy matching to find the closest matches in both symbols and names
    matches = process.extract(user_input, stock_symbols + stock_names, limit=5)

    # Filter matches by threshold
    valid_matches = [match for match, score in matches if score >= threshold]

    if valid_matches:
        # Create a list of dictionaries to store matches and their details
        match_list = []
        for match in valid_matches:
            if match in stock_symbols:
                match_list.append({'symbol': match, 'name': stock_dict[match]})
            else:
                # If the match is a company name, find the corresponding symbol
                symbol = next(symbol for symbol, name in stock_dict.items() if name == match)
                match_list.append({'symbol': symbol, 'name': match})
        return match_list

    return None





# Comprehensive list of stock exchanges by country
MARKET_INDEX_MAP = {
    "Afghanistan": None,  # No major stock exchange
    "Anguilla": None,  # No major stock exchange
    "Argentina": "^MERV",  # MERVAL Index
    "Australia": "^AXJO",  # ASX 200
    "Austria": "^ATX",  # Austrian Traded Index (ATX)
    "Azerbaijan": None,  # No major stock exchange
    "Bahamas": None,  # No major stock exchange
    "Bangladesh": "^DSEX",  # DSEX Index
    "Barbados": None,  # No major stock exchange
    "Belgium": "^BFX",  # BEL 20 Index
    "Belize": None,  # No major stock exchange
    "Bermuda": None,  # Bermuda Stock Exchange (BSX)
    "Botswana": None,  # Botswana Stock Exchange
    "Brazil": "^BVSP",  # Bovespa Index
    "British Virgin Islands": None,  # No major stock exchange
    "Cambodia": None,  # Cambodia Securities Exchange (CSX)
    "Canada": "^GSPTSE",  # S&P/TSX Composite Index
    "Cayman Islands": None,  # No major stock exchange
    "Chile": "^IPSA",  # Santiago Stock Exchange (IPSA)
    "China": "^SSEC",  # Shanghai Composite Index
    "Colombia": "^COLCAP",  # COLCAP Index
    "Costa Rica": None,  # No major stock exchange
    "Cyprus": None,  # Cyprus Stock Exchange (CSE)
    "Czech Republic": "^PX",  # PX Index (Prague)
    "Denmark": "^OMXC25",  # OMX Copenhagen 25
    "Dominican Republic": None,  # No major stock exchange
    "Egypt": "^EGX30",  # EGX 30 Index
    "Estonia": None,  # Tallinn Stock Exchange
    "Falkland Islands": None,  # No major stock exchange
    "Finland": "^OMXH25",  # OMX Helsinki 25
    "France": "^FCHI",  # CAC 40 Index
    "French Guiana": None,  # No major stock exchange
    "Gabon": None,  # No major stock exchange
    "Georgia": None,  # Georgian Stock Exchange (GSE)
    "Germany": "^GDAXI",  # DAX Index
    "Ghana": None,  # Ghana Stock Exchange
    "Gibraltar": None,  # No major stock exchange
    "Greece": "^ATG",  # Athens General Composite Index
    "Greenland": None,  # No major stock exchange
    "Guernsey": None,  # No major stock exchange
    "Hong Kong": "^HSI",  # Hang Seng Index
    "Hungary": "^BUX",  # BUX Index (Budapest)
    "Iceland": None,  # Iceland Stock Exchange (OMX Iceland)
    "India": "^NSEI",  # Nifty 50 (NSE)
    "Indonesia": "^JKSE",  # Jakarta Composite Index
    "Ireland": "^ISEQ",  # Irish Stock Exchange Overall Index
    "Isle of Man": None,  # No major stock exchange
    "Israel": "^TA125",  # TA-125 Index (Tel Aviv)
    "Italy": "^FTMIB",  # FTSE MIB Index
    "Ivory Coast": None,  # No major stock exchange
    "Japan": "^N225",  # Nikkei 225
    "Jersey": None,  # No major stock exchange
    "Jordan": None,  # Amman Stock Exchange (ASE)
    "Kazakhstan": None,  # Kazakhstan Stock Exchange (KASE)
    "Kenya": "^NSE20",  # Nairobi Securities Exchange 20 Share Index
    "Kyrgyzstan": None,  # Kyrgyz Stock Exchange
    "Latvia": None,  # Nasdaq Riga
    "Liechtenstein": None,  # No major stock exchange
    "Lithuania": None,  # Nasdaq Vilnius
    "Luxembourg": None,  # Luxembourg Stock Exchange
    "Macau": None,  # No major stock exchange
    "Macedonia": None,  # Macedonian Stock Exchange
    "Malaysia": "^KLSE",  # FTSE Bursa Malaysia KLCI
    "Malta": None,  # Malta Stock Exchange (MSE)
    "Mauritius": None,  # Stock Exchange of Mauritius
    "Mexico": "^MXX",  # IPC (Indice de Precios y Cotizaciones)
    "Monaco": None,  # No major stock exchange
    "Mongolia": None,  # Mongolian Stock Exchange (MSE)
    "Montenegro": None,  # Montenegro Stock Exchange
    "Morocco": "^MASI",  # Moroccan All Shares Index
    "Mozambique": None,  # No major stock exchange
    "Myanmar": None,  # Yangon Stock Exchange (YSX)
    "Namibia": None,  # Namibia Stock Exchange (NSX)
    "Netherlands": "^AEX",  # AEX Index
    "Netherlands Antilles": None,  # No major stock exchange
    "New Zealand": "^NZ50",  # S&P/NZX 50 Index
    "Nigeria": None,  # Nigerian Stock Exchange
    "Norway": "^OBX",  # OBX Total Return Index
    "Panama": None,  # Panama Stock Exchange
    "Papua New Guinea": None,  # Port Moresby Stock Exchange
    "Peru": "^SPBLPGPT",  # S&P Lima General Total Return Index
    "Philippines": "^PSEI",  # PSEi Composite Index
    "Poland": "^WIG20",  # WIG20 Index (Warsaw)
    "Portugal": "^PSI20",  # PSI-20 Index
    "Qatar": "^QSI",  # QE Index (Qatar Exchange)
    "Reunion": None,  # No major stock exchange
    "Romania": "^BETI",  # BET Index (Bucharest)
    "Russia": "^IMOEX",  # MOEX Russia Index
    "Saudi Arabia": "^TASI",  # Tadawul All Share Index (TASI)
    "Senegal": None,  # Regional Securities Exchange (BRVM)
    "Singapore": "^STI",  # Straits Times Index (STI)
    "Slovakia": None,  # Bratislava Stock Exchange
    "Slovenia": None,  # Ljubljana Stock Exchange (SBITOP)
    "South Africa": "^JTOPI",  # JSE Top 40 Index
    "South Korea": "^KS11",  # KOSPI Composite Index
    "Spain": "^IBEX",  # IBEX 35 Index
    "Suriname": None,  # No major stock exchange
    "Sweden": "^OMXS30",  # OMX Stockholm 30
    "Switzerland": "^SSMI",  # Swiss Market Index (SMI)
    "Taiwan": "^TWII",  # Taiwan Weighted Index
    "Tanzania": None,  # Dar es Salaam Stock Exchange
    "Thailand": "^SET50",  # SET50 Index
    "Turkey": "XU100.IS",  # BIST 100 Index
    "Ukraine": None,  # PFTS Stock Exchange
    "United Arab Emirates": "^ADX",  # Abu Dhabi Securities Exchange General Index
    "United Kingdom": "^FTSE",  # FTSE 100 Index
    "United States": "^GSPC",  # S&P 500 Index
    "Uruguay": None,  # No major stock exchange
    "Vietnam": "^VNINDEX",  # Vietnam Ho Chi Minh Stock Index
    "Zambia": None,  # Lusaka Stock Exchange (LuSE)
}