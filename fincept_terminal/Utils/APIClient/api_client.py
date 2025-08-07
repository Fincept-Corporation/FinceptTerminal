# api_client.py - Complete API Client for Fincept API v2.1.0
"""
Complete API Client helper for Fincept API v2.1.0
Provides easy methods for DearPyGUI tabs to interact with the new API
Includes all endpoints: Chat, Database, User, Support, Payment, etc.
"""

import requests
from typing import Dict, Any, List


class FinceptAPIClient:
    """Complete API client for Fincept API v2.1.0 with all endpoints"""

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

    # ============================================
    # AUTHENTICATION STATUS
    # ============================================

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

    def logout(self) -> Dict[str, Any]:
        """Logout user and clear session"""
        if not self.is_authenticated():
            return {"success": False, "error": "User is not authenticated"}

        result = self.make_request("POST", "/auth/logout")

        if result["success"] and result["data"].get("success"):
            # Clear local session data
            self.clear_session()
            return {
                "success": True,
                "message": result["data"]["data"].get("message", "Logged out successfully")
            }
        return {
            "success": False,
            "error": result.get("error", "Failed to logout")
        }

    def clear_session(self) -> None:
        """Clear all session data locally"""
        # Reset session data to default guest state
        device_id = self.session_data.get("device_id", "unknown")  # Preserve device_id

        self.session_data = {
            "user_type": "guest",
            "authenticated": False,
            "api_key": None,
            "device_id": device_id,
            "user_info": {},
            "expires_at": None,
            "requests_today": 0,
            "daily_limit": 50
        }

        # Reset instance variables
        self.api_key = None
        self.user_type = "guest"
        self.request_count = 0

    def force_logout(self) -> Dict[str, Any]:
        """Force logout without API call (for offline situations)"""
        self.clear_session()
        return {
            "success": True,
            "message": "Forced logout completed - session cleared locally"
        }

    # ============================================
    # CHAT ENDPOINTS (NEW)
    # ============================================

    def get_chat_sessions(self, limit: int = 50) -> Dict[str, Any]:
        """Get user's chat sessions"""
        params = {"limit": limit}
        result = self.make_request("GET", "/chat/sessions", params=params)

        if result["success"] and result["data"].get("success"):
            return {
                "success": True,
                "sessions": result["data"]["data"]["sessions"],
                "total": result["data"]["data"].get("total", 0),
                "user_type": result["data"]["data"].get("user_type")
            }
        return {
            "success": False,
            "sessions": [],
            "error": result.get("error", "Failed to get chat sessions")
        }

    def create_chat_session(self, title: str = "New Conversation") -> Dict[str, Any]:
        """Create new chat session"""
        data = {"title": title}
        result = self.make_request("POST", "/chat/sessions", data)

        if result["success"] and result["data"].get("success"):
            return {
                "success": True,
                "session": result["data"]["data"]["session"],
                "message": result["data"]["data"].get("message", "Session created")
            }
        return {
            "success": False,
            "error": result.get("error", "Failed to create chat session")
        }

    def get_chat_session(self, session_uuid: str) -> Dict[str, Any]:
        """Get specific chat session with messages"""
        result = self.make_request("GET", f"/chat/sessions/{session_uuid}")

        if result["success"] and result["data"].get("success"):
            session_detail = result["data"]["data"]
            return {
                "success": True,
                "session": session_detail["session"],
                "messages": session_detail["messages"],
                "total_messages": session_detail["total_messages"]
            }
        return {
            "success": False,
            "error": result.get("error", "Failed to get chat session")
        }

    def send_chat_message(self, session_uuid: str, content: str) -> Dict[str, Any]:
        """Send message to chat session"""
        data = {"content": content}
        result = self.make_request("POST", f"/chat/sessions/{session_uuid}/messages", data)

        if result["success"] and result["data"].get("success"):
            return {
                "success": True,
                "user_message": result["data"]["data"]["user_message"],
                "ai_message": result["data"]["data"]["ai_message"],
                "new_title": result["data"]["data"].get("new_title")
            }
        return {
            "success": False,
            "error": result.get("error", "Failed to send message")
        }

    def activate_chat_session(self, session_uuid: str) -> Dict[str, Any]:
        """Activate a chat session"""
        result = self.make_request("PUT", f"/chat/sessions/{session_uuid}/activate")

        if result["success"] and result["data"].get("success"):
            return {
                "success": True,
                "message": result["data"]["data"].get("message", "Session activated")
            }
        return {
            "success": False,
            "error": result.get("error", "Failed to activate session")
        }

    def update_chat_title(self, session_uuid: str, new_title: str) -> Dict[str, Any]:
        """Update chat session title"""
        data = {"title": new_title}
        result = self.make_request("PUT", f"/chat/sessions/{session_uuid}/title", data)

        if result["success"] and result["data"].get("success"):
            return {
                "success": True,
                "new_title": result["data"]["data"]["new_title"],
                "message": result["data"]["data"].get("message", "Title updated")
            }
        return {
            "success": False,
            "error": result.get("error", "Failed to update title")
        }

    def delete_chat_session(self, session_uuid: str) -> Dict[str, Any]:
        """Delete chat session"""
        result = self.make_request("DELETE", f"/chat/sessions/{session_uuid}")

        if result["success"] and result["data"].get("success"):
            return {
                "success": True,
                "message": result["data"]["data"].get("message", "Session deleted")
            }
        return {
            "success": False,
            "error": result.get("error", "Failed to delete session")
        }

    def get_chat_stats(self) -> Dict[str, Any]:
        """Get chat statistics"""
        result = self.make_request("GET", "/chat/stats")

        if result["success"] and result["data"].get("success"):
            return {
                "success": True,
                "stats": result["data"]["data"]
            }
        return {
            "success": False,
            "error": result.get("error", "Failed to get chat stats")
        }

    def bulk_delete_chat_sessions(self, session_uuids: List[str]) -> Dict[str, Any]:
        """Bulk delete chat sessions (registered users only)"""
        if self.user_type != "registered":
            return {"success": False, "error": "Only available for registered users"}

        data = {"session_uuids": session_uuids}
        result = self.make_request("DELETE", "/chat/sessions/bulk-delete", data)

        if result["success"] and result["data"].get("success"):
            return {
                "success": True,
                "deleted_count": result["data"]["data"]["deleted_count"],
                "message": result["data"]["data"].get("message", "Sessions deleted")
            }
        return {
            "success": False,
            "error": result.get("error", "Failed to delete sessions")
        }

    def export_chat_sessions(self, session_uuids: List[str] = None, format_type: str = "json") -> Dict[str, Any]:
        """Export chat sessions (registered users only)"""
        if self.user_type != "registered":
            return {"success": False, "error": "Only available for registered users"}

        data = {
            "session_uuids": session_uuids or [],
            "format": format_type
        }
        result = self.make_request("POST", "/chat/export", data)

        if result["success"] and result["data"].get("success"):
            return {
                "success": True,
                "export_data": result["data"]["data"]
            }
        return {
            "success": False,
            "error": result.get("error", "Failed to export sessions")
        }

    # ============================================
    # DATABASE OPERATIONS
    # ============================================

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

    def get_public_databases(self) -> Dict[str, Any]:
        """Get public databases (no authentication required)"""
        result = self.make_request("GET", "/databases/public")
        if result["success"] and result["data"].get("success"):
            return {
                "success": True,
                "databases": result["data"]["data"]["databases"],
                "total": result["data"]["data"].get("total", 0)
            }
        return {"success": False, "databases": [], "error": result.get("error", "Failed to get public databases")}

    # ============================================
    # USER OPERATIONS (Registered users only)
    # ============================================

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

    def get_user_transactions(self) -> Dict[str, Any]:
        """Get user's transaction history"""
        if self.user_type != "registered":
            return {"success": False, "error": "Only available for registered users"}

        result = self.make_request("GET", "/user/transactions")
        if result["success"] and result["data"].get("success"):
            return {
                "success": True,
                "transactions": result["data"]["data"]["transactions"]
            }
        return {"success": False, "transactions": [], "error": result.get("error", "Failed to get transactions")}

    def add_secondary_email(self, secondary_email: str) -> Dict[str, Any]:
        """Add secondary email for 2FA"""
        if self.user_type != "registered":
            return {"success": False, "error": "Only available for registered users"}

        data = {"secondary_email": secondary_email}
        result = self.make_request("POST", "/user/add-secondary-email", data)
        if result["success"] and result["data"].get("success"):
            return {
                "success": True,
                "message": result["data"]["data"].get("message", "Secondary email added")
            }
        return {"success": False, "error": result.get("error", "Failed to add secondary email")}

    def verify_secondary_email(self, otp: str) -> Dict[str, Any]:
        """Verify secondary email"""
        if self.user_type != "registered":
            return {"success": False, "error": "Only available for registered users"}

        data = {"otp": otp}
        result = self.make_request("POST", "/user/verify-secondary-email", data)
        if result["success"] and result["data"].get("success"):
            return {
                "success": True,
                "message": result["data"]["data"].get("message", "Secondary email verified")
            }
        return {"success": False, "error": result.get("error", "Failed to verify secondary email")}

    def toggle_2fa(self, enable: bool) -> Dict[str, Any]:
        """Enable/disable 2FA"""
        if self.user_type != "registered":
            return {"success": False, "error": "Only available for registered users"}

        data = {"enable": enable}
        result = self.make_request("POST", "/user/toggle-2fa", data)
        if result["success"] and result["data"].get("success"):
            return {
                "success": True,
                "message": result["data"]["data"].get("message", "2FA toggled")
            }
        return {"success": False, "error": result.get("error", "Failed to toggle 2FA")}

    # ============================================
    # DATABASE SUBSCRIPTION (Registered users only)
    # ============================================

    def subscribe_to_database(self, database_name: str) -> Dict[str, Any]:
        """Subscribe to a database"""
        if self.user_type != "registered":
            return {"success": False, "error": "Only available for registered users"}

        data = {"database_name": database_name}
        result = self.make_request("POST", "/database/subscribe", data)
        if result["success"] and result["data"].get("success"):
            return {
                "success": True,
                "message": result["data"].get("message", "Subscription successful")
            }
        return {"success": False, "error": result.get("error", "Failed to subscribe")}

    # ============================================
    # DEVICE MANAGEMENT (Registered users only)
    # ============================================

    def bind_device(self, device_id: str, device_name: str, platform: str, hardware_info: Dict[str, Any]) -> Dict[
        str, Any]:
        """Bind device to user account"""
        if self.user_type != "registered":
            return {"success": False, "error": "Only available for registered users"}

        data = {
            "device_id": device_id,
            "device_name": device_name,
            "platform": platform,
            "hardware_info": hardware_info
        }
        result = self.make_request("POST", "/device/bind", data)
        if result["success"] and result["data"].get("success"):
            return {
                "success": True,
                "device_id": result["data"]["data"]["device_id"],
                "is_primary": result["data"]["data"]["is_primary"],
                "message": result["data"]["data"].get("message", "Device bound")
            }
        return {"success": False, "error": result.get("error", "Failed to bind device")}

    def list_user_devices(self) -> Dict[str, Any]:
        """List user's devices"""
        if self.user_type != "registered":
            return {"success": False, "error": "Only available for registered users"}

        result = self.make_request("GET", "/device/list")
        if result["success"] and result["data"].get("success"):
            return {
                "success": True,
                "devices": result["data"]["data"]["devices"],
                "total": result["data"]["data"]["total"],
                "max_allowed": result["data"]["data"]["max_allowed"]
            }
        return {"success": False, "devices": [], "error": result.get("error", "Failed to get devices")}

    # ============================================
    # SUPPORT SYSTEM (Registered users only)
    # ============================================

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

    def reply_to_ticket(self, ticket_id: int, message: str) -> Dict[str, Any]:
        """Reply to support ticket"""
        if self.user_type != "registered":
            return {"success": False, "error": "Only available for registered users"}

        data = {"message": message}
        result = self.make_request("POST", f"/support/ticket/{ticket_id}/reply", data)
        if result["success"] and result["data"].get("success"):
            return {
                "success": True,
                "message": result["data"]["data"].get("message", "Reply added")
            }
        return {"success": False, "error": result.get("error", "Failed to reply to ticket")}

    # ============================================
    # LEGACY CHAT SYSTEM (Registered users only)
    # ============================================

    def send_legacy_chat_message(self, channel: str, message: str) -> Dict[str, Any]:
        """Send a legacy chat message"""
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

    def get_legacy_chat_messages(self, channel: str, limit: int = 50) -> Dict[str, Any]:
        """Get legacy chat messages from a channel"""
        params = {"limit": limit}
        result = self.make_request("GET", f"/chat/{channel}/messages", params=params)
        if result["success"] and result["data"].get("success"):
            return {
                "success": True,
                "messages": result["data"]["data"]["messages"],
                "channel": result["data"]["data"]["channel"]
            }
        return {"success": False, "messages": [], "error": result.get("error", "Failed to get messages")}

    # ============================================
    # PAYMENT SYSTEM (Registered users only)
    # ============================================

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

    # ============================================
    # GUEST USER STATUS
    # ============================================

    def get_guest_status(self) -> Dict[str, Any]:
        """Get guest user status and usage"""
        if self.user_type != "guest":
            return {"success": False, "error": "Only available for guest users"}

        # For guest status, we need to send device_id in the request
        device_id = self.session_data.get("device_id")
        if not device_id:
            device_id = self.generate_device_id() if hasattr(self, 'generate_device_id') else None

        headers = self.get_headers()
        if device_id:
            headers["X-Device-ID"] = device_id

        # Make request with device identification
        try:
            url = f"{self.api_base}/guest/status"
            response = requests.get(url, headers=headers, timeout=10)

            return {
                "success": response.status_code < 400,
                "status_code": response.status_code,
                "data": response.json() if response.content else {},
                "headers": dict(response.headers)
            }
        except Exception as e:
            return {"success": False, "error": f"Request error: {str(e)}"}

    def get_or_create_guest_session(self, device_id: str, device_name: str, platform: str,
                                    hardware_info: Dict[str, Any]) -> Dict[str, Any]:
        """Get existing guest session or create new one"""
        try:
            # First try to get existing session
            headers = {"Content-Type": "application/json", "X-Device-ID": device_id}

            # Try guest status endpoint first
            status_response = requests.get(
                f"{self.api_base}/guest/status",
                headers=headers,
                timeout=10
            )

            if status_response.status_code == 200:
                data = status_response.json()
                if data.get("success"):
                    return {
                        "success": True,
                        "data": data.get("data", {}),
                        "message": "Existing session retrieved"
                    }

            # If no existing session, try to register
            register_data = {
                "device_id": device_id,
                "device_name": device_name,
                "platform": platform,
                "hardware_info": hardware_info
            }

            register_response = requests.post(
                f"{self.api_base}/device/register",
                json=register_data,
                headers={"Content-Type": "application/json"},
                timeout=10
            )

            if register_response.status_code == 200:
                data = register_response.json()
                if data.get("success"):
                    return {
                        "success": True,
                        "data": data.get("data", {}),
                        "message": "New session created"
                    }

            # Handle conflict (409) - device already exists
            elif register_response.status_code == 409:
                # Try to get the existing session again with different approach
                auth_headers = {"Content-Type": "application/json"}

                # Try auth/status with device info
                auth_response = requests.get(
                    f"{self.api_base}/auth/status",
                    headers=auth_headers,
                    params={"device_id": device_id},
                    timeout=10
                )

                if auth_response.status_code == 200:
                    auth_data = auth_response.json()
                    if auth_data.get("success") and auth_data.get("data", {}).get("guest"):
                        return {
                            "success": True,
                            "data": auth_data["data"]["guest"],
                            "message": "Existing session found via auth"
                        }

            return {
                "success": False,
                "error": f"Failed to get or create session: {register_response.status_code}"
            }

        except Exception as e:
            return {"success": False, "error": f"Request error: {str(e)}"}

    def extend_guest_session(self) -> Dict[str, Any]:
        """Extend guest session"""
        if self.user_type != "guest":
            return {"success": False, "error": "Only available for guest users"}

        result = self.make_request("POST", "/guest/extend")
        if result["success"] and result["data"].get("success"):
            return {
                "success": True,
                "message": result["data"]["data"]["message"],
                "new_expiry": result["data"]["data"]["new_expiry"],
                "hours_added": result["data"]["data"]["hours_added"]
            }
        return {"success": False, "error": result.get("error", "Failed to extend session")}

    # ============================================
    # SYSTEM INFORMATION
    # ============================================

    def get_system_config(self) -> Dict[str, Any]:
        """Get public system configuration"""
        result = self.make_request("GET", "/config/system")
        if result["success"] and result["data"].get("success"):
            return {
                "success": True,
                "config": result["data"]["data"]["config"]
            }
        return {"success": False, "config": {}, "error": result.get("error", "Failed to get config")}

    def get_health_status(self) -> Dict[str, Any]:
        """Get API health status"""
        result = self.make_request("GET", "/health")
        if result["success"]:
            return {
                "success": True,
                "health": result["data"]
            }
        return {"success": False, "error": result.get("error", "Failed to get health status")}

    def get_api_root(self) -> Dict[str, Any]:
        """Get API root information"""
        result = self.make_request("GET", "/")
        if result["success"] and result["data"].get("success"):
            return {
                "success": True,
                "info": result["data"]["data"]
            }
        return {"success": False, "error": result.get("error", "Failed to get API info")}

    def test_endpoint(self) -> Dict[str, Any]:
        """Test API endpoint"""
        result = self.make_request("GET", "/test")
        if result["success"] and result["data"].get("success"):
            return {
                "success": True,
                "test_result": result["data"]["data"]
            }
        return {"success": False, "error": result.get("error", "Test endpoint failed")}

    # ============================================
    # UTILITY METHODS
    # ============================================

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

    def get_session_data(self) -> Dict[str, Any]:
        """Get session data"""
        return self.session_data

    def update_session_data(self, new_data: Dict[str, Any]) -> None:
        """Update session data"""
        self.session_data.update(new_data)
        # Update relevant attributes
        self.api_key = self.session_data.get("api_key")
        self.user_type = self.session_data.get("user_type", "guest")

    def reset_request_count(self) -> None:
        """Reset request counter"""
        self.request_count = 0

    # ============================================
    # ERROR HANDLING HELPER
    # ============================================

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
        elif "404" in str(result.get("status_code", "")):
            return "Resource not found. Please check the endpoint or resource ID."
        elif "400" in str(result.get("status_code", "")):
            return "Bad request. Please check your input parameters."
        elif "500" in str(result.get("status_code", "")):
            return "Internal server error. Please try again later."
        else:
            return error

    def get_error_context(self, result: Dict[str, Any]) -> Dict[str, Any]:
        """Get detailed error context for debugging"""
        return {
            "success": result.get("success", False),
            "status_code": result.get("status_code"),
            "error": result.get("error"),
            "headers": result.get("headers", {}),
            "request_count": self.request_count,
            "user_type": self.user_type,
            "api_key_present": bool(self.api_key),
            "session_authenticated": self.is_authenticated()
        }

    # ============================================
    # BATCH OPERATIONS
    # ============================================

    def batch_request(self, requests: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
        """Execute multiple API requests in sequence"""
        results = []

        for req in requests:
            method = req.get("method", "GET")
            endpoint = req.get("endpoint", "/")
            data = req.get("data")
            params = req.get("params")
            timeout = req.get("timeout", 10)

            result = self.make_request(method, endpoint, data, params, timeout)
            results.append({
                "request": req,
                "result": result
            })

        return results

    def validate_endpoints(self, endpoints: List[str]) -> Dict[str, bool]:
        """Validate multiple endpoints availability"""
        results = {}

        for endpoint in endpoints:
            try:
                result = self.make_request("GET", endpoint, timeout=5)
                results[endpoint] = result["success"]
            except Exception:
                results[endpoint] = False

        return results

    # ============================================
    # PERFORMANCE MONITORING
    # ============================================

    def get_performance_stats(self) -> Dict[str, Any]:
        """Get performance statistics for this session"""
        return {
            "total_requests": self.request_count,
            "user_type": self.user_type,
            "authenticated": self.is_authenticated(),
            "api_base": self.api_base,
            "session_start": getattr(self, '_session_start', 'unknown')
        }

    def benchmark_endpoint(self, endpoint: str, iterations: int = 5) -> Dict[str, Any]:
        """Benchmark an endpoint performance"""
        import time

        results = []

        for i in range(iterations):
            start_time = time.time()
            result = self.make_request("GET", endpoint, timeout=30)
            end_time = time.time()

            results.append({
                "iteration": i + 1,
                "response_time": end_time - start_time,
                "success": result["success"],
                "status_code": result.get("status_code")
            })

        # Calculate statistics
        response_times = [r["response_time"] for r in results]
        success_count = sum(1 for r in results if r["success"])

        return {
            "endpoint": endpoint,
            "iterations": iterations,
            "success_rate": success_count / iterations,
            "avg_response_time": sum(response_times) / len(response_times),
            "min_response_time": min(response_times),
            "max_response_time": max(response_times),
            "results": results
        }


# ============================================
# HELPER FUNCTIONS
# ============================================

def create_api_client(session_data: Dict[str, Any]) -> FinceptAPIClient:
    """Create API client instance from session data"""
    client = FinceptAPIClient(session_data)
    # Store session start time for performance tracking
    import time
    client._session_start = time.time()
    return client


def validate_session_data(session_data: Dict[str, Any]) -> Dict[str, Any]:
    """Validate session data before creating API client"""
    required_fields = ["user_type", "authenticated"]
    missing_fields = []

    for field in required_fields:
        if field not in session_data:
            missing_fields.append(field)

    validation_result = {
        "valid": len(missing_fields) == 0,
        "missing_fields": missing_fields,
        "has_api_key": bool(session_data.get("api_key")),
        "user_type": session_data.get("user_type", "unknown"),
        "authenticated": session_data.get("authenticated", False)
    }

    return validation_result


def create_mock_api_client(user_type: str = "guest") -> FinceptAPIClient:
    """Create a mock API client for testing purposes"""
    mock_session = {
        "user_type": user_type,
        "authenticated": True,
        "api_key": f"mock_key_{user_type}",
        "device_id": "mock_device",
        "user_info": {
            "username": "mock_user",
            "email": "mock@example.com"
        } if user_type == "registered" else {}
    }

    return create_api_client(mock_session)


def get_api_client_info(client: FinceptAPIClient) -> Dict[str, Any]:
    """Get information about an API client instance"""
    return {
        "api_base": client.api_base,
        "user_type": client.user_type,
        "authenticated": client.is_authenticated(),
        "has_api_key": client.has_api_key(),
        "request_count": client.get_request_count(),
        "user_info": client.get_user_info()
    }


# ============================================
# CONSTANTS AND CONFIGURATIONS
# ============================================

# API Endpoints Categories
CHAT_ENDPOINTS = [
    "/chat/sessions",
    "/chat/sessions/{uuid}",
    "/chat/sessions/{uuid}/messages",
    "/chat/sessions/{uuid}/activate",
    "/chat/sessions/{uuid}/title",
    "/chat/stats"
]

DATABASE_ENDPOINTS = [
    "/databases",
    "/databases/public",
    "/database/{name}/tables",
    "/database/{name}/{table}/data"
]

USER_ENDPOINTS = [
    "/user/profile",
    "/user/usage",
    "/user/regenerate-api-key",
    "/user/transactions"
]

SUPPORT_ENDPOINTS = [
    "/support/ticket",
    "/support/tickets",
    "/support/ticket/{id}/reply"
]

GUEST_ENDPOINTS = [
    "/guest/status",
    "/guest/extend"
]

SYSTEM_ENDPOINTS = [
    "/health",
    "/",
    "/test",
    "/config/system"
]

# Rate Limits (requests per hour)
RATE_LIMITS = {
    "guest": {
        "chat": 20,
        "database": 50,
        "total": 100
    },
    "registered": {
        "chat": 1000,
        "database": 5000,
        "total": 10000
    }
}

# Error Codes
ERROR_CODES = {
    400: "Bad Request",
    401: "Unauthorized",
    403: "Forbidden",
    404: "Not Found",
    429: "Too Many Requests",
    500: "Internal Server Error",
    502: "Bad Gateway",
    503: "Service Unavailable"
}

# Default Timeouts (seconds)
DEFAULT_TIMEOUTS = {
    "short": 5,
    "medium": 10,
    "long": 30,
    "very_long": 60
}