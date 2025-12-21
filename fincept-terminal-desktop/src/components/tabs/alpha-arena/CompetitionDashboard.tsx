/**
 * Alpha Arena Competition Dashboard
 * Shows all competitions (past and active) with management controls
 */

import React, { useState, useEffect } from 'react';
import { Trophy, Play, Eye, Trash2, Plus, TrendingUp, Calendar, Clock } from 'lucide-react';
import { sqliteService, AlphaCompetition } from '@/services/sqliteService';

const BLOOMBERG = {
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  GRAY: '#787878',
  MUTED: '#555555',
  WHITE: '#FFFFFF',
  ORANGE: '#FF8800',
  GREEN: '#00D66F',
  RED: '#FF3B3B',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
};

interface CompetitionDashboardProps {
  onCreateNew: () => void;
  onViewCompetition: (competition: AlphaCompetition) => void;
  currentCompetitionId?: string;
}

export const CompetitionDashboard: React.FC<CompetitionDashboardProps> = ({
  onCreateNew,
  onViewCompetition,
  currentCompetitionId
}) => {
  const [competitions, setCompetitions] = useState<AlphaCompetition[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    loadCompetitions();
  }, []);

  const loadCompetitions = async () => {
    try {
      setLoading(true);
      const comps = await sqliteService.getAlphaCompetitions(50);
      setCompetitions(comps);
    } catch (err) {
      console.error('[CompetitionDashboard] Failed to load competitions:', err);
    } finally {
      setLoading(false);
    }
  };

  const handleDelete = async (id: string, e: React.MouseEvent) => {
    e.stopPropagation();
    if (confirm('Are you sure you want to delete this competition? All data will be lost.')) {
      try {
        await sqliteService.deleteAlphaCompetition(id);
        await loadCompetitions();
      } catch (err) {
        console.error('[CompetitionDashboard] Failed to delete competition:', err);
      }
    }
  };

  const getStatusColor = (status: string) => {
    switch (status) {
      case 'running': return BLOOMBERG.GREEN;
      case 'paused': return BLOOMBERG.YELLOW;
      case 'completed': return BLOOMBERG.GRAY;
      default: return BLOOMBERG.CYAN;
    }
  };

  const formatDate = (isoString: string) => {
    const date = new Date(isoString);
    return date.toLocaleDateString('en-US', {
      month: 'short',
      day: 'numeric',
      year: 'numeric',
      hour: '2-digit',
      minute: '2-digit'
    });
  };

  if (loading) {
    return (
      <div style={{
        flex: 1,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        color: BLOOMBERG.GRAY,
        fontSize: '11px',
        fontFamily: '"IBM Plex Mono", monospace'
      }}>
        Loading competitions...
      </div>
    );
  }

  if (competitions.length === 0) {
    return (
      <div style={{
        flex: 1,
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        justifyContent: 'center',
        gap: '24px',
        padding: '48px'
      }}>
        <Trophy size={64} color={BLOOMBERG.GRAY} strokeWidth={1.5} />
        <div style={{
          textAlign: 'center',
          color: BLOOMBERG.GRAY,
          fontFamily: '"IBM Plex Mono", monospace'
        }}>
          <div style={{ fontSize: '16px', fontWeight: 700, marginBottom: '8px', color: BLOOMBERG.WHITE }}>
            NO COMPETITIONS YET
          </div>
          <div style={{ fontSize: '11px', maxWidth: '400px', lineHeight: '1.6' }}>
            Create your first Alpha Arena competition to pit AI models against each other in live trading simulations.
          </div>
        </div>
        <button
          onClick={onCreateNew}
          style={{
            padding: '12px 32px',
            backgroundColor: BLOOMBERG.ORANGE,
            color: BLOOMBERG.DARK_BG,
            border: 'none',
            fontSize: '11px',
            fontWeight: 700,
            fontFamily: '"IBM Plex Mono", monospace',
            cursor: 'pointer',
            letterSpacing: '0.5px',
            display: 'flex',
            alignItems: 'center',
            gap: '8px'
          }}
          onMouseOver={(e) => e.currentTarget.style.backgroundColor = '#FFA033'}
          onMouseOut={(e) => e.currentTarget.style.backgroundColor = BLOOMBERG.ORANGE}
        >
          <Plus size={16} />
          CREATE COMPETITION
        </button>
      </div>
    );
  }

  return (
    <div style={{
      flex: 1,
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden'
    }}>
      {/* Header */}
      <div style={{
        padding: '16px 20px',
        backgroundColor: BLOOMBERG.HEADER_BG,
        borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center'
      }}>
        <div style={{
          fontSize: '11px',
          fontWeight: 700,
          color: BLOOMBERG.ORANGE,
          letterSpacing: '0.5px',
          fontFamily: '"IBM Plex Mono", monospace'
        }}>
          COMPETITION HISTORY ({competitions.length})
        </div>
        <button
          onClick={onCreateNew}
          style={{
            padding: '8px 16px',
            backgroundColor: BLOOMBERG.ORANGE,
            color: BLOOMBERG.DARK_BG,
            border: 'none',
            fontSize: '10px',
            fontWeight: 700,
            fontFamily: '"IBM Plex Mono", monospace',
            cursor: 'pointer',
            letterSpacing: '0.5px',
            display: 'flex',
            alignItems: 'center',
            gap: '6px'
          }}
          onMouseOver={(e) => e.currentTarget.style.backgroundColor = '#FFA033'}
          onMouseOut={(e) => e.currentTarget.style.backgroundColor = BLOOMBERG.ORANGE}
        >
          <Plus size={14} />
          NEW
        </button>
      </div>

      {/* Competition List */}
      <div style={{
        flex: 1,
        overflow: 'auto',
        padding: '16px'
      }}>
        <div style={{
          display: 'grid',
          gridTemplateColumns: 'repeat(auto-fill, minmax(350px, 1fr))',
          gap: '16px'
        }}>
          {competitions.map((comp) => (
            <div
              key={comp.id}
              onClick={() => onViewCompetition(comp)}
              style={{
                backgroundColor: comp.id === currentCompetitionId ? BLOOMBERG.HEADER_BG : BLOOMBERG.PANEL_BG,
                border: `1px solid ${comp.id === currentCompetitionId ? BLOOMBERG.ORANGE : BLOOMBERG.BORDER}`,
                padding: '16px',
                cursor: 'pointer',
                transition: 'all 0.2s',
                fontFamily: '"IBM Plex Mono", monospace'
              }}
              onMouseOver={(e) => {
                if (comp.id !== currentCompetitionId) {
                  e.currentTarget.style.borderColor = BLOOMBERG.GRAY;
                }
              }}
              onMouseOut={(e) => {
                if (comp.id !== currentCompetitionId) {
                  e.currentTarget.style.borderColor = BLOOMBERG.BORDER;
                }
              }}
            >
              {/* Header */}
              <div style={{
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'flex-start',
                marginBottom: '12px'
              }}>
                <div style={{ flex: 1 }}>
                  <div style={{
                    fontSize: '12px',
                    fontWeight: 700,
                    color: BLOOMBERG.WHITE,
                    marginBottom: '4px'
                  }}>
                    {comp.name}
                  </div>
                  <div style={{
                    fontSize: '9px',
                    color: getStatusColor(comp.status),
                    fontWeight: 600,
                    letterSpacing: '0.5px'
                  }}>
                    ● {comp.status.toUpperCase()}
                  </div>
                </div>
                <div style={{ display: 'flex', gap: '8px' }}>
                  <button
                    onClick={(e) => {
                      e.stopPropagation();
                      onViewCompetition(comp);
                    }}
                    style={{
                      padding: '4px 8px',
                      backgroundColor: 'transparent',
                      border: `1px solid ${BLOOMBERG.CYAN}`,
                      color: BLOOMBERG.CYAN,
                      fontSize: '9px',
                      fontWeight: 600,
                      cursor: 'pointer',
                      fontFamily: '"IBM Plex Mono", monospace'
                    }}
                    title="View Details"
                  >
                    <Eye size={12} />
                  </button>
                  <button
                    onClick={(e) => handleDelete(comp.id, e)}
                    style={{
                      padding: '4px 8px',
                      backgroundColor: 'transparent',
                      border: `1px solid ${BLOOMBERG.RED}`,
                      color: BLOOMBERG.RED,
                      fontSize: '9px',
                      fontWeight: 600,
                      cursor: 'pointer',
                      fontFamily: '"IBM Plex Mono", monospace'
                    }}
                    title="Delete"
                  >
                    <Trash2 size={12} />
                  </button>
                </div>
              </div>

              {/* Details */}
              <div style={{
                display: 'grid',
                gridTemplateColumns: '1fr 1fr',
                gap: '8px',
                marginBottom: '12px'
              }}>
                <div>
                  <div style={{ fontSize: '8px', color: BLOOMBERG.GRAY, marginBottom: '2px' }}>SYMBOL</div>
                  <div style={{ fontSize: '10px', color: BLOOMBERG.CYAN, fontWeight: 600 }}>{comp.symbol}</div>
                </div>
                <div>
                  <div style={{ fontSize: '8px', color: BLOOMBERG.GRAY, marginBottom: '2px' }}>MODE</div>
                  <div style={{ fontSize: '10px', color: BLOOMBERG.YELLOW, fontWeight: 600 }}>{comp.mode.toUpperCase()}</div>
                </div>
                <div>
                  <div style={{ fontSize: '8px', color: BLOOMBERG.GRAY, marginBottom: '2px' }}>CYCLES</div>
                  <div style={{ fontSize: '10px', color: BLOOMBERG.WHITE, fontWeight: 600 }}>{comp.cycle_count}</div>
                </div>
                <div>
                  <div style={{ fontSize: '8px', color: BLOOMBERG.GRAY, marginBottom: '2px' }}>CAPITAL</div>
                  <div style={{ fontSize: '10px', color: BLOOMBERG.GREEN, fontWeight: 600 }}>${comp.initial_capital.toLocaleString()}</div>
                </div>
              </div>

              {/* Timestamps */}
              <div style={{
                borderTop: `1px solid ${BLOOMBERG.BORDER}`,
                paddingTop: '8px',
                fontSize: '8px',
                color: BLOOMBERG.MUTED
              }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '4px', marginBottom: '4px' }}>
                  <Calendar size={10} />
                  Created: {formatDate(comp.created_at)}
                </div>
                {comp.started_at && (
                  <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                    <Clock size={10} />
                    Started: {formatDate(comp.started_at)}
                  </div>
                )}
              </div>
            </div>
          ))}
        </div>
      </div>
    </div>
  );
};

export default CompetitionDashboard;
