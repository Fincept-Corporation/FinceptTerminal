// Fyers Authentication Service
// Automated token generation flow

import { fetch } from '@tauri-apps/plugin-http';
import * as OTPAuth from 'otpauth';
import { invoke } from '@tauri-apps/api/core';

export interface FyersAuthConfig {
  clientId: string;
  pin: string;
  appId: string;
  appType: string;
  appSecret: string;
  totpKey: string;
}

export interface FyersAuthResult {
  accessToken: string;
  connectionString: string; // APP_ID-APP_TYPE:ACCESS_TOKEN format for WebSocket
}

/**
 * Automated Fyers authentication flow
 * Follows the complete OAuth flow with TOTP
 */
export async function authenticateFyers(config: FyersAuthConfig): Promise<FyersAuthResult> {
  console.log('[FyersAuth] Starting authentication...');

  try {
    // Step 1: Send login OTP
    console.log('[FyersAuth] Step 1: Sending login OTP...');
    let response = await fetch('https://api-t2.fyers.in/vagator/v2/send_login_otp', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({
        fy_id: config.clientId,
        app_id: '2'
      })
    });

    let data = await response.json();
    console.log('[FyersAuth] OTP Response status:', response.status);
    console.log('[FyersAuth] OTP Response data:', data);

    if (response.status !== 200) {
      throw new Error(`Failed to send OTP: HTTP ${response.status}`);
    }

    if (!data.request_key) {
      throw new Error(`Failed to send OTP: ${JSON.stringify(data)}`);
    }

    let requestKey = data.request_key;
    console.log('[FyersAuth] ✓ OTP sent successfully');

    // Step 2: Generate TOTP and verify
    console.log('[FyersAuth] Step 2: Generating TOTP...');
    const totpGenerator = new OTPAuth.TOTP({
      secret: config.totpKey
    });
    const totp = totpGenerator.generate();
    console.log('[FyersAuth] TOTP generated:', totp);

    response = await fetch('https://api-t2.fyers.in/vagator/v2/verify_otp', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({
        request_key: requestKey,
        otp: totp
      })
    });

    data = await response.json();
    console.log('[FyersAuth] Verify OTP Response status:', response.status);
    console.log('[FyersAuth] Verify OTP Response data:', data);

    if (response.status !== 200) {
      throw new Error(`Failed to verify OTP: HTTP ${response.status}`);
    }

    if (!data.request_key) {
      throw new Error(`Failed to verify OTP: ${JSON.stringify(data)}`);
    }

    requestKey = data.request_key;
    console.log('[FyersAuth] ✓ TOTP verified successfully');

    // Step 3: Verify PIN
    console.log('[FyersAuth] Step 3: Verifying PIN...');
    response = await fetch('https://api-t2.fyers.in/vagator/v2/verify_pin', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({
        request_key: requestKey,
        identity_type: 'pin',
        identifier: config.pin
      })
    });

    data = await response.json();
    console.log('[FyersAuth] Verify PIN Response status:', response.status);
    console.log('[FyersAuth] Verify PIN Response data:', data);

    if (response.status !== 200) {
      throw new Error(`Failed to verify PIN: HTTP ${response.status}`);
    }

    if (!data.data?.access_token) {
      throw new Error(`Failed to verify PIN: ${JSON.stringify(data)}`);
    }

    const token1 = data.data.access_token;
    console.log('[FyersAuth] ✓ PIN verified successfully');

    // Step 4: Get authorization code (expects HTTP 308 redirect)
    console.log('[FyersAuth] Step 4: Getting authorization code...');
    response = await fetch('https://api-t1.fyers.in/api/v3/token', {
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

    data = await response.json();
    console.log('[FyersAuth] Token API Response status:', response.status);
    console.log('[FyersAuth] Token API Response data:', data);

    if (response.status !== 308) {
      throw new Error(`Expected HTTP 308 but got ${response.status}: ${JSON.stringify(data)}`);
    }

    if (!data?.Url) {
      throw new Error('Failed to get authorization URL');
    }

    const authUrl = new URL(data.Url);
    const authCode = authUrl.searchParams.get('auth_code');

    if (!authCode) {
      throw new Error('No auth_code in response URL');
    }

    console.log('[FyersAuth] ✓ Authorization code obtained');

    // Step 5: Generate app hash and validate auth code
    console.log('[FyersAuth] Step 5: Validating auth code...');
    const appIdHash = await invoke<string>('sha256_hash', {
      input: `${config.appId}-${config.appType}:${config.appSecret}`
    });

    console.log('[FyersAuth] Generated appIdHash');

    response = await fetch('https://api-t1.fyers.in/api/v3/validate-authcode', {
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

    data = await response.json();
    console.log('[FyersAuth] Validate Auth Code Response status:', response.status);
    console.log('[FyersAuth] Validate Auth Code Response data:', data);

    if (response.status !== 200) {
      throw new Error(`Failed to validate auth code: HTTP ${response.status}`);
    }

    if (!data.access_token) {
      throw new Error(`Failed to validate auth code: ${JSON.stringify(data)}`);
    }

    const accessToken = data.access_token;
    const connectionString = `${config.appId}-${config.appType}:${accessToken}`;

    console.log('[FyersAuth] ✓ Authentication successful!');
    console.log('[FyersAuth] Final access token obtained');

    return {
      accessToken,
      connectionString
    };
  } catch (error: any) {
    console.error('[FyersAuth] Authentication failed:', error);
    throw new Error(error.message || 'Authentication failed');
  }
}
