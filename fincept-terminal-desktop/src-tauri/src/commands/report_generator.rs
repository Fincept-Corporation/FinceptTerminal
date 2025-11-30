// Financial Report Generator Commands using Rust printpdf
#![allow(dead_code)]
use serde::{Deserialize, Serialize};
use printpdf::*;
use std::fs::File;
use std::io::BufWriter;

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct ReportComponent {
    pub id: String,
    #[serde(rename = "type")]
    pub component_type: String,
    pub content: Option<String>,
    pub config: ReportComponentConfig,
}

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct ReportComponentConfig {
    #[serde(rename = "fontSize")]
    pub font_size: Option<String>,
    #[serde(rename = "fontWeight")]
    pub font_weight: Option<String>,
    pub color: Option<String>,
    pub alignment: Option<String>,
    #[serde(rename = "chartType")]
    pub chart_type: Option<String>,
    pub columns: Option<Vec<String>>,
    #[serde(rename = "imageUrl")]
    pub image_url: Option<String>,
}

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct ReportMetadata {
    pub author: Option<String>,
    pub company: Option<String>,
    pub date: Option<String>,
    pub title: Option<String>,
}

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct ReportStyles {
    #[serde(rename = "pageSize")]
    pub page_size: Option<String>,
    pub orientation: Option<String>,
    pub margins: Option<String>,
    #[serde(rename = "headerFooter")]
    pub header_footer: Option<bool>,
}

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct ReportTemplate {
    pub id: String,
    pub name: String,
    pub description: String,
    pub components: Vec<ReportComponent>,
    pub metadata: ReportMetadata,
    pub styles: ReportStyles,
}

#[derive(Debug, Serialize)]
pub struct ReportGenerationResult {
    pub success: bool,
    pub output_path: Option<String>,
    pub file_size: Option<u64>,
    pub error: Option<String>,
}

/// Generate PDF from report template using Rust printpdf library
#[tauri::command]
pub async fn generate_report_pdf(
    template_json: String,
    output_path: String,
) -> Result<String, String> {
    // Parse template
    let template: ReportTemplate = match serde_json::from_str(&template_json) {
        Ok(t) => t,
        Err(e) => {
            let result = ReportGenerationResult {
                success: false,
                output_path: None,
                file_size: None,
                error: Some(format!("Failed to parse template: {}", e)),
            };
            return Ok(serde_json::to_string(&result).unwrap());
        }
    };

    // Generate PDF
    match generate_pdf_document(&template, &output_path) {
        Ok(file_size) => {
            let result = ReportGenerationResult {
                success: true,
                output_path: Some(output_path),
                file_size: Some(file_size),
                error: None,
            };
            Ok(serde_json::to_string(&result).unwrap())
        }
        Err(e) => {
            let result = ReportGenerationResult {
                success: false,
                output_path: None,
                file_size: None,
                error: Some(e),
            };
            Ok(serde_json::to_string(&result).unwrap())
        }
    }
}

