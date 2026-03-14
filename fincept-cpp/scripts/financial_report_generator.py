#!/usr/bin/env python3
"""
Financial Report Generator using Jinja2
Generates professional PDF reports from JSON templates
"""

import sys
import json
import os
from datetime import datetime
from typing import Dict, List, Any, Optional
from jinja2 import Environment, FileSystemLoader, select_autoescape
import base64
from io import BytesIO

# Try to import PDF generation libraries
try:
    from weasyprint import HTML, CSS
    WEASYPRINT_AVAILABLE = True
except (ImportError, OSError) as e:
    WEASYPRINT_AVAILABLE = False
    print(f"Warning: WeasyPrint not available: {e}", file=sys.stderr)

# Alternative: Try pdfkit (wkhtmltopdf wrapper)
try:
    import pdfkit
    PDFKIT_AVAILABLE = True
except ImportError:
    PDFKIT_AVAILABLE = False

# Alternative: Try reportlab
try:
    from reportlab.lib.pagesizes import letter, A4
    from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, PageBreak, Table, TableStyle, Image as RLImage
    from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
    from reportlab.lib.units import inch
    from reportlab.lib import colors as rl_colors
    REPORTLAB_AVAILABLE = True
except ImportError:
    REPORTLAB_AVAILABLE = False

try:
    import matplotlib
    matplotlib.use('Agg')  # Non-interactive backend
    import matplotlib.pyplot as plt
    import matplotlib.dates as mdates
    MATPLOTLIB_AVAILABLE = True
except ImportError:
    MATPLOTLIB_AVAILABLE = False

try:
    import pandas as pd
    PANDAS_AVAILABLE = True
except ImportError:
    PANDAS_AVAILABLE = False


