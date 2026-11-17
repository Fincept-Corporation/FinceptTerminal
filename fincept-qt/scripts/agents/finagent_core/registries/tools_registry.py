"""
Tools Registry - Unified access to 100+ Agno tools

Provides lazy loading and categorized tool access.
"""

from typing import Dict, List, Any, Optional, Tuple, Type
import logging
import os

logger = logging.getLogger(__name__)


class ToolsRegistry:
    """
    Central registry for all Agno tools.
    Lazy loads tools on demand to minimize startup time.
    """

    # Tool categories and their available tools
    TOOL_CATALOG = {
        # SEC EDGAR (internal Fincept MCP — no API key required)
        "sec_edgar": {
            "edgar": "finagent_core.tools.edgar_tools.EdgarTools",
        },

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
            "crawl4ai": "agno.tools.crawl4ai.Crawl4aiTools",
            "spider": "agno.tools.spider.SpiderTools",
            "newspaper": "agno.tools.newspaper.NewspaperTools",
            "newspaper4k": "agno.tools.newspaper4k.Newspaper4kTools",
            "trafilatura": "agno.tools.trafilatura.TrafilaturaTools",
            "jina": "agno.tools.jina.JinaReaderTools",
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
            "csv_toolkit": "agno.tools.csv_toolkit.CsvTools",
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
            "google_maps": "agno.tools.google_maps.GoogleMapTools",
        },

        # AWS Services
        "aws": {
            "aws_lambda": "agno.tools.aws_lambda.AWSLambdaTools",
            "aws_ses": "agno.tools.aws_ses.AWSSESTool",
        },

        # AI & ML
        "ai": {
            "openai": "agno.tools.openai.OpenAITools",
            "dalle": "agno.tools.dalle.DalleTools",
            "replicate": "agno.tools.replicate.ReplicateTools",
            "fal": "agno.tools.fal.FalTools",
            "models_labs": "agno.tools.models_labs.ModelsLabTools",
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
            "mcp_toolbox": "agno.tools.mcp_toolbox.MCPToolbox",
        },

        # Blockchain
        "blockchain": {
            "evm": "agno.tools.evm.EvmTools",
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

    # A requested tool that cannot work on this install — its package is missing,
    # or the API key it needs is set neither in api_keys nor in the environment —
    # is swapped for a free equivalent that can, and the run says so:
    # {tool: (substitute, note for the run's warnings)}. Most shipped agent cards
    # ask for Tavily, a paid search the terminal does not ship.
    TOOL_FALLBACKS: Dict[str, Tuple[str, str]] = {
        "tavily": ("duckduckgo", "web search: using DuckDuckGo because Tavily is not configured"),
    }

    @classmethod
    def _unusable_reason(cls, tool_name: str, api_keys: Optional[Dict[str, str]]) -> Optional[str]:
        """Why `tool_name` cannot do its job here: its package is missing, or the
        API key it needs is not set. None when it can."""
        reason = cls.unavailable_reason(tool_name)
        if reason:
            return reason
        key_name = cls.TOOL_API_KEY_MAP.get(tool_name)
        if key_name and not ((api_keys or {}).get(key_name) or os.environ.get(key_name)):
            return f"{key_name} is not set"
        return None

    @classmethod
    def unavailable_reason(cls, tool_name: str) -> Optional[str]:
        """None when the tool can be loaded, else why not (e.g. the package it
        needs is not installed). Unknown names get a reason too."""
        cls._build_tool_map()
        if tool_name not in cls._tool_map:
            return "not a known tool"
        from finagent_core.registries.availability import class_unavailable_reason
        return class_unavailable_reason(cls._tool_map[tool_name])

    @classmethod
    def unavailable_tools(cls) -> Dict[str, str]:
        """{tool_name: reason} for every catalogue entry that cannot load here
        (fast source-level check, see availability.class_static_reason)."""
        from finagent_core.registries.availability import class_static_reason
        cls._build_tool_map()
        out: Dict[str, str] = {}
        for name, path in cls._tool_map.items():
            reason = class_static_reason(path)
            if reason:
                out[name] = reason
        return out

    @classmethod
    def get_tool(cls, tool_name: str, api_keys: Optional[Dict[str, str]] = None, **kwargs) -> Optional[Any]:
        """
        Get a tool instance by name with proper API key injection.

        Args:
            tool_name: Name of the tool (e.g., 'yfinance', 'duckduckgo')
            api_keys: Dict of API keys to inject
            **kwargs: Additional arguments to pass to tool constructor

        Returns:
            Tool instance or None if not available (resolve_tools() also says why)
        """
        tool, _reason = cls._load_tool(tool_name, api_keys, **kwargs)
        return tool

    @classmethod
    def _load_tool(cls, tool_name: str, api_keys: Optional[Dict[str, str]] = None,
                   **kwargs) -> "tuple[Optional[Any], Optional[str]]":
        """(tool instance, None) or (None, reason it could not be loaded)."""
        cls._build_tool_map()

        if tool_name not in cls._tool_map:
            logger.warning(f"Tool '{tool_name}' not found in registry")
            return None, "not a known tool"

        reason = cls.unavailable_reason(tool_name)
        if reason:
            logger.warning(f"Tool '{tool_name}' is unavailable: {reason}")
            return None, reason

        # Build tool kwargs with API keys
        tool_kwargs = dict(kwargs)
        if api_keys and tool_name in cls.TOOL_API_KEY_MAP:
            api_key_name = cls.TOOL_API_KEY_MAP[tool_name]
            if api_key_name in api_keys:
                tool_kwargs["api_key"] = api_keys[api_key_name]

        # Check cache (without API keys in cache key for security)
        cache_key = tool_name
        if cache_key in cls._loaded_tools and not tool_kwargs:
            return cls._loaded_tools[cache_key], None

        tool_class = None
        try:
            from finagent_core.registries.availability import load_class
            tool_class = load_class(cls._tool_map[tool_name])

            # Instantiate tool with kwargs
            tool_instance = tool_class(**tool_kwargs) if tool_kwargs else tool_class()

            # Only cache tools without API keys
            if not tool_kwargs:
                cls._loaded_tools[cache_key] = tool_instance

            logger.debug(f"Loaded tool: {tool_name}")
            return tool_instance, None

        except ImportError as e:
            logger.warning(f"Tool '{tool_name}' is unavailable: {e}")
            return None, str(e)
        except TypeError as e:
            # Try without kwargs if constructor doesn't accept them
            try:
                tool_instance = tool_class()
                cls._loaded_tools[cache_key] = tool_instance
                logger.debug(f"Loaded tool (no args): {tool_name}")
                return tool_instance, None
            except Exception as e2:
                logger.warning(f"Tool '{tool_name}' initialization failed: {e}")
                return None, f"initialization failed: {e2}"
        except Exception as e:
            logger.error(f"Tool '{tool_name}' initialization failed: {e}")
            return None, f"initialization failed: {e}"

    @classmethod
    def resolve_tools(cls, tool_names: List[str], api_keys: Optional[Dict[str, str]] = None,
                      notes: Optional[List[str]] = None,
                      **kwargs) -> "tuple[List[Any], Dict[str, str]]":
        """Load the named tools. Returns (tool instances, {name: reason}) where
        the second map lists every requested tool that could not be loaded, so
        the run can tell the user instead of quietly answering without it.

        A tool in TOOL_FALLBACKS that cannot work here is replaced by its
        substitute when that one loads (once, also when it is requested too);
        the substitution's note is appended to `notes` when a list is given,
        and the replaced tool is not reported as missing. When the substitute
        cannot load either, the requested tool is reported as before."""
        names = [n for n in tool_names if isinstance(n, str)]

        replaced: Dict[str, Tuple[str, str, str]] = {}  # tool -> (substitute, note, why)
        for name in names:
            fallback = cls.TOOL_FALLBACKS.get(name)
            if not fallback or name in replaced:
                continue
            why = cls._unusable_reason(name, api_keys)
            if why and cls.unavailable_reason(fallback[0]) is None:
                replaced[name] = (fallback[0], fallback[1], why)

        effective: List[str] = []
        for name in names:
            if name in replaced:
                substitute = replaced[name][0]
                if substitute in names or substitute in effective:
                    continue  # requested anyway / already added: never load it twice
                effective.append(substitute)
            else:
                effective.append(name)

        tools: List[Any] = []
        problems: Dict[str, str] = {}
        for name in effective:
            tool, reason = cls._load_tool(name, api_keys=api_keys, **kwargs)
            if tool is not None:
                tools.append(tool)
            else:
                problems[name] = reason or "could not be loaded"

        for name, (substitute, note, why) in replaced.items():
            if substitute in problems:
                if substitute not in names:
                    problems.pop(substitute)  # not asked for; the requested tool is what is missing
                problems[name] = why
            elif notes is not None and note not in notes:
                notes.append(note)
        return tools, problems

    @classmethod
    def get_tools(cls, tool_names: List[str], api_keys: Optional[Dict[str, str]] = None, **kwargs) -> List[Any]:
        """
        Get multiple tool instances with API key injection.

        Args:
            tool_names: List of tool names
            api_keys: Dict of API keys for tools that need them
            **kwargs: Additional arguments to pass to tool constructors

        Returns:
            List of tool instances (excludes None values; resolve_tools() also
            returns why each missing tool could not be loaded)
        """
        tools, problems = cls.resolve_tools(tool_names, api_keys=api_keys, **kwargs)
        if problems:
            logger.warning(f"Loaded {len(tools)}/{len(tool_names)} tools; unavailable: {problems}")
        return tools

    @classmethod
    def list_tools(cls, category: Optional[str] = None) -> Dict[str, List[str]]:
        """
        List the catalogue (every known tool, loadable or not — see
        unavailable_tools() for the ones that cannot load here).

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