fn generate_pdf_document(template: &ReportTemplate, output_path: &str) -> Result<u64, String> {
    // Create PDF document
    let (doc, page1, layer1) = PdfDocument::new(
        &template.metadata.title.clone().unwrap_or_else(|| "Financial Report".to_string()),
        Mm(210.0), // A4 width
        Mm(297.0), // A4 height
        "Layer 1",
    );

    let current_layer = doc.get_page(page1).get_layer(layer1);

    // Built-in font
    let font = doc.add_builtin_font(BuiltinFont::Helvetica).map_err(|e| format!("Font error: {}", e))?;
    let font_bold = doc.add_builtin_font(BuiltinFont::HelveticaBold).map_err(|e| format!("Font error: {}", e))?;

    let mut y_position = 270.0; // Start from top (A4 height is 297mm)
    let margin_left = 20.0;
    let margin_right = 190.0; // 210 - 20
    let line_height = 7.0;

    // Title
    if let Some(title) = &template.metadata.title {
        current_layer.use_text(
            title,
            24.0,
            Mm(margin_left),
            Mm(y_position),
            &font_bold,
        );
        y_position -= 12.0;
    }

    // Company
    if let Some(company) = &template.metadata.company {
        current_layer.use_text(
            company,
            14.0,
            Mm(margin_left),
            Mm(y_position),
            &font,
        );
        y_position -= 8.0;
    }

    // Metadata (Author and Date)
    let mut metadata_line = String::new();
    if let Some(author) = &template.metadata.author {
        metadata_line.push_str(&format!("Author: {} ", author));
    }
    if let Some(date) = &template.metadata.date {
        if !metadata_line.is_empty() {
            metadata_line.push_str("| ");
        }
        metadata_line.push_str(&format!("Date: {}", date));
    }
    if !metadata_line.is_empty() {
        current_layer.use_text(
            &metadata_line,
            10.0,
            Mm(margin_left),
            Mm(y_position),
            &font,
        );
        y_position -= 10.0;
    }

    // Separator line
    let line_points = vec![
        (Point::new(Mm(margin_left), Mm(y_position)), false),
        (Point::new(Mm(margin_right), Mm(y_position)), false),
    ];
    let line = Line {
        points: line_points,
        is_closed: false,
    };
    current_layer.set_outline_thickness(0.5);
    current_layer.add_line(line);
    y_position -= 10.0;

    // Process components
    for component in &template.components {
        // Check if we need a new page
        if y_position < 30.0 {
            let (new_page, new_layer) = doc.add_page(Mm(210.0), Mm(297.0), "Layer 1");
            let _current_layer = doc.get_page(new_page).get_layer(new_layer);
            y_position = 270.0;
        }

        match component.component_type.as_str() {
            "heading" => {
                if let Some(content) = &component.content {
                    if !content.is_empty() {
                        current_layer.use_text(
                            content,
                            18.0,
                            Mm(margin_left),
                            Mm(y_position),
                            &font_bold,
                        );
                        y_position -= line_height * 2.0;
                    }
                }
            }
            "text" => {
                if let Some(content) = &component.content {
                    if !content.is_empty() {
                        // Word wrap for long text
                        let wrapped_lines = wrap_text(content, 85); // ~85 chars per line for A4
                        for line in wrapped_lines {
                            if y_position < 30.0 {
                                let (new_page, new_layer) = doc.add_page(Mm(210.0), Mm(297.0), "Layer 1");
                                let _current_layer = doc.get_page(new_page).get_layer(new_layer);
                                y_position = 270.0;
                            }
                            current_layer.use_text(
                                &line,
                                11.0,
                                Mm(margin_left),
                                Mm(y_position),
                                &font,
                            );
                            y_position -= line_height;
                        }
                        y_position -= 3.0; // Extra space after paragraph
                    }
                }
            }
            "divider" => {
                y_position -= 5.0;
                let line_points = vec![
                    (Point::new(Mm(margin_left), Mm(y_position)), false),
                    (Point::new(Mm(margin_right), Mm(y_position)), false),
                ];
                let line = Line {
                    points: line_points,
                    is_closed: false,
                };
                current_layer.set_outline_thickness(0.5);
                current_layer.add_line(line);
                y_position -= 5.0;
            }
            "code" => {
                if let Some(content) = &component.content {
                    if !content.is_empty() {
                        y_position -= 3.0;
                        // Background rectangle for code block
                        let _rect = Rect::new(
                            Mm(margin_left - 2.0),
                            Mm(y_position - (content.lines().count() as f32 * 5.0) + 2.0),
                            Mm(margin_right - margin_left + 4.0),
                            Mm(content.lines().count() as f32 * 5.0 + 4.0),
                        );
                        // TODO: Add gray background (requires setting fill color)

                        for line in content.lines() {
                            if y_position < 30.0 {
                                let (new_page, new_layer) = doc.add_page(Mm(210.0), Mm(297.0), "Layer 1");
                                let _current_layer = doc.get_page(new_page).get_layer(new_layer);
                                y_position = 270.0;
                            }
                            current_layer.use_text(
                                line,
                                9.0,
                                Mm(margin_left),
                                Mm(y_position),
                                &font,
                            );
                            y_position -= 5.0;
                        }
                        y_position -= 3.0;
                    }
                }
            }
            "table" => {
                // Simple table rendering
                current_layer.use_text(
                    "[Table component - detailed tables coming soon]",
                    10.0,
                    Mm(margin_left),
                    Mm(y_position),
                    &font,
                );
                y_position -= line_height * 2.0;
            }
            "chart" => {
                current_layer.use_text(
                    "[Chart component - charts coming soon]",
                    10.0,
                    Mm(margin_left),
                    Mm(y_position),
                    &font,
                );
                y_position -= line_height * 2.0;
            }
            "image" => {
                current_layer.use_text(
                    "[Image component - images coming soon]",
                    10.0,
                    Mm(margin_left),
                    Mm(y_position),
                    &font,
                );
                y_position -= line_height * 2.0;
            }
            _ => {}
        }
    }

    // Save PDF
    let file = File::create(output_path).map_err(|e| format!("Failed to create file: {}", e))?;
    let mut writer = BufWriter::new(file);
    doc.save(&mut writer).map_err(|e| format!("Failed to save PDF: {}", e))?;

    // Get file size
    let metadata = std::fs::metadata(output_path).map_err(|e| format!("Failed to get file metadata: {}", e))?;
    Ok(metadata.len())
}

