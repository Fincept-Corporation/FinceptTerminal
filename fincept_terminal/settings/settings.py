import ujson as json
import click
import os

# Path for storing user settings
SETTINGS_FILE = "user_settings.json"

def load_settings():
    """Load settings from the file if they exist, otherwise create default settings."""
    default_settings = {
        "theme": "dark",  # Default theme is dark
        "notifications": True,
        "display_rows": 10,  # Number of rows to display in tables
        "auto_update": False  # Auto-update is off by default
    }
    if os.path.exists(SETTINGS_FILE):
        with open(SETTINGS_FILE, "r") as f:
            return json.load(f)
    return default_settings


def save_settings(settings):
    """Save settings to the file."""
    with open(SETTINGS_FILE, "w") as f:
        json.dump(settings, f, indent=4)


# SETTINGS command group
@click.command()
@click.pass_context
def settings_menu(ctx):
    """Customize your Fincept terminal settings."""
    click.echo("\n[Settings Menu]")
    click.echo("1. Change Theme")
    click.echo("2. Configure Display Options")
    click.echo("3. Toggle Notifications")
    click.echo("4. Toggle Auto-Update")
    click.echo("5. Back to Main Menu")

    choice = click.prompt("\nPlease choose an option", type=int)
    if choice == 1:
        change_theme()
    elif choice == 2:
        configure_display()
    elif choice == 3:
        toggle_notifications()
    elif choice == 4:
        toggle_auto_update()
    elif choice == 5:
        return  # Return to main menu
    else:
        click.echo("[Error] Invalid option. Please try again.")


def change_theme():
    from fincept_terminal.utils.themes import console,available_themes,switch_theme
    """Let the user change the terminal theme."""
    # List available themes
    available_theme_names = list(available_themes.keys())

    console.print("\n[bold cyan]Available Themes[/bold cyan]\n", style="info")

    # Display available themes as numbered options
    for i, theme_name in enumerate(available_theme_names, 1):
        console.print(f"{i}. {theme_name.replace('_', ' ').title()}")

    from rich.prompt import Prompt
    choice = Prompt.ask("\nEnter the number corresponding to the theme you want to switch to")

    try:
        choice = int(choice)  # Convert input to an integer
        selected_theme = available_theme_names[choice - 1]  # Get theme based on the number
        switch_theme(selected_theme)
        console.print(f"\n[bold green]Theme switched to {selected_theme.replace('_', ' ').title()}![/bold green]")
    except (ValueError, IndexError):
        console.print("[bold red]Invalid choice. Please select a valid theme number.[/bold red]")


def configure_display():
    """Configure display settings such as the number of rows."""
    settings = load_settings()
    click.echo("\nCurrent number of rows displayed: {}".format(settings["display_rows"]))
    new_rows = click.prompt("Enter the number of rows to display", type=int)
    settings["display_rows"] = new_rows
    save_settings(settings)
    click.echo(f"\nDisplay settings updated. Now showing {new_rows} rows.")


def toggle_notifications():
    """Enable or disable notifications."""
    settings = load_settings()
    click.echo("\nCurrent notifications setting: {}".format("Enabled" if settings["notifications"] else "Disabled"))
    choice = click.prompt("Enable or disable notifications? (enable/disable)", type=str)
    if choice == "enable":
        settings["notifications"] = True
        save_settings(settings)
        click.echo("\nNotifications enabled.")
    elif choice == "disable":
        settings["notifications"] = False
        save_settings(settings)
        click.echo("\nNotifications disabled.")
    else:
        click.echo("\n[Error] Invalid choice. Please choose 'enable' or 'disable'.")


def toggle_auto_update():
    """Toggle the auto-update feature."""
    settings = load_settings()
    current_status = "Enabled" if settings.get("auto_update", False) else "Disabled"
    click.echo(f"\nAuto-Update is currently {current_status}.")
    choice = click.prompt("Do you want to enable or disable Auto-Update? (enable/disable)", type=str)
    if choice == "enable":
        settings["auto_update"] = True
        save_settings(settings)
        click.echo("\nAuto-Update enabled.")
    elif choice == "disable":
        settings["auto_update"] = False
        save_settings(settings)
        click.echo("\nAuto-Update disabled.")
    else:
        click.echo("\n[Error] Invalid choice. Please choose 'enable' or 'disable'.")


if __name__ == "__main__":
    settings_menu()
