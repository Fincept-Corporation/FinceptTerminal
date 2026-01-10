/**
 * Login Audit Dashboard
 * ======================
 * Admin dashboard for monitoring user login activity and security events.
 * 
 * Features:
 * - Real-time login activity feed
 * - Security alerts (brute force, suspicious IPs)
 * - DAU/MAU metrics
 * - Geographic distribution
 * - Failed login analysis
 */

import React, { useState, useEffect } from 'react';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { useAuth } from '@/contexts/AuthContext';
import { useTranslation } from 'react-i18next';

// Types
interface LoginEvent {
    logId: string;
    userId: string | null;
    email?: string;
    eventType: 'LOGIN_SUCCESS' | 'LOGIN_FAILURE' | 'LOGOUT' | 'SUSPICIOUS_BLOCK';
    timestamp: string;
    ipAddress: string;
    geoCountry?: string;
    geoCity?: string;
    deviceId?: string;
    userAgent?: string;
    clientVersion?: string;
    metadata?: Record<string, unknown>;
}

interface SecurityAlert {
    id: string;
    type: 'brute_force' | 'impossible_travel' | 'credential_stuffing' | 'suspicious_device';
    severity: 'low' | 'medium' | 'high' | 'critical';
    description: string;
    ipAddress?: string;
    userId?: string;
    timestamp: string;
    details: Record<string, unknown>;
}

interface MetricsSummary {
    dauToday: number;
    dauYesterday: number;
    mauCurrent: number;
    loginSuccessRate: number;
    failedAttemptsLast24h: number;
    activeSessionsCount: number;
    uniqueIpsToday: number;
}

// Mock data for demo (replace with API calls)
const mockLoginEvents: LoginEvent[] = [
    {
        logId: '1',
        userId: 'user-123',
        email: 'john@example.com',
        eventType: 'LOGIN_SUCCESS',
        timestamp: new Date().toISOString(),
        ipAddress: '192.168.1.1',
        geoCountry: 'US',
        geoCity: 'New York',
        clientVersion: '3.1.4',
    },
    {
        logId: '2',
        userId: null,
        email: 'unknown@test.com',
        eventType: 'LOGIN_FAILURE',
        timestamp: new Date(Date.now() - 5 * 60000).toISOString(),
        ipAddress: '10.0.0.5',
        geoCountry: 'CN',
        metadata: { failure_reason: 'bad_password' },
    },
];

const mockAlerts: SecurityAlert[] = [
    {
        id: 'alert-1',
        type: 'brute_force',
        severity: 'high',
        description: '12 failed attempts from IP 10.0.0.5 in 10 minutes',
        ipAddress: '10.0.0.5',
        timestamp: new Date(Date.now() - 2 * 60000).toISOString(),
        details: { attempts: 12, duration_minutes: 10 },
    },
];

const mockMetrics: MetricsSummary = {
    dauToday: 1247,
    dauYesterday: 1189,
    mauCurrent: 8432,
    loginSuccessRate: 94.7,
    failedAttemptsLast24h: 342,
    activeSessionsCount: 567,
    uniqueIpsToday: 892,
};

type DashboardView = 'overview' | 'events' | 'alerts' | 'analytics';

