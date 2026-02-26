import React, { useMemo } from 'react';
import { marked } from 'marked';
import DOMPurify from 'dompurify';
import katex from 'katex';
import 'katex/dist/katex.min.css';

interface MarkdownRendererProps {
  content: string;
  style?: React.CSSProperties;
}

// Configure marked with GFM enabled
marked.setOptions({
  gfm: true,
  breaks: true,
});

// Render LaTeX formulas using KaTeX
const renderLatex = (text: string): string => {
  // Block formulas: \[ ... \] or $$ ... $$
  text = text.replace(/\\\[([\s\S]*?)\\\]|\$\$([\s\S]*?)\$\$/g, (match, b1, b2) => {
    const formula = b1 || b2;
    try {
      return `<div class="katex-block">${katex.renderToString(formula.trim(), { displayMode: true, throwOnError: false })}</div>`;
    } catch (e) {
      console.warn('[KaTeX] Block formula error:', e);
      return match;
    }
  });

  // Inline formulas: \( ... \) or $ ... $ (but not $$ which is block)
  text = text.replace(/\\\((.*?)\\\)|\$([^$\n]+?)\$/g, (match, i1, i2) => {
    const formula = i1 || i2;
    try {
      return katex.renderToString(formula.trim(), { displayMode: false, throwOnError: false });
    } catch (e) {
      console.warn('[KaTeX] Inline formula error:', e);
      return match;
    }
  });

  return text;
};

