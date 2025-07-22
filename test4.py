import google.generativeai as genai
import pandas as pd
import numpy as np
import json
import time
import random
from datetime import datetime, timedelta
from typing import Dict, List, Any
import threading
import logging
from dataclasses import dataclass

# Configure Gemini API
GEMINI_API_KEY = "AIzaSyD_FXxazCSRO9m1UreocvcpFEn2iFF87A0"
# genai.configure(api_key=GEMINI_API_KEY)

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


@dataclass
class MarketEvent:
    """Represents a market-moving event"""
    event_type: str
    description: str
    impact_magnitude: float  # -1.0 to 1.0
    duration_minutes: int
    affected_metrics: List[str]
    timestamp: datetime


class DynamicMarketSimulator:
    """Simulates realistic market conditions with events, news, and sentiment changes"""

    def __init__(self, ticker: str, initial_price: float = 150.0):
        self.ticker = ticker
        self.current_price = initial_price
        self.base_price = initial_price
        self.price_history = [initial_price]
        self.volume_history = [1000000]
        self.news_events = []
        self.sentiment_score = 0.5  # 0-1 scale
        self.rsi = 50.0
        self.macd = 0.0
        self.volume_multiplier = 1.0
        self.active_events = []
        self.last_update = datetime.now()

        # Market session state
        self.market_session = "open"  # open, pre_market, after_hours, closed
        self.volatility_regime = "normal"  # low, normal, high, extreme

        # Initialize technical indicators
        self._initialize_technical_state()

    def _initialize_technical_state(self):
        """Initialize realistic technical indicators"""
        # Create some price history for realistic indicators
        for i in range(50):
            price_change = random.gauss(0, 0.02)  # 2% daily volatility
            new_price = self.price_history[-1] * (1 + price_change)
            self.price_history.append(new_price)

            volume = random.randint(800000, 1200000)
            self.volume_history.append(volume)

        self.current_price = self.price_history[-1]
        self._update_technical_indicators()

    def generate_market_event(self) -> MarketEvent:
        """Generate realistic market events"""
        event_types = [
            {
                "type": "earnings_beat",
                "description": f"{self.ticker} reports Q3 earnings of $2.85 vs $2.42 expected (+17.8% beat)",
                "impact": random.uniform(0.3, 0.8),
                "duration": random.randint(60, 240),
                "metrics": ["price", "volume", "sentiment"]
            },
            {
                "type": "earnings_miss",
                "description": f"{self.ticker} misses Q3 estimates: $1.95 vs $2.30 expected (-15.2%)",
                "impact": random.uniform(-0.7, -0.3),
                "duration": random.randint(120, 360),
                "metrics": ["price", "volume", "sentiment"]
            },
            {
                "type": "analyst_upgrade",
                "description": f"Goldman Sachs upgrades {self.ticker} to BUY, raises target to ${self.current_price * 1.2:.0f}",
                "impact": random.uniform(0.1, 0.4),
                "duration": random.randint(30, 120),
                "metrics": ["sentiment", "price"]
            },
            {
                "type": "analyst_downgrade",
                "description": f"Morgan Stanley downgrades {self.ticker} to SELL, cuts target to ${self.current_price * 0.8:.0f}",
                "impact": random.uniform(-0.4, -0.1),
                "duration": random.randint(60, 180),
                "metrics": ["sentiment", "price"]
            },
            {
                "type": "partnership_news",
                "description": f"{self.ticker} announces strategic partnership with tech giant worth $3.2B over 5 years",
                "impact": random.uniform(0.2, 0.6),
                "duration": random.randint(90, 300),
                "metrics": ["price", "sentiment", "volume"]
            },
            {
                "type": "regulatory_concern",
                "description": f"SEC opens investigation into {self.ticker}'s accounting practices",
                "impact": random.uniform(-0.6, -0.2),
                "duration": random.randint(180, 720),
                "metrics": ["price", "sentiment", "volume"]
            },
            {
                "type": "insider_selling",
                "description": f"{self.ticker} CEO sells 500,000 shares in planned transaction",
                "impact": random.uniform(-0.2, -0.05),
                "duration": random.randint(30, 90),
                "metrics": ["sentiment", "volume"]
            },
            {
                "type": "breakthrough_product",
                "description": f"{self.ticker} unveils revolutionary AI chip with 40% performance improvement",
                "impact": random.uniform(0.4, 0.9),
                "duration": random.randint(120, 480),
                "metrics": ["price", "sentiment", "volume"]
            },
            {
                "type": "macro_fed_cut",
                "description": "Fed cuts interest rates by 0.25%, tech stocks rally on growth optimism",
                "impact": random.uniform(0.1, 0.3),
                "duration": random.randint(60, 240),
                "metrics": ["price", "sentiment"]
            },
            {
                "type": "macro_inflation",
                "description": "CPI data shows inflation rising to 4.2%, growth stocks under pressure",
                "impact": random.uniform(-0.3, -0.1),
                "duration": random.randint(120, 360),
                "metrics": ["price", "sentiment"]
            },
            {
                "type": "social_media_buzz",
                "description": f"Reddit WallStreetBets shows massive interest in {self.ticker}, mentions up 400%",
                "impact": random.uniform(0.1, 0.5),
                "duration": random.randint(60, 180),
                "metrics": ["sentiment", "volume", "price"]
            },
            {
                "type": "competitor_news",
                "description": f"Major competitor announces product delay, benefiting {self.ticker}",
                "impact": random.uniform(0.1, 0.4),
                "duration": random.randint(90, 240),
                "metrics": ["price", "sentiment"]
            }
        ]

        event_data = random.choice(event_types)

        return MarketEvent(
            event_type=event_data["type"],
            description=event_data["description"],
            impact_magnitude=event_data["impact"],
            duration_minutes=event_data["duration"],
            affected_metrics=event_data["metrics"],
            timestamp=datetime.now()
        )

    def update_market_state(self):
        """Update market state with realistic dynamics"""
        current_time = datetime.now()
        time_elapsed = (current_time - self.last_update).total_seconds() / 60  # minutes

        # Random event generation (5% chance per update)
        if random.random() < 0.05:
            new_event = self.generate_market_event()
            self.active_events.append(new_event)
            self.news_events.append(new_event)
            logger.info(f"🚨 MARKET EVENT: {new_event.description}")

        # Update market session
        self._update_market_session()

        # Calculate price movement
        price_change = self._calculate_price_movement(time_elapsed)

        # Apply price change
        self.current_price *= (1 + price_change)
        self.price_history.append(self.current_price)

        # Update volume
        self._update_volume()

        # Update technical indicators
        self._update_technical_indicators()

        # Update sentiment
        self._update_sentiment()

        # Clean up expired events
        self._cleanup_expired_events()

        self.last_update = current_time

    def _calculate_price_movement(self, time_elapsed: float) -> float:
        """Calculate realistic price movement"""
        base_volatility = 0.0002  # Base 2 basis points per minute

        # Adjust for market session
        session_multiplier = {
            "pre_market": 0.5,
            "open": 1.0,
            "after_hours": 0.3,
            "closed": 0.1
        }

        volatility = base_volatility * session_multiplier[self.market_session]

        # Apply event impacts
        event_impact = 0.0
        for event in self.active_events:
            minutes_since = (datetime.now() - event.timestamp).total_seconds() / 60
            if minutes_since < event.duration_minutes and "price" in event.affected_metrics:
                # Decay impact over time
                decay_factor = 1.0 - (minutes_since / event.duration_minutes)
                event_impact += event.impact_magnitude * 0.001 * decay_factor  # Scale down impact

        # Random walk component
        random_change = random.gauss(0, volatility * time_elapsed)

        # Mean reversion component (subtle)
        mean_reversion = (self.base_price - self.current_price) / self.current_price * 0.0001

        total_change = random_change + event_impact + mean_reversion

        return total_change

    def _update_volume(self):
        """Update trading volume realistically"""
        base_volume = 1000000

        # Session impact
        session_multiplier = {
            "pre_market": 0.2,
            "open": 1.5,  # Higher volume at open
            "after_hours": 0.3,
            "closed": 0.05
        }

        volume_multiplier = session_multiplier[self.market_session]

        # Event impact on volume
        for event in self.active_events:
            if "volume" in event.affected_metrics:
                minutes_since = (datetime.now() - event.timestamp).total_seconds() / 60
                if minutes_since < event.duration_minutes:
                    volume_multiplier *= (1 + abs(event.impact_magnitude) * 2)

        # Random variation
        volume_multiplier *= random.uniform(0.7, 1.3)

        current_volume = int(base_volume * volume_multiplier)
        self.volume_history.append(current_volume)

    def _update_technical_indicators(self):
        """Update technical indicators based on price history"""
        if len(self.price_history) < 14:
            return

        # RSI calculation
        prices = np.array(self.price_history[-15:])
        deltas = np.diff(prices)
        gains = np.where(deltas > 0, deltas, 0)
        losses = np.where(deltas < 0, -deltas, 0)

        avg_gain = np.mean(gains[-14:])
        avg_loss = np.mean(losses[-14:])

        if avg_loss != 0:
            rs = avg_gain / avg_loss
            self.rsi = 100 - (100 / (1 + rs))

        # MACD calculation (simplified)
        if len(self.price_history) >= 26:
            prices = np.array(self.price_history[-26:])
            ema_12 = self._calculate_ema(prices, 12)
            ema_26 = self._calculate_ema(prices, 26)
            self.macd = ema_12 - ema_26

    def _calculate_ema(self, prices: np.array, period: int) -> float:
        """Calculate exponential moving average"""
        alpha = 2 / (period + 1)
        ema = prices[0]
        for price in prices[1:]:
            ema = alpha * price + (1 - alpha) * ema
        return ema

    def _update_sentiment(self):
        """Update market sentiment based on events and price action"""
        # Base sentiment decay
        self.sentiment_score *= 0.99

        # Event impact on sentiment
        for event in self.active_events:
            if "sentiment" in event.affected_metrics:
                minutes_since = (datetime.now() - event.timestamp).total_seconds() / 60
                if minutes_since < event.duration_minutes:
                    decay_factor = 1.0 - (minutes_since / event.duration_minutes)
                    sentiment_impact = event.impact_magnitude * 0.1 * decay_factor
                    self.sentiment_score = max(0, min(1, self.sentiment_score + sentiment_impact))

        # Price momentum impact on sentiment
        if len(self.price_history) >= 5:
            recent_return = (self.current_price / self.price_history[-5]) - 1
            self.sentiment_score += recent_return * 0.1
            self.sentiment_score = max(0, min(1, self.sentiment_score))

    def _update_market_session(self):
        """Update market session based on time"""
        hour = datetime.now().hour

        if 9 <= hour < 16:
            self.market_session = "open"
        elif 4 <= hour < 9:
            self.market_session = "pre_market"
        elif 16 <= hour < 20:
            self.market_session = "after_hours"
        else:
            self.market_session = "closed"

    def _cleanup_expired_events(self):
        """Remove expired events"""
        current_time = datetime.now()
        self.active_events = [
            event for event in self.active_events
            if (current_time - event.timestamp).total_seconds() / 60 < event.duration_minutes
        ]

    def get_current_market_data(self) -> Dict:
        """Get current market snapshot"""
        if len(self.price_history) >= 2:
            price_change = self.current_price - self.price_history[-2]
            price_change_pct = (price_change / self.price_history[-2]) * 100
        else:
            price_change = 0
            price_change_pct = 0

        current_volume = self.volume_history[-1] if self.volume_history else 1000000
        avg_volume = np.mean(self.volume_history[-20:]) if len(self.volume_history) >= 20 else current_volume

        return {
            "ticker": self.ticker,
            "current_price": round(self.current_price, 2),
            "price_change": round(price_change, 2),
            "price_change_pct": round(price_change_pct, 2),
            "volume": current_volume,
            "avg_volume": int(avg_volume),
            "rsi": round(self.rsi, 1),
            "macd": round(self.macd, 3),
            "sentiment_score": round(self.sentiment_score * 100, 1),
            "market_session": self.market_session,
            "active_events": len(self.active_events),
            "timestamp": datetime.now()
        }

    def get_recent_news(self, count: int = 3) -> List[Dict]:
        """Get recent news events"""
        recent_events = sorted(self.news_events[-count:], key=lambda x: x.timestamp, reverse=True)

        return [
            {
                "headline": event.description,
                "impact": "Positive" if event.impact_magnitude > 0 else "Negative",
                "magnitude": abs(event.impact_magnitude),
                "timestamp": event.timestamp,
                "minutes_ago": int((datetime.now() - event.timestamp).total_seconds() / 60)
            }
            for event in recent_events
        ]


