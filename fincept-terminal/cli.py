import click
from display import display_art
from market import show_market_tracker_menu
from assets import search_assets
from themes import console
from rich.panel import Panel

@click.group(invoke_without_command=True)
@click.pass_context
def cli(ctx):
    """Fincept Investments CLI"""
    if ctx.invoked_subcommand is None:
        start()

@cli.command()
def start():
    """Start the Fincept Investments terminal"""
    display_art()
    show_main_menu()

def show_main_menu():
    while True:
        console.print("[highlight]MAIN MENU[/highlight]\n")

        menu_text = """
1. MARKET TRACKER
2. ECONOMICS
3. NEWS
4. EXIT
        """

        menu_panel = Panel(menu_text, title="SELECT AN OPTION", title_align="center", style="highlight", padding=(1, 2))
        console.print(menu_panel)
        
        choice = click.prompt("Please select an option", type=int)

        if choice == 1:
            show_market_tracker_menu()
        elif choice == 2:
            console.print("\n[success]ECONOMICS SELECTED.[/success] (This feature is under development)\n")
        elif choice == 3:
            search_assets()
        elif choice == 4:
            console.print("\n[danger]EXITING FINCEPT INVESTMENTS.[/danger]")
            break
        else:
            console.print("\n[danger]INVALID OPTION. PLEASE TRY AGAIN.[/danger]")

if __name__ == "__main__":
    cli()
