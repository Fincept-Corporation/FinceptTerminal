// Connection Selector - Select from existing data sources

import React, { useEffect, useState } from 'react';
import { Database, CheckCircle, AlertCircle, RefreshCw } from 'lucide-react';
import { DataSourceConnection } from '../../data-sources/types';
import { connectionService } from '../services/ConnectionService';

interface ConnectionSelectorProps {
  selectedConnectionId?: string;
  onSelect: (connection: DataSourceConnection) => void;
}

export function ConnectionSelector({ selectedConnectionId, onSelect }: ConnectionSelectorProps) {
  const [connections, setConnections] = useState<DataSourceConnection[]>([]);
  const [loading, setLoading] = useState(true);
  const [filter, setFilter] = useState<'all' | 'connected'>('connected');

  useEffect(() => {
    loadConnections();
  }, [filter]);

  const loadConnections = async () => {
    setLoading(true);
    try {
      const allConnections = await connectionService.loadConnections();
      const filtered =
        filter === 'connected'
          ? allConnections.filter((c) => c.status === 'connected')
          : allConnections;
      setConnections(filtered);
    } catch (error) {
      console.error('Failed to load connections:', error);
    } finally {
      setLoading(false);
    }
  };

  const getStatusIcon = (status: DataSourceConnection['status']) => {
    switch (status) {
      case 'connected':
        return <CheckCircle size={14} className="text-green-500" />;
      case 'error':
        return <AlertCircle size={14} className="text-red-500" />;
      case 'testing':
        return <RefreshCw size={14} className="text-yellow-500 animate-spin" />;
      default:
        return <AlertCircle size={14} className="text-gray-500" />;
    }
  };

  const getStatusColor = (status: DataSourceConnection['status']) => {
    switch (status) {
      case 'connected':
        return 'text-green-500';
      case 'error':
        return 'text-red-500';
      case 'testing':
        return 'text-yellow-500';
      default:
        return 'text-gray-500';
    }
  };

  if (loading) {
    return (
      <div className="flex items-center justify-center p-8">
        <RefreshCw size={24} className="text-orange-500 animate-spin" />
      </div>
    );
  }

  if (connections.length === 0) {
    return (
      <div className="bg-zinc-900 rounded-lg border border-zinc-800 p-8 text-center">
        <Database size={48} className="mx-auto mb-4 text-gray-700" />
        <h3 className="text-sm font-bold text-gray-400 mb-2">No Connections Found</h3>
        <p className="text-xs text-gray-500 mb-4">
          You need to configure data source connections first.
        </p>
        <button
          onClick={() => {
            // Navigate to data sources tab
            // This would be handled by parent component
          }}
          className="bg-orange-600 hover:bg-orange-700 text-white px-4 py-2 rounded text-xs font-bold"
        >
          GO TO DATA SOURCES
        </button>
      </div>
    );
  }

  return (
    <div className="space-y-4">
      {/* Header */}
      <div className="flex items-center justify-between">
        <div>
          <h3 className="text-xs font-bold text-orange-500 uppercase">Select Data Source Connection</h3>
          <p className="text-xs text-gray-500 mt-1">
            Choose a configured connection to fetch data from
          </p>
        </div>

        {/* Filter */}
        <div className="flex gap-2">
          <button
            onClick={() => setFilter('all')}
            className={`px-3 py-1 text-xs rounded font-bold ${
              filter === 'all'
                ? 'bg-orange-600 text-white'
                : 'bg-zinc-800 text-gray-400 hover:bg-zinc-700'
            }`}
          >
            All
          </button>
          <button
            onClick={() => setFilter('connected')}
            className={`px-3 py-1 text-xs rounded font-bold ${
              filter === 'connected'
                ? 'bg-orange-600 text-white'
                : 'bg-zinc-800 text-gray-400 hover:bg-zinc-700'
            }`}
          >
            Connected Only
          </button>
        </div>
      </div>

      {/* Connections List */}
      <div className="space-y-3">
        {connections.map((connection) => {
          const isSelected = selectedConnectionId === connection.id;

          return (
            <button
              key={connection.id}
              onClick={() => onSelect(connection)}
              disabled={connection.status !== 'connected'}
              className={`w-full text-left p-4 rounded-lg border transition-all ${
                isSelected
                  ? 'border-orange-500 bg-orange-900/20 shadow-lg'
                  : connection.status === 'connected'
                  ? 'border-zinc-700 bg-zinc-900 hover:border-zinc-600'
                  : 'border-zinc-800 bg-zinc-900/50 opacity-50 cursor-not-allowed'
              }`}
            >
              <div className="flex items-start justify-between mb-2">
                <div className="flex-1">
                  <h4 className="text-sm font-bold text-white mb-1">{connection.name}</h4>
                  <div className="flex items-center gap-2">
                    <span className="text-xs px-2 py-1 bg-zinc-800 text-gray-400 rounded font-mono">
                      {connection.type}
                    </span>
                    <span className="text-xs text-gray-500">•</span>
                    <div className={`flex items-center gap-1 text-xs ${getStatusColor(connection.status)}`}>
                      {getStatusIcon(connection.status)}
                      <span className="uppercase font-bold">{connection.status}</span>
                    </div>
                  </div>
                </div>

                {isSelected && (
                  <span className="text-xs bg-orange-600 text-white px-2 py-1 rounded font-bold">
                    SELECTED
                  </span>
                )}
              </div>

              {/* Connection Details */}
              <div className="mt-3 pt-3 border-t border-zinc-800">
                <div className="grid grid-cols-2 gap-2 text-xs">
                  <div>
                    <div className="text-gray-500">Category</div>
                    <div className="text-white capitalize">{connection.category}</div>
                  </div>
                  <div>
                    <div className="text-gray-500">Last Updated</div>
                    <div className="text-white">
                      {new Date(connection.updatedAt).toLocaleDateString()}
                    </div>
                  </div>
                </div>
              </div>

              {connection.errorMessage && (
                <div className="mt-2 text-xs text-red-400 bg-red-900/20 p-2 rounded">
                  {connection.errorMessage}
                </div>
              )}
            </button>
          );
        })}
      </div>

      {/* Refresh Button */}
      <button
        onClick={loadConnections}
        className="w-full py-2 bg-zinc-800 hover:bg-zinc-700 text-gray-400 hover:text-white rounded text-xs font-bold flex items-center justify-center gap-2"
      >
        <RefreshCw size={12} />
        REFRESH CONNECTIONS
      </button>
    </div>
  );
}
