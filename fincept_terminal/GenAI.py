API_URL = "https://fincept.share.zrok.io/process-gemini/"
HEADERS = {"Content-Type": "application/json"}

def genai_query(user_input):
    """
    Send the user's query to the GenAI API and return the response.

    Parameters:
    user_input (str): The query/question entered by the user.

    Returns:
    str: The formatted response from the GenAI API.
    """
    data = {"user_input": user_input}

    try:
        import requests
        response = requests.post(API_URL, headers=HEADERS, json=data)
        response.raise_for_status()  # Check for HTTP errors

        api_response = response.json()
        raw_text = api_response.get("gemini_response", "No response received from the server.")

        return format_genai_response(raw_text)

    except requests.exceptions.RequestException as e:
        return f"Error processing query: {str(e)}"

def format_genai_response(response):
    """
    Dynamically format the response text: remove unnecessary symbols, handle Markdown-like syntax, and apply styling.

    Parameters:
    response (str): The raw response text from the API.

    Returns:
    Text: A Rich Text object with formatted response.
    """
    response = response.replace("**", "").replace("##", "").strip()

    from rich.text import Text
    return Text(response, style="bold cyan")

def show_genai_query():
    """Prompt the user for a finance-related query and send it to the GenAI API."""
    from fincept_terminal.utils.themes import console
    console.print("[bold cyan]GENAI QUERY[/bold cyan]\n", style="info")

    while True:
        from rich.prompt import Prompt
        query = Prompt.ask("Enter your finance-related query (or type 'back' to return to main menu)")

        if query.lower() == 'back':
            from fincept_terminal.cli import show_main_menu
            return show_main_menu()

        console.print("\n[bold yellow]Processing your query...[/bold yellow]\n", style="info")
        response = genai_query(query)

        from rich.panel import Panel
        console.print(Panel(response, title="GenAI Query Response", style="cyan on #282828"))

        another_query = Prompt.ask("\nWould you like to make another query? (yes/no)")

        if another_query.lower() == 'no':
            console.print("\n[bold yellow]Redirecting to the main menu...[/bold yellow]", style="info")
            from fincept_terminal.cli import show_main_menu
            show_main_menu()
            return  # Redirect back to the main menu if user types 'no'

import requests

def fetch_gov_data():
    url = "https://datos.gob.es/en/apidata#/"
    response = requests.get(url)
    if response.status_code == 200:
        return response.json()
    else:
        return None

def display_gov_data(data):
    if data:
        # Format and display the data as needed
        print("Government Data:")
        for item in data:
            print(f"Title: {item['title']}, Description: {item['description']}")
    else:
        print("Failed to fetch data.")

def main_menu():
    while True:
        print("Main Menu:")
        print("1. Existing Option 1")
        print("2. Existing Option 2")
        print("3. Fetch Government Data")
        print("4. Exit")
        choice = input("Enter your choice: ")

        if choice == '1':
            # Existing functionality
            pass
        elif choice == '2':
            # Existing functionality
            pass
        elif choice == '3':
            data = fetch_gov_data()
            display_gov_data(data)
        elif choice == '4':
            break
        else:
            print("Invalid choice. Please try again.")

if __name__ == "__main__":
    main_menu()