const LoginAuditDashboard: React.FC = () => {
    const { t } = useTranslation('loginAudit');
    const { colors } = useTerminalTheme();
    const { session } = useAuth();
    const [currentView, setCurrentView] = useState<DashboardView>('overview');
    const [loginEvents, setLoginEvents] = useState<LoginEvent[]>(mockLoginEvents);
    const [alerts, setAlerts] = useState<SecurityAlert[]>(mockAlerts);
    const [metrics, setMetrics] = useState<MetricsSummary>(mockMetrics);
    const [loading, setLoading] = useState(false);
    const [autoRefresh, setAutoRefresh] = useState(true);
    const [currentTime, setCurrentTime] = useState(new Date());

    // Auto-refresh timer
    useEffect(() => {
        const timer = setInterval(() => setCurrentTime(new Date()), 1000);
        return () => clearInterval(timer);
    }, []);

    // Auto-refresh data
    useEffect(() => {
        if (!autoRefresh) return;
        const interval = setInterval(() => {
            // In production, fetch from API
            console.log('Auto-refreshing dashboard data...');
        }, 30000);
        return () => clearInterval(interval);
    }, [autoRefresh]);

    const formatTimestamp = (ts: string) => {
        const date = new Date(ts);
        return date.toLocaleTimeString('en-US', { hour12: false });
    };

    const formatDate = (ts: string) => {
        return new Date(ts).toLocaleDateString('en-US', {
            month: 'short',
            day: 'numeric',
            hour: '2-digit',
            minute: '2-digit',
        });
    };

    const getEventColor = (eventType: string) => {
        switch (eventType) {
            case 'LOGIN_SUCCESS': return colors.secondary;
            case 'LOGIN_FAILURE': return colors.alert;
            case 'LOGOUT': return colors.textMuted;
            case 'SUSPICIOUS_BLOCK': return colors.warning;
            default: return colors.text;
        }
    };

    const getSeverityColor = (severity: string) => {
        switch (severity) {
            case 'critical': return '#FF0000';
            case 'high': return colors.alert;
            case 'medium': return colors.warning;
            case 'low': return colors.info;
            default: return colors.textMuted;
        }
    };

    const renderMetricCard = (label: string, value: string | number, change?: number, color?: string) => (
        <div style={{
            backgroundColor: colors.panel,
            border: `1px solid ${colors.textMuted}`,
            padding: '8px',
            flex: 1,
            minWidth: '120px',
        }}>
            <div style={{ color: colors.textMuted, fontSize: '11px', marginBottom: '4px' }}>{label}</div>
            <div style={{ color: color || colors.text, fontSize: '18px', fontWeight: 'bold' }}>{value}</div>
            {change !== undefined && (
                <div style={{
                    color: change >= 0 ? colors.secondary : colors.alert,
                    fontSize: '11px',
                    marginTop: '2px'
                }}>
                    {change >= 0 ? '↑' : '↓'} {Math.abs(change).toFixed(1)}%
                </div>
            )}
        </div>
    );

    const renderOverview = () => {
        const dauChange = ((metrics.dauToday - metrics.dauYesterday) / metrics.dauYesterday) * 100;

        return (
            <div style={{ display: 'flex', flexDirection: 'column', gap: '8px', height: '100%' }}>
                {/* Metrics Row */}
                <div style={{ display: 'flex', gap: '4px', flexWrap: 'wrap' }}>
                    {renderMetricCard(t('metrics.dauToday'), metrics.dauToday.toLocaleString(), dauChange, colors.accent)}
                    {renderMetricCard(t('metrics.mau'), metrics.mauCurrent.toLocaleString())}
                    {renderMetricCard(t('metrics.successRate'), `${metrics.loginSuccessRate}%`, undefined, colors.secondary)}
                    {renderMetricCard(t('metrics.failed24h'), metrics.failedAttemptsLast24h.toLocaleString(), undefined, colors.alert)}
                    {renderMetricCard(t('metrics.activeSessions'), metrics.activeSessionsCount.toLocaleString())}
                    {renderMetricCard(t('metrics.uniqueIps'), metrics.uniqueIpsToday.toLocaleString())}
                </div>

                <div style={{ display: 'flex', gap: '4px', flex: 1 }}>
                    {/* Recent Activity */}
                    <div style={{ flex: 2, backgroundColor: colors.panel, border: `1px solid ${colors.textMuted}`, padding: '8px', overflow: 'auto' }}>
                        <div style={{ color: colors.primary, fontSize: '12px', fontWeight: 'bold', marginBottom: '8px' }}>
                            {t('activity.recentLoginActivity')}
                        </div>
                        {loginEvents.slice(0, 10).map((event, idx) => (
                            <div key={event.logId} style={{
                                display: 'flex',
                                alignItems: 'center',
                                gap: '8px',
                                padding: '4px',
                                backgroundColor: idx % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent',
                                borderLeft: `2px solid ${getEventColor(event.eventType)}`,
                                marginBottom: '2px',
                                fontSize: '11px',
                            }}>
                                <span style={{ color: colors.textMuted, width: '60px' }}>{formatTimestamp(event.timestamp)}</span>
                                <span style={{
                                    color: getEventColor(event.eventType),
                                    width: '80px',
                                    fontWeight: 'bold'
                                }}>
                                    {event.eventType.replace('LOGIN_', '')}
                                </span>
                                <span style={{ color: colors.text, flex: 1 }}>{event.email || 'Unknown'}</span>
                                <span style={{ color: colors.textMuted }}>{event.ipAddress}</span>
                                <span style={{ color: colors.info, width: '30px' }}>{event.geoCountry || '??'}</span>
                            </div>
                        ))}
                    </div>

                    {/* Alerts Panel */}
                    <div style={{ flex: 1, backgroundColor: colors.panel, border: `1px solid ${colors.textMuted}`, padding: '8px', overflow: 'auto' }}>
                        <div style={{ color: colors.alert, fontSize: '12px', fontWeight: 'bold', marginBottom: '8px' }}>
                            [WARN]️ {t('activity.securityAlerts')} ({alerts.length})
                        </div>
                        {alerts.length === 0 ? (
                            <div style={{ color: colors.secondary, fontSize: '11px' }}>[OK] {t('activity.noActiveAlerts')}</div>
                        ) : (
                            alerts.map((alert) => (
                                <div key={alert.id} style={{
                                    backgroundColor: 'rgba(255,0,0,0.05)',
                                    border: `1px solid ${getSeverityColor(alert.severity)}`,
                                    padding: '6px',
                                    marginBottom: '4px',
                                    fontSize: '11px',
                                }}>
                                    <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
                                        <span style={{ color: getSeverityColor(alert.severity), fontWeight: 'bold', textTransform: 'uppercase' }}>
                                            {alert.severity}
                                        </span>
                                        <span style={{ color: colors.textMuted }}>{formatTimestamp(alert.timestamp)}</span>
                                    </div>
                                    <div style={{ color: colors.text, marginBottom: '2px' }}>{alert.description}</div>
                                    {alert.ipAddress && (
                                        <div style={{ color: colors.textMuted }}>IP: {alert.ipAddress}</div>
                                    )}
                                </div>
                            ))
                        )}
                    </div>
                </div>
            </div>
        );
    };

    const renderEvents = () => (
        <div style={{ backgroundColor: colors.panel, border: `1px solid ${colors.textMuted}`, padding: '8px', height: '100%', overflow: 'auto' }}>
            <div style={{ color: colors.primary, fontSize: '12px', fontWeight: 'bold', marginBottom: '8px' }}>
                {t('activity.loginEventLog')}
            </div>
            <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '11px' }}>
                <thead>
                    <tr style={{ borderBottom: `1px solid ${colors.textMuted}` }}>
                        <th style={{ textAlign: 'left', padding: '4px', color: colors.textMuted }}>Time</th>
                        <th style={{ textAlign: 'left', padding: '4px', color: colors.textMuted }}>Event</th>
                        <th style={{ textAlign: 'left', padding: '4px', color: colors.textMuted }}>User</th>
                        <th style={{ textAlign: 'left', padding: '4px', color: colors.textMuted }}>IP Address</th>
                        <th style={{ textAlign: 'left', padding: '4px', color: colors.textMuted }}>Location</th>
                        <th style={{ textAlign: 'left', padding: '4px', color: colors.textMuted }}>Device</th>
                        <th style={{ textAlign: 'left', padding: '4px', color: colors.textMuted }}>Details</th>
                    </tr>
                </thead>
                <tbody>
                    {loginEvents.map((event, idx) => (
                        <tr key={event.logId} style={{
                            backgroundColor: idx % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent',
                            borderLeft: `2px solid ${getEventColor(event.eventType)}`,
                        }}>
                            <td style={{ padding: '4px', color: colors.textMuted }}>{formatDate(event.timestamp)}</td>
                            <td style={{ padding: '4px', color: getEventColor(event.eventType), fontWeight: 'bold' }}>
                                {event.eventType}
                            </td>
                            <td style={{ padding: '4px', color: colors.text }}>{event.email || event.userId || '-'}</td>
                            <td style={{ padding: '4px', color: colors.accent }}>{event.ipAddress}</td>
                            <td style={{ padding: '4px', color: colors.info }}>
                                {event.geoCity ? `${event.geoCity}, ${event.geoCountry}` : event.geoCountry || '-'}
                            </td>
                            <td style={{ padding: '4px', color: colors.textMuted }}>{event.deviceId?.substring(0, 12) || '-'}</td>
                            <td style={{ padding: '4px', color: colors.textMuted }}>
                                {event.metadata?.failure_reason as string || '-'}
                            </td>
                        </tr>
                    ))}
                </tbody>
            </table>
        </div>
    );

    const renderAlerts = () => (
        <div style={{ backgroundColor: colors.panel, border: `1px solid ${colors.textMuted}`, padding: '8px', height: '100%', overflow: 'auto' }}>
            <div style={{ color: colors.alert, fontSize: '12px', fontWeight: 'bold', marginBottom: '8px' }}>
                SECURITY ALERTS & INCIDENTS
            </div>
            {alerts.length === 0 ? (
                <div style={{
                    textAlign: 'center',
                    padding: '40px',
                    color: colors.secondary
                }}>
                    <div style={{ fontSize: '24px', marginBottom: '8px' }}>[OK]</div>
                    <div>No security alerts at this time</div>
                </div>
            ) : (
                alerts.map((alert) => (
                    <div key={alert.id} style={{
                        backgroundColor: 'rgba(255,0,0,0.05)',
                        border: `1px solid ${getSeverityColor(alert.severity)}`,
                        padding: '12px',
                        marginBottom: '8px',
                    }}>
                        <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '8px' }}>
                            <span style={{
                                color: getSeverityColor(alert.severity),
                                fontWeight: 'bold',
                                textTransform: 'uppercase',
                                fontSize: '12px'
                            }}>
                                [{alert.severity}] {alert.type.replace('_', ' ')}
                            </span>
                            <span style={{ color: colors.textMuted, fontSize: '11px' }}>{formatDate(alert.timestamp)}</span>
                        </div>
                        <div style={{ color: colors.text, fontSize: '12px', marginBottom: '8px' }}>{alert.description}</div>
                        <div style={{ display: 'flex', gap: '16px', fontSize: '11px' }}>
                            {alert.ipAddress && (
                                <div><span style={{ color: colors.textMuted }}>IP:</span> <span style={{ color: colors.accent }}>{alert.ipAddress}</span></div>
                            )}
                            {alert.userId && (
                                <div><span style={{ color: colors.textMuted }}>User:</span> <span style={{ color: colors.text }}>{alert.userId}</span></div>
                            )}
                        </div>
                        <div style={{ marginTop: '8px', display: 'flex', gap: '4px' }}>
                            <button style={{
                                padding: '4px 8px',
                                backgroundColor: colors.secondary,
                                border: 'none',
                                color: colors.background,
                                fontSize: '10px',
                                cursor: 'pointer',
                            }}>
                                DISMISS
                            </button>
                            <button style={{
                                padding: '4px 8px',
                                backgroundColor: colors.alert,
                                border: 'none',
                                color: '#fff',
                                fontSize: '10px',
                                cursor: 'pointer',
                            }}>
                                BLOCK IP
                            </button>
                            <button style={{
                                padding: '4px 8px',
                                backgroundColor: colors.background,
                                border: `1px solid ${colors.textMuted}`,
                                color: colors.text,
                                fontSize: '10px',
                                cursor: 'pointer',
                            }}>
                                INVESTIGATE
                            </button>
                        </div>
                    </div>
                ))
            )}
        </div>
    );

    const renderAnalytics = () => (
        <div style={{ display: 'flex', gap: '8px', height: '100%' }}>
            {/* Country breakdown */}
            <div style={{ flex: 1, backgroundColor: colors.panel, border: `1px solid ${colors.textMuted}`, padding: '8px', overflow: 'auto' }}>
                <div style={{ color: colors.primary, fontSize: '12px', fontWeight: 'bold', marginBottom: '8px' }}>
                    LOGINS BY COUNTRY (30 days)
                </div>
                {[
                    { country: 'US', count: 4521, pct: 35.2 },
                    { country: 'IN', count: 2103, pct: 16.4 },
                    { country: 'GB', count: 1842, pct: 14.3 },
                    { country: 'DE', count: 1205, pct: 9.4 },
                    { country: 'JP', count: 987, pct: 7.7 },
                ].map((item, idx) => (
                    <div key={idx} style={{ marginBottom: '8px' }}>
                        <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '11px', marginBottom: '2px' }}>
                            <span style={{ color: colors.text }}>{item.country}</span>
                            <span style={{ color: colors.textMuted }}>{item.count.toLocaleString()} ({item.pct}%)</span>
                        </div>
                        <div style={{ backgroundColor: colors.background, height: '8px' }}>
                            <div style={{
                                width: `${item.pct}%`,
                                height: '100%',
                                backgroundColor: colors.primary,
                            }} />
                        </div>
                    </div>
                ))}
            </div>

            {/* Failure reasons */}
            <div style={{ flex: 1, backgroundColor: colors.panel, border: `1px solid ${colors.textMuted}`, padding: '8px', overflow: 'auto' }}>
                <div style={{ color: colors.alert, fontSize: '12px', fontWeight: 'bold', marginBottom: '8px' }}>
                    FAILURE REASONS (24h)
                </div>
                {[
                    { reason: 'bad_password', count: 187, pct: 54.7 },
                    { reason: 'user_not_found', count: 78, pct: 22.8 },
                    { reason: 'account_locked', count: 45, pct: 13.2 },
                    { reason: 'mfa_failed', count: 32, pct: 9.3 },
                ].map((item, idx) => (
                    <div key={idx} style={{
                        display: 'flex',
                        justifyContent: 'space-between',
                        padding: '4px',
                        backgroundColor: idx % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent',
                        fontSize: '11px',
                    }}>
                        <span style={{ color: colors.text }}>{item.reason}</span>
                        <span style={{ color: colors.alert }}>{item.count} ({item.pct}%)</span>
                    </div>
                ))}
            </div>

            {/* Hourly distribution */}
            <div style={{ flex: 1, backgroundColor: colors.panel, border: `1px solid ${colors.textMuted}`, padding: '8px', overflow: 'auto' }}>
                <div style={{ color: colors.info, fontSize: '12px', fontWeight: 'bold', marginBottom: '8px' }}>
                    PEAK HOURS (UTC)
                </div>
                {[
                    { hour: '09:00', logins: 342 },
                    { hour: '14:00', logins: 298 },
                    { hour: '10:00', logins: 276 },
                    { hour: '15:00', logins: 254 },
                    { hour: '08:00', logins: 231 },
                ].map((item, idx) => (
                    <div key={idx} style={{
                        display: 'flex',
                        justifyContent: 'space-between',
                        padding: '4px',
                        backgroundColor: idx % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent',
                        fontSize: '11px',
                    }}>
                        <span style={{ color: colors.textMuted }}>{item.hour}</span>
                        <span style={{ color: colors.info }}>{item.logins} logins</span>
                    </div>
                ))}
            </div>
        </div>
    );

    // Check admin access
    const isAdmin = session?.user_info?.account_type === 'admin' ||
        (session?.user_info as any)?.role === 'admin';

    if (!isAdmin) {
        return (
            <div style={{
                height: '100%',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                backgroundColor: colors.background,
                color: colors.alert,
            }}>
                <div style={{ textAlign: 'center' }}>
                    <div style={{ fontSize: '24px', marginBottom: '8px' }}>🔒</div>
                    <div>Admin access required to view Login Audit Dashboard</div>
                </div>
            </div>
        );
    }

    return (
        <div style={{ height: '100%', display: 'flex', flexDirection: 'column', backgroundColor: colors.background, overflow: 'hidden' }}>
            {/* Header */}
            <div style={{ backgroundColor: colors.panel, borderBottom: `1px solid ${colors.textMuted}`, padding: '4px 8px', flexShrink: 0 }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '13px' }}>
                    <span style={{ color: colors.primary, fontWeight: 'bold' }}>🔐 {t('title')}</span>
                    <span style={{ color: colors.text }}>|</span>
                    <span style={{ color: colors.text }}>{currentTime.toISOString().replace('T', ' ').substring(0, 19)} UTC</span>
                    <span style={{ color: colors.text }}>|</span>
                    <span style={{ color: colors.textMuted }}>Alerts:</span>
                    <span style={{ color: alerts.length > 0 ? colors.alert : colors.secondary, fontWeight: 'bold' }}>
                        {alerts.length}
                    </span>
                    <div style={{ marginLeft: 'auto', display: 'flex', gap: '8px', alignItems: 'center' }}>
                        <label style={{ display: 'flex', alignItems: 'center', gap: '4px', fontSize: '11px', color: colors.textMuted }}>
                            <input
                                type="checkbox"
                                checked={autoRefresh}
                                onChange={(e) => setAutoRefresh(e.target.checked)}
                            />
                            {t('controls.autoRefresh')}
                        </label>
                        <button
                            onClick={() => setLoading(true)}
                            disabled={loading}
                            style={{
                                padding: '2px 8px',
                                backgroundColor: colors.primary,
                                color: colors.background,
                                border: 'none',
                                fontSize: '11px',
                                cursor: loading ? 'not-allowed' : 'pointer',
                                fontWeight: 'bold',
                            }}
                        >
                            {loading ? t('controls.loading') : '↻ ' + t('controls.refresh')}
                        </button>
                    </div>
                </div>
            </div>

            {/* Navigation */}
            <div style={{ backgroundColor: colors.panel, borderBottom: `1px solid ${colors.textMuted}`, padding: '2px 4px', flexShrink: 0 }}>
                <div style={{ display: 'flex', gap: '2px' }}>
                    {[
                        { key: 'overview', label: t('views.overview') },
                        { key: 'events', label: t('views.eventLog') },
                        { key: 'alerts', label: t('views.alerts') },
                        { key: 'analytics', label: t('views.analytics') },
                    ].map((item) => (
                        <button
                            key={item.key}
                            onClick={() => setCurrentView(item.key as DashboardView)}
                            style={{
                                backgroundColor: currentView === item.key ? colors.primary : colors.background,
                                border: `1px solid ${colors.textMuted}`,
                                color: currentView === item.key ? colors.background : colors.text,
                                padding: '4px 12px',
                                fontSize: '11px',
                                fontWeight: currentView === item.key ? 'bold' : 'normal',
                                cursor: 'pointer',
                            }}
                        >
                            {item.label}
                        </button>
                    ))}
                </div>
            </div>

            {/* Main Content */}
            <div style={{ flex: 1, overflow: 'auto', padding: '8px', backgroundColor: '#050505' }}>
                {currentView === 'overview' && renderOverview()}
                {currentView === 'events' && renderEvents()}
                {currentView === 'alerts' && renderAlerts()}
                {currentView === 'analytics' && renderAnalytics()}
            </div>

            {/* Status Bar */}
            <div style={{
                borderTop: `1px solid ${colors.textMuted}`,
                backgroundColor: colors.panel,
                padding: '2px 8px',
                fontSize: '11px',
                color: colors.textMuted,
                flexShrink: 0
            }}>
                <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span>Admin: {session?.user_info?.email || 'Unknown'}</span>
                    <span>
                        DAU: {metrics.dauToday} |
                        Active Sessions: {metrics.activeSessionsCount} |
                        Status: {loading ? 'UPDATING' : 'READY'}
                    </span>
                </div>
            </div>
        </div>
    );
};

export default LoginAuditDashboard;
