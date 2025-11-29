// PostgreSQL Configuration Form
// Collects connection details and builds a PostgreSQL connection string

import React, { useState } from 'react';

interface PostgresFormProps {
  onSubmit: (config: { args: string[]; env: Record<string, string> }) => void;
  onCancel: () => void;
}

const PostgresForm: React.FC<PostgresFormProps> = ({ onSubmit, onCancel }) => {
  const [pgHost, setPgHost] = useState('localhost');
  const [pgPort, setPgPort] = useState('5432');
  const [pgUser, setPgUser] = useState('postgres');
  const [pgPassword, setPgPassword] = useState('');
  const [pgDatabase, setPgDatabase] = useState('postgres');
  const [pgSsl, setPgSsl] = useState(false);

  const BLOOMBERG_ORANGE = '#FFA500';
  const BLOOMBERG_WHITE = '#FFFFFF';
  const BLOOMBERG_GRAY = '#787878';
  const BLOOMBERG_DARK_BG = '#000000';
  const BLOOMBERG_YELLOW = '#FFFF00';

  const buildPostgresUrl = () => {
    const encodedPassword = encodeURIComponent(pgPassword);
    const sslParam = pgSsl ? '?sslmode=require' : '';
    return `postgresql://${pgUser}:${encodedPassword}@${pgHost}:${pgPort}/${pgDatabase}${sslParam}`;
  };

  const handleSubmit = () => {
    const dbUrl = buildPostgresUrl();
    onSubmit({
      args: [],  // Connection string passed via env var, not CLI arg
      env: { DATABASE_URL: dbUrl }
    });
  };

  return (
    <div>
      <div style={{
        color: BLOOMBERG_WHITE,
        fontSize: '10px',
        marginBottom: '12px'
      }}>
        Configure your PostgreSQL database connection. The connection string will be passed securely to the MCP server.
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
          value={pgHost}
          onChange={(e) => setPgHost(e.target.value)}
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
            value={pgPort}
            onChange={(e) => setPgPort(e.target.value)}
            placeholder="5432"
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
            value={pgDatabase}
            onChange={(e) => setPgDatabase(e.target.value)}
            placeholder="postgres"
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
          value={pgUser}
          onChange={(e) => setPgUser(e.target.value)}
          placeholder="postgres"
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
          value={pgPassword}
          onChange={(e) => setPgPassword(e.target.value)}
          placeholder="Enter database password"
          style={{
            width: '100%',
            backgroundColor: BLOOMBERG_DARK_BG,
            border: `1px solid ${pgPassword ? BLOOMBERG_ORANGE : BLOOMBERG_GRAY}`,
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
            checked={pgSsl}
            onChange={(e) => setPgSsl(e.target.checked)}
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
          {buildPostgresUrl()}
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
          disabled={!pgPassword.trim()}
          style={{
            backgroundColor: pgPassword.trim() ? BLOOMBERG_ORANGE : BLOOMBERG_GRAY,
            border: 'none',
            color: 'black',
            padding: '6px 12px',
            fontSize: '10px',
            fontWeight: 'bold',
            cursor: pgPassword.trim() ? 'pointer' : 'not-allowed'
          }}
        >
          INSTALL
        </button>
      </div>
    </div>
  );
};

export default PostgresForm;
