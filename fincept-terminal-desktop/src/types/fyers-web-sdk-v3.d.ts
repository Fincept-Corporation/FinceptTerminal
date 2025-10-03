declare module 'fyers-web-sdk-v3' {
  export class fyersModel {
    constructor(config?: any);
    setAccessToken(token: string): void;
    get_profile(): Promise<any>;
    get_funds(): Promise<any>;
    get_holdings(): Promise<any>;
    get_positions(): Promise<any>;
    get_orders(): Promise<any>;
    place_order(orderData: any): Promise<any>;
    modify_order(orderData: any): Promise<any>;
    cancel_order(orderId: string): Promise<any>;
    get_quotes(symbols: any): Promise<any>;
    get_market_depth(symbol: any): Promise<any>;
    get_history(params: any): Promise<any>;
    get_option_chain(params: any): Promise<any>;
  }

  export class fyersOrderSocket {
    constructor(accessToken: string);
    on(event: string, callback: (data: any) => void): void;
    subscribe(channels: any[]): void;
    unsubscribe(channels: any[]): void;
    disconnect(): void;
    autoreconnect(enable?: boolean): void;
    orderUpdates: any;
    tradeUpdates: any;
    positionUpdates: any;
    edis: any;
    pricealerts: any;
  }

  export class fyersDataSocket {
    constructor(accessToken: string);
    on(event: string, callback: (data: any) => void): void;
    subscribe(symbols: any[]): void;
    unsubscribe(symbols: any[]): void;
    disconnect(): void;
    autoreconnect(enable?: boolean): void;
    symbolData: any;
    symbolUpdate: any;
    ltpc: any;
    depth: any;
  }
}
