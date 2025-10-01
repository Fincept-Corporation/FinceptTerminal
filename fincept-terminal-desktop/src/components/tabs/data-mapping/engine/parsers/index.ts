// Parser Registry

import { ParserEngine } from '../../types';
import { BaseParser } from './BaseParser';
import { JSONPathParser } from './JSONPathParser';
import { JSONataParser } from './JSONataParser';
import { JMESPathParser } from './JMESPathParser';
import { DirectParser } from './DirectParser';
import { JavaScriptParser } from './JavaScriptParser';

// Parser instances
const parsers: Record<ParserEngine, BaseParser> = {
  [ParserEngine.JSONPATH]: new JSONPathParser(),
  [ParserEngine.JSONATA]: new JSONataParser(),
  [ParserEngine.JMESPATH]: new JMESPathParser(),
  [ParserEngine.DIRECT]: new DirectParser(),
  [ParserEngine.CUSTOM_JS]: new JavaScriptParser(),
  [ParserEngine.REGEX]: new DirectParser(), // TODO: Implement RegexParser
};

/**
 * Get parser instance by engine type
 */
export function getParser(engine: ParserEngine): BaseParser {
  const parser = parsers[engine];
  if (!parser) {
    throw new Error(`Parser not found for engine: ${engine}`);
  }
  return parser;
}

/**
 * Get all available parsers
 */
export function getAllParsers(): Array<{ engine: ParserEngine; parser: BaseParser }> {
  return Object.entries(parsers).map(([engine, parser]) => ({
    engine: engine as ParserEngine,
    parser,
  }));
}

/**
 * Get parser info
 */
export function getParserInfo(engine: ParserEngine): { name: string; description: string; example: string } {
  const parser = getParser(engine);
  return {
    name: parser.name,
    description: parser.description,
    example: parser.getExample(),
  };
}

/**
 * Auto-detect best parser for expression
 */
export function detectParser(expression: string): ParserEngine {
  // JSONPath - starts with $ or @
  if (expression.startsWith('$') || expression.startsWith('@')) {
    return ParserEngine.JSONPATH;
  }

  // JSONata - contains object construction {}
  if (expression.includes('{') && expression.includes('}')) {
    return ParserEngine.JSONATA;
  }

  // JMESPath - contains [?...] or [*] or &
  if (expression.includes('[?') || expression.includes('[*]') || expression.includes('&')) {
    return ParserEngine.JMESPATH;
  }

  // JavaScript - contains arrow function or map/filter/reduce
  if (expression.includes('=>') || expression.match(/\.(map|filter|reduce|find)\(/)) {
    return ParserEngine.CUSTOM_JS;
  }

  // Default to Direct for simple paths
  return ParserEngine.DIRECT;
}

/**
 * Execute expression with auto-detected parser
 */
export async function autoExecute(data: any, expression: string): Promise<any> {
  const engine = detectParser(expression);
  const parser = getParser(engine);
  return await parser.execute(data, expression);
}

// Re-export parsers
export {
  BaseParser,
  JSONPathParser,
  JSONataParser,
  JMESPathParser,
  DirectParser,
  JavaScriptParser,
};
