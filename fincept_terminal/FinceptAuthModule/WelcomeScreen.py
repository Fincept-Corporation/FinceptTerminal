from textual.screen import Screen
from textual.app import ComposeResult
from textual.widgets import Static, Button, Header, Footer
from textual.containers import Horizontal, VerticalScroll, Container
from datetime import datetime
from textual.binding import Binding
from fincept_terminal.FinceptSettingModule.FinceptTerminalSettingUtils import clear_user_data

BINDINGS = [
    Binding(key="q", action="quit", description="Quit the app"),
]


class WelcomeScreen(Screen):
    """Welcome Screen for the Fincept Terminal."""

    # Include TCSS styling directly as a string
    CSS = """
    #fincept-app {
        align: center middle;
        padding: 0;
        border: round white;
        background: #282c34;
    }

    Screen {
        border: solid red; /* Ensure the screen border is applied */
    }

    .fincept-container {
        background: #282828;
        border: solid red; /* Solid red border for debugging */
        margin: 0; /* Add margin to ensure border visibility */
        padding: 0; /* Add padding inside the container */
    }

    .fincept-horizontal {
        align: center middle;
        height: auto;
        padding: 0;
        border: solid red; /* Solid red border for horizontal layout */
    }

    #fincept-button-container {
        dock: bottom;
        height: 10%;
        align-horizontal: center;
        margin: 0;
        border: solid red; /* Ensure button container border is visible */
    }

    .fincept-button {
        width: auto;
        border: solid green;
        background: darkorange;
        margin: 1 0 0 5;
    }

    #fincept-registration-container {
        width: 40%;
        content-align: center middle;
        align: center middle;
        padding: 10;
        border: solid orange;
        background: black;
    }
    """

    def compose(self) -> ComposeResult:
        # Header with a clock
        yield Header(show_clock=True, classes="fincept-header")

        # Main vertical scroll container to hold all content
        with VerticalScroll(id="fincept-scroll-container"):
            # Banner section
            yield Container(
                Static(
                    """[red]
                             _____                                                                                                                                              _____ 
                            ( ___ )--------------------------------------------------------------------------------------------------------------------------------------------( ___ )
                             |   |                                                                                                                                              |   | 
                             |   |  $$$$$$$$\ $$\                                          $$\           $$$$$$$$\                                $$\                     $$\   |   | 
                             |   |  $$  _____|\__|                                         $$ |          \__$$  __|                               \__|                    $$ |  |   | 
                             |   |  $$ |      $$\ $$$$$$$\   $$$$$$$\  $$$$$$\   $$$$$$\ $$$$$$\            $$ | $$$$$$\   $$$$$$\  $$$$$$\$$$$\  $$\ $$$$$$$\   $$$$$$\  $$ |  |   | 
                             |   |  $$$$$\    $$ |$$  __$$\ $$  _____|$$  __$$\ $$  __$$\\_$$  _|            $$ |$$  __$$\ $$  __$$\ $$  _$$  _$$\ $$ |$$  __$$\  \____$$\ $$ |  |   | 
                             |   |  $$  __|   $$ |$$ |  $$ |$$ /      $$$$$$$$ |$$ /  $$ | $$ |             $$ |$$$$$$$$ |$$ |  \__|$$ / $$ / $$ |$$ |$$ |  $$ | $$$$$$$ |$$ |  |   | 
                             |   |  $$ |      $$ |$$ |  $$ |$$ |      $$   ____|$$ |  $$ | $$ |$$\          $$ |$$   ____|$$ |      $$ | $$ | $$ |$$ |$$ |  $$ |$$  __$$ |$$ |  |   | 
                             |   |  $$ |      $$ |$$ |  $$ |\$$$$$$$\ \$$$$$$$\ $$$$$$$  | \$$$$  |         $$ |\$$$$$$$\ $$ |      $$ | $$ | $$ |$$ |$$ |  $$ |\$$$$$$$ |$$ |  |   | 
                             |   |  \__|      \__|\__|  \__| \_______| \_______|$$  ____/   \____/          \__| \_______|\__|      \__| \__| \__|\__|\__|  \__| \_______|\__|  |   | 
                             |   |                                              $$ |                                                                                            |   | 
                             |   |                                              $$ |                                                                                            |   | 
                             |   |                                              \__|                                                                                            |   | 
                             |___|                                                                                                                                              |___| 
                            (_____)--------------------------------------------------------------------------------------------------------------------------------------------(_____)                         
                    [/red]""",
                    id="fincept-banner-static",
                ),
                id="fincept-banner-container",
                classes="fincept-container",
            )

            # Information section
            yield Container(
                Static(
                    """
        [red]Welcome to Fincept Investments !![/red]
        At Fincept Investments, we are your ultimate gateway to unparalleled financial insights and market analysis. Our platform is dedicated to empowering investors, traders, and financial enthusiasts with 
        comprehensive data and cutting-edge tools to make informed decisions in the fast-paced world of finance.

        [red]Our Services:[/red] 
        - [red]Technical Analysis:[/red] Dive deep into charts and trends with our advanced technical analysis tools. We provide real-time data and indicators to help you spot opportunities in the market.
        - [red]Fundamental Analysis:[/red] Understand the core value of assets with our in-depth fundamental analysis. We offer detailed reports on company financials, earnings, and key ratios to help you evaluate
          potential investments.
        - [red]Sentiment Analysis:[/red] Stay ahead of the market mood with our sentiment analysis. We track and analyze public opinion and market sentiment across social media and news sources to give you a 
          competitive edge.
        - [red]Quantitative Analysis:[/red] Harness the power of numbers with our quantitative analysis tools. We provide robust models and algorithms to help you identify patterns and optimize your trading strategies.
        - [red]Economic Data:[/red] Access a wealth of economic indicators and macroeconomic data. Our platform delivers key economic metrics from around the globe, helping you understand the broader market context.

        Connect with Us:

        Instagram: [red]https://instagram.com/finceptcorp[/red]
                    """,
                    id="fincept-info-static",
                ),
                id="fincept-info-container",
                classes="fincept-container",
            )

            # Greeting section based on time of day
            current_hour = datetime.now().hour
            greeting = (
                "Good morning" if current_hour < 12 else
                "Good afternoon" if 12 <= current_hour < 18 else
                "Good evening"
            )

            from fincept_terminal.FinceptSettingModule.FinceptTerminalSettingUtils import is_registered
            if is_registered():
                from fincept_terminal.FinceptSettingModule.FinceptTerminalSettingUtils import get_user_name
                username = get_user_name() or "Guest"
                yield Static(f"{greeting}, {username}! Welcome back to Fincept Terminal.", id="fincept-greeting")

                # Buttons for registered users
                with Horizontal(classes="fincept-horizontal"):
                    yield Button("Continue", id="fincept-button-continue", classes="fincept-button")
                    yield Button("Logout", id="fincept-button-logout", classes="fincept-button")
            else:
                yield Static(f"{greeting}, Guest! Welcome to Fincept Terminal.", id="fincept-greeting")

                # Buttons for new users
                with Horizontal(classes="fincept-horizontal"):
                    yield Button("Login", id="fincept-button-login", classes="fincept-button")
                    yield Button("Guest User", id="fincept-button-guest", classes="fincept-button")
                    yield Button("Register", id="fincept-button-register", classes="fincept-button")

            yield Footer()


    async def on_button_pressed(self, event: Button.Pressed):
        """Handle button presses."""
        button = event.button
        if button.id == "fincept-button-register":
            await self.app.push_screen("registration")
        elif button.id == "fincept-button-guest":
            await self.app.push_screen("dashboard")
        elif button.id == "fincept-button-login":
            # Push the LoginScreen when the Login button is pressed
            from fincept_terminal.FinceptAuthModule.LoginScreen import LoginScreen
            await self.app.push_screen("login")
        elif button.id == "fincept-button-continue":
            await self.app.push_screen("dashboard")
        elif button.id == "fincept-button-logout":
            clear_user_data()
