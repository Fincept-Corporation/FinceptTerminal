/**
 * SystemView - System capabilities, LLM config status, cache stats, features
 */

import React from 'react';
import { Cpu, RefreshCw, CheckCircle, AlertCircle } from 'lucide-react';
import { FINCEPT, AGENT_CACHE_KEYS } from '../types';
import { cacheService } from '@/services/cache/cacheService';
import type { AgentConfigState } from '../hooks/useAgentConfig';

interface SystemViewProps {
  state: AgentConfigState;
}

export const SystemView: React.FC<SystemViewProps> = ({ state }) => {
  const {
    systemInfo, toolsInfo, configuredLLMs,
    discoveredAgents, loadSystemData, loadLLMConfigs,
    clearAgentCache, getCacheStats,
  } = state;

  const handleRefreshSystem = async () => {
    await Promise.all([
      cacheService.delete(AGENT_CACHE_KEYS.SYSTEM_INFO),
      cacheService.delete(AGENT_CACHE_KEYS.TOOLS_INFO),
    ]).catch(() => {});
    loadSystemData();
  };

  return (
    <div style={{ display: 'flex', height: '100%', overflow: 'hidden' }}>
      <div style={{ flex: 1, overflow: 'auto', padding: '16px' }}>
        {/* Header */}
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '16px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <Cpu size={16} style={{ color: FINCEPT.ORANGE }} />
            <span style={{ fontSize: '12px', fontWeight: 700, color: FINCEPT.WHITE }}>SYSTEM CAPABILITIES</span>
          </div>
          <button
            onClick={handleRefreshSystem}
            style={{ padding: '6px 10px', backgroundColor: 'transparent', border: `1px solid ${FINCEPT.BORDER}`, color: FINCEPT.GRAY, fontSize: '9px', fontWeight: 700, cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '4px', borderRadius: '2px' }}
          >
            <RefreshCw size={10} /> REFRESH
          </button>
        </div>

        {/* Stats Grid */}
        {systemInfo ? (
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '12px', marginBottom: '16px' }}>
            {[
              { value: systemInfo.capabilities.model_providers, label: 'PROVIDERS', color: FINCEPT.ORANGE },
              { value: systemInfo.capabilities.tools, label: 'TOOLS', color: FINCEPT.CYAN },
              { value: discoveredAgents.length, label: 'AGENTS', color: FINCEPT.GREEN },
              { value: configuredLLMs.length, label: 'LLMs', color: FINCEPT.PURPLE },
            ].map((stat, idx) => (
              <div key={idx} style={{ padding: '16px', backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', textAlign: 'center' }}>
                <div style={{ fontSize: '24px', fontWeight: 700, color: stat.color }}>{stat.value}</div>
                <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginTop: '4px' }}>{stat.label}</div>
              </div>
            ))}
          </div>
        ) : (
          <div style={{ padding: '32px', textAlign: 'center', color: FINCEPT.GRAY, fontSize: '11px' }}>
            Loading system info...
          </div>
        )}

        {/* LLM Providers */}
        <SectionCard
          title="CONFIGURED LLM PROVIDERS"
          action={<button onClick={() => loadLLMConfigs()} style={{ padding: '2px 6px', backgroundColor: 'transparent', border: `1px solid ${FINCEPT.BORDER}`, color: FINCEPT.GRAY, fontSize: '8px', cursor: 'pointer', borderRadius: '2px' }}>REFRESH</button>}
        >
          {configuredLLMs.length > 0 ? (
            <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
              {configuredLLMs.map((llm: any, idx: number) => {
                const hasKey = !!llm.api_key && llm.api_key.length > 0;
                return (
                  <div key={idx} style={{ padding: '10px 12px', backgroundColor: FINCEPT.DARK_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                      {llm.is_active && <span style={{ padding: '2px 6px', backgroundColor: `${FINCEPT.GREEN}20`, color: FINCEPT.GREEN, fontSize: '8px', fontWeight: 700, borderRadius: '2px' }}>ACTIVE</span>}
                      <span style={{ fontSize: '10px', fontWeight: 600, color: FINCEPT.WHITE }}>{llm.provider.toUpperCase()}</span>
                      <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>{llm.model}</span>
                    </div>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                      {hasKey ? (
                        <><CheckCircle size={10} style={{ color: FINCEPT.GREEN }} /><span style={{ fontSize: '9px', color: FINCEPT.GREEN }}>KEY SET</span></>
                      ) : (
                        <><AlertCircle size={10} style={{ color: FINCEPT.RED }} /><span style={{ fontSize: '9px', color: FINCEPT.RED }}>NO KEY</span></>
                      )}
                    </div>
                  </div>
                );
              })}
            </div>
          ) : (
            <div style={{ padding: '24px', textAlign: 'center', color: FINCEPT.GRAY }}>
              <AlertCircle size={20} style={{ marginBottom: '8px', opacity: 0.5 }} />
              <div style={{ fontSize: '10px' }}>No LLM providers configured</div>
              <div style={{ fontSize: '9px', color: FINCEPT.MUTED, marginTop: '4px' }}>Go to Settings → LLM Configuration</div>
            </div>
          )}
        </SectionCard>

        {/* Cache Stats */}
        <SectionCard
          title="CACHE STATISTICS"
          action={<button onClick={() => clearAgentCache()} style={{ padding: '2px 6px', backgroundColor: 'transparent', border: `1px solid ${FINCEPT.RED}`, color: FINCEPT.RED, fontSize: '8px', cursor: 'pointer', borderRadius: '2px' }}>CLEAR</button>}
        >
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '12px' }}>
            {[
              { label: 'STATIC', value: getCacheStats().cacheSize },
              { label: 'RESPONSE', value: getCacheStats().responseCacheSize },
              { label: 'MAX SIZE', value: getCacheStats().maxSize },
            ].map((stat, idx) => (
              <div key={idx} style={{ padding: '12px', backgroundColor: FINCEPT.DARK_BG, borderRadius: '2px', textAlign: 'center' }}>
                <div style={{ fontSize: '16px', fontWeight: 700, color: FINCEPT.CYAN }}>{stat.value}</div>
                <div style={{ fontSize: '8px', color: FINCEPT.GRAY, marginTop: '4px' }}>{stat.label}</div>
              </div>
            ))}
          </div>
        </SectionCard>

        {/* Features */}
        {systemInfo?.features && (
          <SectionCard title="FEATURES">
            <div style={{ display: 'flex', flexWrap: 'wrap', gap: '6px' }}>
              {systemInfo.features.map((feature: string) => (
                <span key={feature} style={{ padding: '4px 8px', backgroundColor: FINCEPT.DARK_BG, color: FINCEPT.WHITE, fontSize: '9px', borderRadius: '2px' }}>
                  {feature.replace(/_/g, ' ').toUpperCase()}
                </span>
              ))}
            </div>
          </SectionCard>
        )}

        {/* Version Info */}
        <SectionCard title="VERSION INFO">
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
            <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>VERSION</div>
            <div style={{ fontSize: '10px', color: FINCEPT.CYAN }}>{systemInfo?.version || 'N/A'}</div>
            <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>FRAMEWORK</div>
            <div style={{ fontSize: '10px', color: FINCEPT.CYAN }}>{systemInfo?.framework || 'N/A'}</div>
          </div>
        </SectionCard>
      </div>
    </div>
  );
};

const SectionCard: React.FC<{ title: string; action?: React.ReactNode; children: React.ReactNode }> = ({ title, action, children }) => (
  <div style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', marginBottom: '12px' }}>
    <div style={{ padding: '12px', backgroundColor: FINCEPT.HEADER_BG, borderBottom: `1px solid ${FINCEPT.BORDER}`, display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
      <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY }}>{title}</span>
      {action}
    </div>
    <div style={{ padding: '12px' }}>{children}</div>
  </div>
);
