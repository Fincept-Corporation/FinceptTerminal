# api_client.py - New API Client Helper for DearPyGUI Integration
"""
API Client helper for Fincept API v2.1.0
Provides easy methods for DearPyGUI tabs to interact with the new API
"""

import requests
import json
from datetime import datetime
from typing import Optional, Dict, Any, List


class FinceptAPIClient:
    """Enhanced API client for new Fincept API v2.1.0"""

    def __init__(self, session_data: Dict[str, Any]):
        self.api_base = "https://finceptbackend.share.zrok.io"
        self.session_data = session_data
        self.api_key = session_data.get("api_key")
        self.user_type = session_data.get("user_type", "guest")
        self.request_count = 0

    def get_headers(self) -> Dict[str, str]:
        """Get authentication headers"""
        headers = {"Content-Type": "application/json"}

        if self.api_key:
            headers["X-API-Key"] = self.api_key

        return headers

    def make_request(self, method: str, endpoint: str, data: dict = None, params: dict = None, timeout: int = 10) -> \
    Dict[str, Any]:
        """Make authenticated API request"""
        try:
            url = f"{self.api_base}{endpoint}"
            headers = self.get_headers()

            # Track requests
            self.request_count += 1

            if method.upper() == "GET":
                response = requests.get(url, headers=headers, params=params, timeout=timeout)
            elif method.upper() == "POST":
                response = requests.post(url, headers=headers, json=data, timeout=timeout)
            elif method.upper() == "PUT":
                response = requests.put(url, headers=headers, json=data, timeout=timeout)
            elif method.upper() == "DELETE":
                response = requests.delete(url, headers=headers, timeout=timeout)
            else:
                return {"success": False, "error": f"Unsupported method: {method}"}

            return {
                "success": response.status_code < 400,
                "status_code": response.status_code,
                "data": response.json() if response.content else {},
                "headers": dict(response.headers)
            }

        except requests.exceptions.Timeout:
            return {"success": False, "error": "Request timeout"}
        except requests.exceptions.ConnectionError:
            return {"success": False, "error": "Connection error - API server not available"}
        except requests.exceptions.RequestException as e:
            return {"success": False, "error": f"Request error: {str(e)}"}
        except Exception as e:
            return {"success": False, "error": f"Unexpected error: {str(e)}"}

    # Authentication Status
    def check_auth_status(self) -> Dict[str, Any]:
        """Check current authentication status"""
        result = self.make_request("GET", "/auth/status")
        if result["success"] and result["data"].get("success"):
            return {
                "success": True,
                "authenticated": result["data"]["data"].get("authenticated", False),
                "user_type": result["data"]["data"].get("user_type"),
                "user_info": result["data"]["data"].get("user", result["data"]["data"].get("guest"))
            }
        return {"success": False, "authenticated": False}

    # Database Operations
    def get_databases(self) -> Dict[str, Any]:
        """Get list of available databases"""
        result = self.make_request("GET", "/databases")
        if result["success"] and result["data"].get("success"):
            return {
                "success": True,
                "databases": result["data"]["data"]["databases"],
                "user_type": result["data"]["data"].get("user_type"),
                "total": result["data"]["data"].get("total_available", 0)
            }
        return {"success": False, "databases": [], "error": result.get("error", "Failed to get databases")}

    def get_database_tables(self, database_name: str) -> Dict[str, Any]:
        """Get tables in a specific database"""
        result = self.make_request("GET", f"/database/{database_name}/tables")
        if result["success"] and result["data"].get("success"):
            return {
                "success": True,
                "tables": result["data"]["data"]["tables"],
                "database": result["data"]["data"]["database"],
                "total_tables": result["data"]["data"].get("total_tables", 0),
                "user_type": result["data"]["data"].get("user_type"),
                "access_level": result["data"]["data"].get("access_level")
            }
        return {"success": False, "tables": [], "error": result.get("error", "Failed to get tables")}

    def get_table_data(self, database_name: str, table_name: str, page: int = 1, limit: int = 50) -> Dict[str, Any]:
        """Get data from a specific table"""
        params = {"page": page, "limit": limit}
        result = self.make_request("GET", f"/database/{database_name}/{table_name}/data", params=params)

        if result["success"] and result["data"].get("success"):
            response_data = result["data"]["data"]
            return {
                "success": True,
                "data": response_data["data"],
                "pagination": {
                    "page": response_data["page"],
                    "limit": response_data["limit"],
                    "rows_returned": response_data["rows_returned"]
                },
                "database": response_data["database"],
                "table": response_data["table"],
                "credits_used": response_data.get("credits_used", 0),
                "user_type": response_data.get("user_type"),
                "guest_info": response_data.get("guest_info")
            }
        return {"success": False, "data": [], "error": result.get("error", "Failed to get table data")}

    # User Profile Operations (Registered users only)
    def get_user_profile(self) -> Dict[str, Any]:
        """Get user profile information"""
        if self.user_type != "registered":
            return {"success": False, "error": "Only available for registered users"}

        result = self.make_request("GET", "/user/profile")
        if result["success"] and result["data"].get("success"):
            return {
                "success": True,
                "profile": result["data"]["data"]
            }
        return {"success": False, "error": result.get("error", "Failed to get profile")}

    def get_user_usage(self) -> Dict[str, Any]:
        """Get user's API usage statistics"""
        if self.user_type != "registered":
            return {"success": False, "error": "Only available for registered users"}

        result = self.make_request("GET", "/user/usage")
        if result["success"] and result["data"].get("success"):
            return {
                "success": True,
                "usage": result["data"]["data"]
            }
        return {"success": False, "error": result.get("error", "Failed to get usage")}

    def regenerate_api_key(self) -> Dict[str, Any]:
        """Regenerate user API key"""
        if self.user_type != "registered":
            return {"success": False, "error": "Only available for registered users"}

        result = self.make_request("POST", "/user/regenerate-api-key")
        if result["success"] and result["data"].get("success"):
            new_api_key = result["data"]["data"]["api_key"]
            # Update our stored API key
            self.api_key = new_api_key
            self.session_data["api_key"] = new_api_key
            return {
                "success": True,
                "api_key": new_api_key,
                "message": result["data"]["data"].get("message", "API key regenerated")
            }
        return {"success": False, "error": result.get("error", "Failed to regenerate API key")}

    # Database Subscription (Registered users only)
    def subscribe_to_database(self, database_name: str) -> Dict[str, Any]:
        """Subscribe to a database"""
        if self.user_type != "registered":
            return {"success": False, "error": "Only available for registered users"}

        result = self.make_request("POST", "/database/subscribe", {"database_name": database_name})
        if result["success"] and result["data"].get("success"):
            return {
                "success": True,
                "message": result["data"].get("message", "Subscription successful")
            }
        return {"success": False, "error": result.get("error", "Failed to subscribe")}

    # Support System (Registered users only)
    def create_support_ticket(self, subject: str, description: str, category: str = "general") -> Dict[str, Any]:
        """Create a support ticket"""
        if self.user_type != "registered":
            return {"success": False, "error": "Only available for registered users"}

        data = {
            "subject": subject,
            "description": description,
            "category": category
        }

        result = self.make_request("POST", "/support/ticket", data)
        if result["success"] and result["data"].get("success"):
            return {
                "success": True,
                "ticket_id": result["data"]["data"]["ticket_id"],
                "message": result["data"]["data"].get("message", "Ticket created")
            }
        return {"success": False, "error": result.get("error", "Failed to create ticket")}

    def get_support_tickets(self) -> Dict[str, Any]:
        """Get user's support tickets"""
        if self.user_type != "registered":
            return {"success": False, "error": "Only available for registered users"}

        result = self.make_request("GET", "/support/tickets")
        if result["success"] and result["data"].get("success"):
            return {
                "success": True,
                "tickets": result["data"]["data"]["tickets"]
            }
        return {"success": False, "tickets": [], "error": result.get("error", "Failed to get tickets")}

    # Chat System (Registered users only)
    def send_chat_message(self, channel: str, message: str) -> Dict[str, Any]:
        """Send a chat message"""
        if self.user_type != "registered":
            return {"success": False, "error": "Only available for registered users"}

        data = {"message": message}
        result = self.make_request("POST", f"/chat/{channel}/message", data)
        if result["success"] and result["data"].get("success"):
            return {
                "success": True,
                "message_id": result["data"]["data"]["message_id"],
                "message": result["data"]["data"].get("message", "Message sent")
            }
        return {"success": False, "error": result.get("error", "Failed to send message")}

    def get_chat_messages(self, channel: str, limit: int = 50) -> Dict[str, Any]:
        """Get chat messages from a channel"""
        params = {"limit": limit}
        result = self.make_request("GET", f"/chat/{channel}/messages", params=params)
        if result["success"] and result["data"].get("success"):
            return {
                "success": True,
                "messages": result["data"]["data"]["messages"],
                "channel": result["data"]["data"]["channel"]
            }
        return {"success": False, "messages": [], "error": result.get("error", "Failed to get messages")}

    # Payment System (Registered users only)
    def create_payment_order(self, amount_inr: int) -> Dict[str, Any]:
        """Create a payment order"""
        if self.user_type != "registered":
            return {"success": False, "error": "Only available for registered users"}

        data = {"amount_inr": amount_inr}
        result = self.make_request("POST", "/payment/create-order", data)
        if result["success"] and result["data"].get("success"):
            return {
                "success": True,
                "order": result["data"]["data"]
            }
        return {"success": False, "error": result.get("error", "Failed to create order")}

    # System Information
    def get_system_config(self) -> Dict[str, Any]:
        """Get public system configuration"""
        result = self.make_request("GET", "/config/system")
        if result["success"] and result["data"].get("success"):
            return {
                "success": True,
                "config": result["data"]["data"]["config"]
            }
        return {"success": False, "config": {}, "error": result.get("error", "Failed to get config")}

    # Utility Methods
    def is_authenticated(self) -> bool:
        """Check if user is authenticated"""
        return self.session_data.get("authenticated", False)

    def is_guest(self) -> bool:
        """Check if user is a guest"""
        return self.user_type == "guest"

    def is_registered(self) -> bool:
        """Check if user is registered"""
        return self.user_type == "registered"

    def has_api_key(self) -> bool:
        """Check if user has an API key"""
        return bool(self.api_key)

    def get_request_count(self) -> int:
        """Get number of requests made in this session"""
        return self.request_count

    def get_user_info(self) -> Dict[str, Any]:
        """Get user information"""
        if self.user_type == "registered":
            return self.session_data.get("user_info", {})
        else:
            return {
                "user_type": "guest",
                "device_id": self.session_data.get("device_id", "unknown"),
                "expires_at": self.session_data.get("expires_at"),
                "daily_limit": self.session_data.get("daily_limit", 50)
            }

    # Error handling helper
    def handle_api_error(self, result: Dict[str, Any], default_message: str = "API request failed") -> str:
        """Extract meaningful error message from API result"""
        if result.get("success"):
            return "Success"

        error = result.get("error", default_message)

        # Handle common error scenarios
        if "Connection error" in error:
            return "API server is not available. Please check if the server is running on https://finceptbackend.share.zrok.io"
        elif "timeout" in error.lower():
            return "Request timed out. Please try again."
        elif "401" in str(result.get("status_code", "")):
            return "Authentication failed. Please check your API key."
        elif "403" in str(result.get("status_code", "")):
            return "Access denied. You may need to subscribe to this database or upgrade your account."
        elif "429" in str(result.get("status_code", "")):
            return "Rate limit exceeded. Please wait before making more requests."
        else:
            return error


# Helper function to create API client from session data
def create_api_client(session_data: Dict[str, Any]) -> FinceptAPIClient:
    """Create API client instance from session data"""
    return FinceptAPIClient(session_data)