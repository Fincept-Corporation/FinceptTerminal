from rich.console import Console
from rich.panel import Panel
from rich.align import Align
from rich.markdown import Markdown
from rich.text import Text

def display_art():
    console = Console()
    art = """

$$$$$$$$\ $$\                                          $$\           $$$$$$\                                           $$\                                        $$\               
$$  _____|\__|                                         $$ |          \_$$  _|                                          $$ |                                       $$ |              
$$ |      $$\ $$$$$$$\   $$$$$$$\  $$$$$$\   $$$$$$\ $$$$$$\           $$ |  $$$$$$$\ $$\    $$\  $$$$$$\   $$$$$$$\ $$$$$$\   $$$$$$\$$$$\   $$$$$$\  $$$$$$$\ $$$$$$\    $$$$$$$\ 
$$$$$\    $$ |$$  __$$\ $$  _____|$$  __$$\ $$  __$$\\_$$  _|          $$ |  $$  __$$\\$$\  $$  |$$  __$$\ $$  _____|\_$$  _|  $$  _$$  _$$\ $$  __$$\ $$  __$$\\_$$  _|  $$  _____|
$$  __|   $$ |$$ |  $$ |$$ /      $$$$$$$$ |$$ /  $$ | $$ |            $$ |  $$ |  $$ |\$$\$$  / $$$$$$$$ |\$$$$$$\    $$ |    $$ / $$ / $$ |$$$$$$$$ |$$ |  $$ | $$ |    \$$$$$$\  
$$ |      $$ |$$ |  $$ |$$ |      $$   ____|$$ |  $$ | $$ |$$\         $$ |  $$ |  $$ | \$$$  /  $$   ____| \____$$\   $$ |$$\ $$ | $$ | $$ |$$   ____|$$ |  $$ | $$ |$$\  \____$$\ 
$$ |      $$ |$$ |  $$ |\$$$$$$$\ \$$$$$$$\ $$$$$$$  | \$$$$  |      $$$$$$\ $$ |  $$ |  \$  /   \$$$$$$$\ $$$$$$$  |  \$$$$  |$$ | $$ | $$ |\$$$$$$$\ $$ |  $$ | \$$$$  |$$$$$$$  |
\__|      \__|\__|  \__| \_______| \_______|$$  ____/   \____/       \______|\__|  \__|   \_/     \_______|\_______/    \____/ \__| \__| \__| \_______|\__|  \__|  \____/ \_______/ 
                                            $$ |                                                                                                                                    
                                            $$ |                                                                                                                                    
                                            \__|                                                                                                                                    

    """
    description = (
        "Welcome to Fincept Investments\n\n"
        "At **Fincept Investments**, we are your ultimate gateway to unparalleled financial insights and market analysis. "
        "Our platform is dedicated to empowering investors, traders, and financial enthusiasts with comprehensive data and "
        "cutting-edge tools to make informed decisions in the fast-paced world of finance.\n\n"
        "Our Services:\n\n"
        "- **Technical Analysis:** Dive deep into charts and trends with our advanced technical analysis tools. We provide "
        "real-time data and indicators to help you spot opportunities in the market.\n\n"
        "- **Fundamental Analysis:** Understand the core value of assets with our in-depth fundamental analysis. We offer "
        "detailed reports on company financials, earnings, and key ratios to help you evaluate potential investments.\n\n"
        "- **Sentiment Analysis:** Stay ahead of the market mood with our sentiment analysis. We track and analyze public "
        "opinion and market sentiment across social media and news sources to give you a competitive edge.\n\n"
        "- **Quantitative Analysis:** Harness the power of numbers with our quantitative analysis tools. We provide robust "
        "models and algorithms to help you identify patterns and optimize your trading strategies.\n\n"
        "- **Economic Data:** Access a wealth of economic indicators and macroeconomic data. Our platform delivers key "
        "economic metrics from around the globe, helping you understand the broader market context.\n\n"
        "Connect with Us:\n \n"
        "Instagram: https://instagram.com/finceptcorp \n"
    )
    

    # Align art to center and wrap it in a Panel
    art_panel = Panel(Align.center(art, vertical="middle"), style="bold yellow")
    description_panel = Panel(Align.center(Markdown(description), vertical="middle"), style="green")

    console.print(art_panel)
    console.print(description_panel)
