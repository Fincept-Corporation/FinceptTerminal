// Microsoft Graph API Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class MSGraphAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const accessToken = this.getConfig<string>('accessToken');

      if (!accessToken) {
        return this.createErrorResult('Access token is required');
      }

      console.log('Testing Microsoft Graph API connection...');

      try {
        // Test with /me endpoint
        const response = await fetch('https://graph.microsoft.com/v1.0/me', {
          method: 'GET',
          headers: {
            Authorization: `Bearer ${accessToken}`,
          },
        });

        if (!response.ok) {
          if (response.status === 401) {
            throw new Error('Invalid or expired access token');
          }
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const data = await response.json();

        return this.createSuccessResult('Successfully connected to Microsoft Graph API', {
          displayName: data.displayName || 'Unknown',
          userPrincipalName: data.userPrincipalName || 'Unknown',
          id: data.id || 'Unknown',
          timestamp: new Date().toISOString(),
        });
      } catch (fetchError) {
        if (fetchError instanceof Error) {
          return this.createErrorResult(fetchError.message);
        }
        throw fetchError;
      }
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(endpoint: string, method: string = 'GET', body?: any): Promise<any> {
    try {
      const accessToken = this.getConfig<string>('accessToken');

      const options: RequestInit = {
        method,
        headers: {
          Authorization: `Bearer ${accessToken}`,
          'Content-Type': 'application/json',
        },
      };

      if (body && (method === 'POST' || method === 'PUT' || method === 'PATCH')) {
        options.body = JSON.stringify(body);
      }

      const url = endpoint.startsWith('http')
        ? endpoint
        : `https://graph.microsoft.com/v1.0${endpoint}`;

      const response = await fetch(url, options);

      if (!response.ok) {
        throw new Error(`Query failed: ${response.status} ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`Microsoft Graph query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      hasAccessToken: !!this.getConfig('accessToken'),
      provider: 'Microsoft Graph',
    };
  }

  /**
   * Get current user profile
   */
  async getMe(): Promise<any> {
    return await this.query('/me');
  }

  /**
   * Get user's mail messages
   */
  async getMessages(top: number = 10): Promise<any> {
    return await this.query(`/me/messages?$top=${top}`);
  }

  /**
   * Get user's calendar events
   */
  async getCalendarEvents(top: number = 10): Promise<any> {
    return await this.query(`/me/events?$top=${top}`);
  }

  /**
   * Get user's OneDrive files
   */
  async getOneDriveFiles(): Promise<any> {
    return await this.query('/me/drive/root/children');
  }

  /**
   * Get users in organization
   */
  async getUsers(top: number = 10): Promise<any> {
    return await this.query(`/users?$top=${top}`);
  }

  /**
   * Get user by ID
   */
  async getUser(userId: string): Promise<any> {
    return await this.query(`/users/${userId}`);
  }

  /**
   * Get groups
   */
  async getGroups(top: number = 10): Promise<any> {
    return await this.query(`/groups?$top=${top}`);
  }

  /**
   * Send mail
   */
  async sendMail(subject: string, body: string, toRecipients: string[]): Promise<any> {
    const message = {
      message: {
        subject,
        body: {
          contentType: 'Text',
          content: body,
        },
        toRecipients: toRecipients.map((email) => ({
          emailAddress: { address: email },
        })),
      },
    };

    return await this.query('/me/sendMail', 'POST', message);
  }

  /**
   * Create calendar event
   */
  async createEvent(subject: string, start: string, end: string, location?: string): Promise<any> {
    const event = {
      subject,
      start: {
        dateTime: start,
        timeZone: 'UTC',
      },
      end: {
        dateTime: end,
        timeZone: 'UTC',
      },
      location: location ? { displayName: location } : undefined,
    };

    return await this.query('/me/events', 'POST', event);
  }

  /**
   * Search files in OneDrive
   */
  async searchOneDrive(query: string): Promise<any> {
    return await this.query(`/me/drive/root/search(q='${query}')`);
  }
}
