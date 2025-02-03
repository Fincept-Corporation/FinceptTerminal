import requests
from scholarly import scholarly
import google.generativeai as genai
from rich.console import Console
from rich.panel import Panel
from rich.text import Text
from rich.prompt import Prompt
from io import BytesIO
from PyPDF2 import PdfReader
import re

# Configure Gemini API Key
genai.configure(api_key="AIzaSyDqwj91u7AjqWk7KcjgUX73VTX5PawNZxY")
gemini_model = genai.GenerativeModel('gemini-1.5-flash')
console = Console()

# Cache for storing the last searched city's data
search_cache = {"last_city": None, "papers": []}

# -----------------------------------------------
# Fetch Research Papers
# -----------------------------------------------
def fetch_research_papers(city, max_papers=10):
    """
    Fetch research papers related to consumer behavior in a given city.
    """
    if search_cache["last_city"] == city:
        console.print(Panel(f"[bold cyan]Using cached research papers for {city}.[/bold cyan]"))
        return search_cache["papers"]

    console.print(Panel(f"[bold cyan]Searching for research papers on consumer behavior in {city}...[/bold cyan]"))
    query = f"consumer behavior in {city}"
    search_results = scholarly.search_pubs(query)
    papers = []
    for result in search_results:
        title = result.get("title", "Unknown Title")
        url = result.get("eprint_url", result.get("pub_url", ""))
        if title and url:
            papers.append({"title": title, "url": url})
        if len(papers) >= max_papers:
            break
    if not papers:
        console.print(Panel("[bold red]No research papers found.[/bold red]"))
    else:
        console.print(Panel(f"[bold green]Found {len(papers)} research papers.[/bold green]"))

    # Update cache
    search_cache["last_city"] = city
    search_cache["papers"] = papers
    return papers

# -----------------------------------------------
# Fetch and Process Paper Content
# -----------------------------------------------
def fetch_paper_content(url):
    """
    Fetch the content of the paper from the URL. Handles both text and PDF content.
    """
    try:
        response = requests.get(url, stream=True)
        response.raise_for_status()
        if "application/pdf" in response.headers.get("Content-Type", ""):
            pdf_file = BytesIO(response.content)
            reader = PdfReader(pdf_file)
            text = ""
            for page in reader.pages:
                text += page.extract_text()
            return text or "Unable to extract text from PDF."
        return response.text
    except Exception as e:
        return f"Error fetching content: {e}"

def consolidate_paper_content(papers, max_papers=5):
    """
    Consolidate content from multiple research papers.
    """
    combined_content = ""
    inaccessible_papers = []
    count = 0

    for paper in papers:
        if count >= max_papers:
            break
        content = fetch_paper_content(paper["url"])
        if "Error fetching content" in content:
            inaccessible_papers.append({"title": paper["title"], "url": paper["url"], "error": content})
            combined_content += f"Paper Title: {paper['title']}\nURL: {paper['url']}\n[Note: Inaccessible]\n\n"
        else:
            combined_content += f"Paper Title: {paper['title']}\nContent:\n{content}\n\n"
        count += 1

    if inaccessible_papers:
        console.print(Panel("[bold yellow]Some papers could not be accessed but are included with metadata.[/bold yellow]"))

    return combined_content

# -----------------------------------------------
# Extract Facts Dynamically
# -----------------------------------------------
def extract_facts_dynamically(content):
    """
    Dynamically extract factual data from the consolidated content.
    """
    facts = []
    patterns = [
        r"(\d+%)",  # Percentages
        r"₹[\d,]+",  # Currency values in Indian Rupees
        r"\d+\s?(times|times per month|years|days)",  # Frequency or time-related data
        r"\d+\s?(respondents|samples|people|users|shoppers)",  # Sample size or user data
    ]

    for pattern in patterns:
        matches = re.findall(pattern, content)
        for match in matches:
            if match not in facts:  # Avoid duplicates
                facts.append(match)

    return facts

# -----------------------------------------------
# Enhance Summary with Facts
# -----------------------------------------------
def enhance_summary_with_facts(summary, facts):
    """
    Enhance the summary by embedding extracted facts into the appropriate sections.
    """
    enhanced_summary = summary
    for fact in facts:
        # Dynamically insert facts into summary where appropriate
        if "%" in fact or "₹" in fact or "times" in fact or "respondents" in fact:
            enhanced_summary = enhanced_summary.replace("FILL_FACT", fact, 1)
    return enhanced_summary