fn wrap_text(text: &str, max_width: usize) -> Vec<String> {
    let mut lines = Vec::new();
    let mut current_line = String::new();

    for word in text.split_whitespace() {
        if current_line.len() + word.len() + 1 > max_width {
            if !current_line.is_empty() {
                lines.push(current_line.trim().to_string());
                current_line = String::new();
            }
        }
        if !current_line.is_empty() {
            current_line.push(' ');
        }
        current_line.push_str(word);
    }

    if !current_line.is_empty() {
        lines.push(current_line.trim().to_string());
    }

    if lines.is_empty() {
        lines.push(String::new());
    }

    lines
}

/// Generate HTML from report template (kept for compatibility)
#[tauri::command]
pub async fn generate_report_html(
    template_json: String,
) -> Result<String, String> {
    // Simple HTML generation as fallback
    let template: ReportTemplate = serde_json::from_str(&template_json)
        .map_err(|e| format!("Failed to parse template: {}", e))?;

    let mut html = format!(
        r#"<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>{}</title>
    <style>
        body {{ font-family: Arial, sans-serif; max-width: 800px; margin: 40px auto; padding: 20px; }}
        h1 {{ color: #0066cc; border-bottom: 3px solid #0066cc; padding-bottom: 10px; }}
        h2 {{ color: #0066cc; margin-top: 30px; }}
        .metadata {{ color: #666; font-size: 0.9em; }}
        pre {{ background: #f5f5f5; padding: 15px; border-left: 4px solid #0066cc; }}
    </style>
</head>
<body>
    <h1>{}</h1>
"#,
        template.metadata.title.as_deref().unwrap_or("Report"),
        template.metadata.title.as_deref().unwrap_or("Report")
    );

    if let Some(company) = &template.metadata.company {
        html.push_str(&format!("<p><strong>{}</strong></p>", company));
    }

    let mut meta_parts = Vec::new();
    if let Some(author) = &template.metadata.author {
        meta_parts.push(format!("Author: {}", author));
    }
    if let Some(date) = &template.metadata.date {
        meta_parts.push(format!("Date: {}", date));
    }
    if !meta_parts.is_empty() {
        html.push_str(&format!("<p class=\"metadata\">{}</p>", meta_parts.join(" | ")));
    }

    for component in &template.components {
        match component.component_type.as_str() {
            "heading" => {
                if let Some(content) = &component.content {
                    html.push_str(&format!("<h2>{}</h2>", html_escape(content)));
                }
            }
            "text" => {
                if let Some(content) = &component.content {
                    html.push_str(&format!("<p>{}</p>", html_escape(content)));
                }
            }
            "divider" => html.push_str("<hr>"),
            "code" => {
                if let Some(content) = &component.content {
                    html.push_str(&format!("<pre><code>{}</code></pre>", html_escape(content)));
                }
            }
            _ => {}
        }
    }

    html.push_str("</body></html>");

    let result = serde_json::json!({
        "success": true,
        "html": html,
        "error": null
    });

    Ok(serde_json::to_string(&result).unwrap())
}

fn html_escape(s: &str) -> String {
    s.replace('&', "&amp;")
        .replace('<', "&lt;")
        .replace('>', "&gt;")
        .replace('"', "&quot;")
        .replace('\'', "&#39;")
}

/// Create default template (kept for compatibility)
#[tauri::command]
pub async fn create_default_report_template() -> Result<String, String> {
    let result = serde_json::json!({
        "success": true,
        "message": "Using built-in Rust PDF generator",
        "template_path": null
    });
    Ok(serde_json::to_string(&result).unwrap())
}

/// Open folder in file explorer
#[tauri::command]
pub async fn open_folder(path: String) -> Result<(), String> {
    #[cfg(target_os = "windows")]
    {
        std::process::Command::new("explorer")
            .arg(&path)
            .spawn()
            .map_err(|e| format!("Failed to open folder: {}", e))?;
    }

    #[cfg(target_os = "macos")]
    {
        std::process::Command::new("open")
            .arg(&path)
            .spawn()
            .map_err(|e| format!("Failed to open folder: {}", e))?;
    }

    #[cfg(target_os = "linux")]
    {
        std::process::Command::new("xdg-open")
            .arg(&path)
            .spawn()
            .map_err(|e| format!("Failed to open folder: {}", e))?;
    }

    Ok(())
}
