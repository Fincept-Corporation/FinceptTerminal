// File: src/services/chatModeApi.ts
// API service for Chat Mode - integrates with Fincept API

export interface ChatModeMessage {
  id: string;
  role: 'user' | 'assistant';
  content: string;
  timestamp: Date;
  message_uuid?: string;
  tokens_used?: number;
  response_time_ms?: number;
}

export interface ChatSession {
  id: number;
  title: string;
  session_uuid: string;
  is_active: boolean;
  message_count: number;
  created_at: string;
  updated_at: string;
  last_message_at: string | null;
  user_type: 'registered' | 'guest';
}

export interface ChatModeResponse {
  success: boolean;
  message?: string;
  data?: any;
  error?: string;
}

class ChatModeApiService {
  private baseUrl: string;
  private apiKey: string | null = null;

  constructor() {
    this.baseUrl = import.meta.env.VITE_API_URL || 'https://finceptbackend.share.zrok.io';
  }

  /**
   * Initialize API key from localStorage
   */
  private async initialize() {
    try {
      // Get session from localStorage
      const sessionStr = localStorage.getItem('fincept_session');
      if (sessionStr) {
        const session = JSON.parse(sessionStr);
        this.apiKey = session.api_key || null;
      }
    } catch (error) {
      console.error('Failed to initialize chat mode API:', error);
    }
  }

  /**
   * Quick chat without session management
   */
  async quickChat(message: string): Promise<ChatModeResponse> {
    try {
      await this.initialize();

      const response = await fetch(`${this.baseUrl}/chat/quick-chat`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          ...(this.apiKey && { 'X-API-Key': this.apiKey })
        },
        body: JSON.stringify({ content: message })
      });

      if (!response.ok) {
        throw new Error(`API request failed: ${response.statusText}`);
      }

      const result = await response.json();

      if (result.success) {
        return {
          success: true,
          data: {
            user_message: result.data.user_message,
            ai_response: result.data.ai_response,
            timestamp: result.data.timestamp
          }
        };
      }

      throw new Error(result.message || 'Unknown error');
    } catch (error) {
      console.error('Quick chat error:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Failed to send message'
      };
    }
  }

  /**
   * Create a new chat session
   */
  async createSession(title?: string): Promise<ChatModeResponse> {
    try {
      await this.initialize();

      const response = await fetch(`${this.baseUrl}/chat/sessions`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          ...(this.apiKey && { 'X-API-Key': this.apiKey })
        },
        body: JSON.stringify({ title: title || 'New Conversation' })
      });

      if (!response.ok) {
        throw new Error(`Failed to create session: ${response.statusText}`);
      }

      const result = await response.json();

      if (result.success) {
        return {
          success: true,
          data: result.data.session
        };
      }

      throw new Error(result.message || 'Unknown error');
    } catch (error) {
      console.error('Create session error:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Failed to create session'
      };
    }
  }

  /**
   * Get all user sessions
   */
  async getSessions(): Promise<ChatModeResponse> {
    try {
      await this.initialize();

      const response = await fetch(`${this.baseUrl}/chat/sessions`, {
        method: 'GET',
        headers: {
          'Content-Type': 'application/json',
          ...(this.apiKey && { 'X-API-Key': this.apiKey })
        }
      });

      if (!response.ok) {
        throw new Error(`Failed to get sessions: ${response.statusText}`);
      }

      const result = await response.json();

      if (result.success) {
        return {
          success: true,
          data: result.data.sessions
        };
      }

      throw new Error(result.message || 'Unknown error');
    } catch (error) {
      console.error('Get sessions error:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Failed to get sessions'
      };
    }
  }

  /**
   * Send message to a session
   */
  async sendMessageToSession(sessionUuid: string, message: string): Promise<ChatModeResponse> {
    try {
      await this.initialize();

      const response = await fetch(`${this.baseUrl}/chat/sessions/${sessionUuid}/messages`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          ...(this.apiKey && { 'X-API-Key': this.apiKey })
        },
        body: JSON.stringify({ content: message })
      });

      if (!response.ok) {
        throw new Error(`Failed to send message: ${response.statusText}`);
      }

      const result = await response.json();

      if (result.success) {
        return {
          success: true,
          data: {
            user_message: result.data.user_message,
            ai_message: result.data.ai_message
          }
        };
      }

      throw new Error(result.message || 'Unknown error');
    } catch (error) {
      console.error('Send message error:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Failed to send message'
      };
    }
  }

  /**
   * Get a specific session with messages
   */
  async getSession(sessionUuid: string): Promise<ChatModeResponse> {
    try {
      await this.initialize();

      const response = await fetch(`${this.baseUrl}/chat/sessions/${sessionUuid}`, {
        method: 'GET',
        headers: {
          'Content-Type': 'application/json',
          ...(this.apiKey && { 'X-API-Key': this.apiKey })
        }
      });

      if (!response.ok) {
        throw new Error(`Failed to get session: ${response.statusText}`);
      }

      const result = await response.json();

      if (result.success) {
        return {
          success: true,
          data: result.data
        };
      }

      throw new Error(result.message || 'Unknown error');
    } catch (error) {
      console.error('Get session error:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Failed to get session'
      };
    }
  }

  /**
   * Delete a session
   */
  async deleteSession(sessionUuid: string): Promise<ChatModeResponse> {
    try {
      await this.initialize();

      const response = await fetch(`${this.baseUrl}/chat/sessions/${sessionUuid}`, {
        method: 'DELETE',
        headers: {
          'Content-Type': 'application/json',
          ...(this.apiKey && { 'X-API-Key': this.apiKey })
        }
      });

      if (!response.ok) {
        throw new Error(`Failed to delete session: ${response.statusText}`);
      }

      const result = await response.json();

      if (result.success) {
        return { success: true };
      }

      throw new Error(result.message || 'Unknown error');
    } catch (error) {
      console.error('Delete session error:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Failed to delete session'
      };
    }
  }

  /**
   * Update session title
   */
  async updateSessionTitle(sessionUuid: string, title: string): Promise<ChatModeResponse> {
    try {
      await this.initialize();

      const response = await fetch(`${this.baseUrl}/chat/sessions/${sessionUuid}/title`, {
        method: 'PUT',
        headers: {
          'Content-Type': 'application/json',
          ...(this.apiKey && { 'X-API-Key': this.apiKey })
        },
        body: JSON.stringify({ title })
      });

      if (!response.ok) {
        throw new Error(`Failed to update title: ${response.statusText}`);
      }

      const result = await response.json();

      if (result.success) {
        return { success: true };
      }

      throw new Error(result.message || 'Unknown error');
    } catch (error) {
      console.error('Update title error:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Failed to update title'
      };
    }
  }
}

export const chatModeApiService = new ChatModeApiService();
