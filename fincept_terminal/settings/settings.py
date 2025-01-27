import ujson as json
import click
import os
from rich.console import Console
from rich.prompt import Prompt

console = Console()

# Path for storing user settings
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
SETTINGS_FILE = os.path.join(BASE_DIR, "settings.json")


def load_settings():
    """Load settings from the file if they exist, otherwise create default settings."""
    default_settings = {
        "api_key": None,  # Placeholder for user API key
        "email": None,  # Placeholder for user email
        "username": None,
        "theme": "dark",  # Default theme is dark
        "notifications": True,
        "display_rows": 10,  # Number of rows to display in tables
        "auto_update": False,  # Auto-update is off by default
        "data_sources": {}  # Placeholder for data sources
    }
    if os.path.exists(SETTINGS_FILE):
        with open(SETTINGS_FILE, "r") as f:
            existing_settings = json.load(f)
            # Merge existing settings with defaults (to add new fields if not present)
            return {**default_settings, **existing_settings}
    return default_settings


def save_settings(settings):
    """Save settings to the file."""
    with open(SETTINGS_FILE, "w") as f:
        json.dump(settings, f, indent=4)
    console.print(f"\n[green]Settings saved successfully to {SETTINGS_FILE}[/green]")


@click.command()
@click.pass_context
def settings_menu(ctx):
    """Customize your Fincept terminal settings."""
    click.echo("\n[Settings Menu]")
    click.echo("1. Configure Data Sources")
    click.echo("2. Change Theme")
    click.echo("3. Configure Display Options")
    click.echo("4. Toggle Notifications")
    click.echo("5. Toggle Auto-Update")
    click.echo("6. Back to Main Menu")

    choice = click.prompt("\nPlease choose an option", type=int)
    if choice == 1:
        configure_sources()
    elif choice == 2:
        change_theme()
    elif choice == 3:
        configure_display()
    elif choice == 4:
        toggle_notifications()
    elif choice == 5:
        toggle_auto_update()
    elif choice == 6:
        return  # Return to main menu
    else:
        click.echo("[Error] Invalid option. Please try again.")


def configure_sources():
    """Configure data sources for different data menus in a tabular form."""
    from rich.console import Console
    from rich.prompt import Prompt
    from fincept_terminal.utils.const import display_in_columns

    console = Console()

    # Define the data menus and source options
    data_menus = {
        "Global Indices": ["Source A (API)", "Source B (Public Data)", "Source C (Custom API)"],
        "World Economy Tracker": ["Fincept", "FRED (API)", "DataGovIn (API)"],
        "Global News & Sentiment": ["News API", "GNews", "RSS_Feed (API)"],
        "Currency Markets": ["Forex API", "Crypto Exchange Data"],
        "Portfolio Management": ["Manual Input", "Broker API"],
        "GenAI Model": ["Gemini (API)", "OpenAI (API)", "Ollama (API)", "Claude (API)"]
    }

    settings = load_settings()

    while True:
        # Display the modules with their current data sources in columns
        module_names = list(data_menus.keys())
        current_sources = [
            settings.get("data_sources", {}).get(module, {}).get("source", "Not Configured")
            for module in module_names
        ]
        options_to_display = [
            f"{module} (Current: {source})"
            for module, source in zip(module_names, current_sources)
        ]

        display_in_columns("Data Source Modules", options_to_display)

        console.print("\n")
        module_choice = Prompt.ask(
            "Enter the [yellow]option number[/yellow] of the module you want to configure (or type '0' to finish)",
            choices=[str(i + 1) for i in range(len(module_names))] + ["0"],
            default="0"
        )

        if module_choice == "0":
            break

        module_index = int(module_choice) - 1
        selected_module = module_names[module_index]

        # Display source options for the selected module
        console.print(f"\n[cyan]Select a data source for [bold]{selected_module}[/bold]:[/cyan]")
        source_options = data_menus[selected_module]
        display_in_columns(f"Available Sources for {selected_module}", source_options)

        source_choice = Prompt.ask(
            f"Select a source for [cyan]{selected_module}[/cyan] (Enter the number)",
            choices=[str(i + 1) for i in range(len(source_options))]
        )
        selected_source = source_options[int(source_choice) - 1]
        settings["data_sources"][selected_module] = {"source": selected_source}

        # Handle RSS Feed inputs
        if selected_source == "RSS_Feed (API)":
            console.print("\n[cyan]You can add multiple RSS feed URLs. Type 'done' when finished.[/cyan]")
            rss_feeds = []
            while True:
                rss_feed_url = Prompt.ask("Enter an RSS feed URL (or type 'done' to finish)")
                if rss_feed_url.lower() == "done":
                    break
                rss_feeds.append(rss_feed_url)

            settings["data_sources"][selected_module]["rss_feeds"] = rss_feeds

        # Handle API key input
        elif "API" in selected_source and selected_source != "RSS_Feed (API)":
            api_key = Prompt.ask(f"Enter the API key for [cyan]{selected_source}[/cyan]")
            settings["data_sources"][selected_module]["api_key"] = api_key

        console.print(f"[green]Configuration for {selected_module} updated successfully![/green]\n")

    save_settings(settings)
    console.print("[bold green]All data sources configured successfully![/bold green]")





def change_theme():
    """Let the user change the terminal theme."""
    settings = load_settings()
    console.print("\n[bold cyan]Available Themes[/bold cyan]\n")
    themes = ["dark", "light"]
    for i, theme in enumerate(themes, 1):
        console.print(f"{i}. {theme.title()}")

    choice = Prompt.ask("\nEnter the number corresponding to the theme you want to switch to")
    try:
        selected_theme = themes[int(choice) - 1]
        settings["theme"] = selected_theme
        save_settings(settings)
        console.print(f"\n[green]Theme switched to {selected_theme.title()}![/green]")
    except (ValueError, IndexError):
        console.print("[bold red]Invalid choice. Please select a valid theme number.[/bold red]")


def configure_display():
    """Configure display settings such as the number of rows."""
    settings = load_settings()
    click.echo(f"\nCurrent number of rows displayed: {settings['display_rows']}")
    new_rows = click.prompt("Enter the number of rows to display", type=int)
    settings["display_rows"] = new_rows
    save_settings(settings)
    click.echo(f"\nDisplay settings updated. Now showing {new_rows} rows.")


def toggle_notifications():
    """Enable or disable notifications."""
    settings = load_settings()
    current_status = "Enabled" if settings["notifications"] else "Disabled"
    click.echo(f"\nCurrent notifications setting: {current_status}")
    choice = click.prompt("Enable or disable notifications? (enable/disable)", type=str)
    if choice == "enable":
        settings["notifications"] = True
    elif choice == "disable":
        settings["notifications"] = False
    else:
        click.echo("\n[Error] Invalid choice. Please choose 'enable' or 'disable'.")
        return
    save_settings(settings)
    click.echo(f"\nNotifications have been {choice}d.")


def toggle_auto_update():
    """Toggle the auto-update feature."""
    settings = load_settings()
    current_status = "Enabled" if settings["auto_update"] else "Disabled"
    click.echo(f"\nAuto-Update is currently {current_status}.")
    choice = click.prompt("Do you want to enable or disable Auto-Update? (enable/disable)", type=str)
    if choice == "enable":
        settings["auto_update"] = True
    elif choice == "disable":
        settings["auto_update"] = False
    else:
        click.echo("\n[Error] Invalid choice. Please choose 'enable' or 'disable'.")
        return
    save_settings(settings)
    click.echo(f"\nAuto-Update has been {choice}d.")


if __name__ == "__main__":
    settings_menu()
