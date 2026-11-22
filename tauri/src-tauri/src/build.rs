// Port of nfo-*.ts, encoding.ts, archiver.ts, staging.ts, external-tool.ts, stale-drafts.ts and the
// buildRelease/previewOutputs handlers (app/src/main). Output must stay byte-identical to the TypeScript.
//
// Lengths and padding follow JavaScript (UTF-16 code units), see jslen/pad_end.

use serde::Deserialize;
use serde_json::{json, Map, Value};
use std::fs;
use std::path::{Path, PathBuf};
use tauri::{Emitter, Manager};

// ---------------------------------------------------------------- types

#[derive(Deserialize, Clone, Default, Debug)]
#[serde(default)]
pub struct Member {
    pub nickname: String,
    pub role: String,
}

#[derive(Deserialize, Clone, Default, Debug)]
#[serde(default)]
pub struct Credit {
    pub role: String,
    pub name: String,
}

#[derive(Deserialize, Clone, Default, Debug)]
#[serde(default)]
pub struct Board {
    pub name: String,
    pub sysop: String,
    pub hq: String,
    pub status: String,
}

#[derive(Deserialize, Clone, Default, Debug)]
#[serde(default, rename_all = "camelCase")]
pub struct HistoryEntry {
    pub release_number: String,
    pub filenames: String,
    pub title: String,
    pub category: String,
    pub creators: String,
}

#[derive(Deserialize, Clone, Default, Debug)]
#[serde(default, rename_all = "camelCase")]
pub struct Form {
    pub profile_id: String,
    pub release_number_text: Option<String>,
    pub code: String,
    pub format: String,
    pub release_name: String,
    pub platform_tag: String,
    pub release_notes_text: String,
    pub news_text: String,
    pub diz_banner_text: String,
    pub members: Vec<Member>,
    pub entry_title: String,
    pub entry_category: String,
    pub entry_creators: String,
    pub selected_paths: Vec<String>,
    pub description: String,
    pub credits: Vec<Credit>,
    pub boards: Option<Vec<Board>>,
    pub release_date: Option<String>,
    pub coder: Option<String>,
    pub system: Option<String>,
    pub design: Option<String>,
}

#[derive(Clone, Debug, PartialEq)]
pub enum Renderer {
    UpRough,
    XInnovations,
    Placeholder,
}

/// What the build needs of a profile. TODO(Batch A): replace load_profile with the profiles.rs loader.
#[derive(Clone, Debug)]
pub struct Profile {
    pub id: String,
    pub label: String,
    pub url: String,
    pub file_prefix: String,
    pub renderer: Renderer,
    pub doorserver: bool,
}

#[derive(Debug)]
pub struct Generated {
    pub nfo_text: Option<String>,
    pub diz_text: String,
    pub release_number: i64,
}

pub struct Templates {
    pub nfo: Option<String>,
    pub diz: String,
}

pub struct ReleaseListEntry {
    pub display_number: String,
    pub art: String,
}

// ---------------------------------------------------------------- JS-compatible string helpers

fn jslen(s: &str) -> usize {
    s.encode_utf16().count()
}

fn pad_end(s: &str, width: usize) -> String {
    format!("{}{}", s, " ".repeat(width.saturating_sub(jslen(s))))
}

fn pad_start(s: &str, width: usize) -> String {
    format!("{}{}", " ".repeat(width.saturating_sub(jslen(s))), s)
}

/// text.slice(0, n)
fn slice_units(s: &str, n: usize) -> String {
    let mut out = String::new();
    let mut used = 0;
    for c in s.chars() {
        used += c.len_utf16();
        if used > n {
            break;
        }
        out.push(c);
    }
    out
}

fn trim_edge_blanks(lines: &mut Vec<String>) {
    while lines.first().is_some_and(|l| l.is_empty()) {
        lines.remove(0);
    }
    while lines.last().is_some_and(|l| l.is_empty()) {
        lines.pop();
    }
}

fn normalise_line_endings(text: &str) -> String {
    text.replace("\r\n", "\n").replace('\r', "\n")
}

fn latin1_to_string(bytes: &[u8]) -> String {
    bytes.iter().map(|&b| b as char).collect()
}

fn wrap_words(text: &str, first_width: usize, cont_width: usize) -> Vec<String> {
    let mut lines: Vec<String> = Vec::new();
    let mut current = String::new();
    let mut width = first_width;
    for word in text.split_whitespace() {
        let candidate = if current.is_empty() { word.to_string() } else { format!("{current} {word}") };
        if jslen(&candidate) > width && !current.is_empty() {
            lines.push(std::mem::take(&mut current));
            current = word.to_string();
            width = cont_width;
        } else {
            current = candidate;
        }
    }
    if !current.is_empty() {
        lines.push(current);
    }
    if lines.is_empty() {
        vec![String::new()]
    } else {
        lines
    }
}

// ---------------------------------------------------------------- encoding.ts

fn to_ascii_safe(input: &str) -> String {
    let mut out = String::new();
    for c in input.chars() {
        if (c as u32) <= 0x7f {
            out.push(c);
            continue;
        }
        out.push_str(match c {
            'ä' => "ae",
            'ö' => "oe",
            'å' => "aa",
            'ü' => "ue",
            'é' | 'è' | 'ê' => "e",
            'Ä' => "Ae",
            'Ö' => "Oe",
            'Å' => "Aa",
            'Ü' => "Ue",
            'É' => "E",
            'ñ' => "n",
            'ç' => "c",
            'ø' => "oe",
            'Ø' => "Oe",
            'æ' => "ae",
            'Æ' => "Ae",
            _ => "?",
        });
    }
    out
}

fn assert_latin1_safe(text: &str) -> Result<(), String> {
    let mut position = 0;
    for c in text.chars() {
        if (c as u32) > 0xff {
            return Err(format!(
                "Text contains \"{c}\" (U+{:04X}) at character {position}, which is outside the Latin-1 range \
(0-255) and would be silently corrupted if written. Replace it with a plain Latin-1 character \
(e.g. a straight quote ' instead of a curly one) and try again.",
                c as u32
            ));
        }
        position += c.len_utf16();
    }
    Ok(())
}

fn write_latin1(path: &Path, text: &str) -> Result<(), String> {
    assert_latin1_safe(text)?;
    let bytes: Vec<u8> = text.chars().map(|c| c as u8).collect();
    fs::write(path, bytes).map_err(|e| format!("{}: {e}", path.display()))
}

// ---------------------------------------------------------------- naming.ts

fn release_base_name(prefix: &str, code: &str) -> String {
    format!("{prefix}{}", code.to_uppercase())
}

fn archive_file_name(prefix: &str, code: &str, format: &str) -> String {
    format!("{}.{}", release_base_name(prefix, code), if format == "LHA" { "LHA" } else { "ZIP" })
}

// ---------------------------------------------------------------- nfo-generator.ts

const MEMBER_LABEL_MIN_WIDTH: usize = 20;
const HISTORY_LABEL_FIELD_WIDTH: usize = 12;
const HISTORY_BODY_INDENT: usize = 8;
const HISTORY_CONTINUATION_INDENT: usize = HISTORY_BODY_INDENT + HISTORY_LABEL_FIELD_WIDTH;
const HISTORY_WRAP_WIDTH: usize = 68;

const REL_NOTES_HEADING: &str = "        [ r e l e a s e   n o t e s ]";
const NEWS_HEADING: &str = "        [ n e w s ]";
const MEMBERS_HEADING: &str = "        a c t i v e   u p   r o u g h   h e r o e s";
const HISTORY_HEADING: &str = "        r e l e a s e d   s o   f a r";
const ONLINE_LINE: &str = "        online         www.uprough.net";
const FOOTER_LINE: &str = "        c a t c h `e m   a l l   a t   www.uprough.net";
const BANNER: &str = include_str!("uprough_banner.txt");

fn indent_block(text: &str) -> String {
    text.split('\n')
        .map(|l| if l.is_empty() { String::new() } else { format!("{}{l}", " ".repeat(HISTORY_BODY_INDENT)) })
        .collect::<Vec<_>>()
        .join("\n")
}

fn render_member_block(members: &[Member]) -> String {
    let pad_width = members.iter().map(|m| jslen(&m.nickname) + 2).fold(MEMBER_LABEL_MIN_WIDTH, usize::max);
    members
        .iter()
        .map(|m| format!("{}{}{}", " ".repeat(HISTORY_BODY_INDENT), pad_end(&m.nickname, pad_width), m.role))
        .collect::<Vec<_>>()
        .join("\n")
}

fn render_history_entry(entry: &HistoryEntry) -> Vec<String> {
    let indent = " ".repeat(HISTORY_BODY_INDENT);
    let content_width = HISTORY_WRAP_WIDTH - HISTORY_CONTINUATION_INDENT;
    let mut lines = vec![format!("{indent}release {} {}", entry.release_number, entry.filenames)];
    for (label, value) in [("title", &entry.title), ("category", &entry.category), ("creator(s)", &entry.creators)] {
        let wrapped = wrap_words(value, content_width, content_width);
        lines.push(format!("{indent}{}{}", pad_end(label, HISTORY_LABEL_FIELD_WIDTH), wrapped[0]));
        for w in &wrapped[1..] {
            lines.push(format!("{}{w}", " ".repeat(HISTORY_CONTINUATION_INDENT)));
        }
    }
    lines
}

