# global_forum.py - Bloomberg Terminal Style Global Forum UI
# -*- coding: utf-8 -*-

import dearpygui.dearpygui as dpg
import sqlite3
import json
import time
import threading
from datetime import datetime, timedelta
import uuid
import random
from fincept_terminal.Utils.Managers.theme_manager import AutomaticThemeManager


class ForumDatabase:
    """SQLite database manager for forum data"""

    def __init__(self, db_path="global_forum.db"):
        self.db_path = db_path
        self.init_database()

    def init_database(self):
        """Initialize forum database"""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()

        # Users table
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS users (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                username TEXT UNIQUE NOT NULL,
                display_name TEXT NOT NULL,
                email TEXT,
                avatar_color TEXT DEFAULT '#FF8C00',
                reputation INTEGER DEFAULT 0,
                join_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                last_active TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                status TEXT DEFAULT 'online'
            )
        ''')

        # Categories table
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS categories (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT NOT NULL,
                description TEXT,
                color TEXT DEFAULT '#FFD700',
                post_count INTEGER DEFAULT 0,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            )
        ''')

        # Posts table
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS posts (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                title TEXT NOT NULL,
                content TEXT NOT NULL,
                author_id INTEGER,
                category_id INTEGER,
                views INTEGER DEFAULT 0,
                likes INTEGER DEFAULT 0,
                replies INTEGER DEFAULT 0,
                is_pinned INTEGER DEFAULT 0,
                is_locked INTEGER DEFAULT 0,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                FOREIGN KEY (author_id) REFERENCES users (id),
                FOREIGN KEY (category_id) REFERENCES categories (id)
            )
        ''')

        # Comments table
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS comments (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                post_id INTEGER,
                author_id INTEGER,
                content TEXT NOT NULL,
                likes INTEGER DEFAULT 0,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                FOREIGN KEY (post_id) REFERENCES posts (id) ON DELETE CASCADE,
                FOREIGN KEY (author_id) REFERENCES users (id)
            )
        ''')

        # Votes table
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS votes (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                user_id INTEGER,
                post_id INTEGER,
                comment_id INTEGER,
                vote_type TEXT CHECK(vote_type IN ('up', 'down')),
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                FOREIGN KEY (user_id) REFERENCES users (id),
                FOREIGN KEY (post_id) REFERENCES posts (id),
                FOREIGN KEY (comment_id) REFERENCES comments (id)
            )
        ''')

        conn.commit()
        conn.close()

        # Initialize with sample data
        self.create_sample_data()

    def create_sample_data(self):
        """Create sample forum data"""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()

        # Check if data already exists
        cursor.execute("SELECT COUNT(*) FROM users")
        if cursor.fetchone()[0] > 0:
            conn.close()
            return

        # Sample users
        users = [
            ("admin", "Forum Administrator", "admin@fincept.com", "#FF0000", 1000),
            ("trader_pro", "Professional Trader", "trader@fincept.com", "#00FF00", 850),
            ("market_analyst", "Market Analyst", "analyst@fincept.com", "#0080FF", 720),
            ("crypto_bull", "Crypto Enthusiast", "crypto@fincept.com", "#FFD700", 650),
            ("risk_manager", "Risk Management Expert", "risk@fincept.com", "#FF8C00", 580),
            ("newbie_trader", "Trading Newbie", "newbie@fincept.com", "#8A2BE2", 120)
        ]

        for username, display_name, email, color, rep in users:
            cursor.execute('''
                INSERT INTO users (username, display_name, email, avatar_color, reputation)
                VALUES (?, ?, ?, ?, ?)
            ''', (username, display_name, email, color, rep))

        # Sample categories
        categories = [
            ("Market Analysis", "Technical and fundamental market analysis discussions", "#FF8C00"),
            ("Trading Strategies", "Share and discuss trading strategies", "#00C851"),
            ("Crypto Corner", "Cryptocurrency discussions and insights", "#FFD700"),
            ("Risk Management", "Risk management techniques and tools", "#FF4444"),
            ("Economic News", "Latest economic news and market impacts", "#00B0FF"),
            ("Beginner's Help", "Help and guidance for new traders", "#8A2BE2"),
            ("Portfolio Review", "Get feedback on your trading portfolio", "#FF6F00"),
            ("Technology", "Trading platforms, tools, and technology", "#9C27B0")
        ]

        for name, desc, color in categories:
            cursor.execute('''
                INSERT INTO categories (name, description, color)
                VALUES (?, ?, ?)
            ''', (name, desc, color))

        # Sample posts
        sample_posts = [
            ("Market Outlook for Q4 2024",
             "What are your thoughts on the market direction for the rest of 2024? With Fed policy changes and global uncertainties, I'm seeing mixed signals.",
             2, 1, 245, 15, 8),
            ("Best Moving Average Strategy?",
             "I've been experimenting with different MA combinations. Currently using 20/50 EMA crossover. What's working for you?",
             3, 2, 189, 22, 12),
            ("Bitcoin Breaking $70K - What's Next?",
             "BTC just broke through major resistance. Are we heading to new ATHs or is this a bull trap? Technical analysis inside.",
             4, 3, 412, 35, 24),
            ("Risk Management: The 2% Rule",
             "Sharing my experience with the 2% risk rule. How do you manage position sizing and risk per trade?", 5, 4,
             156, 28, 6),
            ("Fed Meeting Impact Analysis",
             "Breaking down today's Fed announcement and its implications for equity markets. Rate cut expectations vs reality.",
             2, 5, 328, 19, 15),
            ("New Trader - Need Help!",
             "Just started trading with $5K account. What should I focus on first? Any recommended resources?", 6, 6,
             98, 12, 18),
            ("Portfolio Diversification Strategy",
             "Looking for feedback on my current portfolio allocation: 60% stocks, 25% bonds, 10% crypto, 5% commodities.",
             3, 7, 167, 8, 9),
            ("AI Trading Algorithms Discussion",
             "Anyone here using machine learning for trading? Interested in discussing algorithmic trading strategies.",
             2, 8, 89, 6, 4)
        ]

        for title, content, author, category, views, likes, replies in sample_posts:
            cursor.execute('''
                INSERT INTO posts (title, content, author_id, category_id, views, likes, replies)
                VALUES (?, ?, ?, ?, ?, ?, ?)
            ''', (title, content, author, category, views, likes, replies))

        # Update category post counts
        cursor.execute('''
            UPDATE categories SET post_count = (
                SELECT COUNT(*) FROM posts WHERE posts.category_id = categories.id
            )
        ''')

        conn.commit()
        conn.close()

    # Database methods for forum operations
    def get_categories(self):
        """Get all categories with post counts"""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        cursor.execute('''
            SELECT c.id, c.name, c.description, c.color, COUNT(p.id) as post_count
            FROM categories c
            LEFT JOIN posts p ON c.id = p.category_id
            GROUP BY c.id, c.name, c.description, c.color
            ORDER BY c.name
        ''')
        result = cursor.fetchall()
        conn.close()
        return result

    def get_posts(self, category_id=None, limit=50, search_term=None):
        """Get posts with optional filtering"""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()

        query = '''
            SELECT p.id, p.title, p.content, p.views, p.likes, p.replies, p.is_pinned, p.is_locked,
                   p.created_at, p.updated_at, u.username, u.display_name, u.avatar_color, u.reputation,
                   c.name as category_name, c.color as category_color
            FROM posts p
            JOIN users u ON p.author_id = u.id
            JOIN categories c ON p.category_id = c.id
        '''

        params = []
        conditions = []

        if category_id:
            conditions.append("p.category_id = ?")
            params.append(category_id)

        if search_term:
            conditions.append("(p.title LIKE ? OR p.content LIKE ?)")
            params.extend([f"%{search_term}%", f"%{search_term}%"])

        if conditions:
            query += " WHERE " + " AND ".join(conditions)

        query += " ORDER BY p.is_pinned DESC, p.updated_at DESC LIMIT ?"
        params.append(limit)

        cursor.execute(query, params)
        result = cursor.fetchall()
        conn.close()
        return result

    def get_user_stats(self):
        """Get forum statistics"""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()

        cursor.execute("SELECT COUNT(*) FROM users")
        total_users = cursor.fetchone()[0]

        cursor.execute("SELECT COUNT(*) FROM posts")
        total_posts = cursor.fetchone()[0]

        cursor.execute("SELECT COUNT(*) FROM comments")
        total_comments = cursor.fetchone()[0]

        cursor.execute("SELECT COUNT(*) FROM users WHERE status = 'online'")
        online_users = cursor.fetchone()[0]

        conn.close()
        return {
            "total_users": total_users,
            "total_posts": total_posts,
            "total_comments": total_comments,
            "online_users": online_users
        }

    def create_post(self, title, content, author_id, category_id):
        """Create new post"""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()

        cursor.execute('''
            INSERT INTO posts (title, content, author_id, category_id)
            VALUES (?, ?, ?, ?)
        ''', (title, content, author_id, category_id))

        post_id = cursor.lastrowid

        # Update category post count
        cursor.execute('''
            UPDATE categories SET post_count = post_count + 1 WHERE id = ?
        ''', (category_id,))

        conn.commit()
        conn.close()
        return post_id


class BloombergTerminalForumUI:
    """Bloomberg Terminal Style Global Forum Interface"""

    def __init__(self):
        self.db = ForumDatabase()
        self.theme_manager = AutomaticThemeManager()
        self.current_category = None
        self.current_user_id = 1  # Default to admin for demo
        self.selected_post_id = None

        # Bloomberg Terminal Colors
        self.BLOOMBERG_ORANGE = [255, 165, 0]
        self.BLOOMBERG_WHITE = [255, 255, 255]
        self.BLOOMBERG_RED = [255, 0, 0]
        self.BLOOMBERG_GREEN = [0, 200, 0]
        self.BLOOMBERG_YELLOW = [255, 255, 0]
        self.BLOOMBERG_GRAY = [120, 120, 120]
        self.BLOOMBERG_BLUE = [0, 128, 255]
        self.BLOOMBERG_PURPLE = [138, 43, 226]

        # Initialize DearPyGUI
        dpg.create_context()
        self.create_forum_ui()
        self.load_initial_data()

    def create_forum_ui(self):
        """Create the main forum interface"""

        with dpg.window(tag="forum_main_window", label="FINCEPT TERMINAL - GLOBAL FORUM"):
            # Header with branding and navigation
            self.create_forum_header()

            # Function keys for forum actions
            self.create_forum_function_keys()

            # Main forum layout
            with dpg.group(horizontal=True):
                # Left panel - Categories and Stats
                self.create_categories_panel()

                # Center panel - Posts List
                self.create_posts_panel()

                # Right panel - User Info and Actions
                self.create_user_panel()

            # Bottom status bar
            self.create_forum_status_bar()

        dpg.set_primary_window("forum_main_window", True)

    def create_forum_header(self):
        """Create Bloomberg Terminal style header"""
        with dpg.group(horizontal=True):
            dpg.add_text("FINCEPT", color=self.BLOOMBERG_ORANGE)
            dpg.add_text("GLOBAL FORUM", color=self.BLOOMBERG_WHITE)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)

            # Search functionality
            dpg.add_input_text(
                label="",
                default_value="Search posts, topics...",
                width=300,
                tag="forum_search_input",
                callback=self.search_posts_callback
            )
            dpg.add_button(label="SEARCH", width=80, callback=self.execute_search)

            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text(datetime.now().strftime('%Y-%m-%d %H:%M:%S'), tag="forum_header_time")

            # User info
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("USER: ADMIN", color=self.BLOOMBERG_GREEN, tag="current_user_info")

        dpg.add_separator()

    def create_forum_function_keys(self):
        """Create function keys for forum actions"""
        with dpg.group(horizontal=True):
            function_keys = [
                ("F1:HELP", self.show_forum_help),
                ("F2:CATEGORIES", self.focus_categories),
                ("F3:NEW POST", self.create_new_post),
                ("F4:SEARCH", self.focus_search),
                ("F5:REFRESH", self.refresh_forum),
                ("F6:PROFILE", self.show_user_profile)
            ]
            for key_label, callback in function_keys:
                dpg.add_button(label=key_label, width=120, height=25, callback=callback)

        dpg.add_separator()

    def create_categories_panel(self):
        """Create left panel with categories"""
        with dpg.child_window(width=300, height=650, border=True, tag="categories_panel"):
            # Categories header
            dpg.add_text("FORUM CATEGORIES", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Category filters
            with dpg.group(horizontal=True):
                dpg.add_button(label="ALL", callback=lambda: self.filter_by_category(None), width=60, height=25)
                dpg.add_button(label="PINNED", callback=self.show_pinned_posts, width=70, height=25)
                dpg.add_button(label="RECENT", callback=self.show_recent_posts, width=70, height=25)

            dpg.add_separator()

            # Categories list
            dpg.add_child_window(height=350, border=False, tag="categories_list")

            dpg.add_separator()

            # Forum statistics
            dpg.add_text("FORUM STATISTICS", color=self.BLOOMBERG_YELLOW)
            dpg.add_text("Total Users: 0", tag="stat_total_users", color=self.BLOOMBERG_WHITE)
            dpg.add_text("Total Posts: 0", tag="stat_total_posts", color=self.BLOOMBERG_WHITE)
            dpg.add_text("Total Comments: 0", tag="stat_total_comments", color=self.BLOOMBERG_WHITE)
            dpg.add_text("Online Now: 0", tag="stat_online_users", color=self.BLOOMBERG_GREEN)

            dpg.add_separator()

            # Quick actions
            dpg.add_text("QUICK ACTIONS", color=self.BLOOMBERG_YELLOW)
            dpg.add_button(label="CREATE NEW POST", callback=self.create_new_post, width=-1, height=30)
            dpg.add_button(label="MY POSTS", callback=self.show_my_posts, width=-1, height=25)
            dpg.add_button(label="TRENDING TOPICS", callback=self.show_trending, width=-1, height=25)

    def create_posts_panel(self):
        """Create center panel with posts list"""
        with dpg.child_window(width=880, height=650, border=True, tag="posts_panel"):
            # Posts header
            with dpg.group(horizontal=True):
                dpg.add_text("FORUM POSTS", color=self.BLOOMBERG_ORANGE, tag="posts_header_title")
                dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                dpg.add_text("All Categories", color=self.BLOOMBERG_WHITE, tag="current_category_name")

                # Sorting options
                dpg.add_spacer(width=50)
                dpg.add_text("SORT:", color=self.BLOOMBERG_GRAY)
                dpg.add_combo(
                    ["Latest", "Most Liked", "Most Viewed", "Most Replies"],
                    default_value="Latest",
                    width=120,
                    tag="sort_combo",
                    callback=self.sort_posts_callback
                )

            dpg.add_separator()

            # Posts list area
            dpg.add_child_window(height=-1, border=False, tag="posts_list_area")

    def create_user_panel(self):
        """Create right panel with user info and actions"""
        with dpg.child_window(width=350, height=650, border=True, tag="user_panel"):
            dpg.add_text("USER DASHBOARD", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Current user info
            dpg.add_text("CURRENT USER", color=self.BLOOMBERG_YELLOW)
            with dpg.group(horizontal=True):
                # User avatar (colored square)
                with dpg.drawlist(width=40, height=40):
                    dpg.draw_rectangle([0, 0], [40, 40], color=self.BLOOMBERG_ORANGE, fill=self.BLOOMBERG_ORANGE)
                    dpg.draw_text([10, 15], "AD", color=self.BLOOMBERG_WHITE, size=12)

                with dpg.group():
                    dpg.add_text("Forum Administrator", color=self.BLOOMBERG_WHITE, tag="user_display_name")
                    dpg.add_text("Reputation: 1000", color=self.BLOOMBERG_GREEN, tag="user_reputation")
                    dpg.add_text("Status: Online", color=self.BLOOMBERG_GREEN, tag="user_status")

            dpg.add_separator()

            # User actions
            dpg.add_text("USER ACTIONS", color=self.BLOOMBERG_YELLOW)
            user_actions = [
                ("VIEW PROFILE", self.show_user_profile),
                ("MY POSTS", self.show_my_posts),
                ("MY COMMENTS", self.show_my_comments),
                ("NOTIFICATIONS", self.show_notifications),
                ("SETTINGS", self.show_user_settings)
            ]

            for action_name, callback in user_actions:
                dpg.add_button(label=action_name, callback=callback, width=-1, height=25)

            dpg.add_separator()

            # Active users
            dpg.add_text("ACTIVE USERS", color=self.BLOOMBERG_YELLOW)
            with dpg.child_window(height=200, border=True, tag="active_users_list"):
                # Sample active users
                active_users = [
                    ("trader_pro", "Professional Trader", "#00FF00", "850"),
                    ("market_analyst", "Market Analyst", "#0080FF", "720"),
                    ("crypto_bull", "Crypto Enthusiast", "#FFD700", "650"),
                    ("risk_manager", "Risk Expert", "#FF8C00", "580")
                ]

                for username, display_name, color, rep in active_users:
                    with dpg.group(horizontal=True):
                        # Status indicator
                        dpg.add_text("●", color=self.BLOOMBERG_GREEN)
                        dpg.add_text(f"{username}", color=self.BLOOMBERG_WHITE)
                        dpg.add_text(f"({rep})", color=self.BLOOMBERG_GRAY)

            dpg.add_separator()

            # Recent activity
            dpg.add_text("RECENT ACTIVITY", color=self.BLOOMBERG_YELLOW)
            with dpg.child_window(height=-1, border=True, tag="recent_activity"):
                dpg.add_text("• New post in Market Analysis", color=self.BLOOMBERG_WHITE)
                dpg.add_text("• trader_pro liked your comment", color=self.BLOOMBERG_GREEN)
                dpg.add_text("• 5 new replies to 'Bitcoin Analysis'", color=self.BLOOMBERG_BLUE)
                dpg.add_text("• market_analyst mentioned you", color=self.BLOOMBERG_YELLOW)

    def create_forum_status_bar(self):
        """Create bottom status bar"""
        dpg.add_separator()
        with dpg.group(horizontal=True):
            dpg.add_text("●", color=self.BLOOMBERG_GREEN)
            dpg.add_text("FORUM ONLINE", color=self.BLOOMBERG_GREEN)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("GLOBAL FORUM", color=self.BLOOMBERG_ORANGE)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("STATUS: ACTIVE", color=self.BLOOMBERG_WHITE, tag="forum_status")
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("SERVER: FORUM-01", color=self.BLOOMBERG_WHITE)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text(f"TIME: {datetime.now().strftime('%H:%M:%S')}",
                         color=self.BLOOMBERG_WHITE, tag="forum_status_time")
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("LATENCY: <15ms", color=self.BLOOMBERG_GREEN)

    def create_category_item(self, cat_id, name, description, color, post_count):
        """Create category item in the list"""
        with dpg.group(parent="categories_list"):
            # Category button
            dpg.add_button(
                label=f"{name} ({post_count})",
                callback=lambda: self.filter_by_category(cat_id),
                width=-1,
                height=35
            )

            # Category description
            dpg.add_text(f"  {description[:40]}...", color=self.BLOOMBERG_GRAY)
            dpg.add_spacer(height=5)

    def create_post_item(self, post_data):
        """Create post item in the list"""
        (post_id, title, content, views, likes, replies, is_pinned, is_locked,
         created_at, updated_at, username, display_name, avatar_color, reputation,
         category_name, category_color) = post_data

        with dpg.group(parent="posts_list_area"):
            # Post container
            with dpg.child_window(width=-1, height=120, border=True):
                # Post header
                with dpg.group(horizontal=True):
                    # Pin/Lock indicators
                    if is_pinned:
                        dpg.add_text("📌", color=self.BLOOMBERG_ORANGE)
                    if is_locked:
                        dpg.add_text("🔒", color=self.BLOOMBERG_RED)

                    # Post title (clickable)
                    dpg.add_button(
                        label=title[:60] + "..." if len(title) > 60 else title,
                        callback=lambda: self.view_post_details(post_id),
                        width=400,
                        height=25
                    )

                    # Category tag
                    dpg.add_text(f"[{category_name}]", color=category_color)

                # Post stats
                with dpg.group(horizontal=True):
                    dpg.add_text(f"👁 {views}", color=self.BLOOMBERG_GRAY)
                    dpg.add_spacer(width=20)
                    dpg.add_text(f"👍 {likes}", color=self.BLOOMBERG_GREEN)
                    dpg.add_spacer(width=20)
                    dpg.add_text(f"💬 {replies}", color=self.BLOOMBERG_BLUE)
                    dpg.add_spacer(width=20)
                    dpg.add_text(f"By: {username}", color=self.BLOOMBERG_WHITE)
                    dpg.add_spacer(width=20)

                    # Format timestamp
                    try:
                        dt = datetime.fromisoformat(created_at.replace('Z', '+00:00'))
                        time_str = dt.strftime("%m/%d %H:%M")
                    except:
                        time_str = "Recent"

                    dpg.add_text(time_str, color=self.BLOOMBERG_GRAY)

                # Post preview
                preview = content[:100] + "..." if len(content) > 100 else content
                dpg.add_text(preview, color=self.BLOOMBERG_WHITE, wrap=650)

                # Action buttons
                with dpg.group(horizontal=True):
                    dpg.add_button(label="VIEW", callback=lambda: self.view_post_details(post_id), width=60, height=20)
                    dpg.add_button(label="LIKE", callback=lambda: self.like_post(post_id), width=60, height=20)
                    dpg.add_button(label="REPLY", callback=lambda: self.reply_to_post(post_id), width=60, height=20)

            dpg.add_spacer(height=5)

    # Event handlers and callbacks
    def filter_by_category(self, category_id):
        """Filter posts by category"""
        self.current_category = category_id

        if category_id is None:
            dpg.set_value("current_category_name", "All Categories")
        else:
            categories = self.db.get_categories()
            for cat_id, name, desc, color, count in categories:
                if cat_id == category_id:
                    dpg.set_value("current_category_name", name)
                    break

        self.refresh_posts_list()

    def refresh_posts_list(self):
        """Refresh the posts list"""
        # Clear existing posts
        dpg.delete_item("posts_list_area", children_only=True)

        # Get posts based on current filter
        posts = self.db.get_posts(category_id=self.current_category)

        # Create post items
        for post_data in posts:
            self.create_post_item(post_data)

    def load_categories(self):
        """Load categories into the panel"""
        # Clear existing categories
        dpg.delete_item("categories_list", children_only=True)

        # Get categories from database
        categories = self.db.get_categories()

        # Create category items
        for cat_id, name, description, color, post_count in categories:
            self.create_category_item(cat_id, name, description, color, post_count)

    def update_forum_stats(self):
        """Update forum statistics"""
        stats = self.db.get_user_stats()

        dpg.set_value("stat_total_users", f"Total Users: {stats['total_users']}")
        dpg.set_value("stat_total_posts", f"Total Posts: {stats['total_posts']}")
        dpg.set_value("stat_total_comments", f"Total Comments: {stats['total_comments']}")
        dpg.set_value("stat_online_users", f"Online Now: {stats['online_users']}")

    def create_new_post(self):
        """Show create new post dialog"""
        if not dpg.does_item_exist("new_post_window"):
            with dpg.window(
                    label="CREATE NEW POST",
                    tag="new_post_window",
                    width=600,
                    height=500,
                    pos=[300, 150],
                    modal=True
            ):
                dpg.add_text("CREATE NEW FORUM POST", color=self.BLOOMBERG_ORANGE)
                dpg.add_separator()

                # Post form
                dpg.add_text("Post Title:")
                dpg.add_input_text(width=550, tag="new_post_title", hint="Enter post title...")

                dpg.add_spacer(height=10)

                dpg.add_text("Category:")
                categories = self.db.get_categories()
                cat_names = [f"{name}" for _, name, _, _, _ in categories]
                dpg.add_combo(cat_names, width=200, tag="new_post_category")

                dpg.add_spacer(height=10)

                dpg.add_text("Post Content:")
                dpg.add_input_text(
                    width=550,
                    height=200,
                    multiline=True,
                    tag="new_post_content",
                    hint="Write your post content here..."
                )

                dpg.add_spacer(height=20)

                # Action buttons
                with dpg.group(horizontal=True):
                    dpg.add_button(
                        label="CANCEL",
                        callback=lambda: dpg.delete_item("new_post_window"),
                        width=100
                    )
                    dpg.add_spacer(width=20)
                    dpg.add_button(
                        label="CREATE POST",
                        callback=self.submit_new_post,
                        width=120
                    )
        else:
            dpg.show_item("new_post_window")

    def submit_new_post(self):
        """Submit new post to database"""
        title = dpg.get_value("new_post_title").strip()
        content = dpg.get_value("new_post_content").strip()
        category_name = dpg.get_value("new_post_category")

        if not title or not content:
            dpg.set_value("forum_status", "STATUS: POST CREATION FAILED - MISSING FIELDS")
            return

        # Get category ID
        categories = self.db.get_categories()
        category_id = 1  # Default
        for cat_id, name, _, _, _ in categories:
            if name == category_name:
                category_id = cat_id
                break

        # Create post
        post_id = self.db.create_post(title, content, self.current_user_id, category_id)

        # Close dialog and refresh
        dpg.delete_item("new_post_window")
        dpg.set_value("forum_status", "STATUS: POST CREATED SUCCESSFULLY")

        self.refresh_posts_list()
        self.load_categories()
        self.update_forum_stats()

    def view_post_details(self, post_id):
        """Show detailed post view"""
        self.selected_post_id = post_id

        if not dpg.does_item_exist("post_detail_window"):
            with dpg.window(
                    label="POST DETAILS",
                    tag="post_detail_window",
                    width=800,
                    height=600,
                    pos=[200, 100]
            ):
                dpg.add_text("POST DETAILS", color=self.BLOOMBERG_ORANGE, tag="post_detail_title")
                dpg.add_separator()

                # Post content area
                dpg.add_child_window(height=400, border=True, tag="post_detail_content")

                dpg.add_separator()

                # Comment input
                dpg.add_text("ADD COMMENT:", color=self.BLOOMBERG_YELLOW)
                dpg.add_input_text(
                    width=750,
                    height=60,
                    multiline=True,
                    tag="new_comment_input",
                    hint="Write your comment..."
                )

                with dpg.group(horizontal=True):
                    dpg.add_button(label="POST COMMENT", callback=self.submit_comment, width=120)
                    dpg.add_spacer(width=20)
                    dpg.add_button(label="CLOSE", callback=lambda: dpg.delete_item("post_detail_window"), width=80)

        # Load post details
        self.load_post_details(post_id)
        dpg.show_item("post_detail_window")

    def load_post_details(self, post_id):
        """Load detailed post information"""
        posts = self.db.get_posts()
        post_data = None

        for post in posts:
            if post[0] == post_id:
                post_data = post
                break

        if not post_data:
            return

        # Clear detail content
        dpg.delete_item("post_detail_content", children_only=True)

        (post_id, title, content, views, likes, replies, is_pinned, is_locked,
         created_at, updated_at, username, display_name, avatar_color, reputation,
         category_name, category_color) = post_data

        with dpg.group(parent="post_detail_content"):
            # Post header
            with dpg.group(horizontal=True):
                if is_pinned:
                    dpg.add_text("📌 PINNED", color=self.BLOOMBERG_ORANGE)
                if is_locked:
                    dpg.add_text("🔒 LOCKED", color=self.BLOOMBERG_RED)

                dpg.add_text(f"[{category_name}]", color=category_color)

            dpg.add_spacer(height=10)

            # Post title
            dpg.add_text(title, color=self.BLOOMBERG_WHITE)

            dpg.add_spacer(height=10)

            # Author info
            with dpg.group(horizontal=True):
                dpg.add_text(f"By: {display_name} ({username})", color=self.BLOOMBERG_YELLOW)
                dpg.add_spacer(width=20)
                dpg.add_text(f"Reputation: {reputation}", color=self.BLOOMBERG_GREEN)
                dpg.add_spacer(width=20)

                try:
                    dt = datetime.fromisoformat(created_at.replace('Z', '+00:00'))
                    time_str = dt.strftime("%Y-%m-%d %H:%M")
                except:
                    time_str = "Unknown"

                dpg.add_text(f"Posted: {time_str}", color=self.BLOOMBERG_GRAY)

            dpg.add_separator()

            # Post content
            dpg.add_text(content, color=self.BLOOMBERG_WHITE, wrap=750)

            dpg.add_separator()

            # Post stats and actions
            with dpg.group(horizontal=True):
                dpg.add_text(f"Views: {views}", color=self.BLOOMBERG_GRAY)
                dpg.add_spacer(width=20)
                dpg.add_text(f"Likes: {likes}", color=self.BLOOMBERG_GREEN)
                dpg.add_spacer(width=20)
                dpg.add_text(f"Replies: {replies}", color=self.BLOOMBERG_BLUE)
                dpg.add_spacer(width=50)

                dpg.add_button(label="👍 LIKE", callback=lambda: self.like_post(post_id), width=80)
                dpg.add_button(label="📤 SHARE", callback=lambda: self.share_post(post_id), width=80)
                dpg.add_button(label="⚠️ REPORT", callback=lambda: self.report_post(post_id), width=80)

    def like_post(self, post_id):
        """Like a post"""
        dpg.set_value("forum_status", f"STATUS: LIKED POST #{post_id}")
        # Here you would update the database

    def reply_to_post(self, post_id):
        """Reply to a post"""
        self.view_post_details(post_id)

    def submit_comment(self):
        """Submit a comment"""
        comment = dpg.get_value("new_comment_input").strip()
        if comment and self.selected_post_id:
            dpg.set_value("forum_status", "STATUS: COMMENT POSTED")
            dpg.set_value("new_comment_input", "")
            # Here you would save to database

    def share_post(self, post_id):
        """Share a post"""
        dpg.set_value("forum_status", f"STATUS: SHARED POST #{post_id}")

    def report_post(self, post_id):
        """Report a post"""
        dpg.set_value("forum_status", f"STATUS: REPORTED POST #{post_id}")

    def search_posts_callback(self, sender, app_data):
        """Handle search input"""
        pass  # Real-time search would go here

    def execute_search(self):
        """Execute search"""
        search_term = dpg.get_value("forum_search_input")
        if search_term and search_term != "Search posts, topics...":
            dpg.set_value("forum_status", f"STATUS: SEARCHING FOR '{search_term}'")
            # Clear posts and show search results
            dpg.delete_item("posts_list_area", children_only=True)
            posts = self.db.get_posts(search_term=search_term)
            for post_data in posts:
                self.create_post_item(post_data)

    def sort_posts_callback(self, sender, app_data):
        """Handle post sorting"""
        dpg.set_value("forum_status", f"STATUS: SORTING BY {app_data.upper()}")
        self.refresh_posts_list()

    def show_pinned_posts(self):
        """Show only pinned posts"""
        dpg.set_value("forum_status", "STATUS: SHOWING PINNED POSTS")
        dpg.set_value("current_category_name", "Pinned Posts")
        # Filter for pinned posts
        self.refresh_posts_list()

    def show_recent_posts(self):
        """Show recent posts"""
        dpg.set_value("forum_status", "STATUS: SHOWING RECENT POSTS")
        dpg.set_value("current_category_name", "Recent Posts")
        self.refresh_posts_list()

    def show_my_posts(self):
        """Show current user's posts"""
        dpg.set_value("forum_status", "STATUS: SHOWING MY POSTS")
        dpg.set_value("current_category_name", "My Posts")
        # Filter for user's posts
        self.refresh_posts_list()

    def show_trending(self):
        """Show trending topics"""
        dpg.set_value("forum_status", "STATUS: SHOWING TRENDING TOPICS")
        dpg.set_value("current_category_name", "Trending Topics")
        self.refresh_posts_list()

    # Function key callbacks
    def show_forum_help(self):
        """Show forum help"""
        if not dpg.does_item_exist("forum_help_window"):
            with dpg.window(
                    label="FORUM HELP",
                    tag="forum_help_window",
                    width=600,
                    height=500,
                    pos=[300, 150]
            ):
                dpg.add_text("FINCEPT GLOBAL FORUM HELP", color=self.BLOOMBERG_ORANGE)
                dpg.add_separator()

                dpg.add_text("FUNCTION KEYS:", color=self.BLOOMBERG_YELLOW)
                dpg.add_text("F1: Help - Show this help screen")
                dpg.add_text("F2: Categories - Focus category panel")
                dpg.add_text("F3: New Post - Create new forum post")
                dpg.add_text("F4: Search - Focus search field")
                dpg.add_text("F5: Refresh - Refresh forum content")
                dpg.add_text("F6: Profile - Show user profile")

                dpg.add_separator()

                dpg.add_text("FORUM FEATURES:", color=self.BLOOMBERG_YELLOW)
                dpg.add_text("• Browse posts by category")
                dpg.add_text("• Create new posts and comments")
                dpg.add_text("• Like and share posts")
                dpg.add_text("• Search across all content")
                dpg.add_text("• View user profiles and reputation")
                dpg.add_text("• Real-time activity updates")

                dpg.add_spacer(height=20)
                dpg.add_button(label="CLOSE", callback=lambda: dpg.delete_item("forum_help_window"))
        else:
            dpg.show_item("forum_help_window")

    def focus_categories(self):
        """Focus categories panel"""
        dpg.focus_item("categories_panel")

    def focus_search(self):
        """Focus search field"""
        dpg.focus_item("forum_search_input")
        dpg.set_value("forum_search_input", "")

    def refresh_forum(self):
        """Refresh entire forum"""
        dpg.set_value("forum_status", "STATUS: REFRESHING FORUM...")
        self.load_categories()
        self.refresh_posts_list()
        self.update_forum_stats()
        dpg.set_value("forum_status", "STATUS: FORUM REFRESHED")

    def show_user_profile(self):
        """Show user profile"""
        if not dpg.does_item_exist("user_profile_window"):
            with dpg.window(
                    label="USER PROFILE",
                    tag="user_profile_window",
                    width=500,
                    height=400,
                    pos=[350, 200]
            ):
                dpg.add_text("USER PROFILE", color=self.BLOOMBERG_ORANGE)
                dpg.add_separator()

                # User info
                with dpg.group(horizontal=True):
                    # Large avatar
                    with dpg.drawlist(width=80, height=80):
                        dpg.draw_rectangle([0, 0], [80, 80], color=self.BLOOMBERG_ORANGE, fill=self.BLOOMBERG_ORANGE)
                        dpg.draw_text([25, 35], "ADMIN", color=self.BLOOMBERG_WHITE, size=14)

                    with dpg.group():
                        dpg.add_text("Forum Administrator", color=self.BLOOMBERG_WHITE)
                        dpg.add_text("Username: admin", color=self.BLOOMBERG_GRAY)
                        dpg.add_text("Reputation: 1000", color=self.BLOOMBERG_GREEN)
                        dpg.add_text("Posts: 25", color=self.BLOOMBERG_BLUE)
                        dpg.add_text("Comments: 150", color=self.BLOOMBERG_BLUE)
                        dpg.add_text("Joined: 2024-01-01", color=self.BLOOMBERG_GRAY)

                dpg.add_separator()

                dpg.add_text("Recent Activity:", color=self.BLOOMBERG_YELLOW)
                dpg.add_text("• Created 'Market Outlook for Q4 2024'")
                dpg.add_text("• Commented on 'Bitcoin Analysis'")
                dpg.add_text("• Liked 5 posts this week")

                dpg.add_spacer(height=20)
                dpg.add_button(label="CLOSE", callback=lambda: dpg.delete_item("user_profile_window"))
        else:
            dpg.show_item("user_profile_window")

    def show_my_comments(self):
        """Show user's comments"""
        dpg.set_value("forum_status", "STATUS: SHOWING MY COMMENTS")

    def show_notifications(self):
        """Show user notifications"""
        dpg.set_value("forum_status", "STATUS: SHOWING NOTIFICATIONS")

    def show_user_settings(self):
        """Show user settings"""
        if not dpg.does_item_exist("user_settings_window"):
            with dpg.window(
                    label="USER SETTINGS",
                    tag="user_settings_window",
                    width=450,
                    height=350,
                    pos=[375, 225]
            ):
                dpg.add_text("USER SETTINGS", color=self.BLOOMBERG_ORANGE)
                dpg.add_separator()

                dpg.add_text("Display Name:")
                dpg.add_input_text(default_value="Forum Administrator", width=300)

                dpg.add_text("Email:")
                dpg.add_input_text(default_value="admin@fincept.com", width=300)

                dpg.add_text("Notification Preferences:")
                dpg.add_checkbox("Email notifications")
                dpg.add_checkbox("Post replies")
                dpg.add_checkbox("Mentions")

                dpg.add_spacer(height=20)

                with dpg.group(horizontal=True):
                    dpg.add_button(label="SAVE", width=100)
                    dpg.add_spacer(width=20)
                    dpg.add_button(label="CANCEL", callback=lambda: dpg.delete_item("user_settings_window"), width=100)
        else:
            dpg.show_item("user_settings_window")

    def update_time_displays(self):
        """Update time displays"""
        current_time = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
        time_only = datetime.now().strftime('%H:%M:%S')

        try:
            dpg.set_value("forum_header_time", current_time)
            dpg.set_value("forum_status_time", f"TIME: {time_only}")
        except:
            pass

    def start_time_updater(self):
        """Start time updater thread"""

        def time_loop():
            while True:
                self.update_time_displays()
                time.sleep(1)

        timer_thread = threading.Thread(target=time_loop, daemon=True)
        timer_thread.start()

    def load_initial_data(self):
        """Load initial forum data"""
        # Apply Bloomberg Terminal theme
        self.theme_manager.apply_theme_globally("bloomberg_terminal")

        # Load forum data
        self.load_categories()
        self.refresh_posts_list()
        self.update_forum_stats()

        # Start time updater
        self.start_time_updater()

    def run(self):
        """Run the forum application"""
        try:
            # Create viewport
            dpg.create_viewport(
                title="FINCEPT TERMINAL - Global Forum Professional Interface",
                width=1600,
                height=1000,
                min_width=1200,
                min_height=800,
                resizable=True,
                vsync=True
            )

            # Setup and show
            dpg.setup_dearpygui()
            dpg.show_viewport()

            # Apply theme
            self.theme_manager.apply_theme_globally("bloomberg_terminal")

            print("=" * 80)
            print("FINCEPT TERMINAL GLOBAL FORUM - PROFESSIONAL EDITION")
            print("=" * 80)
            print("STATUS: ONLINE")
            print("THEME: Bloomberg Terminal")
            print("DATABASE: SQLite Connected")
            print("FEATURES: Global Forum with Categories, Posts, Comments")
            print("=" * 80)

            # Main loop
            while dpg.is_dearpygui_running():
                dpg.render_dearpygui_frame()

        except Exception as e:
            print(f"CRITICAL ERROR: {e}")
            import traceback
            traceback.print_exc()
        finally:
            self.cleanup()

    def cleanup(self):
        """Cleanup resources"""
        try:
            print("SHUTTING DOWN FORUM...")

            if hasattr(self, 'theme_manager'):
                self.theme_manager.cleanup()

            if dpg.is_dearpygui_running():
                dpg.destroy_context()

            print("FORUM SHUTDOWN COMPLETE")

        except Exception as e:
            print(f"CLEANUP WARNING: {e}")


def main():
    """Main entry point for Global Forum"""
    try:
        print("INITIALIZING FINCEPT GLOBAL FORUM...")
        print("Loading Bloomberg Terminal forum interface...")

        # Create and run forum application
        forum_app = BloombergTerminalForumUI()
        forum_app.run()

    except KeyboardInterrupt:
        print("\nFORUM SHUTDOWN INITIATED BY USER")
    except Exception as e:
        print(f"FATAL FORUM ERROR: {e}")
        import traceback
        traceback.print_exc()


if __name__ == "__main__":
    main()