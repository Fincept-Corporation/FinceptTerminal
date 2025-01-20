import requests
import re
from rich.console import Console
from rich.table import Table


def clean_html(raw_html):
    """Remove HTML tags and return clean text."""
    clean_text = re.sub('<.*?>', '', raw_html)
    return clean_text


def fetch_json_data(url):
    """Fetch JSON data from the given URL."""
    response = requests.get(url)
    response.raise_for_status()  # Raise an exception for HTTP errors
    return response.json()


def display_json_data(data):
    console = Console()

    for item in data:
        profile = item["data"]["pageProps"]["profile"]
        title = clean_html(profile["title"])
        label = clean_html(profile.get("label", ""))
        console.print(f"[bold cyan]{title}[/bold cyan]")
        console.print(f"[italic]{label}[/italic]")

        # Display sections
        sections = profile.get("sections", [])
        for section in sections:
            section_title = clean_html(section.get("title", ""))
            console.print(f"\n[bold]Section: {section_title}[/bold]")

            # Display stats within sections
            stats = section.get("stats", [])
            if stats:
                table = Table(show_header=True, header_style="bold magenta")
                table.add_column("Title")
                table.add_column("Value")
                table.add_column("Subtitle")

                for stat in stats:
                    stat_title = clean_html(stat.get("title", ""))
                    stat_value = clean_html(stat.get("value", ""))
                    stat_subtitle = clean_html(stat.get("subtitle", ""))
                    table.add_row(stat_title, stat_value, stat_subtitle)

                console.print(table)
            else:
                console.print("[italic]No stats available for this section.[/italic]")


def main():
    # Replace this URL with the actual API endpoint
    url = "https://fincept.share.zrok.io/OECTradeData/data_00983b4b4209e1f1/data"

    try:
        json_data = fetch_json_data(url)
        display_json_data(json_data)
    except requests.exceptions.RequestException as e:
        print(f"An error occurred while fetching data: {e}")


if __name__ == "__main__":
    main()