fn render_history_block(history: &[HistoryEntry]) -> String {
    history.iter().map(|e| render_history_entry(e).join("\n")).collect::<Vec<_>>().join("\n\n")
}

fn render_nfo(prefix: &str, history: &[HistoryEntry], form: &Form) -> String {
    let platform = if form.platform_tag.is_empty() { String::new() } else { format!(" ({})", form.platform_tag) };
    let release_notes_line = format!(
        "Release named {}{platform} ({})",
        form.release_name,
        archive_file_name(prefix, &form.code, &form.format)
    );
    let release_date = form.release_date.as_deref().map(str::trim).unwrap_or("");
    let release_date_line =
        if release_date.is_empty() { None } else { Some(format!("{}Released {release_date}", " ".repeat(HISTORY_BODY_INDENT))) };

    let mut parts: Vec<String> = vec![
        BANNER.to_string(),
        String::new(),
        REL_NOTES_HEADING.to_string(),
        String::new(),
        format!("{}{release_notes_line}", " ".repeat(HISTORY_BODY_INDENT)),
    ];
    if let Some(l) = release_date_line {
        parts.push(l);
    }
    parts.extend([
        String::new(),
        indent_block(form.release_notes_text.trim()),
        String::new(),
        NEWS_HEADING.to_string(),
        String::new(),
        indent_block(form.news_text.trim()),
        String::new(),
        MEMBERS_HEADING.to_string(),
        String::new(),
        render_member_block(&form.members),
        String::new(),
        String::new(),
        ONLINE_LINE.to_string(),
        String::new(),
        String::new(),
        HISTORY_HEADING.to_string(),
        String::new(),
        render_history_block(history),
        String::new(),
        String::new(),
        FOOTER_LINE.to_string(),
        String::new(),
    ]);
    parts.join("\n")
}

fn trim_diz_banner(text: &str) -> String {
    let mut lines: Vec<String> = text.split('\n').map(|l| l.trim_end_matches([' ', '\t']).to_string()).collect();
    trim_edge_blanks(&mut lines);
    lines.join("\n")
}

fn render_diz(form: &Form) -> String {
    to_ascii_safe(&trim_diz_banner(&form.diz_banner_text)) + "\n"
}

fn generate_outputs(
    profile: &Profile,
    history: &[HistoryEntry],
    release_number: i64,
    form: &Form,
    release_list: &[ReleaseListEntry],
    templates: Option<&Templates>,
) -> Result<Generated, String> {
    match profile.renderer {
        Renderer::Placeholder => {
            let t = templates
                .ok_or_else(|| format!("{}: the placeholder renderer needs the profile's template files", profile.label))?;
            render_placeholder_outputs(profile, t, form)
        }
        Renderer::XInnovations => Ok(Generated {
            nfo_text: Some(render_x_innovations_nfo(&XInput {
                release_name: &form.entry_title,
                release_date: form.release_date.as_deref().unwrap_or(""),
                coder: form.coder.as_deref().unwrap_or(""),
                system: form.system.as_deref().unwrap_or(""),
                design: form.design.as_deref().unwrap_or(""),
                release_notes: &form.release_notes_text,
                news: &form.news_text,
                members: &form.members,
                boards: form.boards.as_deref().unwrap_or(&[]),
                release_list,
            })),
            diz_text: render_diz(form),
            release_number,
        }),
        Renderer::UpRough => Ok(Generated {
            nfo_text: Some(render_nfo(&profile.file_prefix, history, form)),
            diz_text: render_diz(form),
            release_number,
        }),
    }
}

// ---------------------------------------------------------------- placeholder-template.ts

const SCALAR_NAMES: [&str; 11] =
    ["group", "url", "code", "filename", "format", "release_name", "platform", "date", "release_number", "description", "creators"];
const BLOCK_NAMES: [&str; 4] = ["notes", "news", "members", "credits"];

