def display_art():
    art = r"""
 _____                                                                                                                                              _____ 
( ___ )--------------------------------------------------------------------------------------------------------------------------------------------( ___ )
 |   |                                                                                                                                              |   | 
 |   |  $$$$$$$$\ $$\                                          $$\           $$$$$$$$\                                $$\                     $$\   |   | 
 |   |  $$  _____|\__|                                         $$ |          \__$$  __|                               \__|                    $$ |  |   | 
 |   |  $$ |      $$\ $$$$$$$\   $$$$$$$\  $$$$$$\   $$$$$$\ $$$$$$\            $$ | $$$$$$\   $$$$$$\  $$$$$$\$$$$\  $$\ $$$$$$$\   $$$$$$\  $$ |  |   | 
 |   |  $$$$$\    $$ |$$  __$$\ $$  _____|$$  __$$\ $$  __$$\\_$$  _|           $$ |$$  __$$\ $$  __$$\ $$  _$$  _$$\ $$ |$$  __$$\  \____$$\ $$ |  |   | 
 |   |  $$  __|   $$ |$$ |  $$ |$$ /      $$$$$$$$ |$$ /  $$ | $$ |             $$ |$$$$$$$$ |$$ |  \__|$$ / $$ / $$ |$$ |$$ |  $$ | $$$$$$$ |$$ |  |   | 
 |   |  $$ |      $$ |$$ |  $$ |$$ |      $$   ____|$$ |  $$ | $$ |$$\          $$ |$$   ____|$$ |      $$ | $$ | $$ |$$ |$$ |  $$ |$$  __$$ |$$ |  |   | 
 |   |  $$ |      $$ |$$ |  $$ |\$$$$$$$\ \$$$$$$$\ $$$$$$$  | \$$$$  |         $$ |\$$$$$$$\ $$ |      $$ | $$ | $$ |$$ |$$ |  $$ |\$$$$$$$ |$$ |  |   | 
 |   |  \__|      \__|\__|  \__| \_______| \_______|$$  ____/   \____/          \__| \_______|\__|      \__| \__| \__|\__|\__|  \__| \_______|\__|  |   | 
 |   |                                              $$ |                                                                                            |   | 
 |   |                                              $$ |                                                                                            |   | 
 |   |                                              \__|                                                                                            |   | 
 |___|                                                                                                                                              |___| 
(_____)--------------------------------------------------------------------------------------------------------------------------------------------(_____)                         
    """

    description = """
Welcome to Fincept Investments !!

At **Fincept Investments**, we are your ultimate gateway to unparalleled financial insights and market analysis. Our platform is dedicated to empowering investors, traders, and financial enthusiasts with comprehensive data and cutting-edge tools to make informed decisions in the fast-paced world of finance.

Our Services:

- **Technical Analysis:** Dive deep into charts and trends with our advanced technical analysis tools. We provide real-time data and indicators to help you spot opportunities in the market.
- **Fundamental Analysis:** Understand the core value of assets with our in-depth fundamental analysis. We offer detailed reports on company financials, earnings, and key ratios to help you evaluate potential investments.
- **Sentiment Analysis:** Stay ahead of the market mood with our sentiment analysis. We track and analyze public opinion and market sentiment across social media and news sources to give you a competitive edge.
- **Quantitative Analysis:** Harness the power of numbers with our quantitative analysis tools. We provide robust models and algorithms to help you identify patterns and optimize your trading strategies.
- **Economic Data:** Access a wealth of economic indicators and macroeconomic data. Our platform delivers key economic metrics from around the globe, helping you understand the broader market context.

Connect with Us:

Instagram: https://instagram.com/finceptcorp
    """

    # Create a layout that adjusts based on terminal size
    from rich.layout import Layout
    layout = Layout()

    # Split the layout into two areas, with art taking up 60% of the space and description 40%
    layout.split(
        Layout(name="art", ratio=2),
        Layout(name="description", ratio=2)
    )

    # Dynamically align and style the content
    from rich.panel import Panel
    from rich.align import Align
    from rich.markdown import Markdown
    layout["art"].update(Panel(Align.center(art, vertical="middle"), style="bold red on #282828"))
    layout["description"].update(Panel(Align.center(Markdown(description), vertical="middle"), style="bold white on #282828"))

    # Print the dynamic layout, which will adjust based on terminal size
    from .themes import console
    console.print(layout)