# -----------------------------------------------
# Format and Clean Report Content
# -----------------------------------------------
def format_report_content(report):
    """
    Cleans and formats the report content for better readability.
    Removes symbols like **, ##, and replaces with human-readable symbols.
    Adds spacing for titles and paragraphs.
    """
    cleaned_report = report

    # Remove markdown-like symbols and replace with human-readable ones
    cleaned_report = re.sub(r"\*\*", "", cleaned_report)
    cleaned_report = re.sub(r"##", "", cleaned_report)

    # Add spacing for titles and paragraphs
    cleaned_report = re.sub(r"(\d+\.\s)([A-Z])", r"\n\1\2", cleaned_report)
    cleaned_report = re.sub(r"\n{2,}", "\n\n", cleaned_report)  # Ensure proper spacing

    return cleaned_report

# -----------------------------------------------
# Generate Summary Report Using Gemini
# -----------------------------------------------
def generate_summary_report(content):
    """
    Generate a brief summary report based on the consolidated content.
    """
    try:
        response = gemini_model.generate_content(
            f"Summarize the following research content into a structured consumer behavior report with actionable insights and key trends. Use placeholders like FILL_FACT for dynamic fact insertion:\n\n{content}"
        )
        report = response.text
        return format_report_content(report)
    except Exception as e:
        return f"Error generating summary report: {e}"

def display_summary_report(report):
    """
    Formats and displays the summary report in a structured format with Rich.
    """
    lines = report.split("\n")
    formatted_output = []
    for line in lines:
        if line.strip():
            if "Executive Summary" in line or "Key Trends" in line:
                formatted_output.append(Text(line.strip(), style="bold cyan"))
            elif line.strip().endswith(":"):
                formatted_output.append(Text(line.strip(), style="bold green"))
            else:
                formatted_output.append(Text(line.strip(), style="white"))

    report_text = "\n".join([str(item) for item in formatted_output])
    console.print(Panel(report_text, title="[bold cyan]Consumer Behavior Report[/bold cyan]"))

# -----------------------------------------------
# Q&A Using Gemini
# -----------------------------------------------
def ask_questions_on_content(content):
    """
    Allows the user to ask specific questions on the consolidated content.
    """
    while True:
        console.print(Panel("[bold cyan]Ask a question about the research papers:[/bold cyan]"))
        question = Prompt.ask("Your question")
        try:
            response = gemini_model.generate_content(
                f"Answer the following question based on this context:\n{content}\n\nQuestion: {question}"
            )
            console.print(Panel(f"[bold green]Answer:[/bold green]\n{response.text}"))
        except Exception as e:
            console.print(Panel(f"[bold red]Error generating answer: {e}[/bold red]"))
        next_action = Prompt.ask("Do you want to ask another question? (yes/no)").strip().lower()
        if next_action != "yes":
            break

# -----------------------------------------------
# Consumer Behavior Insights Workflow
# -----------------------------------------------
def generate_consumer_behavior_insights(city):
    """
    Fetch papers, generate insights, and provide Q&A functionality.
    """
    papers = fetch_research_papers(city)
    if not papers:
        return

    console.print(Panel("[bold cyan]Fetching and consolidating content from papers...[/bold cyan]"))
    combined_content = consolidate_paper_content(papers)
    if not combined_content.strip():
        console.print(Panel("[bold red]No accessible content found in the papers.[/bold red]"))
        return

    console.print(Panel("[bold cyan]Generating summary report...[/bold cyan]"))
    summary_report = generate_summary_report(combined_content)

    console.print(Panel("[bold cyan]Extracting additional factual insights...[/bold cyan]"))
    facts = extract_facts_dynamically(combined_content)

    enhanced_summary = enhance_summary_with_facts(summary_report, facts)
    display_summary_report(enhanced_summary)

    console.print(Panel("[bold cyan]You can now ask specific questions based on the research content.[/bold cyan]"))
    ask_questions_on_content(combined_content)

# -----------------------------------------------
# Interactive Menu
# -----------------------------------------------
def interactive_menu():
    """
    Interactive menu for the consumer behavior analysis.
    """
    while True:
        console.print(Panel("[bold cyan]Consumer Behavior Analysis Menu[/bold cyan]"))
        console.print(
            """
1. Analyze Consumer Behavior in a City
2. Exit
"""
        )
        choice = Prompt.ask("Choose an option", choices=["1", "2"], default="2")
        if choice == "1":
            city = Prompt.ask("Enter city to analyze")
            generate_consumer_behavior_insights(city)
        elif choice == "2":
            console.print(Panel("[bold green]Goodbye![/bold green]"))
            from fincept_terminal.oldTerminal.cli import show_main_menu
            show_main_menu()

# Run the interactive menu
interactive_menu()