class FinancialReportGenerator:
    """Generate professional financial reports using Jinja2 templates"""

    def __init__(self, templates_dir: Optional[str] = None):
        """
        Initialize the report generator

        Args:
            templates_dir: Directory containing Jinja2 templates
        """
        if templates_dir is None:
            # Use script directory + templates folder
            script_dir = os.path.dirname(os.path.abspath(__file__))
            templates_dir = os.path.join(script_dir, 'report_templates')

        # Create templates directory if it doesn't exist
        os.makedirs(templates_dir, exist_ok=True)

        # Initialize Jinja2 environment
        self.env = Environment(
            loader=FileSystemLoader(templates_dir),
            autoescape=select_autoescape(['html', 'xml'])
        )

        # Add custom filters
        self.env.filters['format_number'] = self.format_number
        self.env.filters['format_currency'] = self.format_currency
        self.env.filters['format_percent'] = self.format_percent
        self.env.filters['format_date'] = self.format_date

        self.templates_dir = templates_dir

    @staticmethod
    def format_number(value: float, decimals: int = 2) -> str:
        """Format number with thousand separators"""
        try:
            return f"{float(value):,.{decimals}f}"
        except (ValueError, TypeError):
            return str(value)

    @staticmethod
    def format_currency(value: float, symbol: str = '$', decimals: int = 2) -> str:
        """Format as currency"""
        try:
            return f"{symbol}{float(value):,.{decimals}f}"
        except (ValueError, TypeError):
            return f"{symbol}{value}"

    @staticmethod
    def format_percent(value: float, decimals: int = 2) -> str:
        """Format as percentage"""
        try:
            return f"{float(value):.{decimals}f}%"
        except (ValueError, TypeError):
            return f"{value}%"

    @staticmethod
    def format_date(value: str, format: str = '%Y-%m-%d') -> str:
        """Format date string"""
        try:
            if isinstance(value, str):
                dt = datetime.fromisoformat(value.replace('Z', '+00:00'))
            else:
                dt = value
            return dt.strftime(format)
        except (ValueError, TypeError, AttributeError):
            return str(value)

    def generate_chart(self, chart_config: Dict[str, Any]) -> str:
        """
        Generate chart and return as base64 encoded image

        Args:
            chart_config: Chart configuration dict

        Returns:
            Base64 encoded image string
        """
        if not MATPLOTLIB_AVAILABLE:
            return ""

        chart_type = chart_config.get('type', 'line')
        data = chart_config.get('data', {})
        title = chart_config.get('title', '')

        fig, ax = plt.subplots(figsize=(10, 6))

        if chart_type == 'line':
            for series_name, series_data in data.items():
                ax.plot(series_data.get('x', []), series_data.get('y', []), label=series_name)
        elif chart_type == 'bar':
            x_data = list(data.keys())
            y_data = [data[k] for k in x_data]
            ax.bar(x_data, y_data)
        elif chart_type == 'pie':
            labels = list(data.keys())
            sizes = [data[k] for k in labels]
            ax.pie(sizes, labels=labels, autopct='%1.1f%%')

        ax.set_title(title)
        ax.legend()
        ax.grid(True, alpha=0.3)

        # Convert to base64
        buf = BytesIO()
        plt.savefig(buf, format='png', dpi=150, bbox_inches='tight')
        buf.seek(0)
        img_base64 = base64.b64encode(buf.read()).decode('utf-8')
        plt.close(fig)

        return f"data:image/png;base64,{img_base64}"

    def create_default_template(self) -> str:
        """Create default HTML template if none exists"""
        template_content = """<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>{{ metadata.title }}</title>
    <style>
        @page {
            size: {{ styles.pageSize }} {{ styles.orientation }};
            margin: {{ styles.margins }};

            @top-center {
                content: "{{ metadata.title }}";
                font-size: 10pt;
                color: #666;
            }

            @bottom-center {
                content: "Page " counter(page) " of " counter(pages);
                font-size: 10pt;
                color: #666;
            }
        }

        body {
            font-family: 'Helvetica', 'Arial', sans-serif;
            line-height: 1.6;
            color: #333;
            max-width: 800px;
            margin: 0 auto;
        }

        h1 {
            color: #1a1a1a;
            border-bottom: 3px solid #0066cc;
            padding-bottom: 10px;
            margin-top: 0;
        }

        h2 {
            color: #0066cc;
            margin-top: 30px;
            border-left: 4px solid #0066cc;
            padding-left: 10px;
        }

        h3 {
            color: #333;
            margin-top: 20px;
        }

        table {
            width: 100%;
            border-collapse: collapse;
            margin: 20px 0;
        }

        th, td {
            border: 1px solid #ddd;
            padding: 12px;
            text-align: left;
        }

        th {
            background-color: #0066cc;
            color: white;
            font-weight: bold;
        }

        tr:nth-child(even) {
            background-color: #f9f9f9;
        }

        .header {
            margin-bottom: 30px;
            padding-bottom: 20px;
            border-bottom: 2px solid #eee;
        }

        .metadata {
            display: flex;
            justify-content: space-between;
            color: #666;
            font-size: 0.9em;
            margin-top: 10px;
        }

        .content-block {
            margin: 20px 0;
        }

        .divider {
            border-top: 2px solid #eee;
            margin: 30px 0;
        }

        .code-block {
            background-color: #f5f5f5;
            border-left: 4px solid #0066cc;
            padding: 15px;
            font-family: 'Courier New', monospace;
            font-size: 0.9em;
            overflow-x: auto;
        }

        img {
            max-width: 100%;
            height: auto;
            display: block;
            margin: 20px auto;
        }

        .text-center {
            text-align: center;
        }

        .text-right {
            text-align: right;
        }

        .text-left {
            text-align: left;
        }
    </style>
</head>
<body>
    <div class="header">
        <h1>{{ metadata.title }}</h1>
        {% if metadata.company %}
        <p style="font-size: 1.2em; color: #0066cc;">{{ metadata.company }}</p>
        {% endif %}
        <div class="metadata">
            {% if metadata.author %}<span>Author: {{ metadata.author }}</span>{% endif %}
            {% if metadata.date %}<span>Date: {{ metadata.date | format_date('%B %d, %Y') }}</span>{% endif %}
        </div>
    </div>

    {% for component in components %}
        {% if component.type == 'heading' %}
            <h2 class="text-{{ component.config.alignment }}">{{ component.content }}</h2>

        {% elif component.type == 'text' %}
            <p class="text-{{ component.config.alignment }} content-block">{{ component.content }}</p>

        {% elif component.type == 'divider' %}
            <div class="divider"></div>

        {% elif component.type == 'code' %}
            <pre class="code-block"><code>{{ component.content }}</code></pre>

        {% elif component.type == 'table' %}
            <table>
                <thead>
                    <tr>
                        {% for col in component.config.columns %}
                        <th>{{ col }}</th>
                        {% endfor %}
                    </tr>
                </thead>
                <tbody>
                    {% if component.content and component.content.rows %}
                        {% for row in component.content.rows %}
                        <tr>
                            {% for cell in row %}
                            <td>{{ cell }}</td>
                            {% endfor %}
                        </tr>
                        {% endfor %}
                    {% else %}
                        <tr>
                            {% for col in component.config.columns %}
                            <td>Sample Data</td>
                            {% endfor %}
                        </tr>
                    {% endif %}
                </tbody>
            </table>

        {% elif component.type == 'chart' %}
            {% if component.content and component.content.image %}
            <img src="{{ component.content.image }}" alt="Chart" />
            {% endif %}

        {% elif component.type == 'image' %}
            {% if component.config.imageUrl %}
            <img src="{{ component.config.imageUrl }}" alt="Image" />
            {% endif %}
        {% endif %}
    {% endfor %}
</body>
</html>"""

        template_path = os.path.join(self.templates_dir, 'default.html')
        with open(template_path, 'w', encoding='utf-8') as f:
            f.write(template_content)

        return template_path

    def generate_html(self, template_data: Dict[str, Any], template_name: str = 'default.html') -> str:
        """
        Generate HTML from template data

        Args:
            template_data: Report template data
            template_name: Name of Jinja2 template file

        Returns:
            Generated HTML string
        """
        # Ensure default template exists
        template_path = os.path.join(self.templates_dir, template_name)
        if not os.path.exists(template_path):
            self.create_default_template()

        # Generate charts if needed
        for component in template_data.get('components', []):
            if component.get('type') == 'chart' and component.get('content', {}).get('data'):
                chart_config = component['content']
                img_data = self.generate_chart(chart_config)
                if img_data:
                    component['content']['image'] = img_data

        # Render template
        template = self.env.get_template(template_name)
        html = template.render(**template_data)

        return html

    def generate_pdf_reportlab(self, template_data: Dict[str, Any], output_path: str) -> Dict[str, Any]:
        """
        Generate PDF using ReportLab (pure Python, cross-platform)

        Args:
            template_data: Report template data
            output_path: Path to save PDF

        Returns:
            Result dict with success status
        """
        if not REPORTLAB_AVAILABLE:
            return {
                "success": False,
                "error": "ReportLab not available. Install with: pip install reportlab",
                "output_path": None
            }

        try:
            # Determine page size
            page_size_map = {
                'A4': A4,
                'Letter': letter,
                'Legal': (8.5*inch, 14*inch)
            }
            page_size = page_size_map.get(template_data.get('styles', {}).get('pageSize', 'A4'), A4)

            # Create PDF
            doc = SimpleDocTemplate(
                output_path,
                pagesize=page_size,
                rightMargin=72,
                leftMargin=72,
                topMargin=72,
                bottomMargin=18,
            )

            # Styles
            styles = getSampleStyleSheet()
            title_style = ParagraphStyle(
                'CustomTitle',
                parent=styles['Heading1'],
                fontSize=24,
                textColor=rl_colors.HexColor('#0066cc'),
                spaceAfter=30,
            )
            heading_style = ParagraphStyle(
                'CustomHeading',
                parent=styles['Heading2'],
                fontSize=18,
                textColor=rl_colors.HexColor('#0066cc'),
                spaceAfter=12,
            )
            normal_style = styles['BodyText']

            # Build story
            story = []

            # Header
            metadata = template_data.get('metadata', {})
            if metadata.get('title'):
                story.append(Paragraph(metadata['title'], title_style))

            if metadata.get('company'):
                company_style = ParagraphStyle(
                    'Company',
                    parent=styles['Normal'],
                    fontSize=14,
                    textColor=rl_colors.HexColor('#0066cc'),
                    spaceAfter=12,
                )
                story.append(Paragraph(metadata['company'], company_style))

            # Metadata line
            meta_text = []
            if metadata.get('author'):
                meta_text.append(f"Author: {metadata['author']}")
            if metadata.get('date'):
                meta_text.append(f"Date: {metadata['date']}")
            if meta_text:
                meta_style = ParagraphStyle('Meta', parent=styles['Normal'], fontSize=10, textColor=rl_colors.grey)
                story.append(Paragraph(' | '.join(meta_text), meta_style))

            story.append(Spacer(1, 0.3*inch))

            # Components
            for component in template_data.get('components', []):
                comp_type = component.get('type')
                content = component.get('content', '')

                if comp_type == 'heading':
                    if content:
                        story.append(Paragraph(content, heading_style))

                elif comp_type == 'text':
                    if content:
                        story.append(Paragraph(content, normal_style))
                        story.append(Spacer(1, 0.1*inch))

                elif comp_type == 'divider':
                    story.append(Spacer(1, 0.2*inch))

                elif comp_type == 'code':
                    if content:
                        code_style = ParagraphStyle(
                            'Code',
                            parent=styles['Code'],
                            fontSize=9,
                            leftIndent=20,
                            rightIndent=20,
                            backColor=rl_colors.HexColor('#f5f5f5'),
                        )
                        story.append(Paragraph(f'<pre>{content}</pre>', code_style))
                        story.append(Spacer(1, 0.1*inch))

                elif comp_type == 'table':
                    config = component.get('config', {})
                    columns = config.get('columns', [])
                    rows_data = component.get('content', {}).get('rows', [])

                    if columns and rows_data:
                        table_data = [columns] + rows_data
                        t = Table(table_data)
                        t.setStyle(TableStyle([
                            ('BACKGROUND', (0, 0), (-1, 0), rl_colors.HexColor('#0066cc')),
                            ('TEXTCOLOR', (0, 0), (-1, 0), rl_colors.white),
                            ('ALIGN', (0, 0), (-1, -1), 'LEFT'),
                            ('FONTNAME', (0, 0), (-1, 0), 'Helvetica-Bold'),
                            ('FONTSIZE', (0, 0), (-1, 0), 12),
                            ('BOTTOMPADDING', (0, 0), (-1, 0), 12),
                            ('BACKGROUND', (0, 1), (-1, -1), rl_colors.white),
                            ('GRID', (0, 0), (-1, -1), 1, rl_colors.grey),
                        ]))
                        story.append(t)
                        story.append(Spacer(1, 0.2*inch))

            # Build PDF
            doc.build(story)

            return {
                "success": True,
                "output_path": output_path,
                "file_size": os.path.getsize(output_path),
                "error": None
            }

        except Exception as e:
            return {
                "success": False,
                "error": str(e),
                "output_path": None
            }

    def generate_pdf(self, template_data: Dict[str, Any], output_path: str, template_name: str = 'default.html') -> Dict[str, Any]:
        """
        Generate PDF from template data (tries multiple backends)

        Args:
            template_data: Report template data
            output_path: Path to save PDF
            template_name: Name of Jinja2 template file

        Returns:
            Result dict with success status
        """
        # Try WeasyPrint first (best HTML/CSS support)
        if WEASYPRINT_AVAILABLE:
            try:
                html_content = self.generate_html(template_data, template_name)
                HTML(string=html_content).write_pdf(output_path)
                return {
                    "success": True,
                    "output_path": output_path,
                    "file_size": os.path.getsize(output_path),
                    "error": None
                }
            except Exception as e:
                print(f"WeasyPrint failed: {e}", file=sys.stderr)

        # Fallback to ReportLab (pure Python, always works)
        if REPORTLAB_AVAILABLE:
            return self.generate_pdf_reportlab(template_data, output_path)

        # No PDF library available
        return {
            "success": False,
            "error": "No PDF generation library available. Install reportlab: pip install reportlab",
            "output_path": None
        }


