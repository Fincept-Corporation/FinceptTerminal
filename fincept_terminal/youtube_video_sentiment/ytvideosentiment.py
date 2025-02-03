# Function to extract video ID from a YouTube URL
def extract_video_id(youtube_url):
    pattern = r"(?:https?:\/\/)?(?:www\.)?(?:youtube\.com\/(?:[^\/]+\/.+\/|(?:v|e(?:mbed)?)\/|.*[?&]v=)|youtu\.be\/)([^\"&?\/\s]{11})"
    import re
    match = re.search(pattern, youtube_url)
    if match:
        return match.group(1)
    return None


# Function to extract captions from a YouTube video
def extract_captions(video_id):
    try:
        from youtube_transcript_api import YouTubeTranscriptApi
        transcript_list = YouTubeTranscriptApi.get_transcript(video_id)

        # Check if the video's length exceeds 3 minutes (180 seconds)
        total_duration = sum([line['duration'] for line in transcript_list])
        from fincept_terminal.FinceptTerminalUtils.themes import console
        if total_duration > 180:
            console.print("[bold red]Video exceeds the 3-minute limit![/bold red]")
            return None

        # Combine the text from transcript into a single string
        captions_text = ' '.join([line['text'] for line in transcript_list])

        # Return the extracted text
        return captions_text
    except Exception as e:
        console.print(f"[bold red]An error occurred while extracting captions: {e}[/bold red]")
        return None


# Function to analyze the sentiment of the extracted captions
def analyze_sentiment(text):
    # Replace this with your GemAI API URL and key
    api_url = "https://fincept.share.zrok.io/process-gemini/"
    headers = {"Content-Type": "application/json"}
    data = {"user_input": text}

    try:
        import requests
        response = requests.post(api_url, headers=headers, json=data)
        response.raise_for_status()

        # Parse the response
        sentiment_response = response.json().get("gemini_response", "No response received.")
        return sentiment_response
    except requests.exceptions.RequestException as e:
        from fincept_terminal.FinceptTerminalUtils.themes import console
        console.print(f"[bold red]Error analyzing sentiment: {e}[/bold red]")
        return None


# Function to get video URL and perform sentiment analysis
def sentiment_analysis_for_video():
    from fincept_terminal.FinceptTerminalUtils.themes import console
    console.print("[bold cyan]YouTube Sentiment Analysis[/bold cyan]")

    from rich.prompt import Prompt
    youtube_link = Prompt.ask("Enter the YouTube video URL (max 3-minute video)")
    video_id = extract_video_id(youtube_link)

    if video_id:
        console.print(f"[bold green]Extracted video ID: {video_id}[/bold green]")
        captions_text = extract_captions(video_id)

        if captions_text:
            console.print("[bold green]Subtitles extracted successfully![/bold green]")
            console.print("\n[bold cyan]Analyzing sentiment...[/bold cyan]")

            sentiment_result = analyze_sentiment(captions_text)

            if sentiment_result:
                from rich.panel import Panel
                console.print(Panel(sentiment_result, title="Sentiment Analysis Result", style="cyan"))
            else:
                console.print("[bold red]Sentiment analysis failed.[/bold red]")
        else:
            console.print("[bold red]No subtitles found or video exceeds length limit.[/bold red]")
    else:
        console.print("[bold red]Invalid YouTube URL.[/bold red]")
