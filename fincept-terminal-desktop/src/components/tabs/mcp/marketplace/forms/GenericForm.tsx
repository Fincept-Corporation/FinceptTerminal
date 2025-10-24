// Generic Configuration Form
// Fallback form for MCP servers that don't have custom forms
// Allows users to configure environment variables in KEY=VALUE format

import React, { useState } from 'react';
import { MCPServerDefinition } from '../serverDefinitions';

interface GenericFormProps {
  server: MCPServerDefinition;
  onSubmit: (config: { args: string[]; env: Record<string, string> }) => void;
  onCancel: () => void;
}

const GenericForm: React.FC<GenericFormProps> = ({ server, onSubmit, onCancel }) => {
  const [envValues, setEnvValues] = useState<Record<string, string>>(
    server.env || {}
  );

  const BLOOMBERG_ORANGE = '#FFA500';
  const BLOOMBERG_WHITE = '#FFFFFF';
  const BLOOMBERG_GRAY = '#787878';
  const BLOOMBERG_DARK_BG = '#000000';

  const handleSubmit = () => {
    // Check if all required env vars are filled
    const allFilled = Object.values(envValues).every(v => v.trim());

    if (!allFilled) {
      alert('Please fill in all required fields');
      return;
    }

    onSubmit({
      args: server.requiresArg && envValues[server.requiresArg]
        ? [envValues[server.requiresArg]]
        : [],
      env: server.requiresArg
        ? {}
        : envValues
    });
  };

  return (
    <div>
      <div style={{
        color: BLOOMBERG_WHITE,
        fontSize: '10px',
        marginBottom: '12px'
      }}>
        {server.description}
      </div>

      {server.env && Object.keys(server.env).length > 0 && (
        <>
          {Object.keys(server.env).map(key => (
            <div key={key} style={{ marginBottom: '12px' }}>
              <label style={{
                display: 'block',
                color: BLOOMBERG_ORANGE,
                fontSize: '10px',
                marginBottom: '4px',
                fontWeight: 'bold'
              }}>
                {key}
              </label>
              <input
                type="text"
                value={envValues[key] || ''}
                onChange={(e) => setEnvValues({...envValues, [key]: e.target.value})}
                placeholder={`Enter ${key}`}
                style={{
                  width: '100%',
                  backgroundColor: BLOOMBERG_DARK_BG,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  color: BLOOMBERG_WHITE,
                  padding: '6px',
                  fontSize: '10px',
                  fontFamily: 'Consolas, monospace'
                }}
              />
            </div>
          ))}
        </>
      )}

      {server.requiresArg && (
        <div style={{ marginBottom: '12px' }}>
          <label style={{
            display: 'block',
            color: BLOOMBERG_ORANGE,
            fontSize: '10px',
            marginBottom: '4px',
            fontWeight: 'bold'
          }}>
            {server.requiresArg} *
          </label>
          <input
            type="text"
            value={envValues[server.requiresArg] || ''}
            onChange={(e) => setEnvValues({...envValues, [server.requiresArg!]: e.target.value})}
            placeholder={`Enter ${server.requiresArg}`}
            style={{
              width: '100%',
              backgroundColor: BLOOMBERG_DARK_BG,
              border: `1px solid ${BLOOMBERG_GRAY}`,
              color: BLOOMBERG_WHITE,
              padding: '6px',
              fontSize: '10px',
              fontFamily: 'Consolas, monospace'
            }}
          />
        </div>
      )}

      {/* Actions */}
      <div style={{
        display: 'flex',
        gap: '8px',
        justifyContent: 'flex-end',
        marginTop: '16px',
        paddingTop: '12px',
        borderTop: `1px solid ${BLOOMBERG_GRAY}`
      }}>
        <button
          onClick={onCancel}
          style={{
            backgroundColor: BLOOMBERG_DARK_BG,
            border: `1px solid ${BLOOMBERG_GRAY}`,
            color: BLOOMBERG_WHITE,
            padding: '6px 12px',
            fontSize: '10px',
            cursor: 'pointer'
          }}
        >
          CANCEL
        </button>
        <button
          onClick={handleSubmit}
          disabled={!Object.values(envValues).every(v => v.trim())}
          style={{
            backgroundColor: Object.values(envValues).every(v => v.trim()) ? BLOOMBERG_ORANGE : BLOOMBERG_GRAY,
            border: 'none',
            color: 'black',
            padding: '6px 12px',
            fontSize: '10px',
            fontWeight: 'bold',
            cursor: Object.values(envValues).every(v => v.trim()) ? 'pointer' : 'not-allowed'
          }}
        >
          INSTALL
        </button>
      </div>
    </div>
  );
};

export default GenericForm;
