from fincept_terminal.utils.themes import console
from gnews import GNews

google_news = GNews(language='en', max_results=25)

# Country code mapping based on GNews documentation
COUNTRY_CODES = {
    'Australia': 'AU', 'Botswana': 'BW', 'Canada': 'CA', 'Ethiopia': 'ET', 'Ghana': 'GH', 'India': 'IN',
    'Indonesia': 'ID', 'Ireland': 'IE', 'Israel': 'IL', 'Kenya': 'KE', 'Latvia': 'LV', 'Malaysia': 'MY',
    'Namibia': 'NA', 'New Zealand': 'NZ', 'Nigeria': 'NG', 'Pakistan': 'PK', 'Philippines': 'PH',
    'Singapore': 'SG', 'South Africa': 'ZA', 'Tanzania': 'TZ', 'Uganda': 'UG', 'United Kingdom': 'GB',
    'United States': 'US', 'Zimbabwe': 'ZW', 'Czech Republic': 'CZ', 'Germany': 'DE', 'Austria': 'AT',
    'Switzerland': 'CH', 'Argentina': 'AR', 'Chile': 'CL', 'Colombia': 'CO', 'Cuba': 'CU', 'Mexico': 'MX',
    'Peru': 'PE', 'Venezuela': 'VE', 'Belgium': 'BE', 'France': 'FR', 'Morocco': 'MA', 'Senegal': 'SN',
    'Italy': 'IT', 'Lithuania': 'LT', 'Hungary': 'HU', 'Netherlands': 'NL', 'Norway': 'NO', 'Poland': 'PL',
    'Brazil': 'BR', 'Portugal': 'PT', 'Romania': 'RO', 'Slovakia': 'SK', 'Slovenia': 'SI', 'Sweden': 'SE',
    'Vietnam': 'VN', 'Turkey': 'TR', 'Greece': 'GR', 'Bulgaria': 'BG', 'Russia': 'RU', 'Ukraine': 'UA',
    'Serbia': 'RS', 'United Arab Emirates': 'AE', 'Saudi Arabia': 'SA', 'Lebanon': 'LB', 'Egypt': 'EG',
    'Bangladesh': 'BD', 'Thailand': 'TH', 'China': 'CN', 'Taiwan': 'TW', 'Hong Kong': 'HK', 'Japan': 'JP',
    'Republic of Korea': 'KR'
}

def set_gnews_country(country):
    google_news.country = COUNTRY_CODES.get(country, None)

def fetch_gnews_news(topic, country):
    console.print(f"[bold cyan]Fetching {topic} news from GNews for {country}...[/bold cyan]")
    set_gnews_country(country)
    articles = google_news.get_news_by_topic(topic.lower())

    if not articles:
        console.print(f"[bold red]No news articles found for {topic} in {country} using GNews.[/bold red]")
        return []

    return articles