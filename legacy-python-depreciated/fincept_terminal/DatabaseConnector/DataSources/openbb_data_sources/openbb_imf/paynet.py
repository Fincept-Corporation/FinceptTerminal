from fastapi import FastAPI, HTTPException, Depends, Form
from fastapi.responses import HTMLResponse
from fastapi.security import HTTPBearer, HTTPAuthorizationCredentials
from pydantic import BaseModel, EmailStr
import duckdb
import hashlib
import secrets
import jwt
from datetime import datetime, timedelta
import requests
from typing import Optional
import uvicorn

app = FastAPI(title="Fincept Payment System")
security = HTTPBearer()


# Database setup
def init_db():
    conn = duckdb.connect("fincept.db")
    conn.execute("""
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY,
            email VARCHAR UNIQUE NOT NULL,
            password_hash VARCHAR NOT NULL,
            full_name VARCHAR NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            subscription_plan VARCHAR DEFAULT NULL,
            subscription_status VARCHAR DEFAULT 'inactive'
        )
    """)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS payments (
            id INTEGER PRIMARY KEY,
            user_id INTEGER,
            plan_name VARCHAR NOT NULL,
            amount DECIMAL(10,2) NOT NULL,
            payment_id VARCHAR UNIQUE,
            status VARCHAR DEFAULT 'pending',
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (user_id) REFERENCES users(id)
        )
    """)
    conn.close()


init_db()


# Models
class UserCreate(BaseModel):
    email: EmailStr
    password: str
    full_name: str


class UserLogin(BaseModel):
    email: EmailStr
    password: str


class PaymentPlan(BaseModel):
    name: str
    price: float
    features: list[str]


# Payment plans
PAYMENT_PLANS = [
    PaymentPlan(name="Basic", price=20.0, features=["Real-time data", "Basic analytics", "Email support"]),
    PaymentPlan(name="Professional", price=50.0,
                features=["Real-time data", "Advanced analytics", "API access", "Priority support"]),
    PaymentPlan(name="Enterprise", price=100.0,
                features=["Real-time data", "Custom analytics", "Full API access", "24/7 support",
                          "Custom integration"]),
    PaymentPlan(name="Premium", price=200.0,
                features=["All Enterprise features", "Dedicated account manager", "Custom reporting", "SLA guarantee"])
]


# Utility functions
def hash_password(password: str) -> str:
    return hashlib.sha256(password.encode()).hexdigest()


def verify_password(password: str, hashed: str) -> bool:
    return hash_password(password) == hashed


def create_token(user_id: int) -> str:
    payload = {
        "user_id": user_id,
        "exp": datetime.utcnow() + timedelta(hours=24)
    }
    return jwt.encode(payload, "your-secret-key", algorithm="HS256")


def verify_token(credentials: HTTPAuthorizationCredentials = Depends(security)):
    try:
        payload = jwt.decode(credentials.credentials, "your-secret-key", algorithms=["HS256"])
        return payload["user_id"]
    except jwt.ExpiredSignatureError:
        raise HTTPException(status_code=401, detail="Token expired")
    except jwt.InvalidTokenError:
        raise HTTPException(status_code=401, detail="Invalid token")


def get_db():
    return duckdb.connect("fincept.db")


# Dodo Payment Integration
def create_dodo_payment(amount: float, plan_name: str, user_email: str):
    """Create payment with Dodo (mock implementation)"""
    payment_id = f"dodo_{secrets.token_hex(8)}"

    # Mock Dodo API call
    dodo_payload = {
        "amount": amount,
        "currency": "USD",
        "description": f"Fincept {plan_name} Plan",
        "customer_email": user_email,
        "return_url": "http://localhost:8000/payment/success",
        "cancel_url": "http://localhost:8000/payment/cancel"
    }

    # In real implementation, make actual API call to Dodo
    # response = requests.post("https://api.dodo.com/payments", json=dodo_payload)

    return {
        "payment_id": payment_id,
        "payment_url": f"https://pay.dodo.com/checkout/{payment_id}",
        "status": "pending"
    }


# Routes
@app.get("/", response_class=HTMLResponse)
async def home():
    return """
    <html>
        <head><title>Fincept Payment System</title></head>
        <body style="font-family: Arial; max-width: 800px; margin: 0 auto; padding: 20px;">
            <h1>Fincept Financial Data Platform</h1>
            <h2>Get Started</h2>
            <div style="background: #f5f5f5; padding: 20px; border-radius: 8px;">
                <h3>Create Account</h3>
                <form action="/register" method="post">
                    <input type="email" name="email" placeholder="Email" required style="width: 300px; padding: 8px; margin: 5px;"><br>
                    <input type="password" name="password" placeholder="Password" required style="width: 300px; padding: 8px; margin: 5px;"><br>
                    <input type="text" name="full_name" placeholder="Full Name" required style="width: 300px; padding: 8px; margin: 5px;"><br>
                    <button type="submit" style="padding: 10px 20px; background: #007bff; color: white; border: none; border-radius: 4px;">Register</button>
                </form>
            </div>
            <div style="background: #e9ecef; padding: 20px; border-radius: 8px; margin-top: 20px;">
                <h3>Login</h3>
                <form action="/login" method="post">
                    <input type="email" name="email" placeholder="Email" required style="width: 300px; padding: 8px; margin: 5px;"><br>
                    <input type="password" name="password" placeholder="Password" required style="width: 300px; padding: 8px; margin: 5px;"><br>
                    <button type="submit" style="padding: 10px 20px; background: #28a745; color: white; border: none; border-radius: 4px;">Login</button>
                </form>
            </div>
        </body>
    </html>
    """


@app.post("/register")
async def register(email: str = Form(...), password: str = Form(...), full_name: str = Form(...)):
    conn = get_db()

    # Check if user exists
    existing = conn.execute("SELECT id FROM users WHERE email = ?", [email]).fetchone()
    if existing:
        raise HTTPException(status_code=400, detail="Email already registered")

    # Create user
    password_hash = hash_password(password)
    conn.execute(
        "INSERT INTO users (email, password_hash, full_name) VALUES (?, ?, ?)",
        [email, password_hash, full_name]
    )

    user = conn.execute("SELECT id FROM users WHERE email = ?", [email]).fetchone()
    conn.close()

    token = create_token(user[0])

    return HTMLResponse(f"""
    <html>
        <head><title>Registration Success</title></head>
        <body style="font-family: Arial; max-width: 800px; margin: 0 auto; padding: 20px;">
            <h1>Welcome to Fincept!</h1>
            <p>Account created successfully for {email}</p>
            <p>Your access token: <code>{token}</code></p>
            <a href="/plans" style="display: inline-block; padding: 10px 20px; background: #007bff; color: white; text-decoration: none; border-radius: 4px;">View Payment Plans</a>
        </body>
    </html>
    """)


@app.post("/login")
async def login(email: str = Form(...), password: str = Form(...)):
    conn = get_db()
    user = conn.execute("SELECT id, password_hash FROM users WHERE email = ?", [email]).fetchone()
    conn.close()

    if not user or not verify_password(password, user[1]):
        raise HTTPException(status_code=401, detail="Invalid credentials")

    token = create_token(user[0])

    return HTMLResponse(f"""
    <html>
        <head><title>Login Success</title></head>
        <body style="font-family: Arial; max-width: 800px; margin: 0 auto; padding: 20px;">
            <h1>Welcome back!</h1>
            <p>Login successful for {email}</p>
            <p>Your access token: <code>{token}</code></p>
            <a href="/plans" style="display: inline-block; padding: 10px 20px; background: #007bff; color: white; text-decoration: none; border-radius: 4px;">View Payment Plans</a>
        </body>
    </html>
    """)


@app.get("/plans", response_class=HTMLResponse)
async def get_plans():
    plans_html = ""
    for plan in PAYMENT_PLANS:
        features_html = "".join([f"<li>{feature}</li>" for feature in plan.features])
        plans_html += f"""
        <div style="border: 1px solid #ddd; padding: 20px; margin: 10px; border-radius: 8px; display: inline-block; width: 300px; vertical-align: top;">
            <h3>{plan.name}</h3>
            <h2>${plan.price}/month</h2>
            <ul>{features_html}</ul>
            <form action="/subscribe/{plan.name.lower()}" method="post">
                <button type="submit" style="width: 100%; padding: 12px; background: #28a745; color: white; border: none; border-radius: 4px; font-size: 16px;">
                    Subscribe to {plan.name}
                </button>
            </form>
        </div>
        """

    return f"""
    <html>
        <head><title>Payment Plans - Fincept</title></head>
        <body style="font-family: Arial; max-width: 1200px; margin: 0 auto; padding: 20px;">
            <h1>Choose Your Plan</h1>
            <p>Select the perfect plan for your financial data needs:</p>
            <div style="text-align: center;">
                {plans_html}
            </div>
            <p style="text-align: center; margin-top: 30px;">
                <small>All plans include 14-day free trial. Cancel anytime.</small>
            </p>
        </body>
    </html>
    """


@app.post("/subscribe/{plan_name}")
async def subscribe(plan_name: str, user_id: int = Depends(verify_token)):
    # Find the plan
    plan = next((p for p in PAYMENT_PLANS if p.name.lower() == plan_name), None)
    if not plan:
        raise HTTPException(status_code=404, detail="Plan not found")

    conn = get_db()
    user = conn.execute("SELECT email FROM users WHERE id = ?", [user_id]).fetchone()

    # Create payment with Dodo
    dodo_payment = create_dodo_payment(plan.price, plan.name, user[0])

    # Save payment record
    conn.execute(
        "INSERT INTO payments (id, user_id, plan_name, amount, payment_id, status) VALUES (nextval('payments_id_seq'), ?, ?, ?, ?, ?)",
        [user_id, plan.name, plan.price, dodo_payment["payment_id"], "pending"]
    )
    conn.close()

    return HTMLResponse(f"""
    <html>
        <head><title>Payment Processing</title></head>
        <body style="font-family: Arial; max-width: 800px; margin: 0 auto; padding: 20px;">
            <h1>Complete Your Payment</h1>
            <p>You're subscribing to: <strong>{plan.name} Plan (${plan.price}/month)</strong></p>
            <p>Payment ID: {dodo_payment["payment_id"]}</p>

            <div style="background: #f8f9fa; padding: 20px; border-radius: 8px; margin: 20px 0;">
                <h3>Payment Instructions:</h3>
                <p>Click the button below to complete your payment securely with Dodo:</p>
                <a href="{dodo_payment["payment_url"]}" target="_blank" 
                   style="display: inline-block; padding: 15px 30px; background: #007bff; color: white; text-decoration: none; border-radius: 6px; font-size: 18px;">
                    Pay ${plan.price} with Dodo
                </a>
            </div>

            <p><small>You will be redirected to Dodo's secure payment page. After successful payment, you'll have full access to your plan features.</small></p>
        </body>
    </html>
    """)


@app.get("/payment/success")
async def payment_success():
    return HTMLResponse("""
    <html>
        <head><title>Payment Successful</title></head>
        <body style="font-family: Arial; max-width: 800px; margin: 0 auto; padding: 20px; text-align: center;">
            <h1 style="color: #28a745;">Payment Successful!</h1>
            <p>Thank you for subscribing to Fincept. Your account has been activated.</p>
            <p>You now have access to all plan features.</p>
            <a href="/dashboard" style="display: inline-block; padding: 12px 24px; background: #007bff; color: white; text-decoration: none; border-radius: 4px;">
                Go to Dashboard
            </a>
        </body>
    </html>
    """)


@app.get("/payment/cancel")
async def payment_cancel():
    return HTMLResponse("""
    <html>
        <head><title>Payment Cancelled</title></head>
        <body style="font-family: Arial; max-width: 800px; margin: 0 auto; padding: 20px; text-align: center;">
            <h1 style="color: #dc3545;">Payment Cancelled</h1>
            <p>Your payment was cancelled. No charges were made.</p>
            <a href="/plans" style="display: inline-block; padding: 12px 24px; background: #6c757d; color: white; text-decoration: none; border-radius: 4px;">
                Back to Plans
            </a>
        </body>
    </html>
    """)


@app.get("/dashboard")
async def dashboard(user_id: int = Depends(verify_token)):
    conn = get_db()
    user_data = conn.execute(
        "SELECT email, full_name, subscription_plan, subscription_status FROM users WHERE id = ?",
        [user_id]
    ).fetchone()

    payments = conn.execute(
        "SELECT plan_name, amount, status, created_at FROM payments WHERE user_id = ? ORDER BY created_at DESC",
        [user_id]
    ).fetchall()
    conn.close()

    payments_html = ""
    for payment in payments:
        payments_html += f"<tr><td>{payment[0]}</td><td>${payment[1]}</td><td>{payment[2]}</td><td>{payment[3]}</td></tr>"

    return HTMLResponse(f"""
    <html>
        <head><title>Dashboard - Fincept</title></head>
        <body style="font-family: Arial; max-width: 1000px; margin: 0 auto; padding: 20px;">
            <h1>Dashboard</h1>
            <div style="background: #f8f9fa; padding: 20px; border-radius: 8px; margin-bottom: 20px;">
                <h2>Account Information</h2>
                <p><strong>Name:</strong> {user_data[1]}</p>
                <p><strong>Email:</strong> {user_data[0]}</p>
                <p><strong>Plan:</strong> {user_data[2] or 'No active subscription'}</p>
                <p><strong>Status:</strong> {user_data[3]}</p>
            </div>

            <div style="background: white; border: 1px solid #ddd; border-radius: 8px;">
                <h2 style="padding: 20px; margin: 0; border-bottom: 1px solid #ddd;">Payment History</h2>
                <table style="width: 100%; border-collapse: collapse;">
                    <thead>
                        <tr style="background: #f8f9fa;">
                            <th style="padding: 12px; text-align: left; border-bottom: 1px solid #ddd;">Plan</th>
                            <th style="padding: 12px; text-align: left; border-bottom: 1px solid #ddd;">Amount</th>
                            <th style="padding: 12px; text-align: left; border-bottom: 1px solid #ddd;">Status</th>
                            <th style="padding: 12px; text-align: left; border-bottom: 1px solid #ddd;">Date</th>
                        </tr>
                    </thead>
                    <tbody>
                        {payments_html}
                    </tbody>
                </table>
            </div>

            <div style="margin-top: 20px;">
                <a href="/plans" style="display: inline-block; padding: 10px 20px; background: #007bff; color: white; text-decoration: none; border-radius: 4px;">
                    Change Plan
                </a>
            </div>
        </body>
    </html>
    """)


if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8000)