struct TemplateValues {
    scalars: Vec<(&'static str, String)>,
    blocks: Vec<(&'static str, Vec<String>)>,
}

impl TemplateValues {
    fn scalar(&self, name: &str) -> &str {
        self.scalars.iter().find(|(n, _)| *n == name).map(|(_, v)| v.as_str()).unwrap_or("")
    }
    fn block(&self, name: &str) -> &[String] {
        self.blocks.iter().find(|(n, _)| *n == name).map(|(_, v)| v.as_slice()).unwrap_or(&[])
    }
}

/// 1-based line number and message.
type TemplateError = (usize, String);

struct Marker {
    start: usize,
    end: usize,
    name: String,
    spaces: String,
}

/// Every match of /\{\{([^{}\s]+)( *)\}\}/g in `text`.
fn find_markers(text: &str) -> Vec<Marker> {
    let mut out = Vec::new();
    let mut i = 0;
    while i < text.len() {
        if text[i..].starts_with("{{") {
            let name_start = i + 2;
            let name_len: usize = text[name_start..]
                .chars()
                .take_while(|c| !matches!(c, '{' | '}') && !c.is_whitespace())
                .map(char::len_utf8)
                .sum();
            let after_name = name_start + name_len;
            let spaces = text[after_name..].bytes().take_while(|&b| b == b' ').count();
            let close = after_name + spaces;
            if name_len > 0 && text[close..].starts_with("}}") {
                out.push(Marker {
                    start: i,
                    end: close + 2,
                    name: text[name_start..after_name].to_string(),
                    spaces: " ".repeat(spaces),
                });
                i = close + 2;
                continue;
            }
        }
        i += text[i..].chars().next().map_or(1, char::len_utf8);
    }
    out
}

fn fit(line: usize, name: &str, spaces: &str, marker: &str, value: &str) -> Result<String, TemplateError> {
    if spaces.is_empty() {
        return Ok(value.to_string());
    }
    let width = jslen(marker);
    if jslen(value) > width {
        return Err((line, format!("value for {name} is {} characters, field is {width}", jslen(value))));
    }
    Ok(pad_end(value, width))
}

fn frame_only(s: &str) -> bool {
    !s.chars().any(|c| c.is_ascii_alphanumeric())
}

fn render_template_line(text: &str, line: usize, values: &TemplateValues) -> Result<Vec<String>, TemplateError> {
    let markers = find_markers(text);
    for m in &markers {
        if !SCALAR_NAMES.contains(&m.name.as_str()) && !BLOCK_NAMES.contains(&m.name.as_str()) {
            return Err((line, format!("unknown placeholder {{{{{}}}}}", m.name)));
        }
    }
    if let Some(block) = markers.iter().find(|m| BLOCK_NAMES.contains(&m.name.as_str())) {
        let prefix = &text[..block.start];
        let suffix = &text[block.end..];
        if markers.len() > 1 || !frame_only(prefix) || !frame_only(suffix) {
            return Err((line, format!("block placeholder {{{{{}}}}} must stand alone on its line", block.name)));
        }
        let marker = &text[block.start..block.end];
        return values
            .block(&block.name)
            .iter()
            .map(|value| {
                let framed = format!("{prefix}{}{suffix}", fit(line, &block.name, &block.spaces, marker, value)?);
                Ok(if value.is_empty() { framed.trim_end().to_string() } else { framed })
            })
            .collect();
    }
    let mut out = String::new();
    let mut last = 0;
    for m in &markers {
        out.push_str(&text[last..m.start]);
        out.push_str(&fit(line, &m.name, &m.spaces, &text[m.start..m.end], values.scalar(&m.name))?);
        last = m.end;
    }
    out.push_str(&text[last..]);
    Ok(vec![out])
}

fn render_template(template: &str, values: &TemplateValues) -> Result<String, TemplateError> {
    let mut lines = Vec::new();
    for (i, text) in template.split('\n').enumerate() {
        lines.extend(render_template_line(text, i + 1, values)?);
    }
    Ok(lines.join("\n"))
}

fn template_problems(template: &str, values: &TemplateValues) -> Vec<String> {
    template
        .split('\n')
        .enumerate()
        .filter_map(|(i, text)| render_template_line(text, i + 1, values).err())
        .map(|(line, msg)| format!("line {line}: {msg}"))
        .collect()
}

// ---------------------------------------------------------------- nfo-placeholder.ts

const TEMPLATE_NFO: &str = "release.nfo";
const TEMPLATE_DIZ: &str = "file_id.diz";

fn text_lines(text: &str) -> Vec<String> {
    let mut lines: Vec<String> = normalise_line_endings(text).split('\n').map(|l| l.trim_end().to_string()).collect();
    trim_edge_blanks(&mut lines);
    lines
}

fn placeholder_values(profile: &Profile, form: &Form) -> TemplateValues {
    let width = form.members.iter().map(|m| jslen(&m.nickname)).max().unwrap_or(0) + 2;
    TemplateValues {
        scalars: vec![
            ("group", profile.label.clone()),
            ("url", profile.url.clone()),
            ("code", form.code.to_uppercase()),
            ("filename", archive_file_name(&profile.file_prefix, &form.code, &form.format)),
            ("format", form.format.clone()),
            ("release_name", form.release_name.clone()),
            ("platform", form.platform_tag.clone()),
            ("date", form.release_date.clone().unwrap_or_default()),
            ("release_number", form.release_number_text.clone().unwrap_or_default()),
            ("description", form.description.clone()),
            ("creators", form.entry_creators.clone()),
        ],
        blocks: vec![
            ("notes", text_lines(&form.release_notes_text)),
            ("news", text_lines(&form.news_text)),
            ("members", form.members.iter().map(|m| format!("{}{}", pad_end(&m.nickname, width), m.role)).collect()),
            (
                "credits",
                form.credits
                    .iter()
                    .filter(|c| !c.role.trim().is_empty() || !c.name.trim().is_empty())
                    .map(|c| format!("{}: {}", c.role.trim(), c.name.trim()))
                    .collect(),
            ),
        ],
    }
}

fn render_file(file: &str, template: &str, values: &TemplateValues) -> Result<String, String> {
    let text = render_template(template, values).map_err(|(line, msg)| format!("{file} line {line}: {msg}"))?;
    Ok(if text.ends_with('\n') { text } else { text + "\n" })
}

fn render_placeholder_outputs(profile: &Profile, templates: &Templates, form: &Form) -> Result<Generated, String> {
    let values = placeholder_values(profile, form);
    Ok(Generated {
        nfo_text: match &templates.nfo {
            None => None,
            Some(t) => Some(render_file(TEMPLATE_NFO, t, &values)?),
        },
        diz_text: render_file(TEMPLATE_DIZ, &templates.diz, &values)?,
        release_number: 0,
    })
}

fn placeholder_problems(profile: &Profile, templates: &Templates, form: &Form) -> Vec<String> {
    let values = placeholder_values(profile, form);
    let of = |file: &str, template: Option<&str>| -> Vec<String> {
        template.map_or(vec![], |t| template_problems(t, &values).into_iter().map(|p| format!("{file} {p}")).collect())
    };
    let mut out = of(TEMPLATE_NFO, templates.nfo.as_deref());
    out.extend(of(TEMPLATE_DIZ, Some(&templates.diz)));
    out
}

// ---------------------------------------------------------------- nfo-x-innovations.ts

const X_TEMPLATE: &str = include_str!("x_innovations_template.txt");
const GUTTER_LEFT: &str = "   | %% |";
const GUTTER_RIGHT: &str = "| %% |";
const FIELD_WIDTH: usize = 59;
const FIELD_FIT_WIDTH: usize = 10;
const BOARD_NAME_COL: usize = 2;
const BOARD_SYSOP_COL: usize = 22;
const BOARD_HQ_END: usize = 36;
const BOARD_STATUS_END: usize = 56;
const LIST_ART_INDENT: usize = 7;

fn frame_line(text: &str) -> String {
    format!("{GUTTER_LEFT}{}{GUTTER_RIGHT}", slice_units(&pad_end(text, FIELD_WIDTH), FIELD_WIDTH))
}

fn entry_rule() -> String {
    format!("- -| %% |{}| %% |- -", "-".repeat(FIELD_WIDTH))
}

/// Left-heavy centring of the release name and news lines.
fn centre_left_heavy(text: &str) -> String {
    let text = slice_units(text, FIELD_WIDTH);
    let pad = FIELD_WIDTH - jslen(&text);
    let left = pad.div_ceil(2);
    format!("{}{text}{}", " ".repeat(left), " ".repeat(pad - left))
}

fn release_name_line(release_name: &str) -> String {
    frame_line(&centre_left_heavy(&format!("<<-- {release_name} -->>")))
}

fn wrap_notes(notes: &str) -> Vec<String> {
    let width = FIELD_WIDTH - 1;
    let mut out = Vec::new();
    for raw in notes.replace("\r\n", "\n").split('\n') {
        let line = raw.trim_end();
        if jslen(line) <= width {
            out.push(line.to_string());
            continue;
        }
        let indent: String = line.chars().take_while(|c| c.is_whitespace()).collect();
        let mut current = String::new();
        for word in line.trim().split_whitespace() {
            let candidate = if current.is_empty() { format!("{indent}{word}") } else { format!("{current} {word}") };
            if jslen(&candidate) > width && !current.is_empty() {
                out.push(std::mem::take(&mut current));
                current = format!("{indent}{word}");
            } else {
                current = candidate;
            }
        }
        if !current.is_empty() {
            out.push(current);
        }
    }
    out
}

fn notes_block(notes: &str) -> Vec<String> {
    wrap_notes(notes).iter().map(|l| frame_line(&format!(" {l}"))).collect()
}

fn member_skills(role: &str) -> Vec<String> {
    role.split(',').map(str::trim).filter(|s| !s.is_empty()).map(String::from).collect()
}

fn spaced_caps(role: &str) -> String {
    role.trim()
        .chars()
        .enumerate()
        .map(|(i, c)| {
            let lower = c.to_lowercase().to_string();
            if i == 0 || lower == "i" {
                lower
            } else {
                c.to_uppercase().to_string()
            }
        })
        .collect::<Vec<_>>()
        .join(" ")
}

fn roster_block(members: &[Member]) -> Vec<String> {
    let mut groups: Vec<(String, Vec<String>)> = Vec::new();
    for member in members {
        for skill in member_skills(&member.role) {
            match groups.iter_mut().find(|(k, _)| *k == skill) {
                Some((_, names)) => names.push(member.nickname.clone()),
                None => groups.push((skill, vec![member.nickname.clone()])),
            }
        }
    }
    let label_width = groups.iter().map(|(r, _)| jslen(&spaced_caps(r))).max().unwrap_or(0);
    let mut out = Vec::new();
    for (i, (role, names)) in groups.iter().enumerate() {
        if i > 0 {
            out.push(frame_line(""));
        }
        let label = format!("  {} >> ", pad_start(&spaced_caps(role), label_width));
        let continuation = " ".repeat(jslen(&label));
        let mut line = label;
        for (j, name) in names.iter().enumerate() {
            let piece = if j == 0 { name.clone() } else { format!(" . {name}") };
            if jslen(&format!("{line}{piece}")) > FIELD_WIDTH - 1 {
                out.push(frame_line(&line));
                line = format!("{continuation}{name}");
            } else {
                line.push_str(&piece);
            }
        }
        out.push(frame_line(&line));
    }
    out
}

fn dots_between(from: usize, to: usize) -> String {
    if to > from {
        ".".repeat(to - from)
    } else {
        String::new()
    }
}

fn boards_block(boards: &[Board]) -> Vec<String> {
    boards
        .iter()
        .map(|b| {
            let mut line = format!("{}{} ", " ".repeat(BOARD_NAME_COL), b.name);
            line = format!("{line}{} ", dots_between(jslen(&line), BOARD_SYSOP_COL - 1));
            line = format!("{line}{} ", b.sysop);
            let hq_start = BOARD_HQ_END.saturating_sub(jslen(&b.hq));
            line = format!("{line}{} {} ", dots_between(jslen(&line), hq_start.saturating_sub(1)), b.hq);
            let status_start = BOARD_STATUS_END.saturating_sub(jslen(&b.status));
            line = format!("{line}{} {}", dots_between(jslen(&line), status_start.saturating_sub(1)), b.status);
            frame_line(&line)
        })
        .collect()
}

fn release_list_block(entries: &[ReleaseListEntry]) -> Vec<String> {
    let mut out = Vec::new();
    for e in entries {
        out.push(entry_rule());
        out.push(frame_line(""));
        out.push(frame_line(&format!("  {}:", e.display_number)));
        for art_line in e.art.split('\n') {
            out.push(frame_line(&format!("{}{art_line}", " ".repeat(LIST_ART_INDENT))));
        }
        out.push(frame_line(""));
        out.push(frame_line(""));
    }
    out.push(entry_rule());
    out.push(frame_line(""));
    out.push(frame_line(""));
    out
}

fn news_block(news: &str) -> Vec<String> {
    let mut lines: Vec<String> = news.replace("\r\n", "\n").split('\n').map(|l| l.trim().to_string()).collect();
    trim_edge_blanks(&mut lines);
    if lines.is_empty() {
        return vec![];
    }
    let mut out = vec![frame_line(""), frame_line("")];
    out.extend(lines.iter().map(|l| frame_line(&centre_left_heavy(l))));
    out.push(frame_line(""));
    out.push(frame_line(""));
    out
}

struct XInput<'a> {
    release_name: &'a str,
    release_date: &'a str,
    coder: &'a str,
    system: &'a str,
    design: &'a str,
    release_notes: &'a str,
    news: &'a str,
    members: &'a [Member],
    boards: &'a [Board],
    release_list: &'a [ReleaseListEntry],
}

fn fit_field(value: &str, width: usize) -> String {
    if jslen(value) >= width {
        slice_units(value, width)
    } else {
        pad_end(value, width)
    }
}

/// An empty block drops the marker's line (`^MARKER\n?` per line, first match only).
fn fill_block(template: &str, marker: &str, lines: &[String]) -> String {
    if !lines.is_empty() {
        return template.replacen(marker, &lines.join("\n"), 1);
    }
    let at_line_start = template.match_indices(marker).map(|(i, _)| i).find(|&i| i == 0 || template.as_bytes()[i - 1] == b'\n');
    match at_line_start {
        None => template.to_string(),
        Some(i) => {
            let mut end = i + marker.len();
            if template[end..].starts_with('\n') {
                end += 1;
            }
            format!("{}{}", &template[..i], &template[end..])
        }
    }
}

fn render_x_innovations_nfo(input: &XInput) -> String {
    let filled = X_TEMPLATE
        .replacen("{{RELEASE_NAME_LINE}}", &release_name_line(input.release_name), 1)
        .replacen("{{DATE}}", &fit_field(input.release_date, FIELD_FIT_WIDTH), 1)
        .replacen("{{CODER}}", &fit_field(input.coder, FIELD_FIT_WIDTH), 1)
        .replacen("{{SYSTEM}}", &fit_field(input.system, FIELD_FIT_WIDTH), 1)
        .replacen("{{DESIGN}}", &fit_field(input.design, FIELD_FIT_WIDTH), 1);
    let blocks: [(&str, Vec<String>); 5] = [
        ("{{NOTES}}", notes_block(input.release_notes)),
        ("{{NEWS}}", news_block(input.news)),
        ("{{ROSTER}}", roster_block(input.members)),
        ("{{BOARDS}}", boards_block(input.boards)),
        ("{{RELEASE_LIST}}", release_list_block(input.release_list)),
    ];
    blocks.iter().fold(filled, |art, (marker, lines)| fill_block(&art, marker, lines))
}

// ---------------------------------------------------------------- numbered history (Batch C boundary)

pub struct Numbered {
    pub release_number: i64,
    pub history: Vec<HistoryEntry>,
}

/// What a release server provides. NoneBackend matches backend-none.ts; the doorserver backend is Batch C.
pub trait NumberedBackend {
    fn create(&self, form: &Form) -> Result<Numbered, String>;
    fn preview(&self, form: &Form) -> Result<Numbered, String>;
    fn release_list(&self, form: &Form, release_number: i64) -> Result<Vec<ReleaseListEntry>, String>;
    /// The door-repository submission, if the backend does one.
    fn after_build(&self, archive_path: &Path) -> Result<Option<Value>, String>;
}

struct NoneBackend;

impl NumberedBackend for NoneBackend {
    fn create(&self, _: &Form) -> Result<Numbered, String> {
        Ok(Numbered { release_number: 0, history: vec![] })
    }
    fn preview(&self, _: &Form) -> Result<Numbered, String> {
        Ok(Numbered { release_number: 0, history: vec![] })
    }
    fn release_list(&self, _: &Form, _: i64) -> Result<Vec<ReleaseListEntry>, String> {
        Ok(vec![])
    }
    fn after_build(&self, _: &Path) -> Result<Option<Value>, String> {
        Ok(None)
    }
}

/// TODO(Batch C): replace with the releases-doorserver client.
struct ServerBackend;

const NOT_PORTED_SERVER: &str = "not ported: server backend";

impl NumberedBackend for ServerBackend {
    fn create(&self, _: &Form) -> Result<Numbered, String> {
        Err(NOT_PORTED_SERVER.into())
    }
    fn preview(&self, _: &Form) -> Result<Numbered, String> {
        Err(NOT_PORTED_SERVER.into())
    }
    fn release_list(&self, _: &Form, _: i64) -> Result<Vec<ReleaseListEntry>, String> {
        Err(NOT_PORTED_SERVER.into())
    }
    fn after_build(&self, _: &Path) -> Result<Option<Value>, String> {
        Err(NOT_PORTED_SERVER.into())
    }
}

fn backend_for(profile: &Profile) -> Box<dyn NumberedBackend> {
    if profile.doorserver {
        Box::new(ServerBackend)
    } else {
        Box::new(NoneBackend)
    }
}

// ---------------------------------------------------------------- stale-drafts.ts

pub const STALE_DRAFT_AGE_SECONDS: i64 = 24 * 60 * 60;

#[derive(Clone, Debug)]
pub struct DraftEntry {
    pub code: String,
    pub release_number: i64,
    pub created_at: i64,
    pub entry_title: String,
}

fn describe_age(seconds: i64) -> String {
    let days = seconds.div_euclid(86400);
    if days >= 2 {
        format!("{days} days ago")
    } else {
        "yesterday".to_string()
    }
}

// Used by the server backend once Batch C lands.
#[allow(dead_code)]
pub fn describe_stale_drafts(drafts: &[DraftEntry], now_seconds: i64) -> Option<String> {
    let mut stale: Vec<&DraftEntry> = drafts.iter().filter(|d| now_seconds - d.created_at >= STALE_DRAFT_AGE_SECONDS).collect();
    if stale.is_empty() {
        return None;
    }
    stale.sort_by_key(|d| d.release_number);
    let listed = stale
        .iter()
        .map(|d| format!("{} (release {}, built {})", d.code, d.release_number, describe_age(now_seconds - d.created_at)))
        .collect::<Vec<_>>()
        .join(", ");
    let one = stale.len() == 1;
    Some(format!(
        "{} never published and never revoked: {listed}. Nobody can see {}, but {} spent. Publish or revoke to get {} back.",
        if one { "An earlier build was" } else { "Earlier builds were" },
        if one { "it" } else { "them" },
        if one { "its release number is" } else { "their release numbers are" },
        if one { "the number" } else { "the numbers" },
    ))
}

// ---------------------------------------------------------------- external-tool.ts, archiver.ts

/// Runs `bin args` in `cwd`. Injected so tests can stand in for lha/zip.
pub type Runner<'a> = &'a dyn Fn(&str, &[String], &Path) -> Result<(), String>;

fn install_hint() -> &'static str {
    if cfg!(target_os = "macos") {
        "install it with \"brew install lha\" (or \"brew install zip\")"
    } else if cfg!(target_os = "linux") {
        "install it with your package manager, e.g. \"sudo apt install lhasa zip\""
    } else {
        "install it and add it to your PATH"
    }
}

