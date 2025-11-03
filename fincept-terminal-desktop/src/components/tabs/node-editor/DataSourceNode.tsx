import React from 'react';

interface DataSourceNodeProps {
  data: {
    label: string;
    selectedConnectionId?: string;
    query: string;
    status: string;
    result?: any;
    error?: string;
    onConnectionChange: (connectionId: string) => void;
    onQueryChange: (query: string) => void;
    onExecute: () => void;
  };
}

const DataSourceNode: React.FC<DataSourceNodeProps> = ({ data }) => {
  return (
    <div style={{
      background: '#1a1a1a',
      border: '2px solid #3b82f6',
      borderRadius: '6px',
      padding: '12px',
      minWidth: '200px'
    }}>
      <div style={{ color: '#3b82f6', fontWeight: 'bold', marginBottom: '8px' }}>
        {data.label}
      </div>
      <div style={{ fontSize: '10px', color: '#a3a3a3' }}>
        Status: {data.status}
      </div>
    </div>
  );
};

export default DataSourceNode;
