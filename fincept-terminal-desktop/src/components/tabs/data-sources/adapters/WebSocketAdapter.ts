// WebSocket API Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class WebSocketAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const url = this.getConfig<string>('url');

      if (!url) {
        return this.createErrorResult('WebSocket URL is required');
      }

      if (!url.startsWith('ws://') && !url.startsWith('wss://')) {
        return this.createErrorResult('URL must start with ws:// or wss://');
      }

      console.log('Validating WebSocket configuration...');

      return this.createSuccessResult(`Configuration validated for WebSocket "${url}"`, {
        validatedAt: new Date().toISOString(),
        url,
        secure: url.startsWith('wss://'),
        note: 'Client-side validation only. WebSocket connections are established on-demand.',
      });
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(message?: string): Promise<any> {
    try {
      const url = this.getConfig<string>('url');

      return new Promise((resolve, reject) => {
        const ws = new WebSocket(url);
        let responseReceived = false;

        ws.onopen = () => {
          if (message) {
            ws.send(message);
          }
        };

        ws.onmessage = (event) => {
          responseReceived = true;
          try {
            const data = JSON.parse(event.data);
            ws.close();
            resolve(data);
          } catch {
            ws.close();
            resolve(event.data);
          }
        };

        ws.onerror = (error) => {
          reject(new Error('WebSocket connection error'));
        };

        ws.onclose = () => {
          if (!responseReceived) {
            resolve({ connected: true, closed: true });
          }
        };

        // Timeout after 5 seconds
        setTimeout(() => {
          if (ws.readyState !== WebSocket.CLOSED) {
            ws.close();
            if (!responseReceived) {
              resolve({ connected: true, timeout: true });
            }
          }
        }, 5000);
      });
    } catch (error) {
      throw new Error(`WebSocket error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      url: this.getConfig('url'),
      secure: this.getConfig('url')?.startsWith('wss://'),
    };
  }

  /**
   * Connect and listen for messages
   */
  connectAndListen(
    onMessage: (data: any) => void,
    onError?: (error: any) => void,
    onClose?: () => void
  ): WebSocket {
    const url = this.getConfig<string>('url');
    const ws = new WebSocket(url);

    ws.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);
        onMessage(data);
      } catch {
        onMessage(event.data);
      }
    };

    ws.onerror = (error) => {
      if (onError) onError(error);
    };

    ws.onclose = () => {
      if (onClose) onClose();
    };

    return ws;
  }

  /**
   * Send message
   */
  send(ws: WebSocket, message: any): void {
    if (ws.readyState === WebSocket.OPEN) {
      const data = typeof message === 'string' ? message : JSON.stringify(message);
      ws.send(data);
    } else {
      throw new Error('WebSocket is not open');
    }
  }
}
