import requests
from bs4 import BeautifulSoup
from selenium import webdriver
from selenium.webdriver.chrome.options import Options
from selenium.webdriver.common.by import By
from selenium.webdriver.support.ui import WebDriverWait
from selenium.webdriver.support import expected_conditions as EC
import json
from datetime import datetime
import time

def scrape_forex_calendar(date_str="oct15.2025"):
    """
    Scrapes Forex Factory calendar for given date
    Args:
        date_str: Date in format like 'oct15.2025'
    Returns:
        JSON with structured data
    """
    url = f"https://www.forexfactory.com/calendar?day={date_str}"

    # Setup headless Chrome
    options = Options()
    options.add_argument('--headless')
    options.add_argument('--no-sandbox')
    options.add_argument('--disable-dev-shm-usage')
    options.add_argument('--user-agent=Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36')

    driver = webdriver.Chrome(options=options)
    events = []

    try:
        driver.get(url)

        # Wait for calendar table to load
        wait = WebDriverWait(driver, 10)
        wait.until(EC.presence_of_element_located((By.CLASS_NAME, "calendar__table")))

        time.sleep(2)  # Allow dynamic content to load

        # Find all calendar rows
        rows = driver.find_elements(By.CSS_SELECTOR, "tr.calendar__row")

        for row in rows:
            try:
                event = {}

                # Time
                time_elem = row.find_elements(By.CLASS_NAME, "calendar__time")
                event['time'] = time_elem[0].text if time_elem else ""

                # Currency
                currency_elem = row.find_elements(By.CLASS_NAME, "calendar__currency")
                event['currency'] = currency_elem[0].text if currency_elem else ""

                # Impact
                impact_elem = row.find_elements(By.CLASS_NAME, "calendar__impact")
                if impact_elem:
                    impact_spans = impact_elem[0].find_elements(By.TAG_NAME, "span")
                    event['impact'] = len([s for s in impact_spans if 'icon--ff-impact-red' in s.get_attribute('class') or 'icon--ff-impact-ora' in s.get_attribute('class') or 'icon--ff-impact-yel' in s.get_attribute('class')])
                else:
                    event['impact'] = 0

                # Event name
                event_elem = row.find_elements(By.CLASS_NAME, "calendar__event")
                event['event'] = event_elem[0].text.strip() if event_elem else ""

                # Actual value
                actual_elem = row.find_elements(By.CLASS_NAME, "calendar__actual")
                event['actual'] = actual_elem[0].text if actual_elem else ""

                # Forecast value
                forecast_elem = row.find_elements(By.CLASS_NAME, "calendar__forecast")
                event['forecast'] = forecast_elem[0].text if forecast_elem else ""

                # Previous value
                previous_elem = row.find_elements(By.CLASS_NAME, "calendar__previous")
                event['previous'] = previous_elem[0].text if previous_elem else ""

                # Only add if event has meaningful data
                if event['event'] and event['event'] not in ['', 'All Day']:
                    events.append(event)

            except Exception as e:
                continue

    finally:
        driver.quit()

    return {
        "date": date_str,
        "url": url,
        "events_count": len(events),
        "events": events
    }

# Alternative using requests-html for dynamic content
def scrape_forex_calendar_alt(date_str="oct15.2025"):
    """Alternative method using requests-html"""
    from requests_html import HTMLSession

    url = f"https://www.forexfactory.com/calendar?day={date_str}"
    session = HTMLSession()

    try:
        r = session.get(url)
        r.html.render(timeout=20)

        events = []

        # Parse calendar rows
        rows = r.html.find('tr.calendar__row')

        for row in rows:
            event = {}

            event['time'] = row.find('.calendar__time', first=True).text if row.find('.calendar__time') else ""
            event['currency'] = row.find('.calendar__currency', first=True).text if row.find('.calendar__currency') else ""

            impact = row.find('.calendar__impact', first=True)
            if impact:
                event['impact'] = len(impact.find('.icon--ff-impact-red, .icon--ff-impact-ora, .icon--ff-impact-yel'))
            else:
                event['impact'] = 0

            event['event'] = row.find('.calendar__event', first=True).text if row.find('.calendar__event') else ""
            event['actual'] = row.find('.calendar__actual', first=True).text if row.find('.calendar__actual') else ""
            event['forecast'] = row.find('.calendar__forecast', first=True).text if row.find('.calendar__forecast') else ""
            event['previous'] = row.find('.calendar__previous', first=True).text if row.find('.calendar__previous') else ""

            if event['event'] and event['event'] not in ['', 'All Day']:
                events.append(event)

    except Exception as e:
        print(f"Error: {e}")
        return {"error": str(e)}

    return {
        "date": date_str,
        "url": url,
        "events_count": len(events),
        "events": events
    }

if __name__ == "__main__":
    # Use Selenium method (more reliable)
    data = scrape_forex_calendar("oct16.2025")

    # Save to JSON file
    with open('forex_calendar.json', 'w') as f:
        json.dump(data, f, indent=2)

    print(json.dumps(data, indent=2))