/// GUI-launched apps inherit a minimal PATH; append the usual install locations.
fn path_with_extras() -> std::ffi::OsString {
    let extra: &[&str] = if cfg!(target_os = "macos") {
        &["/opt/homebrew/bin", "/usr/local/bin", "/opt/local/bin"]
    } else if cfg!(target_os = "linux") {
        &["/usr/local/bin", "/usr/bin", "/bin"]
    } else {
        &[]
    };
    let mut paths: Vec<PathBuf> = std::env::var_os("PATH").map(|p| std::env::split_paths(&p).collect()).unwrap_or_default();
    paths.extend(extra.iter().map(PathBuf::from));
    std::env::join_paths(paths).unwrap_or_default()
}

fn run_external(bin: &str, args: &[String], cwd: &Path) -> Result<(), String> {
    let output = std::process::Command::new(bin)
        .args(args)
        .current_dir(cwd)
        .env("PATH", path_with_extras())
        .output()
        .map_err(|e| {
            if e.kind() == std::io::ErrorKind::NotFound {
                format!("\"{bin}\" was not found on your PATH — {}.", install_hint())
            } else {
                e.to_string()
            }
        })?;
    if output.status.success() {
        return Ok(());
    }
    Err(format!("Command failed: {bin} {}\n{}", args.join(" "), String::from_utf8_lossy(&output.stderr)))
}

fn pack_archive(prefix: &str, outer_dir: &Path, code: &str, format: &str, root: &Path, run: Runner) -> Result<PathBuf, String> {
    let final_archive = root.join(archive_file_name(prefix, code, format));
    let release_dir = release_base_name(prefix, code);
    let archive = final_archive.to_string_lossy().to_string();
    if format == "LHA" {
        run("lha", &["a".into(), archive, "file_id.diz".into(), release_dir], outer_dir)?;
    } else {
        run("zip", &["-r".into(), archive, "file_id.diz".into(), release_dir], outer_dir)?;
    }
    Ok(final_archive)
}

fn finalize_release(prefix: &str, outer_dir: &Path, code: &str, root: &Path) -> Result<PathBuf, String> {
    let release_dir = release_base_name(prefix, code);
    let final_dir = root.join(&release_dir);
    if final_dir.exists() {
        return Err(format!(
            "{release_dir} already exists in {} — archive was written, but the staged folder was not moved into place",
            root.display()
        ));
    }
    move_dir(outer_dir, &final_dir)?;
    Ok(final_dir)
}

// ---------------------------------------------------------------- fs-helpers.ts, staging.ts

fn copy_recursive(src: &Path, dest: &Path) -> Result<(), String> {
    let err = |e: std::io::Error| format!("{}: {e}", src.display());
    if src.is_dir() {
        fs::create_dir_all(dest).map_err(err)?;
        for entry in fs::read_dir(src).map_err(err)? {
            let entry = entry.map_err(err)?;
            copy_recursive(&entry.path(), &dest.join(entry.file_name()))?;
        }
    } else {
        fs::copy(src, dest).map_err(err)?;
    }
    Ok(())
}

fn move_dir(src: &Path, dest: &Path) -> Result<(), String> {
    match fs::rename(src, dest) {
        Ok(()) => Ok(()),
        // EXDEV is 18 on unix, ERROR_NOT_SAME_DEVICE is 17 on Windows.
        Err(e) if e.raw_os_error() == Some(if cfg!(windows) { 17 } else { 18 }) => {
            copy_recursive(src, dest)?;
            fs::remove_dir_all(src).map_err(|e| format!("{}: {e}", src.display()))
        }
        Err(e) => Err(format!("{}: {e}", src.display())),
    }
}

fn remove_path(p: &Path) -> Result<(), String> {
    let result = if p.is_dir() { fs::remove_dir_all(p) } else { fs::remove_file(p) };
    match result {
        Err(e) if e.kind() != std::io::ErrorKind::NotFound => Err(format!("{}: {e}", p.display())),
        _ => Ok(()),
    }
}

fn make_temp_dir() -> Result<PathBuf, String> {
    let nanos = std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map_or(0, |d| d.as_nanos());
    for attempt in 0..100u32 {
        let dir = std::env::temp_dir().join(format!("makerelease-{:x}{attempt}", nanos ^ u128::from(std::process::id())));
        match fs::create_dir(&dir) {
            Ok(()) => return Ok(dir),
            Err(e) if e.kind() == std::io::ErrorKind::AlreadyExists => continue,
            Err(e) => return Err(format!("{}: {e}", dir.display())),
        }
    }
    Err("could not create a temporary folder".into())
}

