// Live Preview - Terminal UI/UX

import React from 'react';
import { CheckCircle2, AlertCircle, Info, Clock, Database, Activity } from 'lucide-react';
import { MappingExecutionResult } from '../types';
import { useTerminalTheme } from '@/contexts/ThemeContext';

interface LivePreviewProps {
  result?: MappingExecutionResult;
  isLoading?: boolean;
}

export function LivePreview({ result, isLoading }: LivePreviewProps) {
  const { colors } = useTerminalTheme();

  const panelStyle = (borderColor: string, bgColor?: string) => ({
    backgroundColor: bgColor || colors.panel,
    border: `1px solid ${borderColor}`,
  });

  const headerStyle = (bgColor: string, borderColor: string) => ({
    backgroundColor: bgColor,
    borderBottom: `1px solid ${borderColor}`,
  });

  if (isLoading) {
    return (
      <div
        className="flex items-center justify-center p-12"
        style={panelStyle('var(--ft-border-color, #2A2A2A)', colors.background)}
      >
        <div className="text-center">
          <div
            className="inline-flex items-center justify-center w-12 h-12 mb-4 animate-spin"
            style={{ border: `2px solid var(--ft-border-color, #2A2A2A)`, borderTopColor: colors.primary }}
          />
          <p className="text-xs font-bold tracking-wider" style={{ color: colors.textMuted }}>TRANSFORMING DATA...</p>
        </div>
      </div>
    );
  }

  if (!result) {
    return (
      <div
        className="flex items-center justify-center p-12"
        style={panelStyle('var(--ft-border-color, #2A2A2A)', colors.background)}
      >
        <div className="text-center">
          <Info size={40} color={colors.textMuted} className="mx-auto mb-3 opacity-40" />
          <p className="text-xs font-bold tracking-wider" style={{ color: colors.textMuted }}>AWAITING TEST RUN</p>
          <p className="text-[11px] mt-1" style={{ color: colors.textMuted }}>Configure mapping and run test to see preview</p>
        </div>
      </div>
    );
  }

  const statusColor = result.success ? colors.success : colors.alert;

  return (
    <div className="space-y-3">

      {/* ── Status Header ── */}
      <div style={panelStyle(statusColor, `${statusColor}08`)}>
        <div
          className="flex items-center justify-between px-3 py-2.5 border-b"
          style={{ borderColor: `${statusColor}30`, backgroundColor: `${statusColor}12` }}
        >
          <div className="flex items-center gap-2">
            {result.success ? (
              <CheckCircle2 size={14} color={statusColor} />
            ) : (
              <AlertCircle size={14} color={statusColor} />
            )}
            <span className="text-xs font-bold tracking-wider" style={{ color: statusColor }}>
              {result.success ? 'TRANSFORMATION SUCCESSFUL' : 'TRANSFORMATION FAILED'}
            </span>
          </div>
          <span className="text-[10px] font-mono" style={{ color: colors.textMuted }}>
            {result.metadata.executedAt ? new Date(result.metadata.executedAt).toLocaleTimeString() : ''}
          </span>
        </div>

        {/* Metrics row */}
        <div className="grid grid-cols-3 divide-x" style={{ borderColor: `${statusColor}20` }}>
          {[
            { icon: Clock, label: 'Duration', value: `${result.metadata.duration}ms` },
            { icon: Database, label: 'Processed', value: String(result.metadata.recordsProcessed) },
            { icon: Activity, label: 'Returned', value: String(result.metadata.recordsReturned) },
          ].map(({ icon: Icon, label, value }) => (
            <div key={label} className="flex items-center gap-2 px-4 py-2.5">
              <Icon size={12} color={colors.text} />
              <div>
                <div className="text-[10px]" style={{ color: colors.text }}>{label}</div>
                <div className="text-xs font-bold font-mono" style={{ color: statusColor }}>{value}</div>
              </div>
            </div>
          ))}
        </div>
      </div>

      {/* ── Transformed Output ── */}
      {result.success && result.data && (
        <div style={panelStyle(colors.success)}>
          <div
            className="flex items-center justify-between px-3 py-2"
            style={headerStyle(`${colors.success}15`, `${colors.success}40`)}
          >
            <div className="flex items-center gap-2">
              <CheckCircle2 size={12} color={colors.success} />
              <span className="text-[11px] font-bold tracking-wider" style={{ color: colors.success }}>TRANSFORMED OUTPUT</span>
            </div>
            <span className="text-[10px] font-mono" style={{ color: colors.textMuted }}>
              {Array.isArray(result.data) ? `${result.data.length} record(s)` : '1 record'}
            </span>
          </div>
          <div className="p-3 max-h-72 overflow-auto" style={{ backgroundColor: colors.background }}>
            <pre className="text-[11px] font-mono" style={{ color: colors.success }}>
              {JSON.stringify(result.data, null, 2)}
            </pre>
          </div>
        </div>
      )}

      {/* ── Raw Input Data ── */}
      {result.rawData && (
        <div style={panelStyle('var(--ft-border-color, #2A2A2A)')}>
          <div
            className="flex items-center gap-2 px-3 py-2 border-b"
            style={{ borderColor: 'var(--ft-border-color, #2A2A2A)', backgroundColor: colors.panel }}
          >
            <Database size={12} color={colors.text} />
            <span className="text-[11px] font-bold tracking-wider" style={{ color: colors.text }}>RAW INPUT DATA</span>
          </div>
          <div className="p-3 max-h-48 overflow-auto" style={{ backgroundColor: colors.background }}>
            <pre className="text-[11px] font-mono" style={{ color: colors.text }}>
              {JSON.stringify(result.rawData, null, 2)}
            </pre>
          </div>
        </div>
      )}

      {/* ── Errors ── */}
      {result.errors && result.errors.length > 0 && (
        <div style={panelStyle(colors.alert)}>
          <div
            className="flex items-center gap-2 px-3 py-2 border-b"
            style={headerStyle(`${colors.alert}15`, `${colors.alert}40`)}
          >
            <AlertCircle size={12} color={colors.alert} />
            <span className="text-[11px] font-bold tracking-wider" style={{ color: colors.alert }}>
              ERRORS ({result.errors.length})
            </span>
          </div>
          <div className="p-3 space-y-1.5">
            {result.errors.map((error, idx) => (
              <div key={idx} className="flex items-start gap-2 text-[11px] font-mono" style={{ color: colors.alert }}>
                <span className="flex-shrink-0 mt-0.5">▸</span>
                <span>{error}</span>
              </div>
            ))}
          </div>
        </div>
      )}

      {/* ── Warnings ── */}
      {result.warnings && result.warnings.length > 0 && (
        <div style={panelStyle(colors.warning || '#F59E0B')}>
          <div
            className="flex items-center gap-2 px-3 py-2 border-b"
            style={headerStyle(`${colors.warning || '#F59E0B'}15`, `${colors.warning || '#F59E0B'}40`)}
          >
            <AlertCircle size={12} color={colors.warning || '#F59E0B'} />
            <span className="text-[11px] font-bold tracking-wider" style={{ color: colors.warning || '#F59E0B' }}>
              WARNINGS ({result.warnings.length})
            </span>
          </div>
          <div className="p-3 space-y-1.5">
            {result.warnings.map((warning, idx) => (
              <div key={idx} className="flex items-start gap-2 text-[11px] font-mono" style={{ color: colors.warning || '#F59E0B' }}>
                <span className="flex-shrink-0 mt-0.5">▸</span>
                <span>{warning}</span>
              </div>
            ))}
          </div>
        </div>
      )}

      {/* ── Validation Results ── */}
      {result.metadata.validationResults && (
        <div
          style={panelStyle(
            result.metadata.validationResults.valid ? colors.success : colors.warning || '#F59E0B'
          )}
        >
          <div
            className="flex items-center gap-2 px-3 py-2 border-b"
            style={headerStyle(
              `${result.metadata.validationResults.valid ? colors.success : colors.warning || '#F59E0B'}15`,
              `${result.metadata.validationResults.valid ? colors.success : colors.warning || '#F59E0B'}40`
            )}
          >
            {result.metadata.validationResults.valid ? (
              <CheckCircle2 size={12} color={colors.success} />
            ) : (
              <AlertCircle size={12} color={colors.warning || '#F59E0B'} />
            )}
            <span
              className="text-[11px] font-bold tracking-wider"
              style={{ color: result.metadata.validationResults.valid ? colors.success : colors.warning || '#F59E0B' }}
            >
              VALIDATION {result.metadata.validationResults.valid ? 'PASSED' : 'WARNINGS'}
            </span>
          </div>
          {!result.metadata.validationResults.valid && (
            <div className="p-3 space-y-1.5">
              {result.metadata.validationResults.errors.map((err, idx) => (
                <div key={idx} className="flex items-start gap-2 text-[11px]" style={{ color: colors.warning || '#F59E0B' }}>
                  <span className="font-bold flex-shrink-0">{err.field}:</span>
                  <span className="font-mono">{err.message}</span>
                </div>
              ))}
            </div>
          )}
        </div>
      )}
    </div>
  );
}
