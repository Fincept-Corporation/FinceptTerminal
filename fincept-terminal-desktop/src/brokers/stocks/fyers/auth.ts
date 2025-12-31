// Fyers Authentication Module
// Automated token generation flow with TOTP

import { fetch } from '@tauri-apps/plugin-http';
import * as OTPAuth from 'otpauth';
import { invoke } from '@tauri-apps/api/core';
import { FyersAuthConfig, FyersAuthResult } from './types';

export class FyersAuth {
  private logger: any;

  constructor(logger?: any) {
    this.logger = logger || {
      info: (...args: any[]) => console.log('[Fyers Auth]', ...args),
      debug: (...args: any[]) => console.log('[Fyers Auth]', ...args),
      error: (...args: any[]) => console.error('[Fyers Auth]', ...args),
    };
  }

  /**
   * Automated Fyers authentication flow
   * Follows the complete OAuth flow with TOTP
   */
  async authenticate(config: FyersAuthConfig): Promise<FyersAuthResult> {
    this.logger.info('Starting authentication...');

    try {
      const requestKey = await this.sendLoginOTP(config.clientId);
      const token1 = await this.verifyOTPAndPIN(requestKey, config);
      const authCode = await this.getAuthorizationCode(token1, config);
      const accessToken = await this.validateAuthCode(authCode, config);

      const connectionString = `${config.appId}-${config.appType}:${accessToken}`;

      this.logger.info('Authentication successful!');

      return {
        accessToken,
        connectionString
      };
    } catch (error: any) {
      this.logger.error('Authentication failed:', error);
      throw new Error(error.message || 'Authentication failed');
    }
  }

  /**
   * Step 1: Send login OTP
   */
  private async sendLoginOTP(clientId: string): Promise<string> {
    this.logger.debug('Step 1: Sending login OTP...');

    const response = await fetch('https://api-t2.fyers.in/vagator/v2/send_login_otp', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({
        fy_id: clientId,
        app_id: '2'
      })
    });

    const data = await response.json();
    this.logger.debug('OTP Response status:', response.status);

    if (response.status !== 200 || !data.request_key) {
      throw new Error(`Failed to send OTP: ${JSON.stringify(data)}`);
    }

    this.logger.info('OTP sent successfully');
    return data.request_key;
  }

  /**
   * Step 2 & 3: Generate TOTP, verify, and verify PIN
   */
  private async verifyOTPAndPIN(requestKey: string, config: FyersAuthConfig): Promise<string> {
    this.logger.debug('Step 2: Generating TOTP...');

    const totpGenerator = new OTPAuth.TOTP({
      secret: config.totpKey
    });
    const totp = totpGenerator.generate();
    this.logger.debug('TOTP generated');

    let response = await fetch('https://api-t2.fyers.in/vagator/v2/verify_otp', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({
        request_key: requestKey,
        otp: totp
      })
    });

    let data = await response.json();
    this.logger.debug('Verify OTP Response status:', response.status);

    if (response.status !== 200 || !data.request_key) {
      throw new Error(`Failed to verify OTP: ${JSON.stringify(data)}`);
    }

    this.logger.info('TOTP verified successfully');

    this.logger.debug('Step 3: Verifying PIN...');
    response = await fetch('https://api-t2.fyers.in/vagator/v2/verify_pin', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({
        request_key: data.request_key,
        identity_type: 'pin',
        identifier: config.pin
      })
    });

    data = await response.json();
    this.logger.debug('Verify PIN Response status:', response.status);

    if (response.status !== 200 || !data.data?.access_token) {
      throw new Error(`Failed to verify PIN: ${JSON.stringify(data)}`);
    }

    this.logger.info('PIN verified successfully');
    return data.data.access_token;
  }

  /**
   * Step 4: Get authorization code
   */
  private async getAuthorizationCode(token1: string, config: FyersAuthConfig): Promise<string> {
    this.logger.debug('Step 4: Getting authorization code...');

    const response = await fetch('https://api-t1.fyers.in/api/v3/token', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Authorization': `Bearer ${token1}`
      },
      body: JSON.stringify({
        fyers_id: config.clientId,
        app_id: config.appId,
        redirect_uri: 'https://trade.fyers.in/api-login/redirect-uri/index.html',
        appType: config.appType,
        code_challenge: '',
        state: 'sample_state',
        scope: '',
        nonce: '',
        response_type: 'code',
        create_cookie: true
      })
    });

    const data = await response.json();
    this.logger.debug('Token API Response status:', response.status);

    if (response.status !== 308 || !data?.Url) {
      throw new Error(`Expected HTTP 308 but got ${response.status}: ${JSON.stringify(data)}`);
    }

    const authUrl = new URL(data.Url);
    const authCode = authUrl.searchParams.get('auth_code');

    if (!authCode) {
      throw new Error('No auth_code in response URL');
    }

    this.logger.info('Authorization code obtained');
    return authCode;
  }

  /**
   * Step 5: Validate auth code and get final access token
   */
  private async validateAuthCode(authCode: string, config: FyersAuthConfig): Promise<string> {
    this.logger.debug('Step 5: Validating auth code...');

    const appIdHash = await invoke<string>('sha256_hash', {
      input: `${config.appId}-${config.appType}:${config.appSecret}`
    });

    this.logger.debug('Generated appIdHash');

    const response = await fetch('https://api-t1.fyers.in/api/v3/validate-authcode', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({
        grant_type: 'authorization_code',
        appIdHash: appIdHash,
        code: authCode
      })
    });

    const data = await response.json();
    this.logger.debug('Validate Auth Code Response status:', response.status);

    if (response.status !== 200 || !data.access_token) {
      throw new Error(`Failed to validate auth code: ${JSON.stringify(data)}`);
    }

    this.logger.info('Final access token obtained');
    return data.access_token;
  }
}
