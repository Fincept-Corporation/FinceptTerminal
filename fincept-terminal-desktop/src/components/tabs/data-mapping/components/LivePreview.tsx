// Live Preview - Real-time transformation preview

import React from 'react';
import { CheckCircle, AlertCircle, Info } from 'lucide-react';
import { MappingExecutionResult } from '../types';

interface LivePreviewProps {
  result?: MappingExecutionResult;
  isLoading?: boolean;
}

export function LivePreview({ result, isLoading }: LivePreviewProps) {
  if (isLoading) {
    return (
      <div className="bg-zinc-900 rounded-lg border border-zinc-800 p-8 flex items-center justify-center">
        <div className="text-center">
          <div className="animate-spin rounded-full h-12 w-12 border-b-2 border-orange-500 mx-auto mb-4"></div>
          <p className="text-gray-400 text-sm">Transforming data...</p>
        </div>
      </div>
    );
  }

  if (!result) {
    return (
      <div className="bg-zinc-900 rounded-lg border border-zinc-800 p-8 flex items-center justify-center">
        <div className="text-center text-gray-500">
          <Info size={48} className="mx-auto mb-4 opacity-30" />
          <p className="text-sm">Configure mapping and test to see preview</p>
        </div>
      </div>
    );
  }

  return (
    <div className="space-y-4">
      {/* Status Header */}
      <div
        className={`rounded-lg border p-3 ${
          result.success
            ? 'bg-green-900/20 border-green-700'
            : 'bg-red-900/20 border-red-700'
        }`}
      >
        <div className="flex items-center gap-2 mb-2">
          {result.success ? (
            <>
              <CheckCircle size={16} className="text-green-500" />
              <span className="text-xs font-bold text-green-500">TRANSFORMATION SUCCESSFUL</span>
            </>
          ) : (
            <>
              <AlertCircle size={16} className="text-red-500" />
              <span className="text-xs font-bold text-red-500">TRANSFORMATION FAILED</span>
            </>
          )}
        </div>

        {/* Metadata */}
        <div className="grid grid-cols-3 gap-4 text-xs">
          <div>
            <div className="text-gray-500">Duration</div>
            <div className="text-white font-bold">{result.metadata.duration}ms</div>
          </div>
          <div>
            <div className="text-gray-500">Records Processed</div>
            <div className="text-white font-bold">{result.metadata.recordsProcessed}</div>
          </div>
          <div>
            <div className="text-gray-500">Records Returned</div>
            <div className="text-white font-bold">{result.metadata.recordsReturned}</div>
          </div>
        </div>
      </div>

      {/* Raw Data (if available) */}
      {result.rawData && (
        <div className="bg-zinc-900 rounded-lg border border-zinc-800 overflow-hidden">
          <div className="bg-zinc-800 px-3 py-2 border-b border-zinc-700">
            <span className="text-xs font-bold text-gray-400">RAW INPUT DATA</span>
          </div>
          <div className="p-3 max-h-64 overflow-auto">
            <pre className="text-xs text-gray-400 font-mono">
              {JSON.stringify(result.rawData, null, 2)}
            </pre>
          </div>
        </div>
      )}

      {/* Transformed Data */}
      {result.success && result.data && (
        <div className="bg-zinc-900 rounded-lg border border-green-700 overflow-hidden">
          <div className="bg-green-900/20 px-3 py-2 border-b border-green-700">
            <span className="text-xs font-bold text-green-500">TRANSFORMED OUTPUT</span>
          </div>
          <div className="p-3 max-h-96 overflow-auto">
            <pre className="text-xs text-green-400 font-mono">
              {JSON.stringify(result.data, null, 2)}
            </pre>
          </div>
        </div>
      )}

      {/* Errors */}
      {result.errors && result.errors.length > 0 && (
        <div className="bg-zinc-900 rounded-lg border border-red-700 overflow-hidden">
          <div className="bg-red-900/20 px-3 py-2 border-b border-red-700">
            <span className="text-xs font-bold text-red-500">ERRORS</span>
          </div>
          <div className="p-3">
            {result.errors.map((error, idx) => (
              <div key={idx} className="text-xs text-red-400 mb-2 font-mono">
                • {error}
              </div>
            ))}
          </div>
        </div>
      )}

      {/* Warnings */}
      {result.warnings && result.warnings.length > 0 && (
        <div className="bg-zinc-900 rounded-lg border border-yellow-700 overflow-hidden">
          <div className="bg-yellow-900/20 px-3 py-2 border-b border-yellow-700">
            <span className="text-xs font-bold text-yellow-500">WARNINGS</span>
          </div>
          <div className="p-3">
            {result.warnings.map((warning, idx) => (
              <div key={idx} className="text-xs text-yellow-400 mb-2 font-mono">
                • {warning}
              </div>
            ))}
          </div>
        </div>
      )}

      {/* Validation Results */}
      {result.metadata.validationResults && (
        <div
          className={`bg-zinc-900 rounded-lg border overflow-hidden ${
            result.metadata.validationResults.valid ? 'border-green-700' : 'border-yellow-700'
          }`}
        >
          <div
            className={`px-3 py-2 border-b ${
              result.metadata.validationResults.valid
                ? 'bg-green-900/20 border-green-700'
                : 'bg-yellow-900/20 border-yellow-700'
            }`}
          >
            <span
              className={`text-xs font-bold ${
                result.metadata.validationResults.valid ? 'text-green-500' : 'text-yellow-500'
              }`}
            >
              VALIDATION {result.metadata.validationResults.valid ? 'PASSED' : 'WARNINGS'}
            </span>
          </div>
          {!result.metadata.validationResults.valid && (
            <div className="p-3">
              {result.metadata.validationResults.errors.map((err, idx) => (
                <div key={idx} className="text-xs text-yellow-400 mb-2">
                  <span className="font-bold">{err.field}:</span> {err.message}
                </div>
              ))}
            </div>
          )}
        </div>
      )}
    </div>
  );
}
