import React, { useState, useEffect } from 'react';
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer, RadarChart, PolarGrid, PolarAngleAxis, PolarRadiusAxis, Radar, BarChart, Bar } from 'recharts';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { geopoliticsService } from '@/services/geopoliticsService';

const GeopoliticsTab: React.FC = () => {
  const { colors, fontSize, fontFamily, fontWeight, fontStyle } = useTerminalTheme();
  const [currentTime, setCurrentTime] = useState(new Date());
  const [loading, setLoading] = useState(true);
  const [qrNotifications, setQrNotifications] = useState<any[]>([]);
  const [epingNotifications, setEpingNotifications] = useState<any[]>([]);
  const [tariffData, setTariffData] = useState<any[]>([]);
  const [qrRestrictions, setQrRestrictions] = useState<any[]>([]);
  const [qrProducts, setQrProducts] = useState<any[]>([]);
  const [tradeFlow, setTradeFlow] = useState<any[]>([]);
  const [tfadData, setTfadData] = useState<any>(null);
  const [selectedCountry, setSelectedCountry] = useState('840');
  const [expandedQR, setExpandedQR] = useState<number | null>(null);
  const [expandedEPing, setExpandedEPing] = useState<number | null>(null);
  const [expandedRestriction, setExpandedRestriction] = useState<number | null>(null);

  const countries = [
    { code: '840', name: 'United States' },
    { code: '156', name: 'China' },
    { code: '276', name: 'Germany' },
    { code: '392', name: 'Japan' },
    { code: '826', name: 'United Kingdom' },
    { code: '356', name: 'India' },
    { code: '124', name: 'Canada' },
    { code: '076', name: 'Brazil' },
    { code: '036', name: 'Australia' }
  ];

  useEffect(() => {
    const timer = setInterval(() => setCurrentTime(new Date()), 2000);
    return () => clearInterval(timer);
  }, []);

  useEffect(() => {
    loadGeopoliticsData();
  }, [selectedCountry]);

  const loadGeopoliticsData = async () => {
    try {
      setLoading(true);
      const data = await geopoliticsService.getComprehensiveGeopoliticsData(selectedCountry);
      setQrNotifications(data.qrNotifications);
      setEpingNotifications(data.epingNotifications);
      setTariffData(data.tariffData);
      setQrRestrictions(data.qrRestrictions);
      setQrProducts(data.qrProducts);
      setTradeFlow(data.tradeFlowData);
      setTfadData(data.tfadData);
    } catch (error) {
      console.error('Failed to load geopolitics data:', error);
    } finally {
      setLoading(false);
    }
  };

  // Calculate threat levels with improved methodology
  const calculateThreatLevel = () => {
    // Trade Restrictions Threat
    // Based on: QR notification count (recent restrictions indicate protectionism)
    // Scale: 0 notifications = 0, 50+ = 10 (critical)
    const qrCount = qrNotifications.length;
    let tradeLevel = 0;
    if (qrCount > 0) {
      tradeLevel = Math.min(2 + (qrCount / 50) * 8, 10); // Base 2 if any exist
    }

    // Tariff Threat
    // Based on: Tariff rate level + trend
    // Scale: 0% = 0, 20%+ = 10, plus trend multiplier
    let tariffLevel = 0;
    if (tariffData.length > 0) {
      const values = tariffData.map(d => d.Value || 0);
      const latest = values[values.length - 1] || 0;
      const previous = values[values.length - 2] || latest;
      const trend = latest - previous;

      // Base level from tariff rate (0-20% range)
      const baseLevel = Math.min((latest / 20) * 7, 7);

      // Trend adjustment (+/- 3 points based on direction)
      let trendAdjustment = 0;
      if (trend > 0.5) trendAdjustment = 3; // Rising tariffs
      else if (trend > 0.1) trendAdjustment = 1.5;
      else if (trend < -0.5) trendAdjustment = -2; // Falling tariffs
      else if (trend < -0.1) trendAdjustment = -1;

      tariffLevel = Math.max(0, Math.min(baseLevel + trendAdjustment, 10));
    }

    // Regulatory Threat
    // Based on: Recent notifications + pending deadlines + affected sectors
    let regulatoryLevel = 0;
    if (epingNotifications.length > 0) {
      // Recent activity (0-4 points)
      const activityScore = Math.min((epingNotifications.length / 100) * 4, 4);

      // Pending deadlines urgency (0-3 points)
      const now = new Date();
      const urgentDeadlines = epingNotifications.filter(n => {
        if (!n.commentDeadlineDate) return false;
        const deadline = new Date(n.commentDeadlineDate);
        const daysUntil = (deadline.getTime() - now.getTime()) / (1000 * 60 * 60 * 24);
        return daysUntil > 0 && daysUntil <= 60; // Within 60 days
      }).length;
      const urgencyScore = Math.min((urgentDeadlines / 20) * 3, 3);

      // Critical regulations (SPS/TBT human health/safety) (0-3 points)
      const criticalCount = epingNotifications.filter(n =>
        n.objectives?.some((o: any) =>
          o.name?.toLowerCase().includes('health') ||
          o.name?.toLowerCase().includes('safety')
        )
      ).length;
      const criticalScore = Math.min((criticalCount / 10) * 3, 3);

      regulatoryLevel = Math.min(activityScore + urgencyScore + criticalScore, 10);
    }

    // Market Access Threat (NEW)
    // Based on: Countries affected + notification types
    let marketAccessLevel = 0;
    const affectedCountriesCount = new Set(
      epingNotifications.flatMap(n =>
        (n.countriesAffected || []).map((c: any) => c.id || c.name)
      )
    ).size;

    if (affectedCountriesCount > 0) {
      // More countries affected = higher threat
      marketAccessLevel = Math.min((affectedCountriesCount / 50) * 6, 6);

      // Add points if many SPS measures (stricter than TBT)
      const spsCount = epingNotifications.filter(n => n.area === 'SPS').length;
      const spsRatio = epingNotifications.length > 0 ? spsCount / epingNotifications.length : 0;
      marketAccessLevel += spsRatio * 4;

      marketAccessLevel = Math.min(marketAccessLevel, 10);
    }

    // Supply Chain Disruption (NEW)
    // Based on: Recent QR notifications + tariff volatility
    let supplyChainLevel = 0;
    if (qrNotifications.length > 0 || tariffData.length > 0) {
      // Recent QR activity
      const recentQR = qrNotifications.filter(n => {
        if (!n.notification_dt) return false;
        const notifDate = new Date(n.notification_dt);
        const monthsAgo = (new Date().getTime() - notifDate.getTime()) / (1000 * 60 * 60 * 24 * 30);
        return monthsAgo <= 12; // Within last year
      }).length;

      const qrScore = Math.min((recentQR / 20) * 5, 5);

      // Tariff volatility
      let volatilityScore = 0;
      if (tariffData.length >= 3) {
        const recentValues = tariffData.slice(-3).map(d => d.Value || 0);
        const volatility = Math.max(...recentValues) - Math.min(...recentValues);
        volatilityScore = Math.min((volatility / 5) * 5, 5);
      }

      supplyChainLevel = Math.min(qrScore + volatilityScore, 10);
    }

    return [
      { threat: 'Trade Restrictions', current: Number(tradeLevel.toFixed(1)), max: 10 },
      { threat: 'Tariff Pressure', current: Number(tariffLevel.toFixed(1)), max: 10 },
      { threat: 'Regulatory Burden', current: Number(regulatoryLevel.toFixed(1)), max: 10 },
      { threat: 'Market Access', current: Number(marketAccessLevel.toFixed(1)), max: 10 },
      { threat: 'Supply Chain', current: Number(supplyChainLevel.toFixed(1)), max: 10 }
    ];
  };

  // Calculate tariff statistics
  const getTariffStats = () => {
    if (tariffData.length === 0) return { min: 0, max: 0, avg: 0, latest: 0, change: 0 };

    const values = tariffData.map(d => d.Value || 0);
    const min = Math.min(...values);
    const max = Math.max(...values);
    const avg = values.reduce((a, b) => a + b, 0) / values.length;
    const latest = values[values.length - 1];
    const previous = values[values.length - 2] || latest;
    const change = latest - previous;

    return { min, max, avg, latest, change };
  };

  // Get upcoming deadlines
  const getUpcomingDeadlines = () => {
    return epingNotifications
      .filter(n => n.commentDeadlineDate || n.proposedEntryIntoForceDate)
      .map(n => ({
        title: n.documentSymbol || 'N/A',
        member: n.notifyingMember || 'N/A',
        commentDeadline: n.commentDeadlineDate,
        entryIntoForce: n.proposedEntryIntoForceDate,
        area: n.area
      }))
      .slice(0, 10);
  };

  // Get affected countries
  const getAffectedCountries = () => {
    const countryMap: { [key: string]: number } = {};

    epingNotifications.forEach(n => {
      if (n.countriesAffected && Array.isArray(n.countriesAffected)) {
        n.countriesAffected.forEach((c: any) => {
          const name = c.name || c.id || 'Unknown';
          countryMap[name] = (countryMap[name] || 0) + 1;
        });
      }
    });

    return Object.entries(countryMap)
      .map(([name, count]) => ({ name, count }))
      .sort((a, b) => b.count - a.count)
      .slice(0, 10);
  };

  // Get notification types breakdown
  const getNotificationTypes = () => {
    const types: { [key: string]: number } = {};

    epingNotifications.forEach(n => {
      const type = n.area || 'Unknown';
      types[type] = (types[type] || 0) + 1;
    });

    return Object.entries(types).map(([name, value]) => ({ name, value }));
  };

  const threatRadarData = calculateThreatLevel();
  const tariffStats = getTariffStats();
  const upcomingDeadlines = getUpcomingDeadlines();
  const affectedCountries = getAffectedCountries();
  const notificationTypes = getNotificationTypes();

  if (loading) {
    return (
      <div style={{
        height: '100%',
        backgroundColor: colors.background,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        color: colors.text,
        fontFamily
      }}>
        Loading geopolitical intelligence data...
      </div>
    );
  }

  return (
    <div style={{
      height: '100%',
      backgroundColor: colors.background,
      color: colors.text,
      fontFamily,
      fontWeight,
      fontStyle,
      overflow: 'hidden',
      display: 'flex',
      flexDirection: 'column',
      fontSize: fontSize.body
    }}>
      {/* Header */}
      <div style={{
        backgroundColor: colors.panel,
        borderBottom: `1px solid ${colors.textMuted}`,
        padding: '8px',
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', fontSize: '14px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <span style={{ color: colors.primary, fontWeight: 'bold' }}>GEOPOLITICAL INTELLIGENCE</span>
            <span style={{ color: colors.text }}>|</span>
            <span style={{ color: colors.secondary }}>● LIVE</span>
            <span style={{ color: colors.text }}>|</span>
            <select
              value={selectedCountry}
              onChange={(e) => setSelectedCountry(e.target.value)}
              style={{
                backgroundColor: colors.background,
                color: colors.text,
                border: `1px solid ${colors.textMuted}`,
                padding: '2px 8px',
                fontSize: '12px',
                borderRadius: '2px',
                cursor: 'pointer'
              }}
            >
              {countries.map(c => (
                <option key={c.code} value={c.code}>{c.name}</option>
              ))}
            </select>
            <span style={{ color: colors.text }}>|</span>
            <span style={{ color: colors.info }}>QR: {qrNotifications.length}</span>
            <span style={{ color: colors.text }}>|</span>
            <span style={{ color: colors.warning }}>ePing: {epingNotifications.length}</span>
            <span style={{ color: colors.text }}>|</span>
            <span style={{ color: colors.text }}>{currentTime.toISOString().substring(0, 19)} UTC</span>
          </div>
          <button
            onClick={loadGeopoliticsData}
            disabled={loading}
            style={{
              backgroundColor: loading ? colors.textMuted : colors.primary,
              color: colors.background,
              border: 'none',
              padding: '4px 12px',
              fontSize: '12px',
              cursor: loading ? 'not-allowed' : 'pointer',
              borderRadius: '2px',
              fontWeight: 'bold'
            }}
          >
            {loading ? 'LOADING...' : '↻ REFRESH'}
          </button>
        </div>
      </div>

      {/* Main Content */}
      <div style={{ flex: 1, overflow: 'auto', padding: '8px', backgroundColor: '#050505' }}>

        {/* Row 1: Statistics Cards */}
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '8px', marginBottom: '8px' }}>

          {/* Tariff Stats Card */}
          <div style={{
            backgroundColor: colors.panel,
            border: `1px solid ${colors.textMuted}`,
            padding: '8px'
          }}>
            <div style={{ color: colors.primary, fontSize: '11px', fontWeight: 'bold', marginBottom: '8px' }}>
              TARIFF STATISTICS
            </div>
            <div style={{ fontSize: '10px', lineHeight: '1.6' }}>
              <div>Latest: <span style={{ color: colors.secondary, fontWeight: 'bold' }}>{tariffStats.latest.toFixed(2)}%</span></div>
              <div>Avg: <span style={{ color: colors.text }}>{tariffStats.avg.toFixed(2)}%</span></div>
              <div>Min: <span style={{ color: colors.success }}>{tariffStats.min.toFixed(2)}%</span></div>
              <div>Max: <span style={{ color: colors.alert }}>{tariffStats.max.toFixed(2)}%</span></div>
              <div>Change: <span style={{ color: tariffStats.change >= 0 ? colors.alert : colors.success }}>
                {tariffStats.change >= 0 ? '+' : ''}{tariffStats.change.toFixed(2)}%
              </span></div>
            </div>
          </div>

          {/* QR Summary */}
          <div style={{
            backgroundColor: colors.panel,
            border: `1px solid ${colors.textMuted}`,
            padding: '8px'
          }}>
            <div style={{ color: colors.primary, fontSize: '11px', fontWeight: 'bold', marginBottom: '8px' }}>
              QR RESTRICTIONS
            </div>
            <div style={{ fontSize: '10px', lineHeight: '1.6' }}>
              <div>Total: <span style={{ color: colors.warning, fontWeight: 'bold' }}>{qrNotifications.length}</span></div>
              <div>Type: Complete Notifications</div>
              <div>Coverage: Global</div>
              <div>Updated: Live</div>
            </div>
          </div>

          {/* ePing Summary */}
          <div style={{
            backgroundColor: colors.panel,
            border: `1px solid ${colors.textMuted}`,
            padding: '8px'
          }}>
            <div style={{ color: colors.primary, fontSize: '11px', fontWeight: 'bold', marginBottom: '8px' }}>
              REGULATORY ALERTS
            </div>
            <div style={{ fontSize: '10px', lineHeight: '1.6' }}>
              <div>Total: <span style={{ color: colors.info, fontWeight: 'bold' }}>{epingNotifications.length}</span></div>
              <div>SPS: {epingNotifications.filter(n => n.area === 'SPS').length}</div>
              <div>TBT: {epingNotifications.filter(n => n.area === 'TBT').length}</div>
              <div>Pending Comments: {epingNotifications.filter(n => n.commentDeadlineDate).length}</div>
            </div>
          </div>

          {/* Threat Level */}
          <div style={{
            backgroundColor: colors.panel,
            border: `1px solid ${colors.textMuted}`,
            padding: '8px'
          }}>
            <div style={{ color: colors.primary, fontSize: '11px', fontWeight: 'bold', marginBottom: '8px' }}>
              THREAT LEVEL
            </div>
            <div style={{ fontSize: '10px', lineHeight: '1.6' }}>
              {threatRadarData.map(t => (
                <div key={t.threat}>
                  {t.threat}: <span style={{
                    color: t.current > 7 ? colors.alert : t.current > 5 ? colors.warning : colors.success,
                    fontWeight: 'bold'
                  }}>{t.current.toFixed(1)}/10</span>
                </div>
              ))}
            </div>
          </div>
        </div>

        {/* Row 2: Charts */}
        <div style={{ display: 'grid', gridTemplateColumns: '2fr 1fr 1fr', gap: '8px', marginBottom: '8px', height: '280px' }}>

          {/* Tariff Time Series */}
          <div style={{
            backgroundColor: colors.panel,
            border: `1px solid ${colors.textMuted}`,
            padding: '8px'
          }}>
            <div style={{ color: colors.primary, fontSize: '11px', fontWeight: 'bold', marginBottom: '8px' }}>
              MFN TARIFF RATES - {countries.find(c => c.code === selectedCountry)?.name.toUpperCase()}
            </div>
            <ResponsiveContainer width="100%" height={230}>
              <LineChart data={tariffData}>
                <CartesianGrid strokeDasharray="3 3" stroke={colors.textMuted} opacity={0.3} />
                <XAxis dataKey="Year" stroke={colors.text} fontSize={9} />
                <YAxis stroke={colors.text} fontSize={9} />
                <Tooltip
                  contentStyle={{
                    backgroundColor: colors.background,
                    border: `1px solid ${colors.textMuted}`,
                    color: colors.text,
                    fontSize: '10px'
                  }}
                />
                <Line type="monotone" dataKey="Value" stroke={colors.secondary} strokeWidth={2} name="Tariff %" />
              </LineChart>
            </ResponsiveContainer>
          </div>

          {/* Threat Radar */}
          <div style={{
            backgroundColor: colors.panel,
            border: `1px solid ${colors.textMuted}`,
            padding: '8px'
          }}>
            <div style={{ color: colors.primary, fontSize: '11px', fontWeight: 'bold', marginBottom: '8px' }}>
              THREAT RADAR
            </div>
            <ResponsiveContainer width="100%" height={230}>
              <RadarChart data={threatRadarData}>
                <PolarGrid stroke={colors.textMuted} />
                <PolarAngleAxis dataKey="threat" tick={{ fontSize: 9, fill: colors.text }} />
                <PolarRadiusAxis angle={90} domain={[0, 10]} tick={{ fontSize: 8, fill: colors.textMuted }} />
                <Radar
                  dataKey="current"
                  stroke={colors.alert}
                  fill={colors.alert}
                  fillOpacity={0.3}
                  strokeWidth={2}
                />
              </RadarChart>
            </ResponsiveContainer>
          </div>

          {/* Notification Types */}
          <div style={{
            backgroundColor: colors.panel,
            border: `1px solid ${colors.textMuted}`,
            padding: '8px'
          }}>
            <div style={{ color: colors.primary, fontSize: '11px', fontWeight: 'bold', marginBottom: '8px' }}>
              NOTIFICATION TYPES
            </div>
            <ResponsiveContainer width="100%" height={230}>
              <BarChart data={notificationTypes}>
                <CartesianGrid strokeDasharray="3 3" stroke={colors.textMuted} opacity={0.3} />
                <XAxis dataKey="name" stroke={colors.text} fontSize={9} />
                <YAxis stroke={colors.text} fontSize={9} />
                <Tooltip
                  contentStyle={{
                    backgroundColor: colors.background,
                    border: `1px solid ${colors.textMuted}`,
                    color: colors.text,
                    fontSize: '10px'
                  }}
                />
                <Bar dataKey="value" fill={colors.info} />
              </BarChart>
            </ResponsiveContainer>
          </div>
        </div>

        {/* Row 3: Detailed Lists */}
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px', marginBottom: '8px', height: '320px' }}>

          {/* QR Notifications Detailed */}
          <div style={{
            backgroundColor: colors.panel,
            border: `1px solid ${colors.textMuted}`,
            padding: '8px',
            overflow: 'auto'
          }}>
            <div style={{ color: colors.primary, fontSize: '11px', fontWeight: 'bold', marginBottom: '8px' }}>
              QR NOTIFICATIONS DETAILS
            </div>
            {qrNotifications.slice(0, 20).map((notif, index) => (
              <div key={index} style={{
                marginBottom: '8px',
                fontSize: '10px',
                borderLeft: `2px solid ${colors.warning}`,
                paddingLeft: '6px',
                backgroundColor: expandedQR === index ? 'rgba(255,255,255,0.05)' : index % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent',
                cursor: 'pointer'
              }}
              onClick={() => setExpandedQR(expandedQR === index ? null : index)}
              >
                <div style={{ color: colors.text, fontWeight: 'bold' }}>
                  {notif.reporter_member?.name?.en || notif.reporter_member?.code || 'N/A'}
                </div>
                <div style={{ color: colors.textMuted, fontSize: '9px' }}>
                  {notif.document_symbol || 'N/A'} | {notif.notification_dt || 'N/A'}
                </div>
                {expandedQR === index && (
                  <div style={{ marginTop: '4px', fontSize: '9px', color: colors.textMuted }}>
                    <div>Type: {notif.type || 'N/A'}</div>
                    <div>Language: {notif.original_language || 'N/A'}</div>
                    <div>Periods: {notif.covered_periods?.join(', ') || 'N/A'}</div>
                    {notif.document_url && (
                      <a href={notif.document_url} target="_blank" rel="noopener noreferrer"
                         style={{ color: colors.info, textDecoration: 'underline' }}>
                        View Document
                      </a>
                    )}
                  </div>
                )}
              </div>
            ))}
          </div>

          {/* ePing Notifications Detailed */}
          <div style={{
            backgroundColor: colors.panel,
            border: `1px solid ${colors.textMuted}`,
            padding: '8px',
            overflow: 'auto'
          }}>
            <div style={{ color: colors.primary, fontSize: '11px', fontWeight: 'bold', marginBottom: '8px' }}>
              EPING NOTIFICATIONS DETAILS
            </div>
            {epingNotifications.slice(0, 20).map((notif, index) => (
              <div key={index} style={{
                marginBottom: '8px',
                fontSize: '10px',
                borderLeft: `2px solid ${notif.area === 'SPS' ? colors.success : colors.info}`,
                paddingLeft: '6px',
                backgroundColor: expandedEPing === index ? 'rgba(255,255,255,0.05)' : index % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent',
                cursor: 'pointer'
              }}
              onClick={() => setExpandedEPing(expandedEPing === index ? null : index)}
              >
                <div style={{ color: colors.text, fontWeight: 'bold' }}>
                  {notif.documentSymbol?.trim() || 'N/A'}
                </div>
                <div style={{ color: colors.textMuted, fontSize: '9px' }}>
                  {notif.notifyingMember} | {notif.area} | {notif.distributionDate?.substring(0, 10)}
                </div>
                {expandedEPing === index && (
                  <div style={{ marginTop: '4px', fontSize: '9px', color: colors.textMuted }}>
                    <div style={{ marginBottom: '2px', fontWeight: 'bold' }}>
                      {notif.titlePlain?.substring(0, 100)}...
                    </div>
                    <div>Type: {notif.notificationType || 'N/A'}</div>
                    {notif.commentDeadlineDate && (
                      <div>Comment Deadline: {notif.commentDeadlineDate.substring(0, 10)}</div>
                    )}
                    {notif.proposedEntryIntoForceDate && (
                      <div>Entry into Force: {notif.proposedEntryIntoForceDate.substring(0, 10)}</div>
                    )}
                    {notif.keywords && notif.keywords.length > 0 && (
                      <div>Keywords: {notif.keywords.map((k: any) => k.name).join(', ')}</div>
                    )}
                    {notif.objectives && notif.objectives.length > 0 && (
                      <div>Objectives: {notif.objectives.map((o: any) => o.name).join(', ')}</div>
                    )}
                    {notif.linkToNotification && (
                      <a href={notif.linkToNotification} target="_blank" rel="noopener noreferrer"
                         style={{ color: colors.info, textDecoration: 'underline' }}>
                        View Document
                      </a>
                    )}
                  </div>
                )}
              </div>
            ))}
          </div>

          {/* Upcoming Deadlines */}
          <div style={{
            backgroundColor: colors.panel,
            border: `1px solid ${colors.textMuted}`,
            padding: '8px',
            overflow: 'auto'
          }}>
            <div style={{ color: colors.primary, fontSize: '11px', fontWeight: 'bold', marginBottom: '8px' }}>
              UPCOMING DEADLINES
            </div>
            {upcomingDeadlines.length > 0 ? upcomingDeadlines.map((item, index) => (
              <div key={index} style={{
                marginBottom: '8px',
                fontSize: '10px',
                borderLeft: `2px solid ${item.area === 'SPS' ? colors.success : colors.info}`,
                paddingLeft: '6px',
                backgroundColor: index % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent'
              }}>
                <div style={{ color: colors.text, fontWeight: 'bold' }}>
                  {item.title}
                </div>
                <div style={{ color: colors.textMuted, fontSize: '9px' }}>
                  {item.member} | {item.area}
                </div>
                {item.commentDeadline && (
                  <div style={{ color: colors.warning, fontSize: '9px' }}>
                    Comment by: {item.commentDeadline.substring(0, 10)}
                  </div>
                )}
                {item.entryIntoForce && (
                  <div style={{ color: colors.alert, fontSize: '9px' }}>
                    Effective: {item.entryIntoForce.substring(0, 10)}
                  </div>
                )}
              </div>
            )) : (
              <div style={{ color: colors.textMuted, fontSize: '10px' }}>No upcoming deadlines</div>
            )}
          </div>
        </div>

        {/* Row 4: Affected Countries */}
        <div style={{ display: 'grid', gridTemplateColumns: '1fr', gap: '8px', height: '200px', marginBottom: '8px' }}>
          <div style={{
            backgroundColor: colors.panel,
            border: `1px solid ${colors.textMuted}`,
            padding: '8px',
            overflow: 'auto'
          }}>
            <div style={{ color: colors.primary, fontSize: '11px', fontWeight: 'bold', marginBottom: '8px' }}>
              AFFECTED COUNTRIES & REGIONS
            </div>
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(5, 1fr)', gap: '8px' }}>
              {affectedCountries.length > 0 ? affectedCountries.map((country, index) => (
                <div key={index} style={{
                  padding: '6px',
                  backgroundColor: 'rgba(255,255,255,0.03)',
                  border: `1px solid ${colors.textMuted}`,
                  fontSize: '10px'
                }}>
                  <div style={{ color: colors.text, fontWeight: 'bold' }}>{country.name}</div>
                  <div style={{ color: colors.warning, fontSize: '9px' }}>
                    {country.count} notification{country.count > 1 ? 's' : ''}
                  </div>
                </div>
              )) : (
                <div style={{ color: colors.textMuted, fontSize: '10px', gridColumn: '1 / -1' }}>
                  No specific country restrictions identified
                </div>
              )}
            </div>
          </div>
        </div>

        {/* Row 5: QR Restrictions & Trade Flow */}
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px', marginBottom: '8px', height: '320px' }}>

          {/* Active QR Restrictions */}
          <div style={{
            backgroundColor: colors.panel,
            border: `1px solid ${colors.textMuted}`,
            padding: '8px',
            overflow: 'auto'
          }}>
            <div style={{ color: colors.primary, fontSize: '11px', fontWeight: 'bold', marginBottom: '8px' }}>
              ACTIVE QR RESTRICTIONS - {countries.find(c => c.code === selectedCountry)?.name.toUpperCase()}
            </div>
            {qrRestrictions.length > 0 ? qrRestrictions.slice(0, 20).map((item, index) => (
              <div key={index} style={{
                marginBottom: '8px',
                fontSize: '10px',
                borderLeft: `2px solid ${colors.alert}`,
                paddingLeft: '6px',
                backgroundColor: expandedRestriction === index ? 'rgba(255,255,255,0.05)' : index % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent',
                cursor: 'pointer'
              }}
              onClick={() => setExpandedRestriction(expandedRestriction === index ? null : index)}
              >
                <div style={{ color: colors.text, fontWeight: 'bold' }}>
                  {item.measure_description || 'Restriction'}
                </div>
                <div style={{ color: colors.textMuted, fontSize: '9px' }}>
                  HS Code: {item.product_code || 'N/A'} | {item.in_force ? 'In Force' : 'Not Active'}
                </div>
                {expandedRestriction === index && (
                  <div style={{ marginTop: '4px', fontSize: '9px', color: colors.textMuted }}>
                    <div>Entry: {item.date_of_entry_into_force || 'N/A'}</div>
                    <div>Legal Basis: {item.legal_basis || 'N/A'}</div>
                    <div>Measure Type: {item.measure_type || 'N/A'}</div>
                  </div>
                )}
              </div>
            )) : (
              <div style={{ color: colors.textMuted, fontSize: '10px' }}>No active restrictions for this country</div>
            )}
          </div>

          {/* Trade Flow Data */}
          <div style={{
            backgroundColor: colors.panel,
            border: `1px solid ${colors.textMuted}`,
            padding: '8px',
            overflow: 'auto'
          }}>
            <div style={{ color: colors.primary, fontSize: '11px', fontWeight: 'bold', marginBottom: '8px' }}>
              TRADE FLOW (MERCHANDISE)
            </div>
            {tradeFlow.length > 0 ? (
              <ResponsiveContainer width="100%" height={280}>
                <LineChart data={tradeFlow}>
                  <CartesianGrid strokeDasharray="3 3" stroke={colors.textMuted} opacity={0.3} />
                  <XAxis dataKey="Year" stroke={colors.text} fontSize={9} />
                  <YAxis stroke={colors.text} fontSize={9} />
                  <Tooltip
                    contentStyle={{
                      backgroundColor: colors.background,
                      border: `1px solid ${colors.textMuted}`,
                      color: colors.text,
                      fontSize: '10px'
                    }}
                  />
                  <Line type="monotone" dataKey="Value" stroke={colors.info} strokeWidth={2} name="Trade Value" />
                </LineChart>
              </ResponsiveContainer>
            ) : (
              <div style={{ color: colors.textMuted, fontSize: '10px' }}>No trade flow data available</div>
            )}
          </div>

          {/* TFAD Trade Facilitation */}
          <div style={{
            backgroundColor: colors.panel,
            border: `1px solid ${colors.textMuted}`,
            padding: '8px',
            overflow: 'auto'
          }}>
            <div style={{ color: colors.primary, fontSize: '11px', fontWeight: 'bold', marginBottom: '8px' }}>
              TRADE FACILITATION (TFAD)
            </div>
            {tfadData ? (
              <div style={{ fontSize: '10px', lineHeight: '1.6' }}>
                <div style={{ marginBottom: '8px' }}>
                  <div style={{ color: colors.secondary, fontWeight: 'bold', marginBottom: '4px' }}>Single Window</div>
                  <div style={{ color: colors.text }}>{tfadData.single_window_available ? 'Available' : 'Not Available'}</div>
                  {tfadData.single_window_url && (
                    <a href={tfadData.single_window_url} target="_blank" rel="noopener noreferrer"
                       style={{ color: colors.info, textDecoration: 'underline', fontSize: '9px' }}>
                      Visit Single Window
                    </a>
                  )}
                </div>
                <div style={{ marginBottom: '8px' }}>
                  <div style={{ color: colors.secondary, fontWeight: 'bold', marginBottom: '4px' }}>Enquiry Points</div>
                  <div style={{ color: colors.text }}>
                    {tfadData.enquiry_points?.length || 0} contact(s) available
                  </div>
                </div>
                <div style={{ marginBottom: '8px' }}>
                  <div style={{ color: colors.secondary, fontWeight: 'bold', marginBottom: '4px' }}>Procedures</div>
                  <div style={{ color: colors.text }}>
                    {tfadData.procedures?.length || 0} procedure(s) documented
                  </div>
                </div>
              </div>
            ) : (
              <div style={{ color: colors.textMuted, fontSize: '10px' }}>No TFAD data available for this country</div>
            )}
          </div>
        </div>

        {/* Row 6: QR Products Affected */}
        <div style={{ display: 'grid', gridTemplateColumns: '1fr', gap: '8px', height: '250px' }}>
          <div style={{
            backgroundColor: colors.panel,
            border: `1px solid ${colors.textMuted}`,
            padding: '8px',
            overflow: 'auto'
          }}>
            <div style={{ color: colors.primary, fontSize: '11px', fontWeight: 'bold', marginBottom: '8px' }}>
              PRODUCTS AFFECTED BY QR (HS CODES)
            </div>
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '6px' }}>
              {qrProducts.length > 0 ? qrProducts.slice(0, 40).map((product, index) => (
                <div key={index} style={{
                  padding: '6px',
                  backgroundColor: 'rgba(255,255,255,0.03)',
                  border: `1px solid ${colors.textMuted}`,
                  fontSize: '9px'
                }}>
                  <div style={{ color: colors.secondary, fontWeight: 'bold' }}>{product.code || 'N/A'}</div>
                  <div style={{ color: colors.textMuted }}>{product.description?.substring(0, 40) || 'No description'}...</div>
                </div>
              )) : (
                <div style={{ color: colors.textMuted, fontSize: '10px', gridColumn: '1 / -1' }}>
                  No product data available
                </div>
              )}
            </div>
          </div>
        </div>

      </div>

      {/* Status Bar */}
      <div style={{
        borderTop: `1px solid ${colors.textMuted}`,
        backgroundColor: colors.panel,
        padding: '4px 8px',
        fontSize: '10px',
        color: colors.textMuted,
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', justifyContent: 'space-between' }}>
          <span>Geopolitical Intelligence System v5.0 | WTO Real-time Data | {qrNotifications.length + epingNotifications.length} Total Notifications</span>
          <span>Last Updated: {currentTime.toTimeString().substring(0, 8)}</span>
        </div>
      </div>
    </div>
  );
};

export default GeopoliticsTab;
