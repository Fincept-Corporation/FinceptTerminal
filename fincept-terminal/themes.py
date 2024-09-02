from rich.console import Console
from rich.panel import Panel
from rich.theme import Theme

custom_theme = Theme({
    "info": "bold white on #282828",
    "warning": "bold yellow on #282828",
    "danger": "bold red on #282828",
    "success": "bold green on #282828",
    "highlight": "bold cyan on #282828"
})

console = Console(theme=custom_theme, style="white on #282828")

def create_panel(text, title):
    return Panel(text, title=title, title_align="center", style="highlight", padding=(1, 2))