class GeminiThinkingAgent:
    """Agent that actually thinks using Gemini API"""

    def __init__(self, name: str, role: str, expertise: str, personality: str):
        self.name = name
        self.role = role
        self.expertise = expertise
        self.personality = personality
        # self.model = genai.GenerativeModel("gemini-1.5-flash")
        self.memory = []
        self.confidence_history = []

    def think_and_analyze(self, market_data: Dict, news: List[Dict], context: str = "") -> Dict:
        """Deep thinking and analysis using Gemini"""

        thinking_prompt = f"""
        You are {self.name}, a {self.role} with expertise in {self.expertise}.
        Your personality: {self.personality}

        CURRENT MARKET DATA:
        - {market_data['ticker']}: ${market_data['current_price']} ({market_data['price_change_pct']:+.2f}%)
        - Volume: {market_data['volume']:,} vs avg {market_data['avg_volume']:,}
        - RSI: {market_data['rsi']} | MACD: {market_data['macd']}
        - Sentiment: {market_data['sentiment_score']}/100
        - Session: {market_data['market_session']} | Active events: {market_data['active_events']}

        RECENT NEWS:
        {chr(10).join([f"- {n['headline']} ({n['minutes_ago']}min ago, {n['impact']})" for n in news])}

        ADDITIONAL CONTEXT:
        {context}

        Think through this step-by-step:

        1. IMMEDIATE REACTION: What's your first impression of this data?

        2. TECHNICAL ANALYSIS: What do the price action and indicators tell you?

        3. NEWS IMPACT: How do recent events affect your outlook?

        4. RISK ASSESSMENT: What are the key risks you see?

        5. OPPORTUNITY ANALYSIS: Where do you see potential upside?

        6. CONFIDENCE LEVEL: How confident are you in your analysis (1-10)?

        7. SPECIFIC RECOMMENDATION: What specific action would you recommend?

        Respond in your characteristic style and personality. Be specific with numbers and reasoning.
        """

        # Call Gemini API (simulated response for demo)
        analysis = self._call_gemini_api(thinking_prompt)

        # Extract confidence and recommendation
        confidence = self._extract_confidence(analysis)
        recommendation = self._extract_recommendation(analysis)

        # Store in memory
        self.memory.append({
            "timestamp": datetime.now(),
            "market_price": market_data['current_price'],
            "analysis": analysis,
            "confidence": confidence,
            "recommendation": recommendation
        })

        # Keep only recent memory
        if len(self.memory) > 10:
            self.memory = self.memory[-10:]

        return {
            "agent": self.name,
            "analysis": analysis,
            "confidence": confidence,
            "recommendation": recommendation,
            "key_insight": self._extract_key_insight(analysis),
            "timestamp": datetime.now()
        }

    def _call_gemini_api(self, prompt: str) -> str:
        """Simulate real Gemini API call with realistic responses"""
        # In real implementation:
        # response = self.model.generate_content(prompt)
        # return response.text

        # Generate realistic agent-specific responses
        responses = {
            "Market Analyst": [
                f"Looking at the technical setup, I'm seeing some interesting price action here. The RSI at this level suggests we're in neutral territory, but the volume pattern is telling a different story. The MACD crossover could signal a potential momentum shift. Based on my technical analysis, I'm leaning towards a cautious approach here with a confidence of 7/10. I'd recommend waiting for a clear break above resistance before committing capital.",

                f"This price movement is concerning from a technical perspective. We're seeing declining volume on the recent move, which typically suggests weak conviction. The RSI divergence I'm observing could indicate a potential reversal. My confidence is only 6/10 given the mixed signals. I'd recommend reducing position size until we see clearer direction.",

                f"Strong bullish setup developing! The volume confirmation on this breakout is exactly what I want to see. RSI has room to run, and the MACD momentum is accelerating. This reminds me of similar patterns that led to 15-20% moves. High confidence at 9/10 - I'd recommend adding to positions on any pullback to support."
            ],

            "Fundamentals Analyst": [
                f"The recent earnings data changes my valuation model significantly. With revenue growth accelerating and margins expanding, I'm revising my fair value upward. The P/E multiple looks reasonable given the growth trajectory. However, I'm concerned about the competitive landscape. Confidence at 7/10 - I'd recommend a moderate buy with careful monitoring of execution metrics.",

                f"I'm troubled by what I'm seeing in the fundamentals. The recent guidance cut suggests management is seeing headwinds they didn't anticipate. Debt levels are creeping up while cash flow growth is slowing. This doesn't justify the current valuation. Low confidence at 4/10 - I'd recommend reducing exposure until we see improvement in operational metrics.",

                f"Exceptional fundamental strength here. The return on invested capital is expanding, free cash flow generation is accelerating, and the balance sheet is fortress-like. This is exactly the kind of quality growth I want to own. Very high confidence at 9/10 - I'd recommend increasing allocation significantly."
            ],

            "News Analyst": [
                f"The recent news flow creates a mixed picture. While the partnership announcement is positive, I'm seeing some concerning regulatory commentary that could impact future growth. Social sentiment has shifted notably in the past 24 hours. Moderate confidence at 6/10 - I'd recommend staying neutral until we get more clarity on regulatory outcomes.",

                f"This news environment is extremely bullish. Multiple positive catalysts are aligning, and I'm seeing broad institutional support in the commentary. The analyst upgrade cycle appears to be just beginning. High confidence at 8/10 - I'd recommend capitalizing on this positive momentum while it lasts.",

                f"Significant headwinds emerging from the news flow. The investigation announcement is more serious than markets are pricing in, and I'm seeing negative sentiment building across multiple channels. This could pressure the stock for weeks. Low confidence in current levels at 3/10 - I'd recommend defensive positioning."
            ]
        }

        agent_responses = responses.get(self.name, [
            f"Based on my analysis of {self.expertise}, I see mixed signals requiring careful consideration. Confidence: 6/10"
        ])

        return random.choice(agent_responses)

    def _extract_confidence(self, analysis: str) -> int:
        """Extract confidence level from analysis"""
        if "9/10" in analysis or "very high confidence" in analysis.lower():
            return 9
        elif "8/10" in analysis or "high confidence" in analysis.lower():
            return 8
        elif "7/10" in analysis:
            return 7
        elif "6/10" in analysis or "moderate" in analysis.lower():
            return 6
        elif "5/10" in analysis:
            return 5
        elif "4/10" in analysis or "low confidence" in analysis.lower():
            return 4
        elif "3/10" in analysis:
            return 3
        else:
            return 5

    def _extract_recommendation(self, analysis: str) -> str:
        """Extract recommendation from analysis"""
        if "recommend adding" in analysis.lower() or "recommend increasing" in analysis.lower():
            return "STRONG BUY"
        elif "recommend" in analysis.lower() and "buy" in analysis.lower():
            return "BUY"
        elif "recommend reducing" in analysis.lower() or "defensive" in analysis.lower():
            return "SELL"
        elif "stay neutral" in analysis.lower() or "cautious" in analysis.lower():
            return "HOLD"
        else:
            return "HOLD"

    def _extract_key_insight(self, analysis: str) -> str:
        """Extract key insight from analysis"""
        sentences = analysis.split('.')
        # Return the most substantive sentence
        for sentence in sentences:
            if len(sentence.strip()) > 50 and (
                    "seeing" in sentence or "suggests" in sentence or "indicates" in sentence):
                return sentence.strip()
        return sentences[0].strip() if sentences else "Mixed signals in current market conditions"


