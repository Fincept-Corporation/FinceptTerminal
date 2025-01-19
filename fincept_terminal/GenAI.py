import requests
from rich.text import Text
from rich.prompt import Prompt
from rich.panel import Panel
from fincept_terminal.themes import console
from fincept_terminal.cli import show_main_menu

def genai_query(user_input):
    """
    Send the user's query to the GenAI API and return the response.

    Parameters:
    user_input (str): The query/question entered by the user.

    Returns:
    str: The formatted response from the GenAI API.
    """
    api_url = "https://fincept.share.zrok.io/process-gemini/"
    headers = {
        "Content-Type": "application/json"
    }
    data = {
        "user_input": user_input
    }

    try:
        # Send POST request
        response = requests.post(api_url, headers=headers, json=data)
        response.raise_for_status()  # Check for HTTP errors

        # Parse the JSON response and extract the 'gemini_response'
        api_response = response.json()
        raw_text = api_response.get("gemini_response", "No response received from the server.")

        # Handle any Markdown-like syntax and format the text dynamically
        formatted_response = format_genai_response(raw_text)
        return formatted_response

    except requests.exceptions.HTTPError as http_err:
        return f"HTTP error occurred: {http_err}"
    except requests.exceptions.ConnectionError as conn_err:
        return f"Connection error occurred: {conn_err}"
    except requests.exceptions.Timeout as timeout_err:
        return f"Timeout error occurred: {timeout_err}"
    except requests.exceptions.RequestException as req_err:
        return f"Error processing query: {req_err}"

def format_genai_response(response):
    """
    Dynamically format the response text: remove unnecessary symbols, handle Markdown-like syntax, and apply styling.

    Parameters:
    response (str): The raw response text from the API.

    Returns:
    Text: A Rich Text object with formatted response.
    """
    # Remove Markdown-like symbols (e.g., **, ##) and apply rich formatting
    response = response.replace("**", "").replace("##", "").strip()

    # Create a Rich Text object with cyan color and bold style
    formatted_text = Text(response, style="bold cyan")

    return formatted_text

def show_genai_query():
    """Prompt the user for a finance-related query and send it to the GenAI API."""
    console.print("[bold cyan]GENAI QUERY[/bold cyan]\n", style="info")

    while True:
        query = Prompt.ask("Enter your finance-related query (or type 'back' to return to main menu)")

        if query.lower() == 'back':
            return show_main_menu()

        # Send the query to the GenAI API
        console.print("\n[bold yellow]Processing your query...[/bold yellow]\n", style="info")
        response = genai_query(query)

        # Display the formatted response in a panel
        console.print(Panel(response, title="GenAI Query Response", style="cyan on #282828"))

        # Ask if the user wants to make another query
        another_query = Prompt.ask("\nWould you like to make another query? (yes/no)")

        if another_query.lower() == 'no':
            console.print("\n[bold yellow]Redirecting to the main menu...[/bold yellow]", style="info")
            show_main_menu()
            return  # Redirect back to the main menu if user types 'no'
