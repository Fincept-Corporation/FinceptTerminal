import requests
from rich.console import Console
from rich.panel import Panel
import os
import ujson as json
from rich.prompt import Prompt
from fincept_terminal.oldTerminal.cli import show_main_menu
# from fincept_terminal.data_connector.genai import genai_query

console = Console()

# Path to settings.json
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
SETTINGS_FILE = os.path.join(BASE_DIR, "..", "settings", "settings.json")

def load_settings():
    """Load user settings from settings.json."""
    if os.path.exists(SETTINGS_FILE):
        with open(SETTINGS_FILE, "r") as file:
            return json.load(file)
    return {}

def genai_query(user_input):
    """
    Handle the query and dynamically determine the data source for processing.

    Parameters:
    user_input (str): The user's input query.

    Returns:
    str: The formatted response from the selected GenAI API.
    """
    settings = load_settings()
    source = settings.get("data_sources", {}).get("GenAI Model", {}).get("source", "Gemini")
    api_key = settings.get("data_sources", {}).get("GenAI Model", {}).get("api_key")

    if source == "OpenAI (API)":
        response = query_openai(user_input, api_key)
    elif source == "Ollama (API)":
        response = query_ollama(user_input, api_key)
    elif source == "Claude (API)":
        response = query_claude(user_input, api_key)
    elif source == "Gemini (API)":
        response = query_gemini_direct(user_input, api_key)
    else:
        response = "[bold red]No valid GenAI source configured in settings.json.[/bold red]"

    # Format the response using format_genai_response
    formatted_response = format_genai_response(response)
    return formatted_response


def query_gemini_direct(user_input, api_key):
    import google.generativeai as genai
    """
    Query Gemini directly via their API using the google.generativeai library.

    Parameters:
    user_input (str): The user's input query.
    api_key (str): The API key for Gemini.

    Returns:
    str: The response text from Gemini.
    """
    try:
        # Configure the Gemini API with the provided API key
        genai.configure(api_key=api_key)

        # Use the Gemini generative model
        model = genai.GenerativeModel("gemini-1.5-flash")

        # Generate content based on the user's query
        response = model.generate_content(user_input)

        # Extract and return the text from the response
        return response.text

    except Exception as e:
        # Handle and return any errors encountered during the request
        return f"[bold red]Error querying Gemini: {str(e)}[/bold red]"


def query_openai(user_input, api_key, model="gpt-4o-mini"):
    import openai
    """
    Query the OpenAI API using the new API structure (>=1.0.0).

    Parameters:
    user_input (str): The user's input query.
    api_key (str): The API key for OpenAI.
    model (str): The model to use for the query (default: 'gpt-4').

    Returns:
    str: The response text from OpenAI.
    """
    try:
        # Configure OpenAI with the user's API key
        openai.api_key = api_key

        # Use the new `openai.ChatCompletion.create` structure
        response = openai.ChatCompletion.create(
            model=model,
            messages=[
                {"role": "system", "content": "You are a helpful assistant."},
                {"role": "user", "content": user_input}
            ]
        )

        # Extract and return the response text
        return response.choices[0].message.content

    except Exception as e:
        # Handle any errors from the OpenAI API
        return f"Error querying OpenAI: {str(e)}"

def query_ollama(user_input, model="llama2", api_key=None):
    """
    Query the Ollama API using the latest documentation.

    Parameters:
    user_input (str): The user's input query.
    model (str): The model to use for the query (default: 'llama2').
    api_key (str): Optional API key for accessing the Ollama API.

    Returns:
    str: The response text from Ollama.
    """
    url = f"http://localhost:11434/api/generate"  # Default local Ollama API URL
    headers = {
        "Content-Type": "application/json",
    }
    data = {
        "model": model,
        "prompt": user_input
    }

    # If an API key is provided, include it in the headers
    if api_key:
        headers["Authorization"] = f"Bearer {api_key}"

    try:
        response = requests.post(url, headers=headers, json=data)
        response.raise_for_status()
        api_response = response.json()
        return api_response.get("text", "No response received from Ollama.")
    except requests.exceptions.RequestException as e:
        return f"Error querying Ollama: {str(e)}"

def query_claude(user_input, api_key, model="claude-3"):
    import anthropic
    """
    Query the Claude API using the `anthropic` Python library.

    Parameters:
    user_input (str): The user's input query.
    api_key (str): The API key for Claude (Anthropic).
    model (str): The Claude model to use (default: 'claude-3').

    Returns:
    str: The response text from Claude.
    """
    try:
        # Initialize the Anthropics client
        client = anthropic.Anthropic(api_key=api_key)

        # Send the query to Claude
        response = client.completions.create(
            model=model,
            max_tokens_to_sample=150,  # Adjust as needed
            prompt=f"{anthropic.HUMAN_PROMPT} {user_input} {anthropic.AI_PROMPT}"
        )

        # Extract and return the response text
        return response["completion"]

    except anthropic.exceptions.AnthropicException as e:
        # Handle any errors from the Anthropics API
        return f"Error querying Claude: {str(e)}"


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
    from rich.text import Text
    formatted_text = Text(response, style="bold cyan")

    return formatted_text


def show_genai_query():
    """
    Prompt the user for a finance-related query and send it to the GenAI API.
    This function dynamically routes the query to the configured data source.
    """
    console.print("[bold cyan]GENAI QUERY[/bold cyan]\n")

    while True:
        query = Prompt.ask("Enter your finance-related query (or type 'back' to return to main menu)")

        if query.lower() == 'back':
            console.print("\n[bold yellow]Redirecting to the main menu...[/bold yellow]")
            return show_main_menu()

        # Send the query to the GenAI API
        console.print("\n[bold yellow]Processing your query...[/bold yellow]\n")
        response = genai_query(query)

        # Display the formatted response in a panel
        console.print(Panel(response, title="GenAI Query Response", style="cyan on #282828"))

        # Ask if the user wants to make another query
        another_query = Prompt.ask("\nWould you like to make another query? (yes/no)", default="yes")

        if another_query.lower() != 'yes':
            console.print("\n[bold yellow]Redirecting to the main menu...[/bold yellow]")
            return show_main_menu()