fn stage_release(prefix: &str, form: &Form, generated: &Generated) -> Result<PathBuf, String> {
    let tmp_root = make_temp_dir()?;
    let release_dir = release_base_name(prefix, &form.code);
    let outer_dir = tmp_root.join(&release_dir);
    let inner_dir = outer_dir.join(&release_dir);
    fs::create_dir_all(&inner_dir).map_err(|e| format!("{}: {e}", inner_dir.display()))?;

    write_latin1(&outer_dir.join("file_id.diz"), &generated.diz_text)?;
    if let Some(nfo) = &generated.nfo_text {
        write_latin1(&inner_dir.join(format!("{release_dir}.NFO")), nfo)?;
    }
    for src in &form.selected_paths {
        let src = Path::new(src);
        let base = src.file_name().ok_or_else(|| format!("{}: not a file or folder path", src.display()))?;
        // A folder already named <PREFIX><CODE> is the release folder itself: unwrap it.
        if base.to_string_lossy().to_uppercase() == release_dir.to_uppercase() && src.is_dir() {
            for entry in fs::read_dir(src).map_err(|e| format!("{}: {e}", src.display()))? {
                let entry = entry.map_err(|e| format!("{}: {e}", src.display()))?;
                copy_recursive(&entry.path(), &inner_dir.join(entry.file_name()))?;
            }
            continue;
        }
        copy_recursive(src, &inner_dir.join(base))?;
    }
    Ok(outer_dir)
}

// ---------------------------------------------------------------- profiles (TODO Batch A: use profiles.rs)

fn load_profile(profiles_dir: &Path, id: &str) -> Result<Profile, String> {
    if id.is_empty() || !id.bytes().all(|b| b.is_ascii_lowercase() || b.is_ascii_digit() || b == b'-') {
        return Err(format!("\"{id}\" is not a valid profile id"));
    }
    let file = profiles_dir.join(id).join("profile.json");
    let bytes = fs::read(&file).map_err(|e| format!("{}: could not be read ({e})", file.display()))?;
    let text = String::from_utf8_lossy(&bytes);
    let v: Value = serde_json::from_str(text.trim_start_matches('\u{feff}')).map_err(|e| format!("{}: {e}", file.display()))?;
    let s = |key: &str| v.get(key).and_then(Value::as_str).unwrap_or("").to_string();
    let renderer = match s("renderer").as_str() {
        "uprough" => Renderer::UpRough,
        "xinnovations" => Renderer::XInnovations,
        "placeholder" => Renderer::Placeholder,
        other => return Err(format!("{}: unknown renderer \"{other}\"", file.display())),
    };
    Ok(Profile {
        id: id.to_string(),
        label: s("label"),
        url: s("url"),
        file_prefix: s("filePrefix"),
        renderer,
        doorserver: s("backend") == "releases-doorserver",
    })
}

fn read_template_file(file: &Path, optional: bool) -> Result<Option<String>, String> {
    match fs::read(file) {
        Ok(bytes) => Ok(Some(normalise_line_endings(&latin1_to_string(&bytes)))),
        Err(e) if optional && e.kind() == std::io::ErrorKind::NotFound => Ok(None),
        Err(e) => Err(format!("{}: could not be read ({e})", file.display())),
    }
}

fn read_templates(profiles_dir: &Path, id: &str) -> Result<Templates, String> {
    let dir = profiles_dir.join(id);
    let nfo = read_template_file(&dir.join(TEMPLATE_NFO), true)?;
    let diz = read_template_file(&dir.join(TEMPLATE_DIZ), false)?.unwrap_or_default();
    Ok(Templates { nfo, diz })
}

// ---------------------------------------------------------------- ipc-handlers.ts: buildRelease, previewOutputs

const DRY_RUN_DIR: &str = "dry-run";

/// Everything the handlers take from the host, so tests can run them without Tauri, lha or zip.
pub struct Env<'a> {
    pub profiles_dir: &'a Path,
    pub output_root: &'a dyn Fn(&Profile) -> PathBuf,
    pub run: Runner<'a>,
    pub progress: &'a dyn Fn(&str),
}

/// The division follows from the profile, never from what the window sent.
fn form_for(profile: &Profile, sent: &Value) -> Result<Form, String> {
    let mut map: Map<String, Value> = sent.as_object().cloned().unwrap_or_default();
    map.remove("division");
    if profile.renderer != Renderer::Placeholder {
        let division = if profile.renderer == Renderer::UpRough { "uprough" } else { "xinnovations" };
        map.insert("division".into(), json!(division));
    }
    serde_json::from_value(Value::Object(map)).map_err(|e| format!("invalid release form: {e}"))
}

fn sent_profile_id(sent: &Value) -> &str {
    sent.get("profileId").and_then(Value::as_str).unwrap_or("")
}

fn assert_local_code_available(profile: &Profile, root: &Path, code: &str) -> Result<(), String> {
    let release_dir = release_base_name(&profile.file_prefix, code);
    let candidates = [
        root.join(&release_dir),
        root.join(archive_file_name(&profile.file_prefix, code, "LHA")),
        root.join(archive_file_name(&profile.file_prefix, code, "ZIP")),
    ];
    for candidate in candidates {
        if candidate.exists() {
            return Err(format!(
                "{release_dir} already exists locally at {} — refusing to build. Remove or rename the leftover file/folder from a previous attempt, or choose a different code.",
                candidate.display()
            ));
        }
    }
    Ok(())
}

pub fn build_release_core(env: &Env, sent: &Value, dry_run: bool) -> Result<Value, String> {
    (env.progress)("Preparing the build");
    let profile = load_profile(env.profiles_dir, sent_profile_id(sent))?;
    let form = form_for(&profile, sent)?;
    let profile_root = (env.output_root)(&profile);
    let output_root = if dry_run { profile_root.join(DRY_RUN_DIR) } else { profile_root.clone() };
    fs::create_dir_all(&output_root).map_err(|e| format!("{}: {e}", output_root.display()))?;
    let prefix = &profile.file_prefix;
    if dry_run {
        // A preview is disposable: the newest replaces the last.
        remove_path(&output_root.join(release_base_name(prefix, &form.code)))?;
        remove_path(&output_root.join(archive_file_name(prefix, &form.code, &form.format)))?;
    } else {
        assert_local_code_available(&profile, &profile_root, &form.code)?;
    }
    let backend = backend_for(&profile);
    (env.progress)(if dry_run { "Previewing the release number" } else { "Contacting the release server" });
    let numbered = if dry_run { backend.preview(&form)? } else { backend.create(&form)? };
    let release_list = backend.release_list(&form, numbered.release_number)?;

    (env.progress)("Writing NFO and diz");
    let templates = if profile.renderer == Renderer::Placeholder { Some(read_templates(env.profiles_dir, &profile.id)?) } else { None };
    let generated = generate_outputs(&profile, &numbered.history, numbered.release_number, &form, &release_list, templates.as_ref())?;
    (env.progress)("Staging files");
    let outer_dir = stage_release(prefix, &form, &generated)?;
    (env.progress)("Packing archive (can take a while for large releases)");
    pack_archive(prefix, &outer_dir, &form.code, &form.format, &output_root, env.run)?;
    finalize_release(prefix, &outer_dir, &form.code, &output_root)?;
    let archive_path = output_root.join(archive_file_name(prefix, &form.code, &form.format));

    let door_submission = if dry_run {
        None
    } else {
        (env.progress)("Submitting to the door repository");
        backend.after_build(&archive_path)?
    };
    let mut result = json!({
        "archivePath": archive_path.to_string_lossy(),
        "releaseNumber": numbered.release_number,
        "isDryRun": dry_run,
    });
    if let Some(d) = door_submission {
        result["doorSubmission"] = d;
    }
    Ok(result)
}

pub fn preview_outputs_core(env: &Env, sent: &Value) -> Result<Value, String> {
    let profile = load_profile(env.profiles_dir, sent_profile_id(sent))?;
    let form = form_for(&profile, sent)?;
    if profile.renderer == Renderer::Placeholder {
        let templates = read_templates(env.profiles_dir, &profile.id)?;
        let problems = placeholder_problems(&profile, &templates, &form);
        let has_nfo_template = templates.nfo.is_some();
        if !problems.is_empty() {
            return Ok(json!({ "dizText": "", "nfoText": null, "hasNfoTemplate": has_nfo_template, "problems": problems }));
        }
        let out = render_placeholder_outputs(&profile, &templates, &form)?;
        return Ok(json!({ "dizText": out.diz_text, "nfoText": out.nfo_text, "hasNfoTemplate": has_nfo_template, "problems": problems }));
    }
    let mut problems: Vec<String> = Vec::new();
    let mut numbered = Numbered { release_number: 0, history: vec![] };
    let mut release_list: Vec<ReleaseListEntry> = vec![];
    let backend = backend_for(&profile);
    let fetched = backend.preview(&form).and_then(|n| backend.release_list(&form, n.release_number).map(|l| (n, l)));
    match fetched {
        Ok((n, l)) => {
            numbered = n;
            release_list = l;
        }
        Err(msg) => problems.push(format!("Release history unavailable: {msg}")),
    }
    match generate_outputs(&profile, &numbered.history, numbered.release_number, &form, &release_list, None) {
        Ok(out) => Ok(json!({ "dizText": out.diz_text, "nfoText": out.nfo_text, "hasNfoTemplate": true, "problems": problems })),
        Err(msg) => {
            problems.push(msg);
            Ok(json!({ "dizText": "", "nfoText": null, "hasNfoTemplate": true, "problems": problems }))
        }
    }
}

