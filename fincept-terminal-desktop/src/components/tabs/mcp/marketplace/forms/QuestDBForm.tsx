// QuestDB Configuration Form
// Collects connection details for QuestDB time-series database

import React, { useState } from 'react';

interface QuestDBFormProps {
  onSubmit: (config: { args: string[]; env: Record<string, string> }) => void;
  onCancel: () => void;
}

const QuestDBForm: React.FC<QuestDBFormProps> = ({ onSubmit, onCancel }) => {
  const [qdbHost, setQdbHost] = useState('localhost');
  const [qdbPort, setQdbPort] = useState('8812'); // QuestDB default PostgreSQL wire protocol port
  const [qdbUser, setQdbUser] = useState('admin');
  const [qdbPassword, setQdbPassword] = useState('');
  const [qdbDatabase, setQdbDatabase] = useState('qdb');
  const [qdbSsl, setQdbSsl] = useState(false);

  const BLOOMBERG_ORANGE = '#FFA500';
  const BLOOMBERG_WHITE = '#FFFFFF';
  const BLOOMBERG_GRAY = '#787878';
  const BLOOMBERG_DARK_BG = '#000000';
  const BLOOMBERG_YELLOW = '#FFFF00';

  const buildQuestDBUrl = () => {
    const encodedPassword = encodeURIComponent(qdbPassword);
    const sslParam = qdbSsl ? '?sslmode=require' : '';
    return `postgresql://${qdbUser}:${encodedPassword}@${qdbHost}:${qdbPort}/${qdbDatabase}${sslParam}`;
  };

  const handleSubmit = () => {
    const dbUrl = buildQuestDBUrl();
    onSubmit({
      args: [dbUrl],
      env: {}
    });
  };

  return (
    <div>
      <div style={{
        color: BLOOMBERG_WHITE,
        fontSize: '10px',
        marginBottom: '12px'
      }}>
        Configure your QuestDB connection. QuestDB uses PostgreSQL wire protocol (default port 8812).
      </div>

      {/* Host */}
      <div style={{ marginBottom: '12px' }}>
        <label style={{
          display: 'block',
          color: BLOOMBERG_ORANGE,
          fontSize: '10px',
          marginBottom: '4px',
          fontWeight: 'bold'
        }}>
          HOST
        </label>
        <input
          type="text"
          value={qdbHost}
          onChange={(e) => setQdbHost(e.target.value)}
          placeholder="localhost"
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

      {/* Port and Database */}
      <div style={{ display: 'flex', gap: '8px', marginBottom: '12px' }}>
        <div style={{ flex: 1 }}>
          <label style={{
            display: 'block',
            color: BLOOMBERG_ORANGE,
            fontSize: '10px',
            marginBottom: '4px',
            fontWeight: 'bold'
          }}>
            PORT
          </label>
          <input
            type="text"
            value={qdbPort}
            onChange={(e) => setQdbPort(e.target.value)}
            placeholder="8812"
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

        <div style={{ flex: 1 }}>
          <label style={{
            display: 'block',
            color: BLOOMBERG_ORANGE,
            fontSize: '10px',
            marginBottom: '4px',
            fontWeight: 'bold'
          }}>
            DATABASE
          </label>
          <input
            type="text"
            value={qdbDatabase}
            onChange={(e) => setQdbDatabase(e.target.value)}
            placeholder="qdb"
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
      </div>

      {/* Username */}
      <div style={{ marginBottom: '12px' }}>
        <label style={{
          display: 'block',
          color: BLOOMBERG_ORANGE,
          fontSize: '10px',
          marginBottom: '4px',
          fontWeight: 'bold'
        }}>
          USERNAME
        </label>
        <input
          type="text"
          value={qdbUser}
          onChange={(e) => setQdbUser(e.target.value)}
          placeholder="admin"
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

      {/* Password */}
      <div style={{ marginBottom: '12px' }}>
        <label style={{
          display: 'block',
          color: BLOOMBERG_ORANGE,
          fontSize: '10px',
          marginBottom: '4px',
          fontWeight: 'bold'
        }}>
          PASSWORD *
        </label>
        <input
          type="password"
          value={qdbPassword}
          onChange={(e) => setQdbPassword(e.target.value)}
          placeholder="Enter database password"
          style={{
            width: '100%',
            backgroundColor: BLOOMBERG_DARK_BG,
            border: `1px solid ${qdbPassword ? BLOOMBERG_ORANGE : BLOOMBERG_GRAY}`,
            color: BLOOMBERG_WHITE,
            padding: '6px',
            fontSize: '10px',
            fontFamily: 'Consolas, monospace'
          }}
        />
      </div>

      {/* SSL */}
      <div style={{ marginBottom: '12px' }}>
        <label style={{
          display: 'flex',
          alignItems: 'center',
          gap: '6px',
          color: BLOOMBERG_WHITE,
          fontSize: '10px',
          cursor: 'pointer'
        }}>
          <input
            type="checkbox"
            checked={qdbSsl}
            onChange={(e) => setQdbSsl(e.target.checked)}
            style={{ cursor: 'pointer' }}
          />
          Require SSL Connection
        </label>
      </div>

      {/* Connection String Preview */}
      <div style={{
        marginTop: '12px',
        padding: '8px',
        backgroundColor: BLOOMBERG_DARK_BG,
        border: `1px solid ${BLOOMBERG_GRAY}`,
        borderRadius: '2px'
      }}>
        <div style={{
          color: BLOOMBERG_GRAY,
          fontSize: '8px',
          marginBottom: '4px',
          fontWeight: 'bold'
        }}>
          CONNECTION STRING PREVIEW:
        </div>
        <div style={{
          color: BLOOMBERG_YELLOW,
          fontSize: '9px',
          fontFamily: 'Consolas, monospace',
          wordBreak: 'break-all'
        }}>
          {buildQuestDBUrl()}
        </div>
      </div>

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
          disabled={!qdbPassword.trim()}
          style={{
            backgroundColor: qdbPassword.trim() ? BLOOMBERG_ORANGE : BLOOMBERG_GRAY,
            border: 'none',
            color: 'black',
            padding: '6px 12px',
            fontSize: '10px',
            fontWeight: 'bold',
            cursor: qdbPassword.trim() ? 'pointer' : 'not-allowed'
          }}
        >
          INSTALL
        </button>
      </div>
    </div>
  );
};

export default QuestDBForm;
