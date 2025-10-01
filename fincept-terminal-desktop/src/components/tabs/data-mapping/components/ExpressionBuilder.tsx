// Expression Builder - Build & test parser expressions

import React, { useState, useEffect } from 'react';
import { Play, BookOpen, AlertCircle, CheckCircle } from 'lucide-react';
import { ParserEngine } from '../types';
import { getParser, getAllParsers } from '../engine/parsers';

interface ExpressionBuilderProps {
  initialEngine?: ParserEngine;
  initialExpression?: string;
  testData?: any;
  onExpressionChange?: (engine: ParserEngine, expression: string) => void;
}

export function ExpressionBuilder({
  initialEngine = ParserEngine.JSONPATH,
  initialExpression = '',
  testData,
  onExpressionChange,
}: ExpressionBuilderProps) {
  const [engine, setEngine] = useState<ParserEngine>(initialEngine);
  const [expression, setExpression] = useState(initialExpression);
  const [testDataInput, setTestDataInput] = useState(testData ? JSON.stringify(testData, null, 2) : '');
  const [result, setResult] = useState<any>(null);
  const [error, setError] = useState<string | null>(null);
  const [isRunning, setIsRunning] = useState(false);
  const [showHelp, setShowHelp] = useState(false);

  const parsers = getAllParsers();

  useEffect(() => {
    if (testData) {
      setTestDataInput(JSON.stringify(testData, null, 2));
    }
  }, [testData]);

  useEffect(() => {
    onExpressionChange?.(engine, expression);
  }, [engine, expression]);

  const handleTest = async () => {
    setIsRunning(true);
    setError(null);
    setResult(null);

    try {
      // Parse test data
      const data = JSON.parse(testDataInput);

      // Get parser and execute
      const parser = getParser(engine);
      const output = await parser.execute(data, expression);

      setResult(output);
    } catch (err) {
      setError(err instanceof Error ? err.message : String(err));
    } finally {
      setIsRunning(false);
    }
  };

  const handleEngineChange = (newEngine: ParserEngine) => {
    setEngine(newEngine);
    // Update expression example when engine changes
    const parser = getParser(newEngine);
    if (!expression) {
      setExpression(parser.getExample());
    }
  };

  const loadExample = () => {
    const parser = getParser(engine);
    setExpression(parser.getExample());
  };

  const currentParser = getParser(engine);

  return (
    <div className="space-y-4">
      {/* Engine Selector */}
      <div>
        <label className="text-xs font-bold text-gray-400 mb-2 block uppercase">Parser Engine</label>
        <div className="flex flex-wrap gap-2">
          {parsers.map(({ engine: eng, parser }) => (
            <button
              key={eng}
              onClick={() => handleEngineChange(eng)}
              className={`px-3 py-2 text-xs rounded font-bold transition-all ${
                engine === eng
                  ? 'bg-orange-600 text-white shadow-lg'
                  : 'bg-zinc-800 text-gray-400 hover:bg-zinc-700 hover:text-white'
              }`}
            >
              {parser.name}
            </button>
          ))}
        </div>
        <p className="text-xs text-gray-500 mt-2">{currentParser.description}</p>
      </div>

      {/* Expression Editor */}
      <div>
        <div className="flex items-center justify-between mb-2">
          <label className="text-xs font-bold text-gray-400 uppercase">Expression</label>
          <div className="flex gap-2">
            <button
              onClick={loadExample}
              className="text-xs text-orange-500 hover:text-orange-400 font-bold"
            >
              Load Example
            </button>
            <button
              onClick={() => setShowHelp(!showHelp)}
              className="text-xs text-purple-500 hover:text-purple-400 font-bold flex items-center gap-1"
            >
              <BookOpen size={12} />
              {showHelp ? 'Hide' : 'Show'} Syntax
            </button>
          </div>
        </div>

        <textarea
          value={expression}
          onChange={(e) => setExpression(e.target.value)}
          placeholder={currentParser.getExample()}
          rows={6}
          className="w-full bg-zinc-900 border border-zinc-700 text-gray-300 p-3 text-xs font-mono rounded focus:outline-none focus:border-orange-500 resize-none"
        />

        {/* Syntax Help */}
        {showHelp && (
          <div className="mt-2 bg-zinc-900 border border-zinc-700 rounded p-3 max-h-64 overflow-auto">
            <pre className="text-xs text-gray-400 whitespace-pre-wrap font-mono">
              {currentParser.getSyntaxHelp()}
            </pre>
          </div>
        )}
      </div>

      {/* Test Data Input */}
      <div>
        <label className="text-xs font-bold text-gray-400 mb-2 block uppercase">Test Data (JSON)</label>
        <textarea
          value={testDataInput}
          onChange={(e) => setTestDataInput(e.target.value)}
          placeholder='{"data": {"value": 123}}'
          rows={6}
          className="w-full bg-zinc-900 border border-zinc-700 text-gray-300 p-3 text-xs font-mono rounded focus:outline-none focus:border-orange-500 resize-none"
        />
      </div>

      {/* Test Button */}
      <button
        onClick={handleTest}
        disabled={isRunning || !expression || !testDataInput}
        className={`w-full py-3 rounded font-bold text-xs flex items-center justify-center gap-2 transition-all ${
          isRunning || !expression || !testDataInput
            ? 'bg-zinc-800 text-gray-600 cursor-not-allowed'
            : 'bg-green-600 hover:bg-green-700 text-white shadow-lg'
        }`}
      >
        <Play size={14} />
        {isRunning ? 'TESTING...' : 'TEST EXPRESSION'}
      </button>

      {/* Result Display */}
      {result !== null && (
        <div className="bg-zinc-900 border border-green-700 rounded-lg overflow-hidden">
          <div className="bg-green-900/20 px-3 py-2 border-b border-green-700 flex items-center gap-2">
            <CheckCircle size={14} className="text-green-500" />
            <span className="text-xs font-bold text-green-500">SUCCESS</span>
          </div>
          <div className="p-3">
            <pre className="text-xs text-green-400 overflow-auto max-h-64 font-mono">
              {JSON.stringify(result, null, 2)}
            </pre>
          </div>
        </div>
      )}

      {/* Error Display */}
      {error && (
        <div className="bg-zinc-900 border border-red-700 rounded-lg overflow-hidden">
          <div className="bg-red-900/20 px-3 py-2 border-b border-red-700 flex items-center gap-2">
            <AlertCircle size={14} className="text-red-500" />
            <span className="text-xs font-bold text-red-500">ERROR</span>
          </div>
          <div className="p-3">
            <pre className="text-xs text-red-400 whitespace-pre-wrap font-mono">{error}</pre>
          </div>
        </div>
      )}
    </div>
  );
}
