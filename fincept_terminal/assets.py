# import click
# from difflib import get_close_matches
# from .data import fetch_equities_data  # Import from the new data.py module
# from .themes import console
# from .utilities import display_options_in_columns, select_option_from_list, display_search_results, fetch_detailed_data

def search_assets():
    from fincept_terminal.themes import console
    console.print("[highlight]SEARCH ASSETS[/highlight]\n")

    from fincept_terminal.data import fetch_equities_data
    equities_df = fetch_equities_data()
    countries = sorted(equities_df['country'].dropna().unique())
    
    console.print(f"Total number of countries available: {len(countries)}", style="info")

    from .utilities import display_options_in_columns, select_option_from_list, display_search_results, \
        fetch_detailed_data
    display_options_in_columns(countries, "Available Countries")

    country_choice = select_option_from_list(countries, "country")
    
    if country_choice == '' or country_choice.lower() == 'worldwide':
        country_choice = 'Worldwide'
        filtered_df = equities_df
    else:
        filtered_df = equities_df[equities_df['country'] == country_choice]

    sectors = sorted(filtered_df['sector'].dropna().unique())
    display_options_in_columns(sectors, f"Available Sectors in {country_choice}")
    sector_choice = select_option_from_list(sectors, "sector")

    industries = sorted(filtered_df[filtered_df['sector'] == sector_choice]['industry'].dropna().unique())
    display_options_in_columns(industries, f"Available Industries in {sector_choice}, {country_choice}")
    industry_choice = select_option_from_list(industries, "industry")

    console.print(f"\n[highlight]LISTING ALL SYMBOLS IN {industry_choice} - {sector_choice}, {country_choice}[/highlight]\n")

    search_results = filtered_df[(filtered_df['sector'] == sector_choice) & (filtered_df['industry'] == industry_choice)]

    if search_results.empty:
        console.print(f"[danger]No symbols found in '{industry_choice}' industry.[/danger]")
        return

    display_search_results(search_results)

    import click
    fetch_data = click.prompt("Would you like to fetch detailed data for any symbol using yfinance? (y/n)", type=str)
    if fetch_data.lower() == 'y':
        input_name = click.prompt("Enter the symbol or name to fetch data for", type=str)
        closest_symbol = match_symbol(input_name, search_results)
        if closest_symbol:
            fetch_detailed_data(closest_symbol)
        else:
            console.print(f"[danger]No matching symbol found for '{input_name}'.[/danger]")

def match_symbol(input_name, df):
    symbols = df['symbol'].tolist()
    names = df['name'].tolist()

    from difflib import get_close_matches
    closest_symbol = get_close_matches(input_name.upper(), symbols, n=1)
    if closest_symbol:
        return closest_symbol[0]

    closest_name = get_close_matches(input_name.lower(), [name.lower() for name in names], n=1)
    if closest_name:
        symbol = df[df['name'].str.lower() == closest_name[0]].iloc[0]['symbol']
        return symbol

    return None
