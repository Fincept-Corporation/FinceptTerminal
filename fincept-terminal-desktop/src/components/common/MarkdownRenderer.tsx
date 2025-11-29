import React from 'react';
import ReactMarkdown from 'react-markdown';
import remarkGfm from 'remark-gfm';

interface MarkdownRendererProps {
  content: string;
  style?: React.CSSProperties;
}

const MarkdownRenderer: React.FC<MarkdownRendererProps> = ({ content, style = {} }) => {
  // Bloomberg color scheme
  const BLOOMBERG_ORANGE = '#FFA500';
  const BLOOMBERG_WHITE = '#FFFFFF';
  const BLOOMBERG_YELLOW = '#FFFF00';
  const BLOOMBERG_GRAY = '#787878';
  const BLOOMBERG_DARK_BG = '#000000';
  const BLOOMBERG_PANEL_BG = '#0a0a0a';
  const BLOOMBERG_GREEN = '#00C800';

  const markdownStyles: React.CSSProperties = {
    color: BLOOMBERG_WHITE,
    fontSize: '14px',
    lineHeight: '1.6',
    ...style
  };

  // Safety check for content
  if (!content || typeof content !== 'string') {
    return (
      <div style={{ ...markdownStyles, color: BLOOMBERG_GRAY }}>
        (No content to display)
      </div>
    );
  }

  try {
    return (
      <div style={markdownStyles}>
        <ReactMarkdown
        remarkPlugins={[remarkGfm]}
        components={{
          // Headings
          h1: ({ children }) => (
            <h1 style={{
              color: BLOOMBERG_ORANGE,
              fontSize: '20px',
              fontWeight: 'bold',
              marginTop: '16px',
              marginBottom: '12px',
              borderBottom: `2px solid ${BLOOMBERG_ORANGE}`
            }}>
              {children}
            </h1>
          ),
          h2: ({ children }) => (
            <h2 style={{
              color: BLOOMBERG_ORANGE,
              fontSize: '18px',
              fontWeight: 'bold',
              marginTop: '14px',
              marginBottom: '10px',
              borderBottom: `1px solid ${BLOOMBERG_GRAY}`
            }}>
              {children}
            </h2>
          ),
          h3: ({ children }) => (
            <h3 style={{
              color: BLOOMBERG_YELLOW,
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
              color: BLOOMBERG_YELLOW,
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
              color: BLOOMBERG_WHITE,
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
              color: BLOOMBERG_WHITE,
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
              color: BLOOMBERG_WHITE
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
              color: BLOOMBERG_WHITE
            }}>
              {children}
            </ul>
          ),
          ol: ({ children }) => (
            <ol style={{
              marginLeft: '20px',
              marginBottom: '12px',
              listStyleType: 'decimal',
              color: BLOOMBERG_WHITE
            }}>
              {children}
            </ol>
          ),
          li: ({ children }) => (
            <li style={{
              marginBottom: '6px',
              color: BLOOMBERG_WHITE
            }}>
              {children}
            </li>
          ),

          // Code blocks
          code: ({ inline, children, ...props }: any) => {
            if (inline) {
              return (
                <code style={{
                  backgroundColor: BLOOMBERG_DARK_BG,
                  color: BLOOMBERG_GREEN,
                  padding: '2px 6px',
                  borderRadius: '3px',
                  fontSize: '13px',
                  fontFamily: 'Consolas, monospace',
                  border: `1px solid ${BLOOMBERG_GRAY}`
                }} {...props}>
                  {children}
                </code>
              );
            }
            return (
              <code style={{
                display: 'block',
                backgroundColor: BLOOMBERG_DARK_BG,
                color: BLOOMBERG_GREEN,
                padding: '12px',
                borderRadius: '4px',
                fontSize: '13px',
                fontFamily: 'Consolas, monospace',
                border: `1px solid ${BLOOMBERG_GRAY}`,
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
              backgroundColor: BLOOMBERG_DARK_BG,
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
              borderLeft: `4px solid ${BLOOMBERG_ORANGE}`,
              marginLeft: '0',
              paddingLeft: '16px',
              marginBottom: '12px',
              color: BLOOMBERG_GRAY,
              fontStyle: 'italic'
            }}>
              {children}
            </blockquote>
          ),

          // Links
          a: ({ children, href }) => (
            <a href={href} style={{
              color: BLOOMBERG_ORANGE,
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
              border: `1px solid ${BLOOMBERG_GRAY}`
            }}>
              {children}
            </table>
          ),
          thead: ({ children }) => (
            <thead style={{
              backgroundColor: BLOOMBERG_DARK_BG
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
              borderBottom: `1px solid ${BLOOMBERG_GRAY}`
            }}>
              {children}
            </tr>
          ),
          th: ({ children }) => (
            <th style={{
              padding: '8px',
              textAlign: 'left',
              color: BLOOMBERG_ORANGE,
              fontWeight: 'bold',
              border: `1px solid ${BLOOMBERG_GRAY}`
            }}>
              {children}
            </th>
          ),
          td: ({ children }) => (
            <td style={{
              padding: '8px',
              color: BLOOMBERG_WHITE,
              border: `1px solid ${BLOOMBERG_GRAY}`
            }}>
              {children}
            </td>
          ),

          // Horizontal rule
          hr: () => (
            <hr style={{
              border: 'none',
              borderTop: `1px solid ${BLOOMBERG_GRAY}`,
              marginTop: '16px',
              marginBottom: '16px'
            }} />
          ),

          // Strong/Bold
          strong: ({ children }) => (
            <strong style={{
              color: BLOOMBERG_YELLOW,
              fontWeight: 'bold'
            }}>
              {children}
            </strong>
          ),

          // Emphasis/Italic
          em: ({ children }) => (
            <em style={{
              color: BLOOMBERG_WHITE,
              fontStyle: 'italic'
            }}>
              {children}
            </em>
          ),

          // Delete/Strikethrough
          del: ({ children }) => (
            <del style={{
              color: BLOOMBERG_GRAY,
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
      <div style={{ ...markdownStyles, color: BLOOMBERG_GRAY }}>
        <div style={{ color: '#ff6b6b', marginBottom: '8px' }}>
          Error rendering markdown content
        </div>
        <pre style={{
          whiteSpace: 'pre-wrap',
          wordBreak: 'break-word',
          fontSize: '12px',
          color: BLOOMBERG_WHITE
        }}>
          {content}
        </pre>
      </div>
    );
  }
};

export default MarkdownRenderer;
