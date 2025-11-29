/**
 * Recording Control Panel
 * Reusable component for tabs to control context recording
 */

import React, { useState, useEffect } from 'react';
import { Play, Square, Settings2 } from 'lucide-react';
import { contextRecorderService } from '@/services/contextRecorderService';
import { useTerminalTheme } from '@/contexts/ThemeContext';

interface RecordingControlPanelProps {
  tabName: string;
  onRecordingChange?: (isRecording: boolean) => void;
  onRecordingStart?: () => void | Promise<void>; // Called when user clicks RECORD button
}

export const RecordingControlPanel: React.FC<RecordingControlPanelProps> = ({
  tabName,
  onRecordingChange,
  onRecordingStart,
}) => {
  const { colors } = useTerminalTheme();
  const [isRecording, setIsRecording] = useState(false);
  const [isSettingsOpen, setIsSettingsOpen] = useState(false);
  const [autoRecord, setAutoRecord] = useState(false);
  const [dataTypes, setDataTypes] = useState<string[]>(['api_response', 'websocket', 'table_data']);
  const [label, setLabel] = useState('');
  const [tags, setTags] = useState('');

  // Check recording status on mount and periodically
  useEffect(() => {
    checkRecordingStatus();

    // Poll recording status every 2 seconds to stay in sync
    const interval = setInterval(() => {
      checkRecordingStatus();
    }, 2000);

    return () => clearInterval(interval);
  }, [tabName]);

  const checkRecordingStatus = async () => {
    try {
      const recording = await contextRecorderService.isRecording(tabName);

      // Only update if status changed to avoid unnecessary re-renders
      if (recording !== isRecording) {
        console.log(`[RecordingPanel] ${tabName} recording status: ${recording ? 'ACTIVE' : 'STOPPED'}`);
        setIsRecording(recording);
        onRecordingChange?.(recording);
      }
    } catch (error) {
      console.error('[RecordingPanel] Failed to check status:', error);
    }
  };

  const handleStartRecording = async () => {
    try {
      await contextRecorderService.startRecording(tabName, {
        autoRecord,
        dataTypes: dataTypes.length > 0 ? dataTypes : undefined,
        label: label || undefined,
        tags: tags ? tags.split(',').map(t => t.trim()) : undefined,
      });
      setIsRecording(true);
      onRecordingChange?.(true);
      console.log(`[RecordingPanel] Started recording for ${tabName}`);

      // Call the onRecordingStart callback to let the tab capture current data
      if (onRecordingStart) {
        console.log(`[RecordingPanel] Triggering onRecordingStart for ${tabName}`);
        await onRecordingStart();
      }
    } catch (error) {
      console.error('[RecordingPanel] Failed to start recording:', error);
    }
  };

  const handleStopRecording = async () => {
    try {
      await contextRecorderService.stopRecording(tabName);
      setIsRecording(false);
      onRecordingChange?.(false);
      console.log(`[RecordingPanel] Stopped recording for ${tabName}`);
    } catch (error) {
      console.error('[RecordingPanel] Failed to stop recording:', error);
    }
  };

  const toggleDataType = (type: string) => {
    setDataTypes(prev =>
      prev.includes(type)
        ? prev.filter(t => t !== type)
        : [...prev, type]
    );
  };

  return (
    <div style={{ display: 'flex', alignItems: 'center', gap: '8px', position: 'relative' }}>
      {/* Recording Status Indicator */}
      {isRecording && (
        <span style={{
          color: colors.alert,
          fontSize: '11px',
          fontWeight: 'bold',
          animation: 'pulse 2s cubic-bezier(0.4, 0, 0.6, 1) infinite'
        }}>
          ● REC
        </span>
      )}

      {/* Start/Stop Button - Bloomberg Style */}
      <button
        onClick={isRecording ? handleStopRecording : handleStartRecording}
        style={{
          backgroundColor: isRecording ? colors.alert : colors.primary,
          color: 'black',
          border: 'none',
          padding: '4px 12px',
          fontSize: '11px',
          fontWeight: 'bold',
          cursor: 'pointer',
          display: 'flex',
          alignItems: 'center',
          gap: '4px'
        }}
        onMouseEnter={(e) => {
          e.currentTarget.style.opacity = '0.8';
        }}
        onMouseLeave={(e) => {
          e.currentTarget.style.opacity = '1';
        }}
      >
        {isRecording ? (
          <>
            <Square size={12} />
            STOP
          </>
        ) : (
          <>
            <Play size={12} />
            RECORD
          </>
        )}
      </button>

      {/* Settings Button - Bloomberg Style with orange background */}
      <button
        onClick={() => setIsSettingsOpen(!isSettingsOpen)}
        style={{
          backgroundColor: colors.primary,
          color: 'black',
          border: 'none',
          padding: '4px 8px',
          fontSize: '11px',
          fontWeight: 'bold',
          cursor: 'pointer',
          display: 'flex',
          alignItems: 'center'
        }}
        onMouseEnter={(e) => {
          e.currentTarget.style.opacity = '0.8';
        }}
        onMouseLeave={(e) => {
          e.currentTarget.style.opacity = '1';
        }}
        title="Recording Settings"
      >
        <Settings2 size={14} />
      </button>

      {/* Settings Popover - Bloomberg Style */}
      {isSettingsOpen && (
        <>
          {/* Backdrop */}
          <div
            style={{
              position: 'fixed',
              top: 0,
              left: 0,
              right: 0,
              bottom: 0,
              zIndex: 999
            }}
            onClick={() => setIsSettingsOpen(false)}
          />

          {/* Settings Panel */}
          <div style={{
            position: 'absolute',
            top: '100%',
            right: 0,
            marginTop: '4px',
            backgroundColor: colors.panel,
            border: `1px solid ${colors.textMuted}`,
            width: '320px',
            padding: '12px',
            zIndex: 1000,
            boxShadow: '0 4px 12px rgba(0,0,0,0.5)'
          }}>
            {/* Header */}
            <div style={{
              borderBottom: `1px solid ${colors.textMuted}`,
              paddingBottom: '8px',
              marginBottom: '12px'
            }}>
              <div style={{ color: colors.primary, fontSize: '12px', fontWeight: 'bold' }}>
                RECORDING SETTINGS
              </div>
              <div style={{ color: colors.textMuted, fontSize: '10px', marginTop: '2px' }}>
                Configure data capture options
              </div>
            </div>

            {/* Auto Record Toggle */}
            <div style={{
              display: 'flex',
              justifyContent: 'space-between',
              alignItems: 'center',
              marginBottom: '12px',
              padding: '6px',
              backgroundColor: colors.background
            }}>
              <span style={{ color: colors.text, fontSize: '11px' }}>Auto Record</span>
              <input
                type="checkbox"
                checked={autoRecord}
                onChange={(e) => setAutoRecord(e.target.checked)}
                disabled={isRecording}
                style={{ cursor: isRecording ? 'not-allowed' : 'pointer' }}
              />
            </div>

            {/* Data Types */}
            <div style={{ marginBottom: '12px' }}>
              <div style={{ color: colors.text, fontSize: '11px', marginBottom: '6px', fontWeight: 'bold' }}>
                DATA TYPES
              </div>
              {[
                { key: 'api_response', label: 'API Responses' },
                { key: 'websocket', label: 'WebSocket Data' },
                { key: 'table_data', label: 'Table Data' }
              ].map(({ key, label }) => (
                <div key={key} style={{
                  display: 'flex',
                  justifyContent: 'space-between',
                  alignItems: 'center',
                  padding: '4px 6px',
                  backgroundColor: dataTypes.includes(key) ? 'rgba(255,120,0,0.1)' : colors.background,
                  marginBottom: '4px'
                }}>
                  <span style={{ color: colors.text, fontSize: '10px' }}>{label}</span>
                  <input
                    type="checkbox"
                    checked={dataTypes.includes(key)}
                    onChange={() => toggleDataType(key)}
                    disabled={isRecording}
                    style={{ cursor: isRecording ? 'not-allowed' : 'pointer' }}
                  />
                </div>
              ))}
            </div>

            {/* Label Input */}
            <div style={{ marginBottom: '12px' }}>
              <label style={{ color: colors.text, fontSize: '11px', display: 'block', marginBottom: '4px' }}>
                SESSION LABEL
              </label>
              <input
                type="text"
                value={label}
                onChange={(e) => setLabel(e.target.value)}
                disabled={isRecording}
                placeholder="e.g., Market Analysis"
                style={{
                  width: '100%',
                  backgroundColor: colors.background,
                  border: `1px solid ${colors.textMuted}`,
                  color: colors.text,
                  padding: '4px 8px',
                  fontSize: '10px',
                  outline: 'none'
                }}
              />
            </div>

            {/* Tags Input */}
            <div>
              <label style={{ color: colors.text, fontSize: '11px', display: 'block', marginBottom: '4px' }}>
                TAGS (comma-separated)
              </label>
              <input
                type="text"
                value={tags}
                onChange={(e) => setTags(e.target.value)}
                disabled={isRecording}
                placeholder="e.g., analysis, portfolio"
                style={{
                  width: '100%',
                  backgroundColor: colors.background,
                  border: `1px solid ${colors.textMuted}`,
                  color: colors.text,
                  padding: '4px 8px',
                  fontSize: '10px',
                  outline: 'none'
                }}
              />
            </div>
          </div>
        </>
      )}

      <style>{`
        @keyframes pulse {
          0%, 100% { opacity: 1; }
          50% { opacity: 0.5; }
        }
      `}</style>
    </div>
  );
};

export default RecordingControlPanel;
