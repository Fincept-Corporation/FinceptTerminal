import React, { useMemo } from 'react';
import { marked } from 'marked';
import DOMPurify from 'dompurify';

interface MarkdownRendererProps {
  content: string;
  style?: React.CSSProperties;
}

// Configure marked with GFM enabled
marked.setOptions({
  gfm: true,
  breaks: true,
});

const MARKDOWN_CSS = `
.fincept-md h1 {
  color: #FFA500;
  font-size: 20px;
  font-weight: bold;
  margin-top: 16px;
  margin-bottom: 12px;
  border-bottom: 2px solid #FFA500;
}
.fincept-md h2 {
  color: #FFA500;
  font-size: 18px;
  font-weight: bold;
  margin-top: 14px;
  margin-bottom: 10px;
  border-bottom: 1px solid #787878;
}
.fincept-md h3 {
  color: #FFFF00;
  font-size: 16px;
  font-weight: bold;
  margin-top: 12px;
  margin-bottom: 8px;
}
.fincept-md h4 {
  color: #FFFF00;
  font-size: 15px;
  font-weight: bold;
  margin-top: 10px;
  margin-bottom: 6px;
}
.fincept-md h5, .fincept-md h6 {
  color: #FFFFFF;
  font-size: 14px;
  font-weight: bold;
  margin-top: 8px;
  margin-bottom: 4px;
}
.fincept-md p {
  margin-bottom: 12px;
  color: #FFFFFF;
}
.fincept-md ul {
  margin-left: 20px;
  margin-bottom: 12px;
  list-style-type: disc;
  color: #FFFFFF;
}
.fincept-md ol {
  margin-left: 20px;
  margin-bottom: 12px;
  list-style-type: decimal;
  color: #FFFFFF;
}
.fincept-md li {
  margin-bottom: 6px;
  color: #FFFFFF;
}
.fincept-md pre {
  background-color: #000000;
  padding: 0;
  margin: 0 0 12px 0;
  overflow-x: auto;
}
.fincept-md pre code {
  display: block;
  background-color: #000000;
  color: #00C800;
  padding: 12px;
  border-radius: 4px;
  font-size: 13px;
  font-family: Consolas, monospace;
  border: 1px solid #787878;
  overflow-x: auto;
  white-space: pre-wrap;
}
.fincept-md code {
  background-color: #000000;
  color: #00C800;
  padding: 2px 6px;
  border-radius: 3px;
  font-size: 13px;
  font-family: Consolas, monospace;
  border: 1px solid #787878;
}
.fincept-md blockquote {
  border-left: 4px solid #FFA500;
  margin-left: 0;
  padding-left: 16px;
  margin-bottom: 12px;
  color: #787878;
  font-style: italic;
}
.fincept-md a {
  color: #FFA500;
  text-decoration: underline;
  cursor: pointer;
}
.fincept-md table {
  border-collapse: collapse;
  width: 100%;
  margin-bottom: 12px;
  border: 1px solid #787878;
}
.fincept-md thead {
  background-color: #000000;
}
.fincept-md tr {
  border-bottom: 1px solid #787878;
}
.fincept-md th {
  padding: 8px;
  text-align: left;
  color: #FFA500;
  font-weight: bold;
  border: 1px solid #787878;
}
.fincept-md td {
  padding: 8px;
  color: #FFFFFF;
  border: 1px solid #787878;
}
.fincept-md hr {
  border: none;
  border-top: 1px solid #787878;
  margin-top: 16px;
  margin-bottom: 16px;
}
.fincept-md strong {
  color: #FFFF00;
  font-weight: bold;
}
.fincept-md em {
  color: #FFFFFF;
  font-style: italic;
}
.fincept-md del {
  color: #787878;
  text-decoration: line-through;
}
.fincept-md input[type="checkbox"] {
  margin-right: 6px;
}
`;

const MarkdownRenderer: React.FC<MarkdownRendererProps> = ({ content, style = {} }) => {
  const markdownStyles: React.CSSProperties = {
    color: '#FFFFFF',
    fontSize: '14px',
    lineHeight: '1.6',
    ...style
  };

  const html = useMemo(() => {
    if (!content || typeof content !== 'string') return '';
    try {
      const raw = marked.parse(content) as string;
      return DOMPurify.sanitize(raw, {
        ADD_ATTR: ['target'],
        ALLOWED_TAGS: [
          'h1', 'h2', 'h3', 'h4', 'h5', 'h6',
          'p', 'br', 'hr',
          'ul', 'ol', 'li',
          'strong', 'em', 'del', 'code', 'pre',
          'blockquote', 'a', 'img',
          'table', 'thead', 'tbody', 'tr', 'th', 'td',
          'input', 'span', 'div', 'sub', 'sup',
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
