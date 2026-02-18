// GovDataTab - Main Government Data Portals tab
// Renders a provider selector sidebar and the active provider's UI

import React, { useState } from 'react';
import { Landmark } from 'lucide-react';
import { FINCEPT, GOV_PROVIDERS } from './constants';
import type { GovProvider } from './types';
import { USTreasuryProvider } from './providers/USTreasuryProvider';
import { CanadaGovProvider } from './providers/CanadaGovProvider';
import { CongressGovProvider } from './providers/CongressGovProvider';
import { OpenAfricaProvider } from './providers/OpenAfricaProvider';
import { SpainDataProvider } from './providers/SpainDataProvider';
import { PxWebProvider } from './providers/PxWebProvider';
import { SwissGovProvider } from './providers/SwissGovProvider';
import { FrenchGovProvider } from './providers/FrenchGovProvider';
import { UniversalCkanProvider } from './providers/UniversalCkanProvider';
import { DataGovHkProvider } from './providers/DataGovHkProvider';

export default function GovDataTab() {
  const [activeProvider, setActiveProvider] = useState<GovProvider>('us-treasury');

  const activeConfig = GOV_PROVIDERS.find(p => p.id === activeProvider) || GOV_PROVIDERS[0];

  const renderProvider = () => {
    switch (activeProvider) {
      case 'us-treasury':
        return <USTreasuryProvider />;
      case 'canada-gov':
        return <CanadaGovProvider />;
      case 'us-congress':
        return <CongressGovProvider />;
      case 'openafrica':
        return <OpenAfricaProvider />;
      case 'spain':
        return <SpainDataProvider />;
      case 'finland-pxweb':
        return <PxWebProvider />;
      case 'swiss':
        return <SwissGovProvider />;
      case 'france':
        return <FrenchGovProvider />;
      case 'universal-ckan':
        return <UniversalCkanProvider />;
      case 'hk':
        return <DataGovHkProvider />;
      default:
        return (
          <div style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            height: '100%',
            color: FINCEPT.GRAY,
            fontSize: '12px',
          }}>
            Provider not implemented yet
          </div>
        );
    }
  };

  return (
    <div
      style={{
        width: '100%',
        height: '100%',
        backgroundColor: FINCEPT.DARK_BG,
        color: FINCEPT.WHITE,
        fontFamily: '"IBM Plex Mono", monospace',
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
      }}
    >
      {/* Header */}
      <div
        style={{
          padding: '10px 16px',
          backgroundColor: FINCEPT.HEADER_BG,
          borderBottom: `2px solid ${activeConfig.color}`,
          display: 'flex',
          alignItems: 'center',
          gap: '12px',
        }}
      >
        <Landmark size={18} color={activeConfig.color} />
        <span style={{ fontSize: '14px', fontWeight: 700, color: FINCEPT.WHITE }}>
          GOVT
        </span>
        <span style={{ fontSize: '10px', color: FINCEPT.GRAY }}>|</span>
        <span style={{ fontSize: '12px', fontWeight: 600, color: activeConfig.color }}>
          {activeConfig.flag} {activeConfig.fullName}
        </span>
      </div>

      {/* Main content */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        {/* Provider sidebar */}
        <div
          style={{
            width: '220px',
            backgroundColor: FINCEPT.PANEL_BG,
            borderRight: `1px solid ${FINCEPT.BORDER}`,
            display: 'flex',
            flexDirection: 'column',
            overflow: 'auto',
          }}
        >
          <div style={{
            padding: '10px 12px',
            fontSize: '9px',
            fontWeight: 700,
            color: FINCEPT.GRAY,
            textTransform: 'uppercase',
            letterSpacing: '1px',
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
          }}>
            Sovereign Data
          </div>

          {GOV_PROVIDERS.map(provider => (
            <button
              key={provider.id}
              onClick={() => setActiveProvider(provider.id)}
              style={{
                padding: '10px 12px',
                backgroundColor: activeProvider === provider.id ? `${provider.color}15` : 'transparent',
                color: activeProvider === provider.id ? provider.color : FINCEPT.WHITE,
                border: 'none',
                borderLeft: activeProvider === provider.id ? `3px solid ${provider.color}` : '3px solid transparent',
                borderBottom: `1px solid ${FINCEPT.BORDER}08`,
                textAlign: 'left',
                cursor: 'pointer',
                display: 'flex',
                flexDirection: 'column',
                gap: '4px',
              }}
            >
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                <span style={{ fontSize: '14px' }}>{provider.flag}</span>
                <span style={{ fontSize: '11px', fontWeight: 600 }}>{provider.name}</span>
              </div>
              <div style={{ fontSize: '9px', color: FINCEPT.GRAY, paddingLeft: '22px' }}>
                {provider.description.length > 60
                  ? provider.description.substring(0, 60) + '...'
                  : provider.description}
              </div>
            </button>
          ))}

        </div>

        {/* Active provider content */}
        <div style={{ flex: 1, overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
          {renderProvider()}
        </div>
      </div>

      {/* Status bar */}
      <div
        style={{
          padding: '6px 16px',
          backgroundColor: FINCEPT.HEADER_BG,
          borderTop: `1px solid ${FINCEPT.BORDER}`,
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          fontSize: '9px',
        }}
      >
        <div style={{ display: 'flex', gap: '16px', color: FINCEPT.WHITE }}>
          <span>
            <span style={{ color: FINCEPT.GRAY }}>PORTAL:</span>{' '}
            <span style={{ color: activeConfig.color }}>{activeConfig.name}</span>
          </span>
          <span>
            <span style={{ color: FINCEPT.GRAY }}>COUNTRY:</span> {activeConfig.country}
          </span>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          <span style={{ width: '6px', height: '6px', borderRadius: '50%', backgroundColor: FINCEPT.GREEN }} />
          <span style={{ color: FINCEPT.GREEN }}>Ready</span>
        </div>
      </div>
    </div>
  );
}
