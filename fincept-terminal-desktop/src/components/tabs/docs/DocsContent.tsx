import React from 'react';
import { Book, Code } from 'lucide-react';
import { DocSubsection } from './types';
import { COLORS } from './constants';

interface DocsContentProps {
  subsection: DocSubsection | undefined;
}

export function DocsContent({ subsection }: DocsContentProps) {
  if (!subsection) {
    return (
      <div
        className="docs-scroll"
        style={{
          flex: '1 1 500px',
          minWidth: '400px',
          overflowY: 'auto',
          padding: '24px',
          backgroundColor: '#050505'
        }}
      >
        <div style={{ textAlign: 'center', paddingTop: '60px', color: COLORS.GRAY }}>
          <Book size={48} style={{ marginBottom: '16px', opacity: 0.5 }} />
          <div style={{ fontSize: '16px' }}>Select a topic from the sidebar</div>
        </div>
      </div>
    );
  }

  return (
    <div
      className="docs-scroll"
      style={{
        flex: '1 1 500px',
        minWidth: '400px',
        overflowY: 'auto',
        padding: '24px',
        backgroundColor: '#050505'
      }}
    >
      <div style={{ maxWidth: '900px', margin: '0 auto' }}>
        <h1
          style={{
            color: COLORS.ORANGE,
            fontSize: '28px',
            marginBottom: '16px',
            borderBottom: `2px solid ${COLORS.ORANGE}`,
            paddingBottom: '12px'
          }}
        >
          {subsection.title}
        </h1>
        <div className="docs-content" style={{ fontSize: '14px', lineHeight: '1.8', color: COLORS.WHITE }}>
          {subsection.content.split('\n\n').map((para, idx) => (
            <p key={idx} style={{ marginBottom: '16px', whiteSpace: 'pre-wrap' }}>
              {para.split('\n').map((line, lineIdx) => {
                if (line.startsWith('**') && line.endsWith('**')) {
                  return (
                    <div
                      key={lineIdx}
                      style={{ color: COLORS.CYAN, fontWeight: 'bold', marginTop: '16px', marginBottom: '8px' }}
                    >
                      {line.replace(/\*\*/g, '')}
                    </div>
                  );
                }
                if (line.startsWith('•')) {
                  return (
                    <li key={lineIdx} style={{ marginLeft: '20px', marginBottom: '4px' }}>
                      {line.substring(1).trim()}
                    </li>
                  );
                }
                return (
                  <span key={lineIdx}>
                    {line}
                    <br />
                  </span>
                );
              })}
            </p>
          ))}
        </div>

        {subsection.codeExample && (
          <div style={{ marginTop: '24px' }}>
            <div
              style={{
                color: COLORS.ORANGE,
                fontSize: '14px',
                fontWeight: 'bold',
                marginBottom: '8px',
                display: 'flex',
                alignItems: 'center',
                gap: '8px'
              }}
            >
              <Code size={16} />
              CODE EXAMPLE
            </div>
            <pre
              style={{
                backgroundColor: COLORS.CODE_BG,
                border: `1px solid ${COLORS.GRAY}`,
                padding: '16px',
                borderRadius: '4px',
                overflowX: 'auto',
                fontSize: '13px',
                lineHeight: '1.6',
                color: COLORS.GREEN
              }}
            >
              <code>{subsection.codeExample}</code>
            </pre>
          </div>
        )}
      </div>
    </div>
  );
}