const MARKDOWN_CSS = `
.fincept-md h1 {
  color: #FF8800;
  font-size: 14px;
  font-weight: 700;
  margin: 12px 0 8px 0;
  padding-bottom: 6px;
  border-bottom: 2px solid #FF8800;
  letter-spacing: 0.5px;
  text-transform: uppercase;
}
.fincept-md h2 {
  color: #FFD700;
  font-size: 12px;
  font-weight: 700;
  margin: 10px 0 6px 0;
  padding-bottom: 4px;
  border-bottom: 1px solid #2A2A2A;
  letter-spacing: 0.5px;
}
.fincept-md h3 {
  color: #00E5FF;
  font-size: 11px;
  font-weight: 700;
  margin: 8px 0 4px 0;
  letter-spacing: 0.5px;
}
.fincept-md h4 {
  color: #FFFFFF;
  font-size: 10px;
  font-weight: 700;
  margin: 6px 0 3px 0;
  letter-spacing: 0.5px;
}
.fincept-md h5, .fincept-md h6 {
  color: #787878;
  font-size: 10px;
  font-weight: 700;
  margin: 4px 0 2px 0;
  letter-spacing: 0.5px;
}
.fincept-md p {
  margin: 0 0 8px 0;
  color: #FFFFFF;
  font-size: 11px;
  line-height: 1.6;
  font-family: "IBM Plex Mono", Consolas, monospace;
}
.fincept-md ul {
  margin: 0 0 8px 0;
  padding-left: 16px;
  list-style-type: none;
  color: #FFFFFF;
}
.fincept-md ul li:before {
  content: "▸";
  color: #FF8800;
  font-weight: bold;
  display: inline-block;
  width: 12px;
  margin-left: -12px;
}
.fincept-md ol {
  margin: 0 0 8px 0;
  padding-left: 16px;
  list-style-type: decimal;
  color: #FFFFFF;
}
.fincept-md li {
  margin-bottom: 4px;
  color: #FFFFFF;
  font-size: 10px;
  line-height: 1.5;
  font-family: "IBM Plex Mono", Consolas, monospace;
}
.fincept-md li strong {
  color: #00E5FF;
  font-weight: 700;
}
.fincept-md pre {
  background-color: #0F0F0F;
  padding: 0;
  margin: 0 0 8px 0;
  overflow-x: auto;
  border: 1px solid #2A2A2A;
  border-radius: 2px;
}
.fincept-md pre code {
  display: block;
  background-color: #0F0F0F;
  color: #00D66F;
  padding: 8px;
  border-radius: 2px;
  font-size: 10px;
  font-family: "IBM Plex Mono", Consolas, monospace;
  overflow-x: auto;
  white-space: pre-wrap;
  line-height: 1.4;
}
.fincept-md code {
  background-color: #0F0F0F;
  color: #00E5FF;
  padding: 2px 4px;
  border-radius: 2px;
  font-size: 10px;
  font-family: "IBM Plex Mono", Consolas, monospace;
  border: 1px solid #2A2A2A;
}
.fincept-md blockquote {
  border-left: 2px solid #FF8800;
  margin: 0 0 8px 0;
  padding: 6px 0 6px 12px;
  background-color: rgba(255, 136, 0, 0.05);
  color: #FFFFFF;
  font-size: 10px;
  font-style: italic;
  border-radius: 2px;
}
.fincept-md a {
  color: #FF8800;
  text-decoration: underline;
  cursor: pointer;
  font-size: 10px;
}
.fincept-md a:hover {
  color: #FFD700;
}
.fincept-md table {
  border-collapse: collapse;
  width: 100%;
  margin: 8px 0;
  border: 1px solid #2A2A2A;
  font-size: 10px;
  background-color: #0F0F0F;
}
.fincept-md thead {
  background-color: #1A1A1A;
}
.fincept-md tr {
  border-bottom: 1px solid #2A2A2A;
}
.fincept-md th {
  padding: 6px 8px;
  text-align: left;
  color: #FF8800;
  font-weight: 700;
  border: 1px solid #2A2A2A;
  letter-spacing: 0.5px;
  font-size: 9px;
  text-transform: uppercase;
}
.fincept-md td {
  padding: 6px 8px;
  color: #FFFFFF;
  border: 1px solid #2A2A2A;
  font-family: "IBM Plex Mono", Consolas, monospace;
}
.fincept-md hr {
  border: none;
  border-top: 1px solid #2A2A2A;
  margin: 12px 0;
}
.fincept-md strong {
  color: #00E5FF;
  font-weight: 700;
}
.fincept-md em {
  color: #787878;
  font-style: italic;
}
.fincept-md del {
  color: #4A4A4A;
  text-decoration: line-through;
}
.fincept-md input[type="checkbox"] {
  margin-right: 4px;
}
/* KaTeX Formula Styles */
.fincept-md .katex-block {
  display: block;
  text-align: center;
  margin: 12px 0;
  padding: 12px;
  background-color: #0F0F0F;
  border: 1px solid #2A2A2A;
  border-left: 2px solid #FF8800;
  border-radius: 2px;
  overflow-x: auto;
}
.fincept-md .katex {
  color: #00E5FF;
  font-size: 1.1em;
}
.fincept-md .katex-display {
  margin: 0;
  padding: 0;
}
.fincept-md .katex-display > .katex {
  color: #00E5FF;
}
/* Price/Data Cards */
.price-card {
  display: inline-flex;
  flex-direction: column;
  background-color: #0F0F0F;
  border: 1px solid #2A2A2A;
  border-left: 2px solid #FF8800;
  border-radius: 2px;
  padding: 6px 8px;
  margin: 4px 4px 4px 0;
  min-width: 120px;
  gap: 2px;
}
.price-card-positive { border-left-color: #00D66F; }
.price-card-negative { border-left-color: #FF3B3B; }
.price-card-label {
  font-size: 8px;
  color: #787878;
  font-weight: 700;
  letter-spacing: 0.5px;
  text-transform: uppercase;
}
.price-card-value {
  font-size: 12px;
  color: #00E5FF;
  font-weight: 700;
  font-family: "IBM Plex Mono", Consolas, monospace;
}
.price-card-change {
  font-size: 9px;
  font-weight: 700;
}
.price-card-change.positive { color: #00D66F; }
.price-card-change.negative { color: #FF3B3B; }
`;

