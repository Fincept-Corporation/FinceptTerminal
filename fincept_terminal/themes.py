from rich.console import Console
from rich.theme import Theme

# Dark theme with high contrast (unchanged)
dark_theme = Theme({
    "info": "bold white on #282828",
    "warning": "bold yellow on #282828",
    "danger": "bold red on #282828",
    "success": "bold green on #282828",
    "highlight": "bold cyan on #282828"
})

# Solar Flare - A warm, vibrant theme with balanced colors
solar_flare_theme = Theme({
    "info": "bold white on #8B4513",      # Saddle brown background with white text
    "warning": "bold yellow on #A0522D",  # Sienna background with yellow text
    "danger": "bold white on #CD5C5C",    # Indian red background with white text
    "success": "bold white on #FFD700",   # Gold background with white text
    "highlight": "bold white on #FF8C00"  # Dark orange background with white text
})

# Ocean Breeze - A calm, blue-based theme with softer backgrounds
ocean_breeze_theme = Theme({
    "info": "bold white on #1C1C1C",      # Dark charcoal background with white text
    "warning": "bold yellow on #4682B4",  # Steel blue background with yellow text
    "danger": "bold white on #5F9EA0",    # Cadet blue background with white text
    "success": "bold white on #20B2AA",   # Light sea green background with white text
    "highlight": "bold white on #1E90FF"  # Dodger blue background with white text
})

# Forest Twilight - A dark green theme with balanced colors
forest_twilight_theme = Theme({
    "info": "bold white on #013220",      # Dark green background with white text
    "warning": "bold yellow on #2E8B57",  # Sea green background with yellow text
    "danger": "bold white on #6B8E23",    # Olive drab background with white text
    "success": "bold white on #556B2F",   # Dark olive green background with white text
    "highlight": "bold white on #228B22"  # Forest green background with white text
})

# Electric Pulse - Neon theme with more muted backgrounds
electric_pulse_theme = Theme({
    "info": "bold white on #4B0082",      # Indigo background with white text
    "warning": "bold yellow on #9400D3",  # Dark violet background with yellow text
    "danger": "bold white on #8A2BE2",    # Blue violet background with white text
    "success": "bold white on #00FF00",   # Neon green background with white text
    "highlight": "bold white on #7B68EE"  # Medium slate blue background with white text
})

# Billion Dollar Theme - Finance inspired, with toned-down colors
billion_dollar_theme = Theme({
    "info": "bold white on #013220",      # Dark green background with white text (symbolizing money)
    "warning": "bold yellow on #2F4F4F",  # Dim grey background with yellow text
    "danger": "bold white on #8B0000",    # Dark red background with white text
    "success": "bold white on #556B2F",   # Dark olive green background with white text
    "highlight": "bold white on #2E8B57"  # Sea green background with white text for highlights
})

# Dictionary of available themes
available_themes = {
    "dark": dark_theme,
    "solar_flare": solar_flare_theme,
    "ocean_breeze": ocean_breeze_theme,
    "forest_twilight": forest_twilight_theme,
    "electric_pulse": electric_pulse_theme,
    "billion_dollar": billion_dollar_theme,  # Updated Billion Dollar Theme
}

# Default theme to start with (you can customize this)
current_theme_name = "dark"
console = Console(theme=available_themes[current_theme_name])

# Function to switch themes
def switch_theme(theme_name):
    global console
    if theme_name in available_themes:
        console = Console(theme=available_themes[theme_name])
        global current_theme_name
        current_theme_name = theme_name
    else:
        console.print(f"[bold red]Theme '{theme_name}' not found. Using default theme.[/bold red]")
