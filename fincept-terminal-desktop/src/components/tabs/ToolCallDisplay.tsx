// Tool Call Display Component
// Shows MCP tool execution in chat messages

import React from 'react';
import { Wrench, CheckCircle, XCircle, Clock } from 'lucide-react';

interface ToolCallDisplayProps {
  toolName: string;
  args: any;
  result?: any;
  status: 'pending' | 'success' | 'error';
  executionTime?: number;
}

const ToolCallDisplay: React.FC<ToolCallDisplayProps> = ({
  toolName,
  args,
  result,
  status,
  executionTime
}) => {
  const BLOOMBERG_ORANGE = '#FFA500';
  const BLOOMBERG_WHITE = '#FFFFFF';
  const BLOOMBERG_GRAY = '#787878';
  const BLOOMBERG_DARK_BG = '#000000';
  const BLOOMBERG_PANEL_BG = '#0a0a0a';
  const BLOOMBERG_GREEN = '#00C800';
  const BLOOMBERG_RED = '#FF0000';
  const BLOOMBERG_YELLOW = '#FFFF00';

  const statusColor = {
    pending: BLOOMBERG_YELLOW,
    success: BLOOMBERG_GREEN,
    error: BLOOMBERG_RED
  }[status];

  const StatusIcon = {
    pending: Clock,
    success: CheckCircle,
    error: XCircle
  }[status];

  // Parse server and tool name
  const [serverId, actualToolName] = toolName.includes('__')
    ? toolName.split('__')
    : ['unknown', toolName];

  return (
    <div style={{
      backgroundColor: BLOOMBERG_PANEL_BG,
      border: `1px solid ${statusColor}`,
      borderLeft: `3px solid ${statusColor}`,
      padding: '8px',
      marginBottom: '8px',
      fontSize: '11px',
      fontFamily: 'Consolas, monospace'
    }}>
      {/* Header */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        gap: '6px',
        marginBottom: '6px'
      }}>
        <Wrench size={12} color={BLOOMBERG_ORANGE} />
        <span style={{ color: BLOOMBERG_ORANGE, fontWeight: 'bold', fontSize: '11px' }}>
          TOOL CALL
        </span>
        <span style={{ color: BLOOMBERG_WHITE }}>→</span>
        <span style={{ color: BLOOMBERG_WHITE, fontWeight: 'bold' }}>{actualToolName}</span>
        <span style={{
          color: BLOOMBERG_GRAY,
          fontSize: '9px',
          backgroundColor: BLOOMBERG_DARK_BG,
          padding: '2px 4px'
        }}>
          {serverId}
        </span>
        <StatusIcon size={12} color={statusColor} style={{ marginLeft: 'auto' }} />
        {status === 'pending' && (
          <span style={{ color: statusColor, fontSize: '10px' }}>Running...</span>
        )}
        {status === 'success' && (
          <span style={{ color: statusColor, fontSize: '10px' }}>✓ Success</span>
        )}
        {status === 'error' && (
          <span style={{ color: statusColor, fontSize: '10px' }}>✗ Failed</span>
        )}
      </div>

      {/* Arguments */}
      <div style={{ marginBottom: '6px' }}>
        <div style={{
          color: BLOOMBERG_GRAY,
          fontSize: '9px',
          marginBottom: '2px',
          fontWeight: 'bold'
        }}>
          Arguments:
        </div>
        <div style={{
          backgroundColor: BLOOMBERG_DARK_BG,
          border: `1px solid ${BLOOMBERG_GRAY}`,
          padding: '4px 6px',
          fontSize: '9px',
          color: BLOOMBERG_WHITE,
          fontFamily: 'Consolas, monospace',
          maxHeight: '100px',
          overflow: 'auto'
        }}>
          <pre style={{ margin: 0, whiteSpace: 'pre-wrap' }}>
            {JSON.stringify(args, null, 2)}
          </pre>
        </div>
      </div>

      {/* Result (if available) */}
      {result && (
        <div>
          <div style={{
            color: BLOOMBERG_GRAY,
            fontSize: '9px',
            marginBottom: '2px',
            fontWeight: 'bold'
          }}>
            Result:
          </div>
          <div style={{
            backgroundColor: BLOOMBERG_DARK_BG,
            border: `1px solid ${BLOOMBERG_GRAY}`,
            padding: '4px 6px',
            fontSize: '9px',
            color: BLOOMBERG_WHITE,
            fontFamily: 'Consolas, monospace',
            maxHeight: '200px',
            overflow: 'auto'
          }}>
            <pre style={{ margin: 0, whiteSpace: 'pre-wrap' }}>
              {typeof result === 'string'
                ? result
                : JSON.stringify(result, null, 2)}
            </pre>
          </div>
        </div>
      )}

      {/* Execution time */}
      {executionTime !== undefined && (
        <div style={{
          color: BLOOMBERG_GRAY,
          fontSize: '8px',
          marginTop: '4px',
          textAlign: 'right'
        }}>
          Executed in {executionTime}ms
        </div>
      )}
    </div>
  );
};

export default ToolCallDisplay;
