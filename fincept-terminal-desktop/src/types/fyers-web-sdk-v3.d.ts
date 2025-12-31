// TypeScript declarations for fyers-web-sdk-v3
declare module 'fyers-web-sdk-v3' {
  export class fyersModel {
    constructor(config: {
      AccessToken: string;
      AppID: string;
      RedirectURL: string;
      enableLogging?: boolean;
    });
  }

  export class fyersDataSocket {
    constructor(accessToken?: string);
    static getInstance(accessToken: string): fyersDataSocket;
    on(event: string, callback: (data: any) => void): void;
    subscribe(symbols: string[], allSymbols?: boolean, mode?: number): void;
    unsubscribe(symbols: string[], allSymbols?: boolean, mode?: number): void;
    mode(modeConstant: any, mode: number): void;
    connect(): void;
    close(): void;
    disconnect(): void;
    autoreconnect(): void;
    LiteMode: any;
    QuoteMode: any;
    FullMode: any;
  }

  export class fyersOrderSocket {
    constructor();
    on(event: string, callback: (data: any) => void): void;
    connect(): void;
    disconnect(): void;
    autoreconnect(): void;
  }
}