class LiveAgentOrchestrator:
    """Orchestrates dynamic agent collaboration"""

    def __init__(self, ticker: str):
        self.ticker = ticker
        self.market_sim = DynamicMarketSimulator(ticker)

        # Initialize agents with distinct personalities
        self.agents = {
            "Market Analyst": GeminiThinkingAgent(
                "Market Analyst",
                "Technical Analysis Expert",
                "Chart patterns, momentum indicators, volume analysis",
                "Data-driven, precise, focused on price action and technical levels"
            ),
            "Fundamentals Analyst": GeminiThinkingAgent(
                "Fundamentals Analyst",
                "Financial Analysis Expert",
                "Valuation models, earnings analysis, financial health",
                "Conservative, thorough, focused on long-term value creation"
            ),
            "News Analyst": GeminiThinkingAgent(
                "News Analyst",
                "Market Intelligence Expert",
                "News impact, sentiment analysis, market psychology",
                "Quick-thinking, intuitive, focused on market sentiment and catalysts"
            )
        }

        self.analysis_history = []
        self.is_running = False

    def start_live_analysis(self, update_interval: int = 30):
        """Start continuous live analysis"""
        self.is_running = True

        def analysis_loop():
            cycle_count = 0
            while self.is_running:
                try:
                    cycle_count += 1

                    # Update market simulation
                    self.market_sim.update_market_state()

                    # Get current data
                    market_data = self.market_sim.get_current_market_data()
                    recent_news = self.market_sim.get_recent_news(3)

                    # Run agent analysis
                    agent_results = self._run_agent_analysis_cycle(market_data, recent_news)

                    # Synthesize results
                    synthesis = self._synthesize_agent_views(agent_results, market_data)

                    # Store results
                    cycle_result = {
                        "cycle": cycle_count,
                        "timestamp": datetime.now(),
                        "market_data": market_data,
                        "news": recent_news,
                        "agent_results": agent_results,
                        "synthesis": synthesis
                    }

                    self.analysis_history.append(cycle_result)

                    # Display results
                    self._display_live_update(cycle_result)

                    # Keep only recent history
                    if len(self.analysis_history) > 50:
                        self.analysis_history = self.analysis_history[-50:]

                    time.sleep(update_interval)

                except Exception as e:
                    logger.error(f"Error in analysis cycle: {e}")
                    time.sleep(10)

        analysis_thread = threading.Thread(target=analysis_loop)
        analysis_thread.daemon = True
        analysis_thread.start()
        return analysis_thread

    def _run_agent_analysis_cycle(self, market_data: Dict, news: List[Dict]) -> Dict:
        """Run analysis for all agents"""
        results = {}

        # Each agent analyzes independently first
        for agent_name, agent in self.agents.items():
            results[agent_name] = agent.think_and_analyze(market_data, news)

        # Then agents can see each other's views for collaboration
        context = f"""
        Other analyst views:
        - Market Analyst confidence: {results.get('Market Analyst', {}).get('confidence', 5)}/10
        - Fundamentals Analyst confidence: {results.get('Fundamentals Analyst', {}).get('confidence', 5)}/10  
        - News Analyst confidence: {results.get('News Analyst', {}).get('confidence', 5)}/10
        """

        # Second round with collaboration
        for agent_name, agent in self.agents.items():
            collaborative_result = agent.think_and_analyze(market_data, news, context)
            results[f"{agent_name}_collaborative"] = collaborative_result

        return results

    def _synthesize_agent_views(self, agent_results: Dict, market_data: Dict) -> Dict:
        """Synthesize agent recommendations into final decision"""

        # Collect recommendations and confidence levels
        recommendations = []
        confidences = []

        for agent_name in ["Market Analyst", "Fundamentals Analyst", "News Analyst"]:
            if agent_name in agent_results:
                rec = agent_results[agent_name]["recommendation"]
                conf = agent_results[agent_name]["confidence"]
                recommendations.append(rec)
                confidences.append(conf)

        # Weight recommendations by confidence
        weighted_score = 0
        total_weight = 0

        score_map = {"STRONG BUY": 2, "BUY": 1, "HOLD": 0, "SELL": -1, "STRONG SELL": -2}

        for rec, conf in zip(recommendations, confidences):
            score = score_map.get(rec, 0)
            weighted_score += score * conf
            total_weight += conf

        if total_weight > 0:
            avg_score = weighted_score / total_weight
        else:
            avg_score = 0

        # Determine final recommendation
        if avg_score >= 1.5:
            final_rec = "STRONG BUY"
        elif avg_score >= 0.5:
            final_rec = "BUY"
        elif avg_score <= -1.5:
            final_rec = "STRONG SELL"
        elif avg_score <= -0.5:
            final_rec = "SELL"
        else:
            final_rec = "HOLD"

        # Calculate overall confidence
        avg_confidence = np.mean(confidences) if confidences else 5

        return {
            "recommendation": final_rec,
            "confidence": round(avg_confidence, 1),
            "agent_consensus": len(set(recommendations)) == 1,
            "price_target": market_data["current_price"] * (1 + avg_score * 0.05),
            "reasoning": f"Based on {len(recommendations)} agent analysis with avg confidence {avg_confidence:.1f}/10"
        }

    def _display_live_update(self, cycle_result: Dict):
        """Display live analysis update"""
        timestamp = cycle_result["timestamp"].strftime("%H:%M:%S")
        market_data = cycle_result["market_data"]
        synthesis = cycle_result["synthesis"]
        news = cycle_result["news"]

        print(f"\n{'=' * 100}")
        print(f"🔴 LIVE MARKET ANALYSIS - {timestamp} (Cycle #{cycle_result['cycle']})")
        print(f"{'=' * 100}")

        # Market snapshot
        print(
            f"📊 {market_data['ticker']}: ${market_data['current_price']} ({market_data['price_change_pct']:+.2f}%) | Vol: {market_data['volume']:,}")
        print(
            f"📈 RSI: {market_data['rsi']} | MACD: {market_data['macd']} | Sentiment: {market_data['sentiment_score']}/100")
        print(f"🕐 Session: {market_data['market_session']} | Active Events: {market_data['active_events']}")

        # Final recommendation
        confidence_emoji = "🔥" if synthesis['confidence'] >= 8 else "✅" if synthesis['confidence'] >= 6 else "⚠️"
        print(f"\n🎯 FINAL RECOMMENDATION: {synthesis['recommendation']} {confidence_emoji}")
        print(f"📊 Confidence: {synthesis['confidence']}/10 | Target: ${synthesis['price_target']:.2f}")
        print(f"🤝 Agent Consensus: {'YES' if synthesis['agent_consensus'] else 'NO'}")

        # Recent news
        if news:
            print(f"\n📰 RECENT NEWS:")
            for n in news[:2]:
                impact_emoji = "📈" if n['impact'] == 'Positive' else "📉"
                print(f"   {impact_emoji} {n['headline'][:80]}... ({n['minutes_ago']}min ago)")

        # Agent insights
        print(f"\n🤖 AGENT INSIGHTS:")
        agent_results = cycle_result["agent_results"]
        for agent_name in ["Market Analyst", "Fundamentals Analyst", "News Analyst"]:
            if agent_name in agent_results:
                result = agent_results[agent_name]
                conf_emoji = "🔥" if result['confidence'] >= 8 else "✅" if result['confidence'] >= 6 else "⚠️"
                print(f"   {agent_name}: {result['recommendation']} ({result['confidence']}/10) {conf_emoji}")
                print(f"      💡 {result['key_insight'][:100]}...")

        # Market alerts
        if market_data['price_change_pct'] > 2:
            print(f"\n🚨 PRICE ALERT: Strong upward movement (+{market_data['price_change_pct']:.1f}%)")
        elif market_data['price_change_pct'] < -2:
            print(f"\n🚨 PRICE ALERT: Significant decline ({market_data['price_change_pct']:.1f}%)")

        if market_data['volume'] > market_data['avg_volume'] * 1.5:
            print(f"🚨 VOLUME ALERT: High volume ({market_data['volume']:,} vs avg {market_data['avg_volume']:,})")

    def stop_analysis(self):
        """Stop the live analysis"""
        self.is_running = False
        logger.info("Live analysis stopped")

    def get_analysis_summary(self, hours: int = 1) -> Dict:
        """Get analysis summary for recent period"""
        cutoff_time = datetime.now() - timedelta(hours=hours)
        recent_analyses = [
            a for a in self.analysis_history
            if a['timestamp'] > cutoff_time
        ]

        if not recent_analyses:
            return {"message": "No recent analyses available"}

        recommendations = [a['synthesis']['recommendation'] for a in recent_analyses]
        confidences = [a['synthesis']['confidence'] for a in recent_analyses]
        prices = [a['market_data']['current_price'] for a in recent_analyses]

        return {
            'period_hours': hours,
            'total_cycles': len(recent_analyses),
            'price_range': f"${min(prices):.2f} - ${max(prices):.2f}",
            'price_change': f"{((prices[-1] / prices[0]) - 1) * 100:+.2f}%" if len(prices) > 1 else "0%",
            'recommendations': {
                'STRONG BUY': recommendations.count('STRONG BUY'),
                'BUY': recommendations.count('BUY'),
                'HOLD': recommendations.count('HOLD'),
                'SELL': recommendations.count('SELL'),
                'STRONG SELL': recommendations.count('STRONG SELL')
            },
            'avg_confidence': round(np.mean(confidences), 1),
            'latest_recommendation': recent_analyses[-1]['synthesis']['recommendation'],
            'consensus_rate': round(np.mean([a['synthesis']['agent_consensus'] for a in recent_analyses]) * 100, 1)
        }

    def force_market_event(self, event_type: str = None):
        """Force a specific market event for testing"""
        if event_type:
            # Create custom event
            event = MarketEvent(
                event_type=event_type,
                description=f"Manual trigger: {event_type} event for {self.ticker}",
                impact_magnitude=random.uniform(-0.5, 0.5),
                duration_minutes=random.randint(60, 180),
                affected_metrics=["price", "volume", "sentiment"],
                timestamp=datetime.now()
            )
        else:
            # Generate random event
            event = self.market_sim.generate_market_event()

        self.market_sim.active_events.append(event)
        self.market_sim.news_events.append(event)
        print(f"🚨 FORCED EVENT: {event.description}")


