const axios = require("axios");
const { authenticator } = require("otplib");
const crypto = require("crypto");
const { JSDOM } = require("jsdom");

// Mock browser environment
const dom = new JSDOM("", { url: "http://localhost", pretendToBeVisual: true });
global.window = dom.window;
global.document = window.document;
global.navigator = window.navigator;
global.WebSocket = require("ws");

const { fyersDataSocket, fyersOrderSocket } = require("fyers-web-sdk-v3");

// Configuration
const CONFIG = {
  CLIENT_ID: "XB02507",
  PIN: "1590",
  APP_ID: "NDM5TDN9DB",
  APP_TYPE: "100",
  APP_SECRET: "UR9IJU1U0E",
  TOTP_KEY: "XOYMD3QRDV6QPNSWK3WSKMD642KGODJQ",
};

async function authenticate() {
  let res = await axios.post("https://api-t2.fyers.in/vagator/v2/send_login_otp", {
    fy_id: CONFIG.CLIENT_ID,
    app_id: "2",
  });
  let key = res.data.request_key;

  const totp = authenticator.generate(CONFIG.TOTP_KEY);
  res = await axios.post("https://api-t2.fyers.in/vagator/v2/verify_otp", {
    request_key: key,
    otp: totp,
  });
  key = res.data.request_key;

  res = await axios.post("https://api-t2.fyers.in/vagator/v2/verify_pin", {
    request_key: key,
    identity_type: "pin",
    identifier: CONFIG.PIN,
  });
  const token1 = res.data.data.access_token;

  try {
    res = await axios.post(
      "https://api-t1.fyers.in/api/v3/token",
      {
        fyers_id: CONFIG.CLIENT_ID,
        app_id: CONFIG.APP_ID,
        redirect_uri: "https://trade.fyers.in/api-login/redirect-uri/index.html",
        appType: CONFIG.APP_TYPE,
        code_challenge: "",
        state: "sample_state",
        scope: "",
        nonce: "",
        response_type: "code",
        create_cookie: true,
      },
      { headers: { Authorization: `Bearer ${token1}` }, validateStatus: () => true }
    );
  } catch (error) {
    res = error.response;
  }

  const authCode = new URL(res.data.Url).searchParams.get("auth_code");
  const hash = crypto.createHash("sha256").update(`${CONFIG.APP_ID}-${CONFIG.APP_TYPE}:${CONFIG.APP_SECRET}`).digest("hex");

  res = await axios.post("https://api-t1.fyers.in/api/v3/validate-authcode", {
    grant_type: "authorization_code",
    appIdHash: hash,
    code: authCode,
  });

  return `${CONFIG.APP_ID}-${CONFIG.APP_TYPE}:${res.data.access_token}`;
}

async function main() {
  const accessToken = await authenticate();
  console.log("Authenticated successfully\n");

  // === MARKET DATA WebSocket ===
  const fyersData = new fyersDataSocket(accessToken);

  fyersData.on("connect", () => {
    console.log("Market Data WebSocket CONNECTED\n");
    fyersData.subscribe(["NSE:PCJEWELLER-EQ"]);
  });

  fyersData.on("message", (msg) => {
    console.log("\n=== MARKET DATA ===");
    console.log(JSON.stringify(msg, null, 2));
    console.log("===================\n");
  });

  fyersData.on("error", (err) => {
    console.log("\n=== MARKET DATA ERROR ===");
    console.log(JSON.stringify(err, null, 2));
    console.log("=========================\n");
  });

  fyersData.autoreconnect();
  fyersData.connect();

  // === ORDER WebSocket ===
  const fyersOrderdata = new fyersOrderSocket(accessToken);

  fyersOrderdata.on("error", (errmsg) => {
    console.log("\n=== ORDER ERROR ===");
    console.log(JSON.stringify(errmsg, null, 2));
    console.log("===================\n");
  });

  fyersOrderdata.on("general", (msg) => {
    console.log("\n=== ORDER GENERAL ===");
    console.log(JSON.stringify(msg, null, 2));
    console.log("=====================\n");
  });

  fyersOrderdata.on("connect", () => {
    console.log("Order WebSocket CONNECTED\n");
    fyersOrderdata.subscribe([
      fyersOrderdata.orderUpdates,
      fyersOrderdata.tradeUpdates,
      fyersOrderdata.positionUpdates,
      fyersOrderdata.edis,
      fyersOrderdata.pricealerts
    ]);
  });

  fyersOrderdata.on("close", () => {
    console.log("\n=== ORDER WEBSOCKET CLOSED ===\n");
  });

  fyersOrderdata.on("orders", (msg) => {
    console.log("\n=== ORDER UPDATE ===");
    console.log(JSON.stringify(msg, null, 2));
    console.log("====================\n");
  });

  fyersOrderdata.on("trades", (msg) => {
    console.log("\n=== TRADE UPDATE ===");
    console.log(JSON.stringify(msg, null, 2));
    console.log("====================\n");
  });

  fyersOrderdata.on("positions", (msg) => {
    console.log("\n=== POSITION UPDATE ===");
    console.log(JSON.stringify(msg, null, 2));
    console.log("=======================\n");
  });

  fyersOrderdata.autoreconnect();
  fyersOrderdata.connect();

  process.on("SIGINT", () => {
    console.log("\nDisconnecting...");
    process.exit(0);
  });
}

main().catch(console.error);