// Parse content for price data patterns and convert to cards
const parseDataCards = (text: string): string => {
  // Pattern: Stock (SYMBOL) - +X.XX% Price: $XXX.XX
  const stockPattern = /([A-Z][a-zA-Z\s&]+)\s*\(([A-Z:]+)\)\s*-\s*([+-]?\d+\.?\d*%)\s*(?:Current|Price):\s*\$?([\d,]+\.?\d*)/g;

  let parsed = text.replace(stockPattern, (match, name, symbol, change, price) => {
    const isPositive = change.startsWith('+');
    const cardClass = isPositive ? 'price-card-positive' : 'price-card-negative';
    const changeClass = isPositive ? 'positive' : 'negative';

    return `<div class="price-card ${cardClass}">
      <div class="price-card-label">${symbol}</div>
      <div class="price-card-value">$${price}</div>
      <div class="price-card-change ${changeClass}">${change}</div>
    </div>`;
  });

  // Pattern: Cryptocurrency (SYMBOL) - +X.XX% Price: $XXX Market Cap: $XXB
  const cryptoPattern = /([A-Z][a-zA-Z\s]+)\s*\(([A-Z]+)\)\s*-\s*([+-]?\d+\.?\d*%)\s*Price:\s*\$?([\d,]+\.?\d*)\s*Market Cap:\s*\$?([\d,]+\.?\d*[BMK]?)/g;

  parsed = parsed.replace(cryptoPattern, (match, name, symbol, change, price, mcap) => {
    const isPositive = change.startsWith('+');
    const cardClass = isPositive ? 'price-card-positive' : 'price-card-negative';
    const changeClass = isPositive ? 'positive' : 'negative';

    return `<div class="price-card ${cardClass}">
      <div class="price-card-label">${symbol} • ${name}</div>
      <div class="price-card-value">$${price}</div>
      <div class="price-card-change ${changeClass}">${change} • MC: $${mcap}</div>
    </div>`;
  });

  return parsed;
};

const MarkdownRenderer: React.FC<MarkdownRendererProps> = ({ content, style = {} }) => {
  const markdownStyles: React.CSSProperties = {
    color: '#FFFFFF',
    fontSize: '11px',
    lineHeight: '1.6',
    fontFamily: '"IBM Plex Mono", Consolas, monospace',
    ...style
  };

  const html = useMemo(() => {
    if (!content || typeof content !== 'string') return '';
    try {
      // First, render LaTeX formulas
      const withLatex = renderLatex(content);

      // Then, parse data cards from raw content
      const withCards = parseDataCards(withLatex);

      // Then parse markdown
      const raw = marked.parse(withCards) as string;

      return DOMPurify.sanitize(raw, {
        ADD_ATTR: ['target', 'class', 'style', 'xmlns', 'viewBox', 'd', 'fill', 'stroke', 'width', 'height', 'aria-hidden', 'focusable'],
        ALLOWED_TAGS: [
          'h1', 'h2', 'h3', 'h4', 'h5', 'h6',
          'p', 'br', 'hr',
          'ul', 'ol', 'li',
          'strong', 'em', 'del', 'code', 'pre',
          'blockquote', 'a', 'img',
          'table', 'thead', 'tbody', 'tr', 'th', 'td',
          'input', 'span', 'div', 'sub', 'sup',
          // KaTeX elements
          'math', 'semantics', 'mrow', 'mi', 'mo', 'mn', 'ms', 'mtext',
          'mfrac', 'msqrt', 'mroot', 'msub', 'msup', 'msubsup', 'munder',
          'mover', 'munderover', 'mtable', 'mtr', 'mtd', 'annotation',
          'svg', 'path', 'line', 'rect', 'circle', 'g', 'use', 'defs',
        ],
      });
    } catch (error) {
      console.error('Error rendering markdown:', error);
      return '';
    }
  }, [content]);

  if (!content || typeof content !== 'string') {
    return (
      <div style={{ ...markdownStyles, color: '#787878' }}>
        (No content to display)
      </div>
    );
  }

  if (!html) {
    return (
      <div style={markdownStyles}>
        <pre style={{ whiteSpace: 'pre-wrap', wordBreak: 'break-word', fontSize: '12px', color: '#FFFFFF' }}>
          {content}
        </pre>
      </div>
    );
  }

  return (
    <>
      <style>{MARKDOWN_CSS}</style>
      <div
        className="fincept-md"
        style={markdownStyles}
        dangerouslySetInnerHTML={{ __html: html }}
      />
    </>
  );
};

export default MarkdownRenderer;