// ---------------------------------------------------------------- Tauri commands

/// TODO(Batch A): ask profiles.rs for its profiles folder; this mirrors the Electron userData location.
fn profiles_dir(app: &tauri::AppHandle) -> Result<PathBuf, String> {
    if let Some(dir) = std::env::var_os("MAKERELEASE_PROFILES_DIR") {
        return Ok(PathBuf::from(dir));
    }
    Ok(app.path().app_data_dir().map_err(|e| e.to_string())?.join("profiles"))
}

/// TODO(Batch A): align with profiles.rs. Packaged Electron output: <Documents>/<label> Releases.
fn output_root(app: &tauri::AppHandle, profile: &Profile) -> PathBuf {
    let base = app
        .path()
        .document_dir()
        .or_else(|_| app.path().home_dir())
        .or_else(|_| app.path().app_data_dir())
        .unwrap_or_else(|_| PathBuf::from("."));
    base.join(format!("{} Releases", profile.label))
}

#[tauri::command]
pub async fn build_release(app: tauri::AppHandle, form: Value, dry_run: bool) -> Result<Value, String> {
    tauri::async_runtime::spawn_blocking(move || {
        let dir = profiles_dir(&app)?;
        let root = |p: &Profile| output_root(&app, p);
        let progress = |stage: &str| {
            let _ = app.emit("build-progress", json!({ "stage": stage }));
        };
        let env = Env { profiles_dir: &dir, output_root: &root, run: &run_external, progress: &progress };
        build_release_core(&env, &form, dry_run)
    })
    .await
    .map_err(|e| e.to_string())?
}

