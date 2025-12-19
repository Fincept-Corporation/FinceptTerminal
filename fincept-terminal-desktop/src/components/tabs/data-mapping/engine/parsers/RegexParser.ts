// Regex Parser - Pattern-based text extraction

import { BaseParser } from './BaseParser';

/**
 * RegexParser - Extract data using regular expressions
 * 
 * Expression format: /pattern/flags[:groupIndex]
 * 
 * Examples:
 *   /\d+/g              - Find all numbers
 *   /price: (\d+)/i:1   - Extract first capture group (case insensitive)
 *   /[A-Z]{2,5}/g       - Find all uppercase sequences (2-5 chars)
 *   /(?<symbol>\w+)/    - Named capture group
 */
export class RegexParser extends BaseParser {
    name = 'Regex';
    description = 'Pattern-based text extraction using regular expressions';

    async execute(data: any, expression: string): Promise<any> {
        try {
            const parsed = this.parseExpression(expression);
            const text = this.toSearchableString(data);

            const regex = new RegExp(parsed.pattern, parsed.flags);

            // Check if global flag is set
            if (parsed.flags.includes('g')) {
                // Return all matches
                const matches = [...text.matchAll(regex)];

                if (matches.length === 0) {
                    return null;
                }

                // If group index specified, extract that group from each match
                if (parsed.groupIndex !== undefined) {
                    return matches.map(m => m[parsed.groupIndex!] ?? null).filter(v => v !== null);
                }

                // If there are capture groups, return them; otherwise return full matches
                if (matches[0].length > 1) {
                    return matches.map(m => m.slice(1)); // Return all capture groups
                }

                return matches.map(m => m[0]); // Return full matches
            } else {
                // Single match
                const match = text.match(regex);

                if (!match) {
                    return null;
                }

                // If group index specified, return that group
                if (parsed.groupIndex !== undefined) {
                    return match[parsed.groupIndex] ?? null;
                }

                // If there are capture groups, return them
                if (match.length > 1) {
                    return match.slice(1); // Return all capture groups
                }

                return match[0]; // Return full match
            }
        } catch (error) {
            throw new Error(`Regex execution failed: ${error instanceof Error ? error.message : String(error)}`);
        }
    }

    validate(expression: string): { valid: boolean; error?: string } {
        if (!expression || expression.trim().length === 0) {
            return { valid: false, error: 'Expression cannot be empty' };
        }

        try {
            const parsed = this.parseExpression(expression);

            // Try to create the RegExp to validate syntax
            new RegExp(parsed.pattern, parsed.flags);

            // Validate group index if specified
            if (parsed.groupIndex !== undefined && parsed.groupIndex < 0) {
                return { valid: false, error: 'Group index must be non-negative' };
            }

            return { valid: true };
        } catch (error: any) {
            return {
                valid: false,
                error: `Invalid regex: ${error.message || String(error)}`
            };
        }
    }

    getExample(): string {
        return '/price:\\s*(\\d+\\.?\\d*)/i:1';
    }

    getSyntaxHelp(): string {
        return `
Regex Parser Syntax:

Expression Format:
  /pattern/flags[:groupIndex]

Basic Examples:
  /\\d+/g                    - Find all numbers (global)
  /\\d+/                     - Find first number
  /[A-Z]+/gi                 - Find uppercase (case insensitive, global)

Capture Groups:
  /price: (\\d+)/i:1         - Extract first capture group
  /price: (\\d+)/            - Returns array of capture groups
  /(\\w+)@(\\w+)/            - Multiple groups [username, domain]

Named Groups (ES2018+):
  /(?<amount>\\d+)/          - Named capture group
  
Common Patterns:
  /\\$[\\d,]+\\.?\\d*/g       - Currency values
  /\\d{4}-\\d{2}-\\d{2}/     - ISO dates (YYYY-MM-DD)
  /[A-Z]{2,5}/g              - Stock ticker symbols
  /"(\\w+)":\\s*([^,}]+)/g   - JSON key-value extraction

Flags:
  g - Global (find all matches)
  i - Case insensitive
  m - Multiline (^ and $ match line boundaries)
  s - Dot matches newlines
  u - Unicode support

Group Index:
  Append :N to extract Nth captured group (0 = full match)
  /pattern/:0   - Full match
  /pattern/:1   - First capture group
  /pattern/:2   - Second capture group

Note: When input is not a string, it will be JSON.stringify'd
before matching. This allows regex to work on object data.
    `.trim();
    }

    /**
     * Parse the expression format: /pattern/flags[:groupIndex]
     */
    private parseExpression(expression: string): { pattern: string; flags: string; groupIndex?: number } {
        const trimmed = expression.trim();

        // Match /pattern/flags or /pattern/flags:groupIndex
        const match = trimmed.match(/^\/(.+)\/([gimsuvy]*)(?::(\d+))?$/);

        if (!match) {
            // Try as raw pattern (no delimiters)
            return { pattern: trimmed, flags: '' };
        }

        const [, pattern, flags, groupStr] = match;

        return {
            pattern,
            flags: flags || '',
            groupIndex: groupStr !== undefined ? parseInt(groupStr, 10) : undefined
        };
    }

    /**
     * Convert data to searchable string
     */
    private toSearchableString(data: any): string {
        if (typeof data === 'string') {
            return data;
        }

        if (data === null || data === undefined) {
            return '';
        }

        // For objects/arrays, stringify for searching
        return JSON.stringify(data);
    }

    /**
     * Test if pattern matches any part of data
     */
    matches(data: any, expression: string): boolean {
        try {
            const parsed = this.parseExpression(expression);
            const text = this.toSearchableString(data);
            const regex = new RegExp(parsed.pattern, parsed.flags.replace('g', ''));
            return regex.test(text);
        } catch {
            return false;
        }
    }

    /**
     * Replace matches with replacement string
     */
    replace(data: any, expression: string, replacement: string): string {
        try {
            const parsed = this.parseExpression(expression);
            const text = this.toSearchableString(data);
            const regex = new RegExp(parsed.pattern, parsed.flags);
            return text.replace(regex, replacement);
        } catch {
            return this.toSearchableString(data);
        }
    }

    /**
     * Split text by pattern
     */
    split(data: any, expression: string): string[] {
        try {
            const parsed = this.parseExpression(expression);
            const text = this.toSearchableString(data);
            const regex = new RegExp(parsed.pattern, parsed.flags);
            return text.split(regex);
        } catch {
            return [this.toSearchableString(data)];
        }
    }
}
