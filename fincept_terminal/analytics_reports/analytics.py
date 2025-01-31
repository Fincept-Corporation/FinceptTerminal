import smtplib
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart
from email.mime.base import MIMEBase
from email import encoders
from rich.prompt import Prompt
from fincept_terminal.utils.themes import console
from fincept_terminal.utils.const import display_in_columns
import yfinance as yf

SMTP_SERVER = "smtp.hostinger.com"
SMTP_PORT = 465
EMAIL_USERNAME = "analytics@fincept.in"
EMAIL_PASSWORD = "12Rjkrs34####"

def generate_stock_report(stock_symbol, file_path):
    """
    Generate a stock report for the given stock symbol and save it as an HTML file.
    Removes timezone information to resolve frequency issues.

    Args:
        stock_symbol (str): The stock symbol to analyze.
        file_path (str): The file path to save the report.

    Returns:
        bool: True if the report was successfully generated, False otherwise.
    """
    try:
        console.print(f"[bold cyan]Fetching data for {stock_symbol}...[/bold cyan]")
        stock = yf.Ticker(stock_symbol)
        data = stock.history(period="max")

        if data.empty:
            console.print(f"[bold red]No data found for {stock_symbol}.[/bold red]")
            return False

        console.print(f"[bold green]Data fetched successfully for {stock_symbol}[/bold green]")
        console.print(f"Data head:\n{data.head()}")
        console.print(f"Data index:\n{data.index}")

        # Ensure index is datetime and remove timezone
        data.index = data.index.tz_localize(None)
        console.print(f"Timezone removed. First 5 indices:\n{data.index[:5]}")

        # Resample to business days
        data = data.resample('B').last().dropna()
        console.print(f"Data resampled to business days. First 5 rows:\n{data.head()}")

        # Optionally, explicitly set the index freq to 'B'
        data.index.freq = 'B'

        # Calculate daily returns
        data['daily_returns'] = data['Close'].pct_change().dropna()
        console.print(f"Calculated daily returns. First 5 rows:\n{data[['Close', 'daily_returns']].head()}")

        if data['daily_returns'].empty:
            console.print(f"[bold red]Daily returns calculation failed: No valid data available.[/bold red]")
            return False

        # Generate the report
        # Explicitly pass freq='D' or freq='B' to quantstats
        import quantstats_updated as qs
        qs.reports.html(
            data['daily_returns'],
            output=file_path,
            title=f"Daily Report: {stock_symbol}",
            freq='B'  # or 'D' if you prefer daily with weekends
        )

        console.print(f"[bold green]Report for {stock_symbol} generated successfully at {file_path}![/bold green]")
        return True

    except AttributeError as ae:
        console.print(f"[bold red]AttributeError: {ae}[/bold red]")
        console.print("[bold yellow]Possible issue with data structure or attributes.[/bold yellow]")
        return False

    except Exception as e:
        console.print(f"[bold red]Error generating report: {e}[/bold red]")
        console.print("[bold yellow]Please verify the stock symbol and data availability on Yahoo Finance.[/bold yellow]")
        return False


def send_email(recipient, subject, body, attachment_path=None):
    """
    Send an email with an optional attachment.

    Args:
        recipient (str): Recipient's email address.
        subject (str): Email subject.
        body (str): Email body.
        attachment_path (str, optional): Path to the file attachment.
    """
    try:
        msg = MIMEMultipart()
        msg['From'] = EMAIL_USERNAME
        msg['To'] = recipient
        msg['Subject'] = subject

        # Attach email body
        msg.attach(MIMEText(body, 'plain'))

        # Attach file if provided
        if attachment_path:
            attachment = MIMEBase('application', 'octet-stream')
            with open(attachment_path, 'rb') as file:
                attachment.set_payload(file.read())
            encoders.encode_base64(attachment)
            attachment.add_header('Content-Disposition', f'attachment; filename={attachment_path.split("/")[-1]}')
            msg.attach(attachment)

        # Send email using SMTP
        with smtplib.SMTP_SSL(SMTP_SERVER, SMTP_PORT) as server:
            server.login(EMAIL_USERNAME, EMAIL_PASSWORD)
            server.sendmail(EMAIL_USERNAME, recipient, msg.as_string())

        console.print(f"[bold green]Email sent successfully to {recipient}![/bold green]")
    except Exception as e:
        console.print(f"[bold red]Error sending email: {e}[/bold red]")


def daily_report_menu():
    """
    Menu to generate and email stock reports.
    """
    while True:
        console.print("[bold cyan]DAILY STOCK REPORT MENU[/bold cyan]")
        menu_options = [
            "Generate and Email Daily Report",
            "Back to Main Menu"
        ]
        display_in_columns("Select an Option", menu_options)

        choice = Prompt.ask("Enter your choice")
        if choice == "1":
            stock_symbol = Prompt.ask("Enter the stock symbol (e.g., AAPL)")
            email_recipient = Prompt.ask("Enter the recipient's email address")
            file_path = f"{stock_symbol}_report.html"

            if generate_stock_report(stock_symbol, file_path):
                email_body = f"Attached is the daily report for {stock_symbol}."
                send_email(email_recipient, f"Daily Report: {stock_symbol}", email_body, file_path)
        elif choice == "2":
            from fincept_terminal.oldTerminal.cli import show_main_menu
            show_main_menu()
            return
        else:
            console.print("[bold red]Invalid choice, please try again.[/bold red]")
