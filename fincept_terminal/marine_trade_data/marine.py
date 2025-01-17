import requests
import folium
from folium.features import CustomIcon
from fincept_terminal.utils.themes import console
from fincept_terminal.utils.const import display_in_columns
from rich.prompt import Prompt
import webbrowser

# Constants for the URLs
AREA_URL = "https://fincept.share.zrok.io/MarineTradeData/mmsi_position_info/area/data?unique=true"
FILTER_URL_TEMPLATE = "https://fincept.share.zrok.io/MarineTradeData/mmsi_position_info/area/filter?value={}"


def fetch_marine_areas():
    """Fetch the list of marine areas from the API."""
    response = requests.get(AREA_URL)
    data = response.json()
    return data.get("area", [])


def fetch_ship_data(area):
    """Fetch ship data for a specific maritime area."""
    url = FILTER_URL_TEMPLATE.format(area.replace(" ", "%20"))
    response = requests.get(url)
    return response.json()


def plot_ship_positions(ship_data):
    """Plot the ship positions on a Folium map."""
    # Filter out invalid coordinates
    locations = []
    for entry in ship_data:
        try:
            latitude = float(entry["latitude"].strip("°"))
            longitude = float(entry["longitude"].strip("°"))
        except ValueError:
            continue

        locations.append({
            "mmsi": entry["mmsi"],
            "latitude": latitude,
            "longitude": longitude,
            "status": entry["status"],
            "speed": entry.get("speed", "N/A"),
            "course": entry.get("course", "N/A"),
            "station": entry.get("station", "N/A"),
            "position_received": entry["position_received"]
        })

    # Set up the Folium map
    if locations:
        average_lat = sum([loc["latitude"] for loc in locations]) / len(locations)
        average_long = sum([loc["longitude"] for loc in locations]) / len(locations)

        folium_map = folium.Map(location=[average_lat, average_long], zoom_start=5)

        # Custom icon URL
        icon_url = "https://product.fincept.in/ship.png"

        for loc in locations:
            icon = CustomIcon(icon_image=icon_url, icon_size=(24, 24))

            tooltip = (
                f"MMSI: {loc['mmsi']}\n"
                f"Status: {loc['status']}\n"
                f"Speed: {loc['speed']}\n"
                f"Course: {loc['course']}\n"
                f"Station: {loc['station']}\n"
                f"Position Received: {loc['position_received']}"
            )

            folium.Marker(
                location=[loc["latitude"], loc["longitude"]],
                icon=icon,
                tooltip=tooltip
            ).add_to(folium_map)

        # Save the map to an HTML file and open it in the default web browser
        map_filename = "ship_positions_map.html"
        folium_map.save(map_filename)
        webbrowser.open(map_filename)
    else:
        console.print("[bold red]No valid ship positions found to plot on the map.[/bold red]")


def paginate_display_in_columns(title, items, items_per_page=21):
    """Paginate and display items in columns with correct indexing."""
    total_items = len(items)
    current_index = 1  # Start indexing from 1

    while current_index <= total_items:
        end_index = min(current_index + items_per_page - 1, total_items)
        paginated_items = [f"{i}. {items[i - 1]}" for i in range(current_index, end_index + 1)]

        display_in_columns(title, paginated_items)

        if end_index < total_items:
            proceed = Prompt.ask("\nMore results available. Would you like to see more? (yes/no)", default="yes")
            if proceed.lower() != "yes":
                break

        current_index += items_per_page


def marine_menu():
    """Display the marine menu with options to view live ship data or return to the main menu."""
    while True:
        console.print("[bold cyan]MARINE MENU[/bold cyan]\n", style="info")
        options = ["View Live Ship Data", "Back to Main Menu"]
        display_in_columns("Select an option", options)

        choice = Prompt.ask("Enter your choice", default="2")

        if choice == "2":
            from fincept_terminal.cli import show_main_menu
            show_main_menu()

        elif choice == "1":
            # View Live Ship Data
            console.print("[bold cyan]Fetching maritime areas...[/bold cyan]")
            marine_areas = fetch_marine_areas()

            if not marine_areas:
                console.print("[bold red]No maritime areas available.[/bold red]")
                continue

            paginate_display_in_columns("Select a Marine Area", marine_areas)
            area_choice = Prompt.ask("Enter the area number")

            # Allow selection by number
            try:
                selected_area = marine_areas[int(area_choice) - 1]
            except (IndexError, ValueError):
                console.print("[bold red]Invalid choice. Please select a valid area.[/bold red]")
                continue

            console.print(f"[bold cyan]Fetching ship data for {selected_area}...[/bold cyan]")
            ship_data = fetch_ship_data(selected_area)

            if not ship_data:
                console.print(f"[bold red]No ship data available for {selected_area}.[/bold red]")
                continue

            plot_ship_positions(ship_data)

            console.print("[bold green]Map displayed successfully! Returning to the Marine Menu...[/bold green]")
        else:
            console.print("[bold red]Invalid choice. Please select a valid option.[/bold red]")
