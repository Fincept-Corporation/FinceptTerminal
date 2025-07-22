# forum_tab.py - Clean Forum Tab for Fincept Terminal
import dearpygui.dearpygui as dpg
import threading
import time
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
        return "Forum"

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
            if self.api_client.is_registered():
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
            dpg.add_text("Loading...", tag="stat_total_categories", color=self.BLOOMBERG_WHITE)
            dpg.add_text("Loading...", tag="stat_total_posts", color=self.BLOOMBERG_WHITE)
            dpg.add_text("Loading...", tag="stat_total_comments", color=self.BLOOMBERG_WHITE)
            dpg.add_text("Loading...", tag="stat_total_votes", color=self.BLOOMBERG_GREEN)

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
                    ["latest", "popular", "views", "replies"],
                    default_value="latest",
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

        if self.api_client.is_registered():
            user_info = self.api_client.get_user_info()
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
            # Extract post data
            post_id = post_data.get("id", "")
            post_uuid = post_data.get("post_uuid", "")
            title = post_data.get("title", "Untitled")
            content = post_data.get("content", "")
            author_name = post_data.get("author_name", "Unknown")
            author_display_name = post_data.get("author_display_name", author_name)
            likes = post_data.get("likes", 0)
            dislikes = post_data.get("dislikes", 0)
            reply_count = post_data.get("reply_count", 0)
            views = post_data.get("views", 0)
            created_at = post_data.get("created_at", "")
            category_name = post_data.get("category_name", "General")
            is_pinned = post_data.get("is_pinned", False)

            # Validate UUID
            if not post_uuid:
                return

            # Calculate vote score
            vote_score = likes - dislikes

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

                        def view_callback():
                            self.view_post_details(post_uuid)

                        dpg.add_button(
                            label=title_display,
                            callback=view_callback,
                            width=350,
                            height=25
                        )

                        # Category tag
                        dpg.add_text(f"[{category_name}]", color=self.BLOOMBERG_BLUE)

                    # Post stats
                    with dpg.group(horizontal=True):
                        dpg.add_text(f"👁 {views}", color=self.BLOOMBERG_GRAY)
                        dpg.add_spacer(width=15)

                        vote_color = self.BLOOMBERG_GREEN if vote_score > 0 else self.BLOOMBERG_RED if vote_score < 0 else self.BLOOMBERG_GRAY
                        dpg.add_text(f"👍 {vote_score}", color=vote_color)
                        dpg.add_spacer(width=15)

                        dpg.add_text(f"💬 {reply_count}", color=self.BLOOMBERG_BLUE)
                        dpg.add_spacer(width=15)

                        dpg.add_text(f"By: {author_display_name}", color=self.BLOOMBERG_WHITE)
                        dpg.add_spacer(width=15)

                        # Format timestamp
                        try:
                            if created_at:
                                if created_at.endswith('Z'):
                                    dt = datetime.fromisoformat(created_at.replace('Z', '+00:00'))
                                else:
                                    dt = datetime.fromisoformat(created_at)
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
                        def view_button_callback():
                            self.view_post_details(post_uuid)

                        dpg.add_button(
                            label="VIEW",
                            callback=view_button_callback,
                            width=60,
                            height=20
                        )

                        if self.api_client.is_registered():
                            def vote_up_callback():
                                self.vote_on_post(post_uuid, "up")

                            def vote_down_callback():
                                self.vote_on_post(post_uuid, "down")

                            def reply_callback():
                                self.reply_to_post(post_uuid)

                            dpg.add_button(
                                label="👍 UP",
                                callback=vote_up_callback,
                                width=70,
                                height=20
                            )

                            dpg.add_button(
                                label="👎 DOWN",
                                callback=vote_down_callback,
                                width=70,
                                height=20
                            )

                            dpg.add_button(
                                label="REPLY",
                                callback=reply_callback,
                                width=60,
                                height=20
                            )

                dpg.add_spacer(height=5)

        except Exception:
            pass

    def vote_on_post(self, post_identifier, vote_type):
        """Vote on a post with user-friendly popups"""
        if not self.api_client.is_registered():
            self.show_notification("❌ Registration Required", "You must be registered to vote on posts.", "error")
            return

        def vote_thread():
            try:
                # Validate identifier
                if not post_identifier:
                    self.show_notification("❌ Error", "Invalid post identifier.", "error")
                    return

                if isinstance(post_identifier, int):
                    self.show_notification("❌ Error", "Post identifier error. Please refresh and try again.", "error")
                    return

                if not isinstance(post_identifier, str):
                    self.show_notification("❌ Error", "Invalid post format. Please refresh and try again.", "error")
                    return

                if len(post_identifier) < 30:
                    self.show_notification("❌ Error", "Invalid post UUID format.", "error")
                    return

                # Make the API request
                result = self.api_client.make_request("POST", f"/forum/posts/{post_identifier}/vote",
                                                      data={"vote_type": vote_type})

                if result["success"] and result["data"].get("success"):
                    vote_action = result["data"]["data"].get("action", "added")

                    if vote_action == "added":
                        self.show_notification("✅ Vote Recorded", f"Your {vote_type}vote has been added!", "success")
                    elif vote_action == "removed":
                        self.show_notification("🔄 Vote Removed", f"Your {vote_type}vote has been removed.", "info")
                    elif vote_action == "changed":
                        old_type = result["data"]["data"].get("old_vote_type", "")
                        new_type = result["data"]["data"].get("new_vote_type", "")
                        self.show_notification("🔄 Vote Changed", f"Changed from {old_type}vote to {new_type}vote.", "info")

                    # Refresh posts to show updated vote count
                    self.load_posts(self.current_category)
                    # Also reload categories to update counts
                    self.load_categories()
                else:
                    error_msg = "Unknown error"
                    if result.get("data") and isinstance(result["data"], dict):
                        error_msg = result["data"].get("message", "Unknown error")
                    elif result.get("error"):
                        error_msg = result["error"]

                    # Handle specific error cases with user-friendly messages
                    if "Cannot vote on your own post" in error_msg:
                        self.show_notification("🚫 Cannot Vote", "You cannot vote on your own posts.", "warning")
                    elif "Post not found" in error_msg:
                        self.show_notification("❌ Post Not Found", "This post no longer exists or has been removed.", "error")
                    elif "already voted" in error_msg.lower():
                        self.show_notification("ℹ️ Already Voted", "You have already voted on this post.", "info")
                    else:
                        self.show_notification("❌ Vote Failed", f"Voting failed: {error_msg}", "error")

            except Exception:
                self.show_notification("❌ Network Error", "Unable to submit vote. Please check your connection and try again.", "error")

        thread = threading.Thread(target=vote_thread, daemon=True)
        thread.start()

    def show_notification(self, title, message, notification_type="info"):
        """Show a user-friendly notification popup"""
        try:
            # Generate unique window ID
            import time
            window_id = f"notification_{int(time.time() * 1000)}"

            # Choose colors based on notification type
            if notification_type == "success":
                title_color = self.BLOOMBERG_GREEN
                bg_color = [0, 50, 0, 200]
            elif notification_type == "error":
                title_color = self.BLOOMBERG_RED
                bg_color = [50, 0, 0, 200]
            elif notification_type == "warning":
                title_color = self.BLOOMBERG_YELLOW
                bg_color = [50, 50, 0, 200]
            else:  # info
                title_color = self.BLOOMBERG_BLUE
                bg_color = [0, 0, 50, 200]

            # Create notification window
            with dpg.window(
                    label=title,
                    tag=window_id,
                    width=400,
                    height=150,
                    pos=[400, 100],
                    modal=True,
                    no_resize=True,
                    no_move=True,
                    no_collapse=True
            ):
                # Add some styling
                with dpg.theme() as notification_theme:
                    with dpg.theme_component(dpg.mvAll):
                        dpg.add_theme_color(dpg.mvThemeCol_WindowBg, bg_color, category=dpg.mvThemeCat_Core)

                dpg.bind_item_theme(window_id, notification_theme)

                dpg.add_spacer(height=10)

                # Title
                dpg.add_text(title, color=title_color)
                dpg.add_separator()
                dpg.add_spacer(height=10)

                # Message
                dpg.add_text(message, color=self.BLOOMBERG_WHITE, wrap=350)

                dpg.add_spacer(height=20)

                # Close button
                with dpg.group(horizontal=True):
                    dpg.add_spacer(width=150)
                    dpg.add_button(
                        label="OK",
                        callback=lambda: dpg.delete_item(window_id),
                        width=100,
                        height=30
                    )

            # Auto-close after 5 seconds for success/info notifications
            if notification_type in ["success", "info"]:
                def auto_close():
                    import time
                    time.sleep(5)
                    if dpg.does_item_exist(window_id):
                        dpg.delete_item(window_id)

                thread = threading.Thread(target=auto_close, daemon=True)
                thread.start()

        except Exception:
            pass

    def view_post_details(self, post_uuid):
        """View detailed post"""
        try:
            # Ensure we're using the UUID string
            if not post_uuid or not isinstance(post_uuid, str):
                return

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
                            dpg.add_button(label="POST COMMENT", callback=lambda: self.add_comment(post_uuid), width=120)

            # Load post details
            def load_post_details():
                try:
                    result = self.api_client.make_request("GET", f"/forum/posts/{post_uuid}")
                    if result["success"] and result["data"].get("success"):
                        post_data = result["data"]["data"]
                        post = post_data.get("post", {})
                        comments = post_data.get("comments", [])

                        # Update post details content
                        if dpg.does_item_exist("post_detail_content"):
                            content_text = f"Title: {post.get('title', 'N/A')}\n\n"
                            content_text += f"Author: {post.get('author_display_name', 'Unknown')}\n"
                            content_text += f"Category: {post.get('category_name', 'General')}\n"
                            content_text += f"Views: {post.get('views', 0)} | "
                            content_text += f"Likes: {post.get('likes', 0)} | "
                            content_text += f"Comments: {len(comments)}\n\n"
                            content_text += f"Content:\n{post.get('content', 'N/A')}\n\n"

                            if comments:
                                content_text += "Comments:\n"
                                for comment in comments[:5]:  # Show first 5 comments
                                    content_text += f"- {comment.get('author_display_name', 'Unknown')}: "
                                    content_text += f"{comment.get('content', '')[:100]}...\n"

                            dpg.set_value("post_detail_content", content_text)

                except Exception:
                    pass

            thread = threading.Thread(target=load_post_details, daemon=True)
            thread.start()

            dpg.show_item("post_detail_window")

        except Exception:
            pass

    def reply_to_post(self, post_uuid):
        """Reply to a post"""
        # Ensure we're using the UUID string
        if not post_uuid or not isinstance(post_uuid, str):
            return
        self.view_post_details(post_uuid)

    def add_comment(self, post_uuid):
        """Add comment to post"""
        if not self.api_client.is_registered():
            return

        # Ensure we're using the UUID string
        if not post_uuid or not isinstance(post_uuid, str):
            return

        comment_text = dpg.get_value("new_comment_input") if dpg.does_item_exist("new_comment_input") else ""

        if not comment_text.strip():
            return

        def add_comment_thread():
            try:
                result = self.api_client.make_request("POST", f"/forum/posts/{post_uuid}/comments",
                                                      data={"content": comment_text.strip()})
                if result["success"] and result["data"].get("success"):
                    if dpg.does_item_exist("new_comment_input"):
                        dpg.set_value("new_comment_input", "")
            except Exception:
                pass

        thread = threading.Thread(target=add_comment_thread, daemon=True)
        thread.start()

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

                # Load initial posts (all categories)
                self.load_posts()

                self.set_loading(False)
            except Exception:
                self.set_loading(False)

        thread = threading.Thread(target=load_data_thread, daemon=True)
        thread.start()

    def load_categories(self):
        """Load categories from API"""
        try:
            result = self.api_client.make_request("GET", "/forum/categories")
            if result["success"] and result["data"].get("success"):
                self.categories = result["data"]["data"].get("categories", [])

                # Update UI on main thread
                def update_categories_ui():
                    # Clear existing categories
                    if dpg.does_item_exist("categories_list"):
                        dpg.delete_item("categories_list", children_only=True)

                    # Add category items
                    for category in self.categories:
                        self.create_category_item(category)

                # Schedule UI update
                if dpg.does_item_exist("categories_list"):
                    update_categories_ui()

        except Exception:
            pass

    def load_posts(self, category_id=None):
        """Load posts from API"""
        try:
            if dpg.does_item_exist("posts_loading"):
                dpg.show_item("posts_loading")
                dpg.set_value("posts_loading", "Loading posts...")

            # Get posts based on current filter
            if category_id:
                sort_by = dpg.get_value("sort_combo") if dpg.does_item_exist("sort_combo") else "latest"
                result = self.api_client.make_request("GET", f"/forum/categories/{category_id}/posts",
                                                      params={"sort_by": sort_by, "limit": 20})

                if result["success"] and result["data"].get("success"):
                    posts_data = result["data"]["data"]
                    self.posts = posts_data.get("posts", [])
                    category_info = posts_data.get("category", {})
                    category_name = category_info.get("name", "Unknown Category")

                    # Clear existing posts
                    if dpg.does_item_exist("posts_list_area"):
                        dpg.delete_item("posts_list_area", children_only=True)

                    # Add post items
                    for post in self.posts:
                        self.create_post_item(post)

                    # Update category name
                    if dpg.does_item_exist("current_category_name"):
                        dpg.set_value("current_category_name", category_name)
            else:
                # For "all posts", load from the first available category
                if self.categories:
                    first_category_id = self.categories[0]["id"]
                    result = self.api_client.make_request("GET", f"/forum/categories/{first_category_id}/posts",
                                                          params={"sort_by": "latest", "limit": 20})

                    if result["success"] and result["data"].get("success"):
                        posts_data = result["data"]["data"]
                        self.posts = posts_data.get("posts", [])
                        category_name = "Recent Posts"

                        # Clear existing posts
                        if dpg.does_item_exist("posts_list_area"):
                            dpg.delete_item("posts_list_area", children_only=True)

                        # Add post items
                        for post in self.posts:
                            self.create_post_item(post)

                        # Update category name
                        if dpg.does_item_exist("current_category_name"):
                            dpg.set_value("current_category_name", category_name)

            if dpg.does_item_exist("posts_loading"):
                dpg.hide_item("posts_loading")

        except Exception:
            if dpg.does_item_exist("posts_loading"):
                dpg.hide_item("posts_loading")

    def load_forum_stats(self):
        """Load forum statistics"""
        try:
            result = self.api_client.make_request("GET", "/forum/stats")
            if result["success"] and result["data"].get("success"):
                stats = result["data"]["data"]

                if dpg.does_item_exist("stat_total_categories"):
                    dpg.set_value("stat_total_categories", f"Categories: {stats.get('total_categories', 0)}")
                if dpg.does_item_exist("stat_total_posts"):
                    dpg.set_value("stat_total_posts", f"Total Posts: {stats.get('total_posts', 0)}")
                if dpg.does_item_exist("stat_total_comments"):
                    dpg.set_value("stat_total_comments", f"Total Comments: {stats.get('total_comments', 0)}")
                if dpg.does_item_exist("stat_total_votes"):
                    dpg.set_value("stat_total_votes", f"Total Votes: {stats.get('total_votes', 0)}")

        except Exception:
            pass

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

                    result = self.api_client.make_request("GET", "/forum/search",
                                                          params={"q": search_term, "post_type": "all", "limit": 20})

                    if result["success"] and result["data"].get("success"):
                        search_data = result["data"]["data"]
                        results = search_data.get("results", {})
                        posts = results.get("posts", [])

                        # Clear and update posts list
                        if dpg.does_item_exist("posts_list_area"):
                            dpg.delete_item("posts_list_area", children_only=True)

                        for post in posts:
                            self.create_post_item(post)

                        if dpg.does_item_exist("current_category_name"):
                            dpg.set_value("current_category_name", f"Search: '{search_term}'")

                    if dpg.does_item_exist("posts_loading"):
                        dpg.hide_item("posts_loading")

                except Exception:
                    if dpg.does_item_exist("posts_loading"):
                        dpg.hide_item("posts_loading")

            thread = threading.Thread(target=search_thread, daemon=True)
            thread.start()

        except Exception:
            pass

    def on_search_input_change(self, sender, app_data):
        """Handle search input changes"""
        pass

    def sort_posts_callback(self, sender, app_data):
        """Handle post sorting"""
        # Reload posts with new sorting
        self.load_posts(self.current_category)

    def create_new_post(self):
        """Create new post dialog"""
        if not self.api_client.is_registered():
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

            # Validation
            if not title or len(title) < 3:
                return

            if not content or len(content) < 10:
                return

            if not category_name:
                return

            # Find category ID
            category_id = None
            for cat in self.categories:
                if cat["name"] == category_name:
                    category_id = cat["id"]
                    break

            if not category_id:
                return

            def submit_thread():
                try:
                    # Prepare post data according to API specification
                    post_data = {
                        "title": title,
                        "content": content,
                        "category_id": category_id
                    }

                    result = self.api_client.make_request("POST", f"/forum/categories/{category_id}/posts",
                                                          data=post_data)

                    if result["success"] and result["data"].get("success"):
                        # Close dialog and refresh
                        if dpg.does_item_exist("new_post_window"):
                            dpg.delete_item("new_post_window")

                        # Refresh posts and categories to show the new post
                        self.load_categories()
                        # If we're in the same category, reload it, otherwise switch to it
                        if self.current_category == category_id:
                            self.load_posts(category_id)
                        else:
                            self.filter_by_category(category_id)

                except Exception:
                    pass

            thread = threading.Thread(target=submit_thread, daemon=True)
            thread.start()

        except Exception:
            pass

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

            except Exception:
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

                # Get all posts from all categories and sort by engagement
                all_posts = []
                for category in self.categories:
                    result = self.api_client.make_request("GET", f"/forum/categories/{category['id']}/posts",
                                                          params={"sort_by": "popular", "limit": 10})
                    if result["success"] and result["data"].get("success"):
                        posts_data = result["data"]["data"]
                        posts = posts_data.get("posts", [])
                        all_posts.extend(posts)

                # Sort by engagement score (likes + views + comments)
                def engagement_score(post):
                    return post.get('likes', 0) * 3 + post.get('views', 0) + post.get('reply_count', 0) * 2

                all_posts.sort(key=engagement_score, reverse=True)
                trending_posts = all_posts[:20]  # Top 20

                # Clear and update posts list
                if dpg.does_item_exist("posts_list_area"):
                    dpg.delete_item("posts_list_area", children_only=True)

                for post in trending_posts:
                    self.create_post_item(post)

                if dpg.does_item_exist("current_category_name"):
                    dpg.set_value("current_category_name", "Trending Posts")

                if dpg.does_item_exist("posts_loading"):
                    dpg.hide_item("posts_loading")

            except Exception:
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

                # Get recent posts from all categories
                all_posts = []
                for category in self.categories:
                    result = self.api_client.make_request("GET", f"/forum/categories/{category['id']}/posts",
                                                          params={"sort_by": "latest", "limit": 10})
                    if result["success"] and result["data"].get("success"):
                        posts_data = result["data"]["data"]
                        posts = posts_data.get("posts", [])
                        all_posts.extend(posts)

                # Sort by creation date
                def parse_date(post):
                    try:
                        created_at = post.get('created_at', '')
                        if created_at.endswith('Z'):
                            return datetime.fromisoformat(created_at.replace('Z', '+00:00'))
                        else:
                            return datetime.fromisoformat(created_at)
                    except:
                        return datetime.min

                all_posts.sort(key=parse_date, reverse=True)
                recent_posts = all_posts[:20]  # Most recent 20

                # Clear and update posts list
                if dpg.does_item_exist("posts_list_area"):
                    dpg.delete_item("posts_list_area", children_only=True)

                for post in recent_posts:
                    self.create_post_item(post)

                if dpg.does_item_exist("current_category_name"):
                    dpg.set_value("current_category_name", "Recent Posts")

                if dpg.does_item_exist("posts_loading"):
                    dpg.hide_item("posts_loading")

            except Exception:
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
                result = self.api_client.make_request("GET", "/forum/profile")
                if result["success"] and result["data"].get("success"):
                    profile_data = result["data"]["data"]
                    profile = profile_data.get("profile", {})

                    profile_text = f"Username: {profile.get('username', 'N/A')}\n"
                    profile_text += f"Display Name: {profile.get('display_name', 'N/A')}\n"
                    profile_text += f"Bio: {profile.get('bio', 'No bio set')}\n"
                    profile_text += f"Reputation: {profile.get('reputation', 0)}\n"
                    profile_text += f"Posts: {profile.get('posts_count', 0)}\n"
                    profile_text += f"Comments: {profile.get('comments_count', 0)}\n"
                    profile_text += f"Likes Received: {profile.get('likes_received', 0)}\n"
                    profile_text += f"Likes Given: {profile.get('likes_given', 0)}\n"

                    if dpg.does_item_exist("profile_content"):
                        dpg.set_value("profile_content", profile_text)
                else:
                    if dpg.does_item_exist("profile_content"):
                        dpg.set_value("profile_content", "Error loading profile")

            except Exception:
                if dpg.does_item_exist("profile_content"):
                    dpg.set_value("profile_content", "Error loading profile")

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

                result = self.api_client.make_request("GET", "/forum/user/activity")
                if result["success"] and result["data"].get("success"):
                    activity_data = result["data"]["data"]
                    posts = activity_data.get("posts", [])

                    # Clear and update posts list
                    if dpg.does_item_exist("posts_list_area"):
                        dpg.delete_item("posts_list_area", children_only=True)

                    for post in posts:
                        self.create_post_item(post)

                    if dpg.does_item_exist("current_category_name"):
                        dpg.set_value("current_category_name", "My Posts")

                if dpg.does_item_exist("posts_loading"):
                    dpg.hide_item("posts_loading")

            except Exception:
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
                result = self.api_client.make_request("GET", "/forum/user/activity")
                if result["success"] and result["data"].get("success"):
                    activity_data = result["data"]["data"]
                    posts = activity_data.get("posts", [])
                    comments = activity_data.get("comments", [])
                    reputation = activity_data.get("reputation", 0)

                    # Update recent activity panel
                    if dpg.does_item_exist("recent_activity"):
                        dpg.delete_item("recent_activity", children_only=True)

                        with dpg.group(parent="recent_activity"):
                            dpg.add_text(f"Reputation: {reputation}", color=self.BLOOMBERG_GREEN)
                            dpg.add_text(f"Posts: {len(posts)}", color=self.BLOOMBERG_BLUE)
                            dpg.add_text(f"Comments: {len(comments)}", color=self.BLOOMBERG_BLUE)
                            dpg.add_spacer(height=10)

                            dpg.add_text("Recent Posts:", color=self.BLOOMBERG_YELLOW)
                            for post in posts[:5]:  # Show last 5 posts
                                post_title = post.get('title', 'Untitled')[:30]
                                dpg.add_text(f"• {post_title}...", color=self.BLOOMBERG_WHITE)

                            if not posts:
                                dpg.add_text("No posts yet", color=self.BLOOMBERG_GRAY)

            except Exception:
                pass

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

                dpg.add_text("Display Name:")
                dpg.add_input_text(default_value="", width=300, tag="settings_display_name")

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
            display_name = dpg.get_value("settings_display_name") if dpg.does_item_exist("settings_display_name") else ""
            bio = dpg.get_value("settings_bio") if dpg.does_item_exist("settings_bio") else ""
            avatar_color = dpg.get_value("settings_avatar_color") if dpg.does_item_exist("settings_avatar_color") else [255, 165, 0, 255]

            # Convert color to hex
            color_hex = f"#{int(avatar_color[0]):02x}{int(avatar_color[1]):02x}{int(avatar_color[2]):02x}"

            def save_settings_thread():
                try:
                    result = self.api_client.make_request("PUT", "/forum/profile", data={
                        "display_name": display_name,
                        "bio": bio,
                        "avatar_color": color_hex
                    })

                    if result["success"] and result["data"].get("success"):
                        if dpg.does_item_exist("user_settings_window"):
                            dpg.delete_item("user_settings_window")

                except Exception:
                    pass

            thread = threading.Thread(target=save_settings_thread, daemon=True)
            thread.start()

        except Exception:
            pass

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

        except Exception:
            pass

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

        except Exception:
            pass