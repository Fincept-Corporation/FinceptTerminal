import React from 'react';
import ReactMarkdown from 'react-markdown';
import remarkGfm from 'remark-gfm';

interface MarkdownRendererProps {
  content: string;
  style?: React.CSSProperties;
}

const MarkdownRenderer: React.FC<MarkdownRendererProps> = ({ content, style = {} }) => {
  // Fincept color scheme
  const FINCEPT_ORANGE = '#FFA500';
  const FINCEPT_WHITE = '#FFFFFF';
  const FINCEPT_YELLOW = '#FFFF00';
  const FINCEPT_GRAY = '#787878';
  const FINCEPT_DARK_BG = '#000000';
  const FINCEPT_PANEL_BG = '#0a0a0a';
  const FINCEPT_GREEN = '#00C800';

  const markdownStyles: React.CSSProperties = {
    color: FINCEPT_WHITE,
    fontSize: '14px',
    lineHeight: '1.6',
    ...style
  };

  // Safety check for content
  if (!content || typeof content !== 'string') {
    return (
      <div style={{ ...markdownStyles, color: FINCEPT_GRAY }}>
        (No content to display)
      </div>
    );
  }

  try {
    return (
      <div style={markdownStyles}>
        <ReactMarkdown
        remarkPlugins={[remarkGfm as any]}
        components={{
          // Headings
          h1: ({ children }) => (
            <h1 style={{
              color: FINCEPT_ORANGE,
              fontSize: '20px',
              fontWeight: 'bold',
              marginTop: '16px',
              marginBottom: '12px',
              borderBottom: `2px solid ${FINCEPT_ORANGE}`
            }}>
              {children}
            </h1>
          ),
          h2: ({ children }) => (
            <h2 style={{
              color: FINCEPT_ORANGE,
              fontSize: '18px',
              fontWeight: 'bold',
              marginTop: '14px',
              marginBottom: '10px',
              borderBottom: `1px solid ${FINCEPT_GRAY}`
            }}>
              {children}
            </h2>
          ),
          h3: ({ children }) => (
            <h3 style={{
              color: FINCEPT_YELLOW,
              fontSize: '16px',
              fontWeight: 'bold',
              marginTop: '12px',
              marginBottom: '8px'
            }}>
              {children}
            </h3>
          ),
          h4: ({ children }) => (
            <h4 style={{
              color: FINCEPT_YELLOW,
              fontSize: '15px',
              fontWeight: 'bold',
              marginTop: '10px',
              marginBottom: '6px'
            }}>
              {children}
            </h4>
          ),
          h5: ({ children }) => (
            <h5 style={{
              color: FINCEPT_WHITE,
              fontSize: '14px',
              fontWeight: 'bold',
              marginTop: '8px',
              marginBottom: '4px'
            }}>
              {children}
            </h5>
          ),
          h6: ({ children }) => (
            <h6 style={{
              color: FINCEPT_WHITE,
              fontSize: '13px',
              fontWeight: 'bold',
              marginTop: '6px',
              marginBottom: '3px'
            }}>
              {children}
            </h6>
          ),

          // Paragraphs
          p: ({ children }) => (
            <p style={{
              marginBottom: '12px',
              color: FINCEPT_WHITE
            }}>
              {children}
            </p>
          ),

          // Lists
          ul: ({ children }) => (
            <ul style={{
              marginLeft: '20px',
              marginBottom: '12px',
              listStyleType: 'disc',
              color: FINCEPT_WHITE
            }}>
              {children}
            </ul>
          ),
          ol: ({ children }) => (
            <ol style={{
              marginLeft: '20px',
              marginBottom: '12px',
              listStyleType: 'decimal',
              color: FINCEPT_WHITE
            }}>
              {children}
            </ol>
          ),
          li: ({ children }) => (
            <li style={{
              marginBottom: '6px',
              color: FINCEPT_WHITE
            }}>
              {children}
            </li>
          ),

          // Code blocks
          code: ({ inline, children, ...props }: any) => {
            if (inline) {
              return (
                <code style={{
                  backgroundColor: FINCEPT_DARK_BG,
                  color: FINCEPT_GREEN,
                  padding: '2px 6px',
                  borderRadius: '3px',
                  fontSize: '13px',
                  fontFamily: 'Consolas, monospace',
                  border: `1px solid ${FINCEPT_GRAY}`
                }} {...props}>
                  {children}
                </code>
              );
            }
            return (
              <code style={{
                display: 'block',
                backgroundColor: FINCEPT_DARK_BG,
                color: FINCEPT_GREEN,
                padding: '12px',
                borderRadius: '4px',
                fontSize: '13px',
                fontFamily: 'Consolas, monospace',
                border: `1px solid ${FINCEPT_GRAY}`,
                marginBottom: '12px',
                overflowX: 'auto',
                whiteSpace: 'pre-wrap'
              }} {...props}>
                {children}
              </code>
            );
          },

          // Pre blocks
          pre: ({ children }) => (
            <pre style={{
              backgroundColor: FINCEPT_DARK_BG,
              padding: '0',
              margin: '0 0 12px 0',
              overflowX: 'auto'
            }}>
              {children}
            </pre>
          ),

          // Blockquotes
          blockquote: ({ children }) => (
            <blockquote style={{
              borderLeft: `4px solid ${FINCEPT_ORANGE}`,
              marginLeft: '0',
              paddingLeft: '16px',
              marginBottom: '12px',
              color: FINCEPT_GRAY,
              fontStyle: 'italic'
            }}>
              {children}
            </blockquote>
          ),

          // Links
          a: ({ children, href }) => (
            <a href={href} style={{
              color: FINCEPT_ORANGE,
              textDecoration: 'underline',
              cursor: 'pointer'
            }} target="_blank" rel="noopener noreferrer">
              {children}
            </a>
          ),

          // Tables
          table: ({ children }) => (
            <table style={{
              borderCollapse: 'collapse',
              width: '100%',
              marginBottom: '12px',
              border: `1px solid ${FINCEPT_GRAY}`
            }}>
              {children}
            </table>
          ),
          thead: ({ children }) => (
            <thead style={{
              backgroundColor: FINCEPT_DARK_BG
            }}>
              {children}
            </thead>
          ),
          tbody: ({ children }) => (
            <tbody>
              {children}
            </tbody>
          ),
          tr: ({ children }) => (
            <tr style={{
              borderBottom: `1px solid ${FINCEPT_GRAY}`
            }}>
              {children}
            </tr>
          ),
          th: ({ children }) => (
            <th style={{
              padding: '8px',
              textAlign: 'left',
              color: FINCEPT_ORANGE,
              fontWeight: 'bold',
              border: `1px solid ${FINCEPT_GRAY}`
            }}>
              {children}
            </th>
          ),
          td: ({ children }) => (
            <td style={{
              padding: '8px',
              color: FINCEPT_WHITE,
              border: `1px solid ${FINCEPT_GRAY}`
            }}>
              {children}
            </td>
          ),

          // Horizontal rule
          hr: () => (
            <hr style={{
              border: 'none',
              borderTop: `1px solid ${FINCEPT_GRAY}`,
              marginTop: '16px',
              marginBottom: '16px'
            }} />
          ),

          // Strong/Bold
          strong: ({ children }) => (
            <strong style={{
              color: FINCEPT_YELLOW,
              fontWeight: 'bold'
            }}>
              {children}
            </strong>
          ),

          // Emphasis/Italic
          em: ({ children }) => (
            <em style={{
              color: FINCEPT_WHITE,
              fontStyle: 'italic'
            }}>
              {children}
            </em>
          ),

          // Delete/Strikethrough
          del: ({ children }) => (
            <del style={{
              color: FINCEPT_GRAY,
              textDecoration: 'line-through'
            }}>
              {children}
            </del>
          )
        }}
      >
        {content}
      </ReactMarkdown>
      </div>
    );
  } catch (error) {
    console.error('Error rendering markdown:', error);
    return (
      <div style={{ ...markdownStyles, color: FINCEPT_GRAY }}>
        <div style={{ color: '#ff6b6b', marginBottom: '8px' }}>
          Error rendering markdown content
        </div>
        <pre style={{
          whiteSpace: 'pre-wrap',
          wordBreak: 'break-word',
          fontSize: '12px',
          color: FINCEPT_WHITE
        }}>
          {content}
        </pre>
      </div>
    );
  }
};

export default MarkdownRenderer;