#[tauri::command]
pub async fn preview_outputs(app: tauri::AppHandle, form: Value) -> Result<Value, String> {
    tauri::async_runtime::spawn_blocking(move || {
        let dir = profiles_dir(&app)?;
        let root = |p: &Profile| output_root(&app, p);
        let env = Env { profiles_dir: &dir, output_root: &root, run: &run_external, progress: &|_| {} };
        preview_outputs_core(&env, &form)
    })
    .await
    .map_err(|e| e.to_string())?
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::RefCell;

    fn fixture(name: &str) -> String {
        let p = format!("{}/../../app/test/fixtures/{name}", env!("CARGO_MANIFEST_DIR"));
        latin1_to_string(&fs::read(&p).unwrap_or_else(|e| panic!("{p}: {e}")))
    }

    fn testdata(name: &str) -> String {
        let p = format!("{}/testdata/{name}", env!("CARGO_MANIFEST_DIR"));
        latin1_to_string(&fs::read(&p).unwrap_or_else(|e| panic!("{p}: {e}")))
    }

    fn scratch(tag: &str) -> PathBuf {
        use std::sync::atomic::{AtomicUsize, Ordering};
        static N: AtomicUsize = AtomicUsize::new(0);
        let nanos = std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).unwrap().as_nanos();
        let d = std::env::temp_dir().join(format!("mr-test-{tag}-{}-{nanos}-{}", std::process::id(), N.fetch_add(1, Ordering::SeqCst)));
        fs::create_dir_all(&d).unwrap();
        d
    }

    fn member(n: &str, r: &str) -> Member {
        Member { nickname: n.into(), role: r.into() }
    }

    fn hist(n: &str, f: &str, t: &str, c: &str, cr: &str) -> HistoryEntry {
        HistoryEntry { release_number: n.into(), filenames: f.into(), title: t.into(), category: c.into(), creators: cr.into() }
    }

    fn ur_profile() -> Profile {
        Profile {
            id: "uprough".into(),
            label: "Up Rough".into(),
            url: "https://uprough.net".into(),
            file_prefix: "UP-".into(),
            renderer: Renderer::UpRough,
            doorserver: false,
        }
    }

    fn ex_profile() -> Profile {
        Profile {
            id: "example".into(),
            label: "Example Group".into(),
            url: "https://example.net".into(),
            file_prefix: "EX-".into(),
            renderer: Renderer::Placeholder,
            doorserver: false,
        }
    }

    // ---- encoding

    #[test]
    fn ascii_safe() {
        assert_eq!(to_ascii_safe("Fjälldata"), "Fjaelldata");
        assert_eq!(to_ascii_safe("Pågadata"), "Paagadata");
        assert_eq!(to_ascii_safe("über"), "ueber");
        assert_eq!(to_ascii_safe("plain"), "plain");
        assert_eq!(to_ascii_safe("日本語"), "???");
    }

    #[test]
    fn latin1_guard() {
        assert!(assert_latin1_safe("Fjälldata Pågadata über").is_ok());
        let e = assert_latin1_safe("it\u{2019}s here").unwrap_err();
        assert!(e.contains("U+2019") && e.contains("character 2"), "{e}");
        for c in ['\u{2018}', '\u{2019}', '\u{201c}', '\u{201d}', '\u{2013}', '\u{2014}'] {
            assert!(assert_latin1_safe(&format!("text {c} text")).is_err());
        }
        let d = scratch("l1");
        let f = d.join("a.txt");
        write_latin1(&f, "Fjälldata Pågadata åäö").unwrap();
        assert_eq!(latin1_to_string(&fs::read(&f).unwrap()), "Fjälldata Pågadata åäö");
        let g = d.join("b.txt");
        assert!(write_latin1(&g, "it\u{2019}s").is_err());
        assert!(!g.exists());
        fs::remove_dir_all(d).unwrap();
    }

    // ---- nfo-generator

    #[test]
    fn member_block() {
        let b = render_member_block(&[
            member("the soul collector", "ink"),
            member("spot", "leader, ink, stereofonik development"),
            member("qwan", "co-leader, stereofonik development"),
        ]);
        let l: Vec<&str> = b.split('\n').collect();
        assert_eq!(l[0], "        the soul collector  ink");
        assert_eq!(l[1], "        spot                leader, ink, stereofonik development");
        assert_eq!(render_member_block(&[member("ab", "x")]), "        ab                  x");
    }

    #[test]
    fn history_wrap_uses_20_space_continuation() {
        let e = hist("208", "up-fb006.lha", "t", "intro (fjälldata 2023)", &"word ".repeat(20));
        let lines = render_history_entry(&e);
        assert_eq!(lines[0], "        release 208 up-fb006.lha");
        assert_eq!(lines[2], "        category    intro (fjälldata 2023)");
        let cont: Vec<&String> = lines.iter().filter(|l| l.starts_with(&" ".repeat(20)) && !l.starts_with(&" ".repeat(21))).collect();
        assert!(!cont.is_empty());
        assert!(lines.iter().all(|l| jslen(l) <= 8 + 12 + 48));
    }

    #[test]
    fn diz_trim() {
        let mut f = Form { diz_banner_text: "   /\\   \n  /__\\  ".into(), ..Default::default() };
        assert_eq!(render_diz(&f), "   /\\\n  /__\\\n");
        f.diz_banner_text = "\n  art   \n     \n\n".into();
        assert_eq!(render_diz(&f), "  art\n");
    }

    #[test]
    fn empty_history_keeps_heading() {
        let f = Form { code: "x".into(), format: "LHA".into(), ..Default::default() };
        let g = generate_outputs(&ur_profile(), &[], 42, &f, &[], None).unwrap();
        let nfo = g.nfo_text.unwrap();
        assert!(!nfo.contains("release 42"));
        assert!(nfo.contains("r e l e a s e d   s o   f a r"));
        assert_eq!(g.release_number, 42);
    }

    #[test]
    fn ur_golden() {
        let diz = fixture("UP-GOLD.DIZ");
        let form = Form {
            code: "gold1".into(),
            format: "LHA".into(),
            release_name: "Hypocrisy".into(),
            platform_tag: "A500".into(),
            release_notes_text: "A demo for the Amiga 500 with a hard drive.\n\nReleased at Computer Space 2025.".into(),
            news_text: "Two new members joined this year.".into(),
            members: vec![member("Spot", "code"), member("Hoffman", "music")],
            entry_title: "Hypocrisy".into(),
            entry_category: "demo".into(),
            entry_creators: "Spot, Hoffman".into(),
            release_date: Some("2025-10-18".into()),
            description: "A500 HDD demo".into(),
            diz_banner_text: diz.strip_suffix('\n').unwrap_or(&diz).to_string(),
            ..Default::default()
        };
        let history = vec![
            hist("218", "up-gold1.lha", "Hypocrisy", "demo", "Spot, Hoffman"),
            hist(
                "217",
                "up-evoke.lha",
                "Evoke invitation",
                "intro",
                "Spot, Hoffman, Lazy, Zalo, Rasmus, Mixer, Juice, Pinchy, Dr Doom, Antichrist and everyone else at the party place",
            ),
            hist("216", "up-md15.lha", "More Doors Cursor Menu", "tool", "Zalo"),
        ];
        let g = generate_outputs(&ur_profile(), &history, 218, &form, &[], None).unwrap();
        assert_eq!(g.nfo_text.unwrap(), fixture("UP-GOLD.NFO"));
        assert_eq!(g.diz_text, diz);
    }

    // ---- placeholder

    fn ex_form() -> Form {
        Form {
            code: "test1".into(),
            format: "LHA".into(),
            release_name: "Demo Disk".into(),
            platform_tag: "A500".into(),
            release_date: Some("2026-10-17".into()),
            release_number_text: Some("007".into()),
            description: "A demo disk released at the party.".into(),
            release_notes_text: "\n\nFirst line of notes.   \r\nSecond line, $& kept.\n\n".into(),
            news_text: String::new(),
            members: vec![member("coder", "code"), member("pixelartist", "graphics")],
            entry_creators: "coder and pixelartist".into(),
            credits: vec![
                Credit { role: "Code".into(), name: "coder".into() },
                Credit { role: " ".into(), name: String::new() },
                Credit { role: "Graphics".into(), name: "pixelartist".into() },
            ],
            ..Default::default()
        }
    }

    fn ex_templates() -> Templates {
        Templates {
            nfo: Some(normalise_line_endings(&testdata("ex-release.nfo.template"))),
            diz: normalise_line_endings(&testdata("ex-file_id.diz.template")),
        }
    }

    #[test]
    fn ex_golden() {
        let g = render_placeholder_outputs(&ex_profile(), &ex_templates(), &ex_form()).unwrap();
        assert_eq!(g.nfo_text.as_deref(), Some(fixture("EX-TEST1.NFO").as_str()));
        assert_eq!(g.diz_text, fixture("EX-TEST1.DIZ"));
        assert_eq!(g.release_number, 0);
        assert!(g.diz_text.lines().filter(|l| l.starts_with('|')).all(|l| jslen(l) == 46));
        let t = ex_templates().nfo.unwrap();
        for n in SCALAR_NAMES.iter().chain(BLOCK_NAMES.iter()) {
            assert!(t.contains(&format!("{{{{{n}")), "template lacks {n}");
        }
        let none = Templates { nfo: None, diz: ex_templates().diz };
        let g2 = render_placeholder_outputs(&ex_profile(), &none, &ex_form()).unwrap();
        assert!(g2.nfo_text.is_none());
        assert_eq!(g2.diz_text, g.diz_text);
    }

    #[test]
    fn placeholder_misc() {
        let f = ex_form();
        let t = |s: &str| Templates { nfo: None, diz: s.into() };
        assert_eq!(render_placeholder_outputs(&ex_profile(), &t("{{group}}"), &f).unwrap().diz_text, "Example Group\n");
        assert_eq!(render_placeholder_outputs(&ex_profile(), &t("{{code}}"), &f).unwrap().diz_text, "TEST1\n");
        let e = render_placeholder_outputs(&ex_profile(), &Templates { nfo: Some("ok\n{{bogus}}".into()), diz: "x".into() }, &f).unwrap_err();
        assert_eq!(e, "release.nfo line 2: unknown placeholder {{bogus}}");
        let e = render_placeholder_outputs(&ex_profile(), &t("{{group }}"), &f).unwrap_err();
        assert_eq!(e, "file_id.diz line 1: value for group is 13 characters, field is 10");
        let v = placeholder_values(&ex_profile(), &f);
        assert_eq!(v.block("notes"), ["First line of notes.", "Second line, $& kept."]);
        assert!(v.block("news").is_empty());
        assert_eq!(v.block("members"), ["coder        code", "pixelartist  graphics"]);
        assert_eq!(v.block("credits"), ["Code: coder", "Graphics: pixelartist"]);
        assert_eq!(v.scalar("code"), "TEST1");
        assert_eq!(v.scalar("filename"), "EX-TEST1.LHA");
        let bare = placeholder_values(&ex_profile(), &Form::default());
        assert_eq!((bare.scalar("date"), bare.scalar("release_number")), ("", ""));
        let p = placeholder_problems(&ex_profile(), &Templates { nfo: Some("{{bogus}}".into()), diz: "x\n{{group }}".into() }, &f);
        assert_eq!(p, ["release.nfo line 1: unknown placeholder {{bogus}}", "file_id.diz line 2: value for group is 13 characters, field is 10"]);
        assert!(placeholder_problems(&ex_profile(), &t("{{code}}"), &f).is_empty());
    }

    // ---- /X renderer

    fn x_members() -> Vec<Member> {
        let mut m = Vec::new();
        for n in ["sANDMAN", "zALO", "hAMLET", "sPOT", "zANER"] {
            m.push(member(n, "sysops"));
        }
        for n in ["sPOT", "zALO"] {
            m.push(member(n, "coders"));
        }
        for n in ["gOTO80", "diNO", "diPSWiTCH", "zANER", "sKOPE", "dMG", "mORTiMER tWANG", "oTRO", "sIXX", "zORRO", "SPOT"] {
            m.push(member(n, "artists"));
        }
        m
    }

    fn x_boards() -> Vec<Board> {
        let b = |n: &str, s: &str, h: &str, st: &str| Board { name: n.into(), sysop: s.into(), hq: h.into(), status: st.into() };
        vec![
            b("uPTOWN", "sPOT", "wEBHQ", "SOON"),
            b("diGiTAL hiGHWAY", "zALO", "sHQ", "ASK4IT"),
            b("tHE yARD", "zANER", "gHQ", "tEMP-dOWN"),
            b("fiRST diViSiON", "hAMLET", "sHQ", "1337-ONLY"),
            b("sANCTUARY", "sANDMAN", "sHQ", "ASK4ACCS"),
        ]
    }

    fn x_release_list(fx: &[&str]) -> Vec<ReleaseListEntry> {
        let mut entries: Vec<(String, Vec<String>)> = Vec::new();
        for line in &fx[137..255] {
            let cs: Vec<char> = line.chars().collect();
            if cs.len() < 74 || !line.starts_with("   | %% |") {
                continue;
            }
            let field: String = cs[9..68].iter().collect();
            let t = field.trim_end();
            if let Some(rest) = t.strip_prefix("  oo") {
                if let Some(d) = rest.strip_suffix(':').filter(|d| d.len() == 1 && d.as_bytes()[0].is_ascii_digit()) {
                    entries.push((format!("oo{d}"), vec![]));
                    continue;
                }
            }
            if !t.trim().is_empty() {
                if let Some(cur) = entries.last_mut() {
                    cur.1.push(field.chars().skip(7).collect::<String>().trim_end().to_string());
                }
            }
        }
        entries.into_iter().map(|(d, art)| ReleaseListEntry { display_number: d, art: art.join("\n") }).collect()
    }

    #[test]
    fn x_golden() {
        let fx_text = fixture("UP-MD15.NFO");
        let fx: Vec<&str> = fx_text.split('\n').collect();
        let notes = fx[38..87].iter().map(|l| l.chars().skip(10).take(58).collect::<String>().trim_end().to_string()).collect::<Vec<_>>().join("\n");
        let list = x_release_list(&fx);
        let members = x_members();
        let boards = x_boards();
        let out = render_x_innovations_nfo(&XInput {
            release_name: "mORE dOORS 1.5",
            release_date: "2026-06-20",
            coder: "zALO/uP!..",
            system: "aMiEXPRESS",
            design: "diNO/uP!..",
            release_notes: &notes,
            news: "iF yOU bREAK iT, fIX iT!",
            members: &members,
            boards: &boards,
            release_list: &list,
        });
        let lines: Vec<&str> = out.split('\n').collect();
        assert_eq!(lines.len(), fx.len());
        assert!(lines.iter().filter(|l| l.starts_with("   | %% |")).all(|l| jslen(l) == 74));
        for i in [25, 28, 29] {
            assert_eq!(lines[i], fx[i], "line {i}");
        }
        for n in [103, 120, 135, 106, 108, 110, 111, 112, 122, 123, 124, 125, 126] {
            assert!(lines.contains(&fx[n]), "fixture line {n} missing");
        }
        for d in 1..=7 {
            assert!(lines.iter().any(|l| l.contains(&format!("  oo{d}:"))), "oo{d}");
        }
        let diff: Vec<usize> = (0..lines.len()).filter(|&i| lines[i] != fx[i]).collect();
        assert_eq!(diff, vec![35]);
        assert_eq!(jslen(fx[35]), 283);
        assert_eq!(jslen(lines[35]), 74);
    }

    #[test]
    fn x_helpers() {
        let fx_text = fixture("UP-MD15.NFO");
        let fx: Vec<&str> = fx_text.split('\n').collect();
        assert_eq!(frame_line(""), fx[36]);
        assert_eq!(entry_rule(), fx[137]);
        let long = frame_line(&"x".repeat(200));
        assert_eq!(jslen(&long), 74);
        assert!(long.ends_with("| %% |"));
        assert_eq!(release_name_line("mORE dOORS 1.5"), fx[25]);
        assert_eq!(spaced_caps("sysops"), "s Y S O P S");
        assert_eq!(spaced_caps("artists"), "a R T i S T S");
        let r = roster_block(&x_members());
        assert_eq!(r[0], fx[106]);
        assert!(r.iter().all(|l| jslen(l) == 74));
        let multi = roster_block(&[member("zALO", "sysops, coders"), member("sPOT", "coders")]);
        assert!(multi.iter().any(|l| l.contains("c O D E R S >> zALO . sPOT")));
        let w = wrap_notes(&format!("  {}\nshort kept", "word ".repeat(30)));
        assert!(w.iter().all(|l| jslen(l) <= 58));
        assert!(w[0].starts_with("  word") && w.last().unwrap() == "short kept");
    }

    // ---- staging

    fn stage(form_paths: Vec<String>, nfo: Option<&str>) -> PathBuf {
        let form = Form { code: "evk26".into(), format: "LHA".into(), selected_paths: form_paths, ..Default::default() };
        let g = Generated { nfo_text: nfo.map(String::from), diz_text: "diz text".into(), release_number: 0 };
        stage_release("UP-", &form, &g).unwrap()
    }

    fn tree(root: &Path) -> Vec<String> {
        fn walk(d: &Path, base: &Path, out: &mut Vec<String>) {
            for e in fs::read_dir(d).unwrap() {
                let p = e.unwrap().path();
                let rel = p.strip_prefix(base).unwrap().to_string_lossy().to_string();
                if p.is_dir() {
                    out.push(format!("{rel}/"));
                    walk(&p, base, out);
                } else {
                    out.push(rel);
                }
            }
        }
        let mut v = Vec::new();
        walk(root, root, &mut v);
        v.sort();
        v
    }

    #[test]
    fn staging() {
        let src = scratch("stage");
        let make = |name: &str| {
            let d = src.join(name);
            fs::create_dir_all(d.join("music")).unwrap();
            fs::write(d.join("dip-ev26.ans"), "a").unwrap();
            fs::write(d.join("music/varin0x-onmymind.mod"), "m").unwrap();
            d.to_string_lossy().to_string()
        };
        let outer = stage(vec![make("UP-EVK26")], Some("nfo text"));
        let t = tree(&outer);
        for want in ["UP-EVK26/", "UP-EVK26/UP-EVK26.NFO", "UP-EVK26/dip-ev26.ans", "UP-EVK26/music/", "UP-EVK26/music/varin0x-onmymind.mod", "file_id.diz"] {
            assert!(t.contains(&want.to_string()), "{want} in {t:?}");
        }
        assert_eq!(fs::read_to_string(outer.join("file_id.diz")).unwrap(), "diz text");
        fs::remove_dir_all(outer.parent().unwrap()).unwrap();

        let lower = src.join("l");
        fs::create_dir_all(lower.join("up-evk26")).unwrap();
        fs::write(lower.join("up-evk26/a.txt"), "a").unwrap();
        let outer = stage(vec![lower.join("up-evk26").to_string_lossy().to_string()], None);
        let t = tree(&outer);
        assert!(t.contains(&"UP-EVK26/a.txt".to_string()) && !t.contains(&"UP-EVK26/up-evk26/".to_string()));
        assert!(!t.contains(&"UP-EVK26/UP-EVK26.NFO".to_string()));
        fs::remove_dir_all(outer.parent().unwrap()).unwrap();

        let gfx = src.join("gfx");
        fs::create_dir_all(&gfx).unwrap();
        fs::write(gfx.join("dip-fast.png"), "p").unwrap();
        let loose = src.join("dip-ev26.ans");
        fs::write(&loose, "a").unwrap();
        let outer = stage(vec![gfx.to_string_lossy().to_string(), loose.to_string_lossy().to_string()], None);
        let t = tree(&outer);
        assert!(t.contains(&"UP-EVK26/gfx/dip-fast.png".to_string()) && t.contains(&"UP-EVK26/dip-ev26.ans".to_string()));
        fs::remove_dir_all(outer.parent().unwrap()).unwrap();
        fs::remove_dir_all(src).unwrap();
    }

    // ---- stale drafts

    #[test]
    fn stale_drafts() {
        let now = 1_787_700_000;
        let d = |code: &str, n: i64, age: i64| DraftEntry { code: code.into(), release_number: n, created_at: now - age, entry_title: String::new() };
        assert!(describe_stale_drafts(&[], now).is_none());
        assert!(describe_stale_drafts(&[d("A", 1, 60), d("B", 2, 86399)], now).is_none());
        let m = describe_stale_drafts(&[d("EVK26", 217, 3 * 86400)], now).unwrap();
        assert!(m.contains("EVK26") && m.contains("release 217") && m.contains("3 days ago") && m.to_lowercase().contains("publish or revoke"));
        assert!(describe_stale_drafts(&[d("X", 1, 86400)], now).unwrap().contains("yesterday"));
        let m = describe_stale_drafts(&[d("EVO26", 218, 2 * 86400), d("EVK26", 217, 2 * 86400)], now).unwrap();
        assert!(m.find("EVK26").unwrap() < m.find("EVO26").unwrap() && m.contains("Earlier builds were"));
    }

    // ---- handlers (reach-placeholder)

    fn setup_profiles() -> (PathBuf, PathBuf) {
        let root = scratch("reach");
        let profiles = root.join("profiles");
        let ex = profiles.join("example");
        fs::create_dir_all(&ex).unwrap();
        fs::write(ex.join("profile.json"), "\u{feff}{\"id\":\"example\",\"label\":\"Example Group\",\"url\":\"https://example.net\",\"filePrefix\":\"EX-\",\"renderer\":\"placeholder\"}").unwrap();
        fs::write(ex.join("file_id.diz"), "SENTINEL-7F3A {{group}}\r\n").unwrap();
        (root, profiles)
    }

    fn sent() -> Value {
        json!({"profileId": "example", "code": "test1", "format": "LHA", "division": "bogus", "selectedPaths": []})
    }

    #[test]
    fn handlers_placeholder() {
        let (root, profiles) = setup_profiles();
        let out = root.join("out");
        let outf = |_: &Profile| out.clone();
        let stages = RefCell::new(Vec::new());
        let prog = |s: &str| stages.borrow_mut().push(s.to_string());
        let ran = RefCell::new(Vec::new());
        // Stub runner: record the call and create the archive file.
        let run = |bin: &str, args: &[String], cwd: &Path| -> Result<(), String> {
            assert!(cwd.join("file_id.diz").exists());
            fs::write(&args[1], bin).unwrap();
            ran.borrow_mut().push(bin.to_string());
            Ok(())
        };
        let env = Env { profiles_dir: &profiles, output_root: &outf, run: &run, progress: &prog };

        let p = preview_outputs_core(&env, &sent()).unwrap();
        assert_eq!(p["dizText"], "SENTINEL-7F3A Example Group\n");
        assert_eq!(p["hasNfoTemplate"], false);
        assert_eq!(p["problems"], json!([]));

        let r = build_release_core(&env, &sent(), true).unwrap();
        assert_eq!(r["archivePath"], out.join("dry-run").join("EX-TEST1.LHA").to_string_lossy().to_string());
        assert_eq!(r["isDryRun"], true);
        assert_eq!(r["releaseNumber"], 0);
        assert_eq!(*ran.borrow(), vec!["lha".to_string()]);
        assert_eq!(stages.borrow().first().map(String::as_str), Some("Preparing the build"));
        assert!(stages.borrow().contains(&"Previewing the release number".to_string()));
        assert!(out.join("dry-run/EX-TEST1").is_dir());
        // A second dry run replaces the first.
        build_release_core(&env, &sent(), true).unwrap();

        // A real build refuses while a leftover exists, then succeeds when clear.
        fs::create_dir_all(out.join("EX-TEST1")).unwrap();
        let e = build_release_core(&env, &sent(), false).unwrap_err();
        assert!(e.starts_with("EX-TEST1 already exists locally at "), "{e}");
        fs::remove_dir_all(out.join("EX-TEST1")).unwrap();
        let r = build_release_core(&env, &sent(), false).unwrap();
        assert_eq!(r["isDryRun"], false);
        assert!(stages.borrow().contains(&"Submitting to the door repository".to_string()));
        assert!(r.get("doorSubmission").is_none());

        // A bad template shows in both preview and build.
        fs::write(profiles.join("example/release.nfo"), "ok\r\n{{bogus}}\r\n").unwrap();
        let p = preview_outputs_core(&env, &sent()).unwrap();
        assert_eq!(p["problems"], json!(["release.nfo line 2: unknown placeholder {{bogus}}"]));
        assert_eq!(p["hasNfoTemplate"], true);
        let e = build_release_core(&env, &sent(), true).unwrap_err();
        assert_eq!(e, "release.nfo line 2: unknown placeholder {{bogus}}");
        fs::remove_dir_all(root).unwrap();
    }

    #[test]
    fn division_override_and_server_stub() {
        let p = ur_profile();
        let f = form_for(&p, &json!({"division": "xinnovations", "code": "a"})).unwrap();
        assert_eq!(f.code, "a");
        let root = scratch("srv");
        let ur = root.join("profiles/uprough");
        fs::create_dir_all(&ur).unwrap();
        fs::write(ur.join("profile.json"), "{\"id\":\"uprough\",\"label\":\"Up Rough\",\"url\":\"u\",\"filePrefix\":\"UP-\",\"renderer\":\"uprough\",\"backend\":\"releases-doorserver\"}").unwrap();
        let profiles = root.join("profiles");
        let outf = |_: &Profile| root.join("out");
        let env = Env { profiles_dir: &profiles, output_root: &outf, run: &|_, _, _| Ok(()), progress: &|_| {} };
        let s = json!({"profileId": "uprough", "code": "t", "format": "LHA"});
        let p = preview_outputs_core(&env, &s).unwrap();
        assert_eq!(p["problems"][0], "Release history unavailable: not ported: server backend");
        assert!(p["nfoText"].is_string());
        assert_eq!(build_release_core(&env, &s, true).unwrap_err(), "not ported: server backend");
        fs::remove_dir_all(root).unwrap();
    }
}
