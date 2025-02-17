from textual.widgets import Static, Button
from textual.containers import VerticalScroll

class HelpWindow(VerticalScroll):
    """Help and About Us Screen that is vertically scrollable."""

    def compose(self):
        # Header (optional)
        yield Static("[bold underline]Help & About Us[/]", id="help-header")

        # Main help text
        yield Static(
            """
**Empowering Your Financial Journey with Precision & Insight**

---

## **📌 About Fincept Terminal**

Fincept Terminal is a cutting-edge financial analysis platform designed to provide 
real-time market data, portfolio management, and actionable insights to investors, traders, 
and financial professionals. Our platform integrates advanced analytics, AI-driven sentiment analysis, 
and the latest market trends to help you make well-informed decisions.

---

## **🔹 Features & Capabilities**

### **📈 Market Tracking & Real-Time Data**
- Access real-time stock prices, indices, bonds, and currency exchange rates.
- Stay ahead with live updates on market movements and trends.

### **💼 Portfolio Management**
- Track, analyze, and optimize your investment portfolio.
- View sector-wise and industry-wise allocations for informed diversification.

### **📊 Advanced Financial Analytics**
- Perform in-depth technical & fundamental analysis.
- Backtest strategies and simulate different investment scenarios.

### **📰 Financial News & Sentiment Analysis**
- Get curated financial news from global sources.
- Analyze public sentiment around stocks, industries, and market events.

---

## **📢 Need Assistance? We're Here to Help!**

Whether you're a beginner exploring the markets or a seasoned investor optimizing your strategy, 
Fincept Terminal provides robust support to ensure you maximize your financial potential.

- **📖 Help & Documentation**: Explore our user guide and FAQs.
- **📧 Contact Support**: Reach us at **support@fincept.com** for any queries.
- **💬 Community Forum**: Engage with other users, share insights, and discuss market trends.

---

## **💡 Suggestions & Feedback**

Your feedback is valuable to us! We continuously work on improving Fincept Terminal to enhance 
your experience. If you have suggestions or feature requests, please let us know at:

- **📩 feedback@fincept.com**
- **📣 Community Polls & Surveys**

---

**Stay Informed. Stay Ahead. Invest Smarter with Fincept Terminal.** 🚀
            """,
            id="help-text",
        )

        # Back Button
        yield Button("Back", id="back_to_dashboard")

    async def on_button_pressed(self, event: Button.Pressed) -> None:
        """Handle button presses inside HelpWindow."""
        if event.button.id == "back_to_dashboard":
            self.notify("Returning to Dashboard...")
            await self.app.pop_screen()
