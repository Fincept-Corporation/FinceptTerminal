# Fincept Terminal - Complete Context Analysis

## 📋 CURRENT AUTHENTICATION & SESSION SYSTEM

### **Authentication Flow**
1. **Login/Register** → User enters credentials
2. **OTP Verification** (for new users)
3. **API Key Generation** → Backend returns `api_key`
4. **Session Creation** → AuthContext stores session
5. **Account Type Check**:
   - Free Account → Redirect to **Pricing Screen**
   - Paid Subscription → Redirect to **Dashboard**
   - Guest Mode → Direct to **Dashboard** (24h limit)

### **User Types**
```typescript
'guest' | 'registered'
```

**Guest Users:**
- Device-based authentication
- 24-hour session expiry
- Daily API call limits (50 calls/day)
- No payment required
- Device ID generated from browser fingerprint

**Registered Users:**
- Email/password authentication
- Persistent sessions
- Account types: `free`, `starter`, `professional`, `enterprise`, `unlimited`
- Subscription management
- Payment integration

### **Session Data Structure**
```typescript
interface SessionData {
  authenticated: boolean;
  user_type: 'guest' | 'registered';
  api_key: string | null;
  device_id: string;
  user_info?: {
    username: string;
    email: string;
    account_type: string;  // 'free', 'starter', 'professional', etc.
    credit_balance: number;
    is_verified: boolean;
    mfa_enabled: boolean;
  };
  expires_at?: string;  // For guest users
  daily_limit?: number;  // For guest users
  requests_today?: number;  // For guest users
  subscription?: {
    has_subscription: boolean;
    subscription?: {
      subscription_uuid: string;
      plan: { name, plan_id, price };
      status: string;
      current_period_start: string;
      current_period_end: string;
      days_remaining: number;
      cancel_at_period_end: boolean;
      usage: {
        api_calls_used: number;
        api_calls_limit: number | string;
        usage_percentage: number;
      };
    };
  };
}
```

---

## 🔐 CURRENT ISSUES IDENTIFIED

### **1. No Logout Functionality in Dashboard Header**
- **Problem**: Dashboard header (DashboardScreen.tsx) has no logout button
- **User Impact**: Users cannot end their session from the terminal
- **Current State**: Menu dropdowns present but no logout option

### **2. Payment Flow Redirect Errors**
- **Problem**: Payment success redirects may have issues
- **Location**: `PaymentSuccessScreen.tsx` + `App.tsx`
- **Current Flow**:
  1. User selects plan → `PricingScreen`
  2. Creates checkout session → Opens payment URL
  3. Payment completed → Redirects to `/payment-success?session_id=...`
  4. `PaymentSuccessScreen` handles verification
  5. Redirect to dashboard
- **Potential Issues**:
  - URL parameter handling
  - Session validation after payment
  - Double-redirects causing loops

### **3. No Session Awareness in Tabs**
- **Problem**: Terminal tabs don't display user session info
- **Missing Features**:
  - User type indicator (Guest vs Registered)
  - Account plan display
  - Session expiry warnings (for guests)
  - API usage stats
  - Subscription status

---

## 🎯 REQUIRED MODIFICATIONS

### **Task 1: Add Logout to Dashboard Header**
**File**: `src/components/dashboard/DashboardScreen.tsx`

**Changes Needed**:
1. Import `useAuth` hook
2. Add logout button to header dropdowns
3. Add logout handler that:
   - Calls `logout()` from AuthContext
   - Clears IndexedDB credentials (if needed)
   - Redirects to login screen

**UI Location**: Top menu bar → Add to "Session" or "File" dropdown

---

### **Task 2: Fix Payment Redirect Flow**
**Files**:
- `src/components/payment/PaymentSuccessScreen.tsx`
- `src/App.tsx`