def main():
    """Main execution for live market simulation"""

    print("🚀 DYNAMIC MARKET SIMULATION WITH AI AGENTS")
    print("=" * 60)

    if GEMINI_API_KEY == "YOUR_GEMINI_API_KEY_HERE":
        print("⚠️  Using simulated AI responses (set GEMINI_API_KEY for real AI)")
        print("   But market simulation is fully dynamic!")
        print("-" * 60)

    # Get user preferences
    ticker = input("Enter stock ticker (default: AAPL): ").upper() or "AAPL"

    try:
        interval = int(input("Update interval in seconds (default: 30): ") or "30")
    except:
        interval = 30

    print(f"\n🎬 Starting live simulation for {ticker}")
    print(f"📊 Updates every {interval} seconds with dynamic events")
    print(f"🤖 3 AI agents thinking and collaborating continuously")
    print("\nCommands:")
    print("  'summary' - Analysis summary")
    print("  'event' - Force random market event")
    print("  'news' - Force news event")
    print("  'earnings' - Force earnings event")
    print("  'stop' - Exit simulation")
    print("\n" + "=" * 100)

    # Initialize orchestrator
    orchestrator = LiveAgentOrchestrator(ticker)

    try:
        # Start live analysis
        analysis_thread = orchestrator.start_live_analysis(interval)

        # Interactive command loop
        while True:
            try:
                command = input().lower().strip()

                if command == 'stop':
                    break
                elif command == 'summary':
                    summary = orchestrator.get_analysis_summary(1)
                    print(f"\n📊 ANALYSIS SUMMARY (Last Hour):")
                    print(f"   Cycles: {summary.get('total_cycles', 0)}")
                    print(f"   Price Range: {summary.get('price_range', 'N/A')}")
                    print(f"   Price Change: {summary.get('price_change', 'N/A')}")
                    print(f"   Recommendations: {summary.get('recommendations', {})}")
                    print(f"   Avg Confidence: {summary.get('avg_confidence', 0)}/10")
                    print(f"   Consensus Rate: {summary.get('consensus_rate', 0)}%")
                elif command == 'event':
                    orchestrator.force_market_event()
                elif command == 'news':
                    orchestrator.force_market_event('partnership_news')
                elif command == 'earnings':
                    event_type = 'earnings_beat' if random.random() > 0.3 else 'earnings_miss'
                    orchestrator.force_market_event(event_type)
                elif command == '':
                    continue
                else:
                    print("Unknown command. Try: summary, event, news, earnings, stop")

            except EOFError:
                break

    except KeyboardInterrupt:
        print("\n\n⏹️  Simulation interrupted")

    finally:
        orchestrator.stop_analysis()
        print("🏁 Simulation ended. Thank you!")


if __name__ == "__main__":
    main()