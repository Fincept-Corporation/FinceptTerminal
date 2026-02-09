"""
Tools Registry - Unified access to 100+ Agno tools

Provides lazy loading and categorized tool access.
"""

from typing import Dict, List, Any, Optional, Type
import logging

logger = logging.getLogger(__name__)


class ToolsRegistry:
    """
    Central registry for all Agno tools.
    Lazy loads tools on demand to minimize startup time.
    """

    # Tool categories and their available tools
    TOOL_CATALOG = {
        # Finance & Trading
        "finance": {
            "yfinance": "agno.tools.yfinance.YFinanceTools",
            "financial_datasets": "agno.tools.financial_datasets.FinancialDatasetsTools",
            "openbb": "agno.tools.openbb.OpenBBTools",
        },

        # Search & Web
        "search": {
            "duckduckgo": "agno.tools.duckduckgo.DuckDuckGoTools",
            "tavily": "agno.tools.tavily.TavilyTools",
            "exa": "agno.tools.exa.ExaTools",
            "serpapi": "agno.tools.serpapi.SerpApiTools",
            "serper": "agno.tools.serper.SerperTools",
            "bravesearch": "agno.tools.bravesearch.BraveSearchTools",
            "baidusearch": "agno.tools.baidusearch.BaiduSearchTools",
            "searxng": "agno.tools.searxng.SearxngTools",
        },

        # Web & Content
        "web": {
            "website": "agno.tools.website.WebsiteTools",
            "webbrowser": "agno.tools.webbrowser.WebBrowserTools",
            "webtools": "agno.tools.webtools.WebTools",
            "firecrawl": "agno.tools.firecrawl.FirecrawlTools",
            "crawl4ai": "agno.tools.crawl4ai.Crawl4AITools",
            "spider": "agno.tools.spider.SpiderTools",
            "newspaper": "agno.tools.newspaper.NewspaperTools",
            "newspaper4k": "agno.tools.newspaper4k.Newspaper4kTools",
            "trafilatura": "agno.tools.trafilatura.TrafilaturaTools",
            "jina": "agno.tools.jina.JinaTools",
        },

        # Development & Code
        "development": {
            "python": "agno.tools.python.PythonTools",
            "shell": "agno.tools.shell.ShellTools",
            "file": "agno.tools.file.FileTools",
            "local_file_system": "agno.tools.local_file_system.LocalFileSystemTools",
            "docker": "agno.tools.docker.DockerTools",
            "e2b": "agno.tools.e2b.E2BTools",
            "daytona": "agno.tools.daytona.DaytonaTools",
        },

        # Version Control & Project Management
        "devops": {
            "github": "agno.tools.github.GithubTools",
            "bitbucket": "agno.tools.bitbucket.BitbucketTools",
            "gitlab": "agno.tools.gitlab.GitlabTools",
            "jira": "agno.tools.jira.JiraTools",
            "linear": "agno.tools.linear.LinearTools",
            "clickup": "agno.tools.clickup.ClickUpTools",
            "trello": "agno.tools.trello.TrelloTools",
            "notion": "agno.tools.notion.NotionTools",
            "confluence": "agno.tools.confluence.ConfluenceTools",
            "todoist": "agno.tools.todoist.TodoistTools",
        },

        # Database & Data
        "database": {
            "sql": "agno.tools.sql.SQLTools",
            "postgres": "agno.tools.postgres.PostgresTools",
            "duckdb": "agno.tools.duckdb.DuckDbTools",
            "neo4j": "agno.tools.neo4j.Neo4jTools",
            "redshift": "agno.tools.redshift.RedshiftTools",
            "google_bigquery": "agno.tools.google_bigquery.GoogleBigQueryTools",
        },

        # Data Processing
        "data": {
            "pandas": "agno.tools.pandas.PandasTools",
            "csv_toolkit": "agno.tools.csv_toolkit.CsvToolkit",
            "visualization": "agno.tools.visualization.VisualizationTools",
        },

        # Communication
        "communication": {
            "email": "agno.tools.email.EmailTools",
            "gmail": "agno.tools.gmail.GmailTools",
            "slack": "agno.tools.slack.SlackTools",
            "discord": "agno.tools.discord.DiscordTools",
            "telegram": "agno.tools.telegram.TelegramTools",
            "whatsapp": "agno.tools.whatsapp.WhatsAppTools",
            "twilio": "agno.tools.twilio.TwilioTools",
            "resend": "agno.tools.resend.ResendTools",
            "webex": "agno.tools.webex.WebexTools",
        },

        # Google Services
        "google": {
            "google_drive": "agno.tools.google_drive.GoogleDriveTools",
            "googlesheets": "agno.tools.googlesheets.GoogleSheetsTools",
            "googlecalendar": "agno.tools.googlecalendar.GoogleCalendarTools",
            "google_maps": "agno.tools.google_maps.GoogleMapsTools",
        },

        # AWS Services
        "aws": {
            "aws_lambda": "agno.tools.aws_lambda.AWSLambdaTools",
            "aws_ses": "agno.tools.aws_ses.AWSSESTools",
        },

        # AI & ML
        "ai": {
            "openai": "agno.tools.openai.OpenAITools",
            "dalle": "agno.tools.dalle.DalleTools",
            "replicate": "agno.tools.replicate.ReplicateTools",
            "fal": "agno.tools.fal.FalTools",
            "models_labs": "agno.tools.models_labs.ModelsLabsTools",
            "eleven_labs": "agno.tools.eleven_labs.ElevenLabsTools",
            "cartesia": "agno.tools.cartesia.CartesiaTools",
            "desi_vocal": "agno.tools.desi_vocal.DesiVocalTools",
        },

        # Media & Content
        "media": {
            "youtube": "agno.tools.youtube.YouTubeTools",
            "spotify": "agno.tools.spotify.SpotifyTools",
            "giphy": "agno.tools.giphy.GiphyTools",
            "moviepy_video": "agno.tools.moviepy_video.MoviePyVideoTools",
            "opencv": "agno.tools.opencv.OpenCVTools",
            "lumalab": "agno.tools.lumalab.LumaLabTools",
        },

        # Research & Knowledge
        "research": {
            "arxiv": "agno.tools.arxiv.ArxivTools",
            "pubmed": "agno.tools.pubmed.PubmedTools",
            "wikipedia": "agno.tools.wikipedia.WikipediaTools",
            "hackernews": "agno.tools.hackernews.HackerNewsTools",
            "reddit": "agno.tools.reddit.RedditTools",
        },

        # Utilities
        "utilities": {
            "calculator": "agno.tools.calculator.CalculatorTools",
            "sleep": "agno.tools.sleep.SleepTools",
            "openweather": "agno.tools.openweather.OpenWeatherTools",
            "calcom": "agno.tools.calcom.CalComTools",
            "zoom": "agno.tools.zoom.ZoomTools",
        },

        # E-commerce & Business
        "business": {
            "shopify": "agno.tools.shopify.ShopifyTools",
            "zendesk": "agno.tools.zendesk.ZendeskTools",
            "brandfetch": "agno.tools.brandfetch.BrandfetchTools",
        },

        # Social Media
        "social": {
            "x": "agno.tools.x.XTools",  # Twitter/X
        },

        # Scraping & Data Extraction
        "scraping": {
            "apify": "agno.tools.apify.ApifyTools",
            "brightdata": "agno.tools.brightdata.BrightDataTools",
            "oxylabs": "agno.tools.oxylabs.OxylabsTools",
            "scrapegraph": "agno.tools.scrapegraph.ScrapeGraphTools",
            "browserbase": "agno.tools.browserbase.BrowserbaseTools",
            "agentql": "agno.tools.agentql.AgentQLTools",
        },

        # Memory & Knowledge
        "memory": {
            "mem0": "agno.tools.mem0.Mem0Tools",
            "zep": "agno.tools.zep.ZepTools",
            "knowledge": "agno.tools.knowledge.KnowledgeTools",
            "memory": "agno.tools.memory.MemoryTools",
        },

        # Workflow & Orchestration
        "workflow": {
            "airflow": "agno.tools.airflow.AirflowTools",
            "mcp": "agno.tools.mcp.MCPTools",
            "mcp_toolbox": "agno.tools.mcp_toolbox.MCPToolboxTools",
        },

        # Blockchain
        "blockchain": {
            "evm": "agno.tools.evm.EVMTools",
        },

        # File Generation
        "file_generation": {
            "file_generation": "agno.tools.file_generation.FileGenerationTools",
        },
    }

    # Flattened tool map for quick lookup
    _tool_map: Dict[str, str] = {}
    _loaded_tools: Dict[str, Any] = {}

    @classmethod
    def _build_tool_map(cls) -> None:
        """Build flattened tool map from catalog"""
        if not cls._tool_map:
            for category, tools in cls.TOOL_CATALOG.items():
                for tool_name, tool_path in tools.items():
                    cls._tool_map[tool_name] = tool_path

    # Tools that require specific API keys
    TOOL_API_KEY_MAP = {
        "tavily": "TAVILY_API_KEY",
        "exa": "EXA_API_KEY",
        "serpapi": "SERPAPI_API_KEY",
        "serper": "SERPER_API_KEY",
        "bravesearch": "BRAVE_API_KEY",
        "firecrawl": "FIRECRAWL_API_KEY",
        "openweather": "OPENWEATHER_API_KEY",
        "financial_datasets": "FINANCIAL_DATASETS_API_KEY",
        "openbb": "OPENBB_API_KEY",
        "github": "GITHUB_TOKEN",
        "gitlab": "GITLAB_TOKEN",
        "slack": "SLACK_BOT_TOKEN",
        "discord": "DISCORD_BOT_TOKEN",
        "telegram": "TELEGRAM_BOT_TOKEN",
        "twilio": "TWILIO_API_KEY",
        "resend": "RESEND_API_KEY",
        "jira": "JIRA_API_TOKEN",
        "linear": "LINEAR_API_KEY",
        "notion": "NOTION_API_KEY",
        "confluence": "CONFLUENCE_API_KEY",
        "shopify": "SHOPIFY_API_KEY",
        "zendesk": "ZENDESK_API_KEY",
        "x": "X_API_KEY",
        "apify": "APIFY_API_TOKEN",
        "brightdata": "BRIGHTDATA_API_KEY",
        "mem0": "MEM0_API_KEY",
        "zep": "ZEP_API_KEY",
        "replicate": "REPLICATE_API_TOKEN",
        "fal": "FAL_API_KEY",
        "eleven_labs": "ELEVEN_LABS_API_KEY",
        "spotify": "SPOTIFY_API_KEY",
        "google_maps": "GOOGLE_MAPS_API_KEY",
    }

    @classmethod
    def get_tool(cls, tool_name: str, api_keys: Optional[Dict[str, str]] = None, **kwargs) -> Optional[Any]:
        """
        Get a tool instance by name with proper API key injection.

        Args:
            tool_name: Name of the tool (e.g., 'yfinance', 'duckduckgo')
            api_keys: Dict of API keys to inject
            **kwargs: Additional arguments to pass to tool constructor

        Returns:
            Tool instance or None if not available
        """
        cls._build_tool_map()

        if tool_name not in cls._tool_map:
            logger.warning(f"Tool '{tool_name}' not found in registry")
            return None

        # Build tool kwargs with API keys
        tool_kwargs = dict(kwargs)
        if api_keys and tool_name in cls.TOOL_API_KEY_MAP:
            api_key_name = cls.TOOL_API_KEY_MAP[tool_name]
            if api_key_name in api_keys:
                tool_kwargs["api_key"] = api_keys[api_key_name]

        # Check cache (without API keys in cache key for security)
        cache_key = tool_name
        if cache_key in cls._loaded_tools and not tool_kwargs:
            return cls._loaded_tools[cache_key]

        try:
            tool_path = cls._tool_map[tool_name]
            module_path, class_name = tool_path.rsplit(".", 1)

            # Dynamic import
            import importlib
            module = importlib.import_module(module_path)
            tool_class = getattr(module, class_name)

            # Instantiate tool with kwargs
            tool_instance = tool_class(**tool_kwargs) if tool_kwargs else tool_class()

            # Only cache tools without API keys
            if not tool_kwargs:
                cls._loaded_tools[cache_key] = tool_instance

            logger.debug(f"Loaded tool: {tool_name}")
            return tool_instance

        except ImportError as e:
            logger.debug(f"Tool '{tool_name}' import failed (may not be installed): {e}")
            return None
        except TypeError as e:
            # Try without kwargs if constructor doesn't accept them
            try:
                tool_instance = tool_class()
                cls._loaded_tools[cache_key] = tool_instance
                logger.debug(f"Loaded tool (no args): {tool_name}")
                return tool_instance
            except Exception:
                logger.warning(f"Tool '{tool_name}' initialization failed: {e}")
                return None
        except Exception as e:
            logger.error(f"Tool '{tool_name}' initialization failed: {e}")
            return None

    @classmethod
    def get_tools(cls, tool_names: List[str], api_keys: Optional[Dict[str, str]] = None, **kwargs) -> List[Any]:
        """
        Get multiple tool instances with API key injection.

        Args:
            tool_names: List of tool names
            api_keys: Dict of API keys for tools that need them
            **kwargs: Additional arguments to pass to tool constructors

        Returns:
            List of tool instances (excludes None values)
        """
        tools = []
        for name in tool_names:
            tool = cls.get_tool(name, api_keys=api_keys, **kwargs)
            if tool is not None:
                tools.append(tool)

        if len(tools) < len(tool_names):
            loaded = [t.__class__.__name__ for t in tools]
            logger.info(f"Loaded {len(tools)}/{len(tool_names)} tools: {loaded}")

        return tools

    @classmethod
    def list_tools(cls, category: Optional[str] = None) -> Dict[str, List[str]]:
        """
        List available tools.

        Args:
            category: Optional category filter

        Returns:
            Dict of category -> tool names
        """
        if category:
            return {category: list(cls.TOOL_CATALOG.get(category, {}).keys())}
        return {cat: list(tools.keys()) for cat, tools in cls.TOOL_CATALOG.items()}

    @classmethod
    def list_categories(cls) -> List[str]:
        """List all tool categories"""
        return list(cls.TOOL_CATALOG.keys())

    @classmethod
    def get_tool_info(cls, tool_name: str) -> Optional[Dict[str, str]]:
        """Get information about a tool"""
        cls._build_tool_map()

        if tool_name not in cls._tool_map:
            return None

        # Find category
        category = None
        for cat, tools in cls.TOOL_CATALOG.items():
            if tool_name in tools:
                category = cat
                break

        return {
            "name": tool_name,
            "path": cls._tool_map[tool_name],
            "category": category
        }

    @classmethod
    def search_tools(cls, query: str) -> List[str]:
        """Search for tools by name"""
        cls._build_tool_map()
        query_lower = query.lower()
        return [name for name in cls._tool_map.keys() if query_lower in name.lower()]
