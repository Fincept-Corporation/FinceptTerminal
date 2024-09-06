from rich.console import Console
from rich.theme import Theme

# Define a custom theme for dark background
custom_theme = Theme({
    "info": "bold white on #282828",
    "warning": "bold yellow on #282828",
    "danger": "bold red on #282828",
    "success": "bold green on #282828",
    "highlight": "bold cyan on #282828"
})


console = Console(theme=custom_theme)