def main():
    """Main CLI function"""
    if len(sys.argv) < 3:
        print("Usage: python financial_report_generator.py <command> <args>")
        print("\nCommands:")
        print("  generate_html <template_json>       Generate HTML from JSON template")
        print("  generate_pdf <template_json> <output_path>   Generate PDF from JSON template")
        print("  create_template                     Create default template")
        sys.exit(1)

    command = sys.argv[1]
    generator = FinancialReportGenerator()

    if command == 'create_template':
        template_path = generator.create_default_template()
        result = {
            "success": True,
            "template_path": template_path,
            "message": "Default template created successfully"
        }
        print(json.dumps(result, indent=2))

    elif command == 'generate_html':
        if len(sys.argv) < 3:
            print("Error: template_json required")
            sys.exit(1)

        template_json = sys.argv[2]

        try:
            # Parse template data
            if template_json.startswith('{'):
                template_data = json.loads(template_json)
            else:
                with open(template_json, 'r', encoding='utf-8') as f:
                    template_data = json.load(f)

            # Generate HTML
            html = generator.generate_html(template_data)

            result = {
                "success": True,
                "html": html,
                "error": None
            }
            print(json.dumps(result, indent=2))

        except Exception as e:
            result = {
                "success": False,
                "html": None,
                "error": str(e)
            }
            print(json.dumps(result, indent=2))
            sys.exit(1)

    elif command == 'generate_pdf':
        if len(sys.argv) < 4:
            print("Error: template_json and output_path required")
            sys.exit(1)

        template_json = sys.argv[2]
        output_path = sys.argv[3]

        try:
            # Parse template data
            if template_json.startswith('{'):
                template_data = json.loads(template_json)
            else:
                with open(template_json, 'r', encoding='utf-8') as f:
                    template_data = json.load(f)

            # Generate PDF
            result = generator.generate_pdf(template_data, output_path)
            print(json.dumps(result, indent=2))

            if not result['success']:
                sys.exit(1)

        except Exception as e:
            result = {
                "success": False,
                "error": str(e),
                "output_path": None
            }
            print(json.dumps(result, indent=2))
            sys.exit(1)

    else:
        print(f"Error: Unknown command '{command}'")
        sys.exit(1)


if __name__ == "__main__":
    main()