**Current Issues**:
```typescript
// In PaymentSuccessScreen.tsx:
// Line 92-94: onComplete() callback
const handleContinueToDashboard = () => {
  onComplete();  // Should redirect to dashboard
};

// In App.tsx:
// Line 231-238: PaymentSuccessScreen props
onComplete={() => {
  setHasChosenFreePlan(false);
  setCameFromLogin(false);
  setCurrentScreen('dashboard');  // May cause redirect loop
}}
```

**Fix Strategy**:
1. Clear URL parameters after payment success
2. Refresh session data before redirect
3. Add proper loading states
4. Handle edge cases (payment failed, session expired, etc.)

---

### **Task 3: Add Session Info to Tabs**
**Files**: All tab components need session awareness

**Required Information Display**:

**For Guest Users**:
```
┌─────────────────────────────────────┐
│ 👤 Guest User                       │
│ ⏰ Session: 18h 23m remaining       │
│ 📊 API Calls: 12/50 today          │
│ ⚠️  Limited access - Upgrade?      │
└─────────────────────────────────────┘
```

**For Registered Users (Free Plan)**:
```
┌─────────────────────────────────────┐
│ 👤 john@example.com                 │
│ 📦 Plan: Free                       │
│ 📊 API Calls: 1,247/10,000 today   │
│ 💳 Upgrade to unlock more features │
└─────────────────────────────────────┘
```

**For Registered Users (Paid Plan)**:
```
┌─────────────────────────────────────┐
│ 👤 john@example.com                 │
│ 📦 Plan: Professional ($49/mo)      │
│ 📊 API Calls: 8,234/50,000 today   │
│ ✅ Subscription: Active             │
│ 📅 Renews: Dec 31, 2025             │
└─────────────────────────────────────┘
```

**Implementation Locations**:
1. **Header/Top Bar** (all tabs): User info widget
2. **Profile/Settings Tab**: Full session details
3. **Status Bar** (optional): Minimal session info

---

## 📁 KEY FILES & THEIR ROLES

### **Authentication System**
| File | Purpose |
|------|---------|
| `AuthContext.tsx` | Global session management, login/logout/register |
| `authApi.tsx` | Backend API calls for authentication |
| `LoginScreen.tsx` | User login UI |
| `RegisterScreen.tsx` | User registration + OTP verification |
| `App.tsx` | Main routing logic, screen navigation |

### **Payment System**
| File | Purpose |
|------|---------|
| `paymentApi.tsx` | Payment API calls, subscription management |
| `PricingScreen.tsx` | Plan selection UI |
| `PaymentProcessingScreen.tsx` | Loading state during payment |
| `PaymentSuccessScreen.tsx` | Payment confirmation + redirect |
| `PaymentOverlay.tsx` | In-app payment window |

### **Dashboard System**
| File | Purpose |
|------|---------|
| `DashboardScreen.tsx` | Main terminal interface, tab navigation, header |
| `*Tab.tsx` (all tabs) | Individual terminal tabs (Markets, News, Settings, etc.) |

---

## 🛠️ IMPLEMENTATION PLAN

### **Phase 1: Add Logout Functionality** ✅
**Priority**: HIGH
**Complexity**: LOW

1. Modify `DashboardScreen.tsx`:
   - Add logout to Session dropdown
   - Call `useAuth().logout()`
   - Navigate to login screen

2. Test scenarios:
   - Registered user logout
   - Guest user logout
   - Session cleanup verification

---

### **Phase 2: Fix Payment Redirects** ✅
**Priority**: HIGH
**Complexity**: MEDIUM

1. Update `PaymentSuccessScreen.tsx`:
   - Clear URL params immediately
   - Add retry logic for API calls
   - Better error handling

2. Update `App.tsx`:
   - Fix redirect logic
   - Prevent loops
   - Add loading states

3. Test scenarios:
   - Successful payment
   - Failed payment
   - User closes payment window early
   - Network errors during verification

---

### **Phase 3: Add Session Info to Tabs** ✅
**Priority**: MEDIUM
**Complexity**: MEDIUM

1. Create reusable session info component:
   - `<SessionInfoWidget />` for header
   - `<SessionDetailsPanel />` for Profile tab

