# forum_tab.py - Updated Forum Tab for Fincept Terminal
import dearpygui.dearpygui as dpg
import threading
from datetime import datetime
from fincept_terminal.Utils.base_tab import BaseTab
from fincept_terminal.Utils.APIClient.api_client import create_api_client


class ForumTab(BaseTab):
    """Global Forum Tab integrated with Fincept Terminal"""

    def __init__(self, app):
        super().__init__(app)

        # Initialize API client
        self.api_client = create_api_client(app.get_session_data())

        # Forum state
        self.current_category = None
        self.current_post_uuid = None
        self.categories = []
        self.posts = []
        self.search_results = []
        self.forum_stats = {}

        # UI state
        self.loading = False
        self.search_query = ""

        # Bloomberg Terminal Colors
        self.BLOOMBERG_ORANGE = [255, 165, 0]
        self.BLOOMBERG_WHITE = [255, 255, 255]
        self.BLOOMBERG_RED = [255, 0, 0]
        self.BLOOMBERG_GREEN = [0, 200, 0]
        self.BLOOMBERG_YELLOW = [255, 255, 0]
        self.BLOOMBERG_GRAY = [120, 120, 120]
        self.BLOOMBERG_BLUE = [0, 128, 255]
        self.BLOOMBERG_PURPLE = [138, 43, 226]

    def get_label(self):
        return "🌐 Global Forum"

    def create_content(self):
        """Create forum tab content"""
        try:
            self.add_section_header("FINCEPT TERMINAL - GLOBAL FORUM")

            # Check authentication status
            if not self.api_client.is_authenticated():
                self.create_auth_error_panel()
                return

            # Create forum header with controls
            self.create_forum_header()
            dpg.add_spacer(height=10)

            # Create main forum layout
            with dpg.group(horizontal=True):
                # Left panel - Categories and Stats
                self.create_categories_panel()
                dpg.add_spacer(width=10)

                # Center panel - Posts List
                self.create_posts_panel()
                dpg.add_spacer(width=10)

                # Right panel - User Info and Actions
                self.create_user_panel()

            # Load initial data
            self.load_initial_data()

        except Exception as e:
            dpg.add_text(f"Error creating forum content: {str(e)}", color=self.BLOOMBERG_RED)

    def create_auth_error_panel(self):
        """Create panel when user is not authenticated"""
        with dpg.child_window(width=-1, height=400, border=True):
            dpg.add_spacer(height=50)

            with dpg.group(horizontal=True):
                dpg.add_spacer(width=300)
                dpg.add_text("🔒 Authentication Required", color=self.BLOOMBERG_YELLOW)

            dpg.add_spacer(height=20)

            with dpg.group(horizontal=True):
                dpg.add_spacer(width=250)
                dpg.add_text("Please authenticate to access the Global Forum", color=self.BLOOMBERG_WHITE)

            dpg.add_spacer(height=30)

            features = [
                "• Access to all forum categories",
                "• Create posts and comments",
                "• Vote on posts and comments",
                "• Real-time forum activity",
                "• User profiles and reputation"
            ]

            for feature in features:
                with dpg.group(horizontal=True):
                    dpg.add_spacer(width=300)
                    dpg.add_text(feature, color=self.BLOOMBERG_GRAY)
                dpg.add_spacer(height=5)

    def create_forum_header(self):
        """Create Bloomberg Terminal style forum header"""
        with dpg.group(horizontal=True):
            # Forum branding
            dpg.add_text("FINCEPT", color=self.BLOOMBERG_ORANGE)
            dpg.add_text("GLOBAL FORUM", color=self.BLOOMBERG_WHITE)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)

            # Search functionality
            dpg.add_input_text(
                label="",
                default_value="Search posts, topics...",
                width=300,
                tag="forum_search_input",
                callback=self.on_search_input_change
            )
            dpg.add_button(label="SEARCH", width=80, callback=self.execute_search)

            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)

            # User status
            user_type = self.api_client.get_user_info().get('user_type', 'guest')
            if user_type == "registered":
                user_info = self.api_client.get_user_info()
                username = user_info.get('username', 'User')
                dpg.add_text(f"USER: {username.upper()}", color=self.BLOOMBERG_GREEN)
            else:
                dpg.add_text("USER: GUEST", color=self.BLOOMBERG_YELLOW)

        # Function keys for forum actions
        dpg.add_spacer(height=10)
        with dpg.group(horizontal=True):
            function_keys = [
                ("F1:HELP", self.show_forum_help),
                ("F2:REFRESH", self.refresh_forum),
                ("F3:NEW POST", self.create_new_post),
                ("F4:SEARCH", self.focus_search),
                ("F5:TRENDING", self.show_trending),
                ("F6:PROFILE", self.show_user_profile)
            ]
            for key_label, callback in function_keys:
                dpg.add_button(label=key_label, width=100, height=25, callback=callback)

    def create_categories_panel(self):
        """Create left panel with categories"""
        with dpg.child_window(width=280, height=650, border=True, tag="categories_panel"):
            # Categories header
            dpg.add_text("FORUM CATEGORIES", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Category filters
            with dpg.group(horizontal=True):
                dpg.add_button(label="ALL", callback=lambda: self.filter_by_category(None), width=50, height=25)
                dpg.add_button(label="TRENDING", callback=self.show_trending_posts, width=70, height=25)
                dpg.add_button(label="RECENT", callback=self.show_recent_posts, width=60, height=25)

            dpg.add_separator()

            # Categories list
            dpg.add_child_window(height=350, border=False, tag="categories_list")

            dpg.add_separator()

            # Forum statistics
            dpg.add_text("FORUM STATISTICS", color=self.BLOOMBERG_YELLOW)
            dpg.add_text("Loading...", tag="stat_total_users", color=self.BLOOMBERG_WHITE)
            dpg.add_text("Loading...", tag="stat_total_posts", color=self.BLOOMBERG_WHITE)
            dpg.add_text("Loading...", tag="stat_total_comments", color=self.BLOOMBERG_WHITE)
            dpg.add_text("Loading...", tag="stat_online_users", color=self.BLOOMBERG_GREEN)

            dpg.add_separator()

            # Quick actions (only for registered users)
            if self.api_client.is_registered():
                dpg.add_text("QUICK ACTIONS", color=self.BLOOMBERG_YELLOW)
                dpg.add_button(label="CREATE NEW POST", callback=self.create_new_post, width=-1, height=30)
                dpg.add_button(label="MY POSTS", callback=self.show_my_posts, width=-1, height=25)
                dpg.add_button(label="MY ACTIVITY", callback=self.show_my_activity, width=-1, height=25)

    def create_posts_panel(self):
        """Create center panel with posts list"""
        with dpg.child_window(width=650, height=650, border=True, tag="posts_panel"):
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

            # Loading indicator
            dpg.add_text("Loading posts...", tag="posts_loading", color=self.BLOOMBERG_YELLOW, show=False)

            # Posts list area
            dpg.add_child_window(height=-1, border=False, tag="posts_list_area")

    def create_user_panel(self):
        """Create right panel with user info"""
        with dpg.child_window(width=300, height=650, border=True, tag="user_panel"):
            dpg.add_text("USER DASHBOARD", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Current user info
            self.create_user_info_section()

            dpg.add_separator()

            # User actions
            self.create_user_actions_section()

            dpg.add_separator()

            # Recent activity
            dpg.add_text("RECENT ACTIVITY", color=self.BLOOMBERG_YELLOW)
            dpg.add_child_window(height=-1, border=True, tag="recent_activity")

    def create_user_info_section(self):
        """Create user information section"""
        dpg.add_text("CURRENT USER", color=self.BLOOMBERG_YELLOW)

        user_info = self.api_client.get_user_info()

        if self.api_client.is_registered():
            username = user_info.get('username', 'User')
            credit_balance = user_info.get('credit_balance', 0)

            # User avatar (colored square)
            with dpg.group(horizontal=True):
                with dpg.drawlist(width=40, height=40):
                    dpg.draw_rectangle([0, 0], [40, 40], color=self.BLOOMBERG_GREEN, fill=self.BLOOMBERG_GREEN)
                    dpg.draw_text([15, 15], username[:2].upper(), color=self.BLOOMBERG_WHITE, size=12)

                with dpg.group():
                    dpg.add_text(username, color=self.BLOOMBERG_WHITE, tag="user_display_name")
                    dpg.add_text(f"Credits: {credit_balance}", color=self.BLOOMBERG_GREEN, tag="user_credits")
                    dpg.add_text("Status: Online", color=self.BLOOMBERG_GREEN, tag="user_status")
        else:
            # Guest user info
            with dpg.group(horizontal=True):
                with dpg.drawlist(width=40, height=40):
                    dpg.draw_rectangle([0, 0], [40, 40], color=self.BLOOMBERG_YELLOW, fill=self.BLOOMBERG_YELLOW)
                    dpg.draw_text([12, 15], "GU", color=self.BLOOMBERG_WHITE, size=12)

                with dpg.group():
                    dpg.add_text("Guest User", color=self.BLOOMBERG_WHITE, tag="user_display_name")
                    dpg.add_text("Limited Access", color=self.BLOOMBERG_YELLOW, tag="user_status")

    def create_user_actions_section(self):
        """Create user actions section"""
        dpg.add_text("USER ACTIONS", color=self.BLOOMBERG_YELLOW)

        if self.api_client.is_registered():
            user_actions = [
                ("VIEW PROFILE", self.show_user_profile),
                ("MY POSTS", self.show_my_posts),
                ("MY ACTIVITY", self.show_my_activity),
                ("SETTINGS", self.show_user_settings)
            ]
        else:
            user_actions = [
                ("VIEW POSTS", lambda: self.filter_by_category(None)),
                ("SEARCH FORUM", self.focus_search),
                ("UPGRADE ACCOUNT", self.show_upgrade_info)
            ]

        for action_name, callback in user_actions:
            dpg.add_button(label=action_name, callback=callback, width=-1, height=25)

    def create_category_item(self, category_data):
        """Create category item in the list"""
        category_id = category_data.get("id")
        name = category_data.get("name", "Unknown")
        description = category_data.get("description", "")
        post_count = category_data.get("post_count", 0)

        with dpg.group(parent="categories_list"):
            # Category button
            dpg.add_button(
                label=f"{name} ({post_count})",
                callback=lambda: self.filter_by_category(category_id),
                width=-1,
                height=35
            )

            # Category description
            desc_text = description[:35] + "..." if len(description) > 35 else description
            dpg.add_text(f"  {desc_text}", color=self.BLOOMBERG_GRAY)
            dpg.add_spacer(height=5)

    def create_post_item(self, post_data):
        """Create post item in the posts list"""
        try:
            post_uuid = post_data.get("post_uuid", "")
            title = post_data.get("title", "Untitled")
            content = post_data.get("content", "")
            author = post_data.get("author", {})
            username = author.get("username", "Unknown")
            vote_score = post_data.get("vote_score", 0)
            comment_count = post_data.get("comment_count", 0)
            view_count = post_data.get("view_count", 0)
            created_at = post_data.get("created_at", "")
            category_name = post_data.get("category", {}).get("name", "General")
            is_pinned = post_data.get("is_pinned", False)

            with dpg.group(parent="posts_list_area"):
                # Post container
                with dpg.child_window(width=-1, height=120, border=True):
                    # Post header
                    with dpg.group(horizontal=True):
                        # Pin indicator
                        if is_pinned:
                            dpg.add_text("📌", color=self.BLOOMBERG_ORANGE)

                        # Post title (clickable)
                        title_display = title[:50] + "..." if len(title) > 50 else title
                        dpg.add_button(
                            label=title_display,
                            callback=lambda p=post_uuid: self.view_post_details(p),
                            width=350,
                            height=25
                        )

                        # Category tag
                        dpg.add_text(f"[{category_name}]", color=self.BLOOMBERG_BLUE)

                    # Post stats
                    with dpg.group(horizontal=True):
                        dpg.add_text(f"👁 {view_count}", color=self.BLOOMBERG_GRAY)
                        dpg.add_spacer(width=15)

                        vote_color = self.BLOOMBERG_GREEN if vote_score > 0 else self.BLOOMBERG_RED if vote_score < 0 else self.BLOOMBERG_GRAY
                        dpg.add_text(f"👍 {vote_score}", color=vote_color)
                        dpg.add_spacer(width=15)

                        dpg.add_text(f"💬 {comment_count}", color=self.BLOOMBERG_BLUE)
                        dpg.add_spacer(width=15)

                        dpg.add_text(f"By: {username}", color=self.BLOOMBERG_WHITE)
                        dpg.add_spacer(width=15)

                        # Format timestamp
                        try:
                            if created_at:
                                dt = datetime.fromisoformat(created_at.replace('Z', '+00:00'))
                                time_str = dt.strftime("%m/%d %H:%M")
                            else:
                                time_str = "Recent"
                        except:
                            time_str = "Recent"

                        dpg.add_text(time_str, color=self.BLOOMBERG_GRAY)

                    # Post preview
                    preview = content[:80] + "..." if len(content) > 80 else content
                    dpg.add_text(preview, color=self.BLOOMBERG_WHITE, wrap=600)

                    # Action buttons
                    with dpg.group(horizontal=True):
                        dpg.add_button(
                            label="VIEW",
                            callback=lambda p=post_uuid: self.view_post_details(p),
                            width=60,
                            height=20
                        )

                        if self.api_client.is_registered():
                            dpg.add_button(
                                label="VOTE",
                                callback=lambda p=post_uuid: self.vote_on_post(p, "up"),
                                width=60,
                                height=20
                            )
                            dpg.add_button(
                                label="REPLY",
                                callback=lambda p=post_uuid: self.reply_to_post(p),
                                width=60,
                                height=20
                            )

                dpg.add_spacer(height=5)

        except Exception as e:
            print(f"Error creating post item: {e}")

    # API Integration Methods
    def load_initial_data(self):
        """Load initial forum data from API"""

        def load_data_thread():
            try:
                self.set_loading(True)

                # Load categories
                self.load_categories()

                # Load forum statistics
                self.load_forum_stats()

                # Load initial posts
                self.load_posts()

                self.set_loading(False)
            except Exception as e:
                print(f"Error loading initial data: {e}")
                self.set_loading(False)

        thread = threading.Thread(target=load_data_thread, daemon=True)
        thread.start()

    def load_categories(self):
        """Load categories from API"""
        try:
            result = self.api_client.get_categories()
            if result["success"]:
                self.categories = result.get("categories", [])

                # Update UI on main thread
                dpg.set_value("posts_loading", "Loading categories...")

                # Clear existing categories
                if dpg.does_item_exist("categories_list"):
                    dpg.delete_item("categories_list", children_only=True)

                # Add category items
                for category in self.categories:
                    self.create_category_item(category)

            else:
                print(f"Failed to load categories: {result.get('error', 'Unknown error')}")
        except Exception as e:
            print(f"Error loading categories: {e}")

    def load_posts(self, category_id=None):
        """Load posts from API"""
        try:
            if dpg.does_item_exist("posts_loading"):
                dpg.show_item("posts_loading")
                dpg.set_value("posts_loading", "Loading posts...")

            # Get posts based on current filter
            if category_id:
                result = self.api_client.get_category_posts(category_id)
            else:
                result = self.api_client.get_all_posts()

            if result["success"]:
                self.posts = result.get("posts", [])

                # Clear existing posts
                if dpg.does_item_exist("posts_list_area"):
                    dpg.delete_item("posts_list_area", children_only=True)

                # Add post items
                for post in self.posts:
                    self.create_post_item(post)

                # Update category name
                if category_id and self.categories:
                    category_name = next(
                        (cat["name"] for cat in self.categories if cat["id"] == category_id),
                        "Unknown Category"
                    )
                    if dpg.does_item_exist("current_category_name"):
                        dpg.set_value("current_category_name", category_name)
                else:
                    if dpg.does_item_exist("current_category_name"):
                        dpg.set_value("current_category_name", "All Categories")
            else:
                print(f"Failed to load posts: {result.get('error', 'Unknown error')}")

            if dpg.does_item_exist("posts_loading"):
                dpg.hide_item("posts_loading")

        except Exception as e:
            print(f"Error loading posts: {e}")
            if dpg.does_item_exist("posts_loading"):
                dpg.hide_item("posts_loading")

    def load_forum_stats(self):
        """Load forum statistics"""
        try:
            result = self.api_client.get_forum_stats()
            if result["success"]:
                stats = result.get("stats", {})

                if dpg.does_item_exist("stat_total_users"):
                    dpg.set_value("stat_total_users", f"Total Users: {stats.get('total_users', 0)}")
                if dpg.does_item_exist("stat_total_posts"):
                    dpg.set_value("stat_total_posts", f"Total Posts: {stats.get('total_posts', 0)}")
                if dpg.does_item_exist("stat_total_comments"):
                    dpg.set_value("stat_total_comments", f"Total Comments: {stats.get('total_comments', 0)}")
                if dpg.does_item_exist("stat_online_users"):
                    dpg.set_value("stat_online_users", f"Online Now: {stats.get('online_users', 0)}")

        except Exception as e:
            print(f"Error loading forum stats: {e}")

    def set_loading(self, loading):
        """Set loading state"""
        self.loading = loading

    # Event Handlers
    def filter_by_category(self, category_id):
        """Filter posts by category"""
        self.current_category = category_id

        def load_category_posts():
            self.load_posts(category_id)

        thread = threading.Thread(target=load_category_posts, daemon=True)
        thread.start()

    def execute_search(self):
        """Execute search"""
        try:
            search_term = dpg.get_value("forum_search_input") if dpg.does_item_exist("forum_search_input") else ""

            if not search_term or search_term == "Search posts, topics...":
                return

            def search_thread():
                try:
                    if dpg.does_item_exist("posts_loading"):
                        dpg.show_item("posts_loading")
                        dpg.set_value("posts_loading", f"Searching for '{search_term}'...")

                    result = self.api_client.search_posts(search_term)

                    if result["success"]:
                        posts = result.get("posts", [])

                        # Clear and update posts list
                        if dpg.does_item_exist("posts_list_area"):
                            dpg.delete_item("posts_list_area", children_only=True)

                        for post in posts:
                            self.create_post_item(post)

                        if dpg.does_item_exist("current_category_name"):
                            dpg.set_value("current_category_name", f"Search: '{search_term}'")

                    if dpg.does_item_exist("posts_loading"):
                        dpg.hide_item("posts_loading")

                except Exception as e:
                    print(f"Search error: {e}")
                    if dpg.does_item_exist("posts_loading"):
                        dpg.hide_item("posts_loading")

            thread = threading.Thread(target=search_thread, daemon=True)
            thread.start()

        except Exception as e:
            print(f"Error executing search: {e}")

    def on_search_input_change(self, sender, app_data):
        """Handle search input changes"""
        # Could implement real-time search here
        pass

    def sort_posts_callback(self, sender, app_data):
        """Handle post sorting"""
        # For now, just reload posts
        # In a full implementation, you'd pass sort parameters to API
        self.load_posts(self.current_category)

    def view_post_details(self, post_uuid):
        """View detailed post"""
        try:
            if not dpg.does_item_exist("post_detail_window"):
                with dpg.window(
                        label="POST DETAILS",
                        tag="post_detail_window",
                        width=800,
                        height=600,
                        pos=[200, 100],
                        modal=True
                ):
                    dpg.add_text("Loading post details...", tag="post_detail_content", color=self.BLOOMBERG_YELLOW)

                    dpg.add_separator()

                    with dpg.group(horizontal=True):
                        dpg.add_button(label="CLOSE", callback=lambda: dpg.delete_item("post_detail_window"), width=100)

                        if self.api_client.is_registered():
                            dpg.add_spacer(width=20)
                            dpg.add_input_text(hint="Add a comment...", width=400, tag="new_comment_input")
                            dpg.add_button(label="POST COMMENT", callback=lambda: self.add_comment(post_uuid),
                                           width=120)

            # Load post details
            def load_post_details():
                try:
                    result = self.api_client.get_post_details(post_uuid)
                    if result["success"]:
                        post = result.get("post", {})
                        comments = result.get("comments", [])

                        # Update post details content
                        if dpg.does_item_exist("post_detail_content"):
                            dpg.set_value("post_detail_content",
                                          f"Title: {post.get('title', 'N/A')}\n\nContent: {post.get('content', 'N/A')}")

                except Exception as e:
                    print(f"Error loading post details: {e}")

            thread = threading.Thread(target=load_post_details, daemon=True)
            thread.start()

            dpg.show_item("post_detail_window")

        except Exception as e:
            print(f"Error viewing post details: {e}")

    def vote_on_post(self, post_uuid, vote_type):
        """Vote on a post"""
        if not self.api_client.is_registered():
            print("Voting requires registration")
            return

        def vote_thread():
            try:
                result = self.api_client.vote_on_post(post_uuid, vote_type)
                if result["success"]:
                    print(f"Vote submitted successfully")
                    # Refresh posts to show updated vote count
                    self.load_posts(self.current_category)
                else:
                    print(f"Vote failed: {result.get('error', 'Unknown error')}")
            except Exception as e:
                print(f"Error voting: {e}")

        thread = threading.Thread(target=vote_thread, daemon=True)
        thread.start()

    def reply_to_post(self, post_uuid):
        """Reply to a post"""
        self.view_post_details(post_uuid)

    def add_comment(self, post_uuid):
        """Add comment to post"""
        if not self.api_client.is_registered():
            return

        comment_text = dpg.get_value("new_comment_input") if dpg.does_item_exist("new_comment_input") else ""

        if not comment_text.strip():
            return

        def add_comment_thread():
            try:
                result = self.api_client.add_comment_to_post(post_uuid, comment_text.strip())
                if result["success"]:
                    print("Comment added successfully")
                    if dpg.does_item_exist("new_comment_input"):
                        dpg.set_value("new_comment_input", "")
                else:
                    print(f"Comment failed: {result.get('error', 'Unknown error')}")
            except Exception as e:
                print(f"Error adding comment: {e}")

        thread = threading.Thread(target=add_comment_thread, daemon=True)
        thread.start()

    def create_new_post(self):
        """Create new post dialog"""
        if not self.api_client.is_registered():
            print("Creating posts requires registration")
            return

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
                category_names = [cat["name"] for cat in self.categories]
                if category_names:
                    dpg.add_combo(category_names, width=200, tag="new_post_category")

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
        """Submit new post to API"""
        try:
            title = dpg.get_value("new_post_title").strip() if dpg.does_item_exist("new_post_title") else ""
            content = dpg.get_value("new_post_content").strip() if dpg.does_item_exist("new_post_content") else ""
            category_name = dpg.get_value("new_post_category") if dpg.does_item_exist("new_post_category") else ""

            if not title or not content:
                print("Please fill in all fields")
                return

            # Find category ID
            category_id = None
            for cat in self.categories:
                if cat["name"] == category_name:
                    category_id = cat["id"]
                    break

            if not category_id:
                print("Please select a valid category")
                return

            def submit_thread():
                try:
                    result = self.api_client.create_post(title, content, category_id)
                    if result["success"]:
                        print("Post created successfully")

                        # Close dialog and refresh
                        if dpg.does_item_exist("new_post_window"):
                            dpg.delete_item("new_post_window")

                        # Refresh posts and categories
                        self.load_posts(self.current_category)
                        self.load_categories()

                    else:
                        print(f"Post creation failed: {result.get('error', 'Unknown error')}")

                except Exception as e:
                    print(f"Error creating post: {e}")

            thread = threading.Thread(target=submit_thread, daemon=True)
            thread.start()

        except Exception as e:
            print(f"Error submitting post: {e}")

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
                dpg.add_text("F2: Refresh - Refresh forum content")
                dpg.add_text("F3: New Post - Create new forum post (Registered users)")
                dpg.add_text("F4: Search - Focus search field")
                dpg.add_text("F5: Trending - Show trending posts")
                dpg.add_text("F6: Profile - Show user profile")

                dpg.add_separator()

                dpg.add_text("FORUM FEATURES:", color=self.BLOOMBERG_YELLOW)
                dpg.add_text("• Browse posts by category")
                dpg.add_text("• Search across all content")
                dpg.add_text("• View post details and comments")
                dpg.add_text("• Create posts and comments (Registered users)")
                dpg.add_text("• Vote on posts and comments (Registered users)")
                dpg.add_text("• Real-time forum statistics")

                dpg.add_separator()

                if not self.api_client.is_registered():
                    dpg.add_text("GUEST LIMITATIONS:", color=self.BLOOMBERG_RED)
                    dpg.add_text("• Read-only access to posts")
                    dpg.add_text("• Cannot create posts or comments")
                    dpg.add_text("• Cannot vote on content")
                    dpg.add_text("• Register for full access!")

                dpg.add_spacer(height=20)
                dpg.add_button(label="CLOSE", callback=lambda: dpg.delete_item("forum_help_window"))
        else:
            dpg.show_item("forum_help_window")

    def refresh_forum(self):
        """Refresh entire forum"""

        def refresh_thread():
            try:
                if dpg.does_item_exist("posts_loading"):
                    dpg.show_item("posts_loading")
                    dpg.set_value("posts_loading", "Refreshing forum...")

                self.load_categories()
                self.load_posts(self.current_category)
                self.load_forum_stats()

                if dpg.does_item_exist("posts_loading"):
                    dpg.hide_item("posts_loading")

                print("Forum refreshed successfully")

            except Exception as e:
                print(f"Error refreshing forum: {e}")
                if dpg.does_item_exist("posts_loading"):
                    dpg.hide_item("posts_loading")

        thread = threading.Thread(target=refresh_thread, daemon=True)
        thread.start()

    def focus_search(self):
        """Focus search field"""
        if dpg.does_item_exist("forum_search_input"):
            dpg.focus_item("forum_search_input")
            dpg.set_value("forum_search_input", "")

    def show_trending(self):
        """Show trending posts"""

        def load_trending():
            try:
                if dpg.does_item_exist("posts_loading"):
                    dpg.show_item("posts_loading")
                    dpg.set_value("posts_loading", "Loading trending posts...")

                result = self.api_client.get_trending_posts()
                if result["success"]:
                    posts = result.get("posts", [])

                    # Clear and update posts list
                    if dpg.does_item_exist("posts_list_area"):
                        dpg.delete_item("posts_list_area", children_only=True)

                    for post in posts:
                        self.create_post_item(post)

                    if dpg.does_item_exist("current_category_name"):
                        dpg.set_value("current_category_name", "Trending Posts")

                if dpg.does_item_exist("posts_loading"):
                    dpg.hide_item("posts_loading")

            except Exception as e:
                print(f"Error loading trending posts: {e}")
                if dpg.does_item_exist("posts_loading"):
                    dpg.hide_item("posts_loading")

        thread = threading.Thread(target=load_trending, daemon=True)
        thread.start()

    def show_trending_posts(self):
        """Show trending posts (button callback)"""
        self.show_trending()

    def show_recent_posts(self):
        """Show recent posts"""

        def load_recent():
            try:
                if dpg.does_item_exist("posts_loading"):
                    dpg.show_item("posts_loading")
                    dpg.set_value("posts_loading", "Loading recent posts...")

                # Load all posts sorted by recent
                result = self.api_client.get_all_posts(sort="recent")
                if result["success"]:
                    posts = result.get("posts", [])

                    # Clear and update posts list
                    if dpg.does_item_exist("posts_list_area"):
                        dpg.delete_item("posts_list_area", children_only=True)

                    for post in posts:
                        self.create_post_item(post)

                    if dpg.does_item_exist("current_category_name"):
                        dpg.set_value("current_category_name", "Recent Posts")

                if dpg.does_item_exist("posts_loading"):
                    dpg.hide_item("posts_loading")

            except Exception as e:
                print(f"Error loading recent posts: {e}")
                if dpg.does_item_exist("posts_loading"):
                    dpg.hide_item("posts_loading")

        thread = threading.Thread(target=load_recent, daemon=True)
        thread.start()

    def show_user_profile(self):
        """Show user profile"""
        if not self.api_client.is_registered():
            self.show_upgrade_info()
            return

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

                # Loading profile
                dpg.add_text("Loading profile...", tag="profile_content", color=self.BLOOMBERG_YELLOW)

                dpg.add_spacer(height=20)
                dpg.add_button(label="CLOSE", callback=lambda: dpg.delete_item("user_profile_window"))

        # Load profile data
        def load_profile():
            try:
                result = self.api_client.get_user_profile()
                if result["success"]:
                    profile = result.get("profile", {})

                    profile_text = f"Username: {profile.get('username', 'N/A')}\n"
                    profile_text += f"Email: {profile.get('email', 'N/A')}\n"
                    profile_text += f"Credits: {profile.get('credit_balance', 0)}\n"
                    profile_text += f"Posts: {profile.get('post_count', 0)}\n"
                    profile_text += f"Comments: {profile.get('comment_count', 0)}\n"
                    profile_text += f"Joined: {profile.get('created_at', 'N/A')}"

                    if dpg.does_item_exist("profile_content"):
                        dpg.set_value("profile_content", profile_text)

            except Exception as e:
                print(f"Error loading profile: {e}")

        thread = threading.Thread(target=load_profile, daemon=True)
        thread.start()

        dpg.show_item("user_profile_window")

    def show_my_posts(self):
        """Show current user's posts"""
        if not self.api_client.is_registered():
            return

        def load_my_posts():
            try:
                if dpg.does_item_exist("posts_loading"):
                    dpg.show_item("posts_loading")
                    dpg.set_value("posts_loading", "Loading your posts...")

                result = self.api_client.get_user_posts()
                if result["success"]:
                    posts = result.get("posts", [])

                    # Clear and update posts list
                    if dpg.does_item_exist("posts_list_area"):
                        dpg.delete_item("posts_list_area", children_only=True)

                    for post in posts:
                        self.create_post_item(post)

                    if dpg.does_item_exist("current_category_name"):
                        dpg.set_value("current_category_name", "My Posts")

                if dpg.does_item_exist("posts_loading"):
                    dpg.hide_item("posts_loading")

            except Exception as e:
                print(f"Error loading user posts: {e}")
                if dpg.does_item_exist("posts_loading"):
                    dpg.hide_item("posts_loading")

        thread = threading.Thread(target=load_my_posts, daemon=True)
        thread.start()

    def show_my_activity(self):
        """Show user activity"""
        if not self.api_client.is_registered():
            return

        def load_activity():
            try:
                result = self.api_client.get_user_activity()
                if result["success"]:
                    activities = result.get("activities", [])

                    # Update recent activity panel
                    if dpg.does_item_exist("recent_activity"):
                        dpg.delete_item("recent_activity", children_only=True)

                        with dpg.group(parent="recent_activity"):
                            for activity in activities[:10]:  # Show last 10 activities
                                activity_text = f"• {activity.get('description', 'Activity')}"
                                dpg.add_text(activity_text, color=self.BLOOMBERG_WHITE)

                            if not activities:
                                dpg.add_text("No recent activity", color=self.BLOOMBERG_GRAY)

            except Exception as e:
                print(f"Error loading user activity: {e}")

        thread = threading.Thread(target=load_activity, daemon=True)
        thread.start()

    def show_user_settings(self):
        """Show user settings"""
        if not self.api_client.is_registered():
            return

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

                user_info = self.api_client.get_user_info()

                dpg.add_text("Display Name:")
                dpg.add_input_text(default_value=user_info.get("username", ""), width=300, tag="settings_display_name")

                dpg.add_text("Bio:")
                dpg.add_input_text(default_value="", width=300, multiline=True, height=60, tag="settings_bio")

                dpg.add_text("Avatar Color:")
                dpg.add_color_edit(default_value=[255, 165, 0, 255], width=200, tag="settings_avatar_color")

                dpg.add_spacer(height=20)

                with dpg.group(horizontal=True):
                    dpg.add_button(label="SAVE", width=100, callback=self.save_user_settings)
                    dpg.add_spacer(width=20)
                    dpg.add_button(label="CANCEL", callback=lambda: dpg.delete_item("user_settings_window"), width=100)
        else:
            dpg.show_item("user_settings_window")

    def save_user_settings(self):
        """Save user settings"""
        try:
            display_name = dpg.get_value("settings_display_name") if dpg.does_item_exist(
                "settings_display_name") else ""
            bio = dpg.get_value("settings_bio") if dpg.does_item_exist("settings_bio") else ""
            avatar_color = dpg.get_value("settings_avatar_color") if dpg.does_item_exist("settings_avatar_color") else [
                255, 165, 0, 255]

            # Convert color to hex
            color_hex = f"#{int(avatar_color[0]):02x}{int(avatar_color[1]):02x}{int(avatar_color[2]):02x}"

            def save_settings_thread():
                try:
                    result = self.api_client.update_user_profile({
                        "display_name": display_name,
                        "bio": bio,
                        "avatar_color": color_hex
                    })

                    if result["success"]:
                        print("Settings saved successfully")
                        if dpg.does_item_exist("user_settings_window"):
                            dpg.delete_item("user_settings_window")
                    else:
                        print(f"Settings save failed: {result.get('error', 'Unknown error')}")

                except Exception as e:
                    print(f"Error saving settings: {e}")

            thread = threading.Thread(target=save_settings_thread, daemon=True)
            thread.start()

        except Exception as e:
            print(f"Error saving user settings: {e}")

    def show_upgrade_info(self):
        """Show upgrade information for guests"""
        if not dpg.does_item_exist("upgrade_info_window"):
            with dpg.window(
                    label="UPGRADE TO FULL ACCESS",
                    tag="upgrade_info_window",
                    width=500,
                    height=400,
                    pos=[350, 200]
            ):
                dpg.add_text("🚀 UPGRADE YOUR ACCOUNT", color=self.BLOOMBERG_ORANGE)
                dpg.add_separator()

                dpg.add_text("Get full forum access with a registered account:", color=self.BLOOMBERG_WHITE)
                dpg.add_spacer(height=10)

                features = [
                    "✅ Create unlimited posts and comments",
                    "✅ Vote on posts and comments",
                    "✅ Access to all forum categories",
                    "✅ User profile and activity tracking",
                    "✅ Direct messaging with other users",
                    "✅ Priority support and features"
                ]

                for feature in features:
                    dpg.add_text(feature, color=self.BLOOMBERG_GREEN)
                    dpg.add_spacer(height=5)

                dpg.add_spacer(height=20)

                dpg.add_text("Visit the Profile tab to create an account or sign in.", color=self.BLOOMBERG_YELLOW)

                dpg.add_spacer(height=20)
                dpg.add_button(label="CLOSE", callback=lambda: dpg.delete_item("upgrade_info_window"))
        else:
            dpg.show_item("upgrade_info_window")

    def cleanup(self):
        """Clean up forum resources"""
        try:
            # Clear data
            self.categories = []
            self.posts = []
            self.search_results = []
            self.forum_stats = {}

            # API client cleanup would be handled by the main app
            print("Forum tab cleaned up")

        except Exception as e:
            print(f"Error cleaning up forum tab: {e}")

    # Resize support
    def resize_components(self, left_width, center_width, right_width, top_height, bottom_height, cell_height):
        """Resize forum components"""
        try:
            # Update panel widths
            if dpg.does_item_exist("categories_panel"):
                dpg.configure_item("categories_panel", width=left_width)

            if dpg.does_item_exist("posts_panel"):
                dpg.configure_item("posts_panel", width=center_width)

            if dpg.does_item_exist("user_panel"):
                dpg.configure_item("user_panel", width=right_width)

        except Exception as e:
            print(f"Error resizing forum components: {e}")


# Additional API methods that need to be added to the API client
class ForumAPIExtensions:
    """
    Additional API methods needed for the forum tab.
    These should be added to the main API client.
    """

    @staticmethod
    def get_categories(api_client):
        """Get forum categories"""
        return api_client.make_request("GET", "/forum/categories")

    @staticmethod
    def get_all_posts(api_client, sort="latest"):
        """Get all posts"""
        params = {"sort": sort}
        return api_client.make_request("GET", "/forum/posts", params=params)

    @staticmethod
    def get_category_posts(api_client, category_id):
        """Get posts for specific category"""
        return api_client.make_request("GET", f"/forum/categories/{category_id}/posts")

    @staticmethod
    def get_post_details(api_client, post_uuid):
        """Get detailed post information"""
        return api_client.make_request("GET", f"/forum/posts/{post_uuid}")

    @staticmethod
    def get_forum_stats(api_client):
        """Get forum statistics"""
        return api_client.make_request("GET", "/forum/stats")

    @staticmethod
    def search_posts(api_client, query):
        """Search posts"""
        params = {"q": query, "post_type": "all"}
        return api_client.make_request("GET", "/forum/search", params=params)

    @staticmethod
    def get_trending_posts(api_client, timeframe="week"):
        """Get trending posts"""
        params = {"timeframe": timeframe}
        return api_client.make_request("GET", "/forum/posts/trending", params=params)

    @staticmethod
    def create_post(api_client, title, content, category_id):
        """Create new post"""
        data = {"title": title, "content": content}
        return api_client.make_request("POST", f"/forum/categories/{category_id}/posts", data=data)

    @staticmethod
    def vote_on_post(api_client, post_uuid, vote_type):
        """Vote on post"""
        data = {"vote_type": vote_type}
        return api_client.make_request("POST", f"/forum/posts/{post_uuid}/vote", data=data)

    @staticmethod
    def add_comment_to_post(api_client, post_uuid, content):
        """Add comment to post"""
        data = {"content": content}
        return api_client.make_request("POST", f"/forum/posts/{post_uuid}/comments", data=data)

    @staticmethod
    def get_user_posts(api_client):
        """Get current user's posts"""
        return api_client.make_request("GET", "/forum/user/posts")

    @staticmethod
    def get_user_activity(api_client):
        """Get user activity"""
        return api_client.make_request("GET", "/forum/user/activity")

    @staticmethod
    def update_user_profile(api_client, profile_data):
        """Update user profile"""
        return api_client.make_request("PUT", "/forum/profile", data=profile_data)


# Monkey patch the API client to add forum methods
def add_forum_methods_to_api_client():
    """Add forum methods to the API client class"""
    from fincept_terminal.Utils.APIClient.api_client import FinceptAPIClient

    # Add methods to the API client
    FinceptAPIClient.get_categories = lambda self: ForumAPIExtensions.get_categories(self)
    FinceptAPIClient.get_all_posts = lambda self, sort="latest": ForumAPIExtensions.get_all_posts(self, sort)
    FinceptAPIClient.get_category_posts = lambda self, category_id: ForumAPIExtensions.get_category_posts(self,
                                                                                                          category_id)
    FinceptAPIClient.get_post_details = lambda self, post_uuid: ForumAPIExtensions.get_post_details(self, post_uuid)
    FinceptAPIClient.get_forum_stats = lambda self: ForumAPIExtensions.get_forum_stats(self)
    FinceptAPIClient.search_posts = lambda self, query: ForumAPIExtensions.search_posts(self, query)
    FinceptAPIClient.get_trending_posts = lambda self, timeframe="week": ForumAPIExtensions.get_trending_posts(self,
                                                                                                               timeframe)
    FinceptAPIClient.create_post = lambda self, title, content, category_id: ForumAPIExtensions.create_post(self, title,
                                                                                                            content,
                                                                                                            category_id)
    FinceptAPIClient.vote_on_post = lambda self, post_uuid, vote_type: ForumAPIExtensions.vote_on_post(self, post_uuid,
                                                                                                       vote_type)
    FinceptAPIClient.add_comment_to_post = lambda self, post_uuid, content: ForumAPIExtensions.add_comment_to_post(self,
                                                                                                                   post_uuid,
                                                                                                                   content)
    FinceptAPIClient.get_user_posts = lambda self: ForumAPIExtensions.get_user_posts(self)
    FinceptAPIClient.get_user_activity = lambda self: ForumAPIExtensions.get_user_activity(self)
    FinceptAPIClient.update_user_profile = lambda self, profile_data: ForumAPIExtensions.update_user_profile(self,
                                                                                                             profile_data)


# Call this when the module is imported
add_forum_methods_to_api_client()