2. Update `DashboardScreen.tsx`:
   - Add session widget to header
   - Replace static "Guest (18h remaining)" text

3. Update individual tabs (optional):
   - Add contextual session warnings
   - Example: News tab shows "Upgrade for full article access"

---

## 🧪 TESTING CHECKLIST

### **Authentication Tests**
- [ ] Login with valid credentials
- [ ] Login with invalid credentials
- [ ] Guest mode access
- [ ] Session persistence after refresh
- [ ] Session expiry handling

### **Logout Tests**
- [ ] Logout from registered account
- [ ] Logout from guest account
- [ ] Session cleared from localStorage
- [ ] Credentials cleared from IndexedDB
- [ ] Redirect to login screen

### **Payment Flow Tests**
- [ ] Select free plan → Go to dashboard
- [ ] Select paid plan → Opens payment window
- [ ] Complete payment → Redirect to success screen
- [ ] Success screen → Refresh subscription data
- [ ] Success screen → Redirect to dashboard
- [ ] Cancel payment → Return to pricing screen
- [ ] Payment failure → Show error, allow retry

### **Session Display Tests**
- [ ] Guest user sees expiry countdown
- [ ] Free user sees plan upgrade prompt
- [ ] Paid user sees subscription details
- [ ] API usage stats update correctly
- [ ] Session warnings appear at 30min remaining (guests)

---

## 📊 SESSION STATE FLOW DIAGRAM

```
┌──────────────────────────────────────────────────────────────┐
│                         App Start                             │
└───────────────────────────┬──────────────────────────────────┘
                            │
                            ▼
                    ┌───────────────┐
                    │  isLoading?   │
                    └───┬───────┬───┘
                        │ Yes   │ No
                        ▼       ▼
                   Loading   Check Session
                   Screen    ┌────┴────┐
                             │         │
                      ┌──────▼─────┐  │
                      │ Guest User │  │
                      └──────┬─────┘  │
                             │        │
                             ▼        ▼
                        Dashboard  ┌────────────┐
                                  │ Registered │
                                  │    User    │
                                  └──────┬─────┘
                                         │
                           ┌─────────────┴─────────────┐
                           │                           │
                    ┌──────▼──────┐           ┌───────▼────────┐
                    │  Free Plan  │           │  Paid Plan     │
                    │             │           │  (Subscribed)  │
                    └──────┬──────┘           └───────┬────────┘
                           │                           │
                           ▼                           ▼
                    Pricing Screen              Dashboard
                           │
                    ┌──────┴──────┐
                    │             │
             ┌──────▼────┐  ┌─────▼──────┐
             │ Choose    │  │  Purchase  │
             │ Free Plan │  │  Paid Plan │
             └──────┬────┘  └─────┬──────┘
                    │              │
                    │              ▼
                    │       Payment Flow
                    │              │
                    │       ┌──────▼──────────┐
                    │       │ Payment Success │
                    │       └──────┬──────────┘
                    │              │
                    └──────────────┴─────────▼
                                      Dashboard
```

---

## 💡 RECOMMENDATIONS

### **Security**
1. ✅ Session tokens stored in localStorage (encrypted would be better)
2. ⚠️  API keys visible in network traffic (HTTPS mitigates)
3. ✅ Guest sessions expire after 24h
4. ⚠️  No automatic session refresh before expiry

### **User Experience**
1. Add session expiry warnings (30min, 5min, 1min)
2. Auto-save work before session expires
3. Seamless re-authentication flow
4. "Remember me" option for longer sessions

### **Feature Enhancements**
1. Multi-device session management
2. Session activity history
3. Force logout from all devices
4. 2FA/MFA for sensitive operations

---

## 🔄 NEXT STEPS

1. **Gather full context** ✅ (COMPLETED)
2. **Add logout button** (In progress...)
3. **Fix payment redirects** (In progress...)
4. **Add session display** (In progress...)
5. **Test all flows** (Pending...)
6. **Deploy changes** (Pending...)