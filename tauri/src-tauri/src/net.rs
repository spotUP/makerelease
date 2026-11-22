// Port of releases-client.ts, backend-doorserver.ts, backend-none.ts, door-repo-client.ts, aminet.ts
// (plus shared/aminet-readme.ts, nfo-x-innovations.ts listArt) and the network IPC handlers.

use crate::build::{
    archive_file_name, describe_stale_drafts, run_tool, to_ascii_safe, trim_diz_banner, write_latin1, DraftEntry, Form,
    HistoryEntry, Member, Numbered, NumberedBackend, ReleaseListEntry,
};
use serde_json::{json, Map, Value};
use std::path::Path;
use std::time::Duration;

pub const NO_SERVER_MESSAGE: &str = "This group has no release server";
pub const DOOR_REPO_SUBMISSIONS_URL: &str = "https://doors.uprough.net/api/door-repo/submissions";
pub const AMINET_UPLOAD_URL: &str = "ftp://main.aminet.net/new/";

// ---------------------------------------------------------------- releases-client.ts

pub struct Config {
    pub base_url: String,
    /// Empty when the profile carries no key: reads work, every write is refused.
    pub write_key: String,
}

/// RELEASES_API_BASE_URL and RELEASES_WRITE_KEY win over the profile's server block.
pub fn client_config_for(profile: &Value, env: &dyn Fn(&str) -> Option<String>) -> Result<Config, String> {
    let server = &profile["server"];
    if !server.is_object() {
        return Err(format!(
            "Profile \"{}\" has no server block; it does not publish to a releases server.",
            profile["id"].as_str().unwrap_or("")
        ));
    }
    let pick = |var: &str, key: &str| {
        env(var).filter(|v| !v.is_empty()).unwrap_or_else(|| server[key].as_str().unwrap_or("").to_string())
    };
    Ok(Config { base_url: pick("RELEASES_API_BASE_URL", "baseUrl"), write_key: pick("RELEASES_WRITE_KEY", "writeKey") })
}

fn process_env(name: &str) -> Option<String> {
    std::env::var(name).ok()
}

/// ReleasesApiError: only the 409 path of publishToWeb keeps `wordpress_post_url`.
#[derive(Debug, Default, Clone)]
pub struct ApiError {
    pub message: String,
    pub status: Option<u16>,
    pub reason: Option<String>,
    pub wordpress_post_url: Option<String>,
}

impl ApiError {
    fn plain(message: String) -> Self {
        ApiError { message, ..Default::default() }
    }
}

impl From<ApiError> for String {
    fn from(e: ApiError) -> String {
        e.message
    }
}

type ApiRes<T> = Result<T, ApiError>;

fn unreachable_err(e: reqwest::Error) -> ApiError {
    ApiError::plain(format!("could not reach releases.uprough.net: {e}"))
}

fn client() -> reqwest::blocking::Client {
    reqwest::blocking::Client::builder().timeout(Duration::from_secs(300)).build().expect("reqwest client")
}

/// JS encodeURIComponent.
fn encode_uri_component(s: &str) -> String {
    let mut out = String::new();
    for b in s.bytes() {
        if b.is_ascii_alphanumeric() || b"-_.!~*'()".contains(&b) {
            out.push(b as char);
        } else {
            out.push_str(&format!("%{b:02X}"));
        }
    }
    out
}

/// `await response.json().catch(() => ({}))`
fn read_body(resp: reqwest::blocking::Response) -> (u16, Value) {
    let status = resp.status().as_u16();
    let body = resp.text().ok().and_then(|t| serde_json::from_str(&t).ok()).unwrap_or_else(|| json!({}));
    (status, body)
}

fn error_text(body: &Value) -> Option<String> {
    body.get("error").and_then(Value::as_str).map(str::to_string)
}

fn get_json(cfg: &Config, path: &str) -> ApiRes<Value> {
    let resp = client().get(format!("{}{path}", cfg.base_url)).send().map_err(unreachable_err)?;
    let (status, body) = read_body(resp);
    if !(200..300).contains(&status) {
        return Err(ApiError::plain(format!(
            "releases.uprough.net returned {status}: {}",
            error_text(&body).unwrap_or_else(|| "unknown error".into())
        )));
    }
    Ok(body)
}

pub fn fetch_seed(cfg: &Config, division: &str) -> ApiRes<Value> {
    get_json(cfg, &format!("/api/releases/seed?division={}", encode_uri_component(division)))
}

pub fn fetch_manifest(cfg: &Config, division: &str) -> ApiRes<Value> {
    get_json(cfg, &format!("/api/releases/manifest?division={}", encode_uri_component(division)))
}

pub fn check_code_exists(cfg: &Config, code: &str) -> ApiRes<bool> {
    let r = get_json(cfg, &format!("/api/releases/{}/exists", encode_uri_component(code)))?;
    Ok(r["exists"].as_bool().unwrap_or(false))
}

pub fn fetch_numbering_check(cfg: &Config) -> ApiRes<Value> {
    get_json(cfg, "/api/releases/numbering-check")
}

fn publish_body(form: &Form, division: &str) -> Value {
    let mut m = Map::new();
    let mut put = |k: &str, v: Value| {
        m.insert(k.to_string(), v);
    };
    put("code", json!(form.code));
    put("format", json!(form.format));
    put("releaseName", json!(form.release_name));
    put("platformTag", json!(form.platform_tag));
    put("releaseNotes", json!(form.release_notes_text));
    put("news", json!(form.news_text));
    put("dizText", json!(trim_diz_banner(&form.diz_banner_text)));
    put("entryTitle", json!(form.entry_title));
    put("entryCategory", json!(form.entry_category));
    put("entryCreators", json!(form.entry_creators));
    put("division", json!(division));
    if let Some(b) = &form.boards {
        put("boards", json!(b));
    }
    put("members", json!(form.members));
    put("description", json!(form.description));
    put("credits", json!(form.credits));
    if let Some(e) = &form.media_embeds {
        put("mediaEmbeds", e.clone());
    }
    Value::Object(m)
}

pub fn publish_release(cfg: &Config, body: &Value) -> ApiRes<Value> {
    let resp = client()
        .post(format!("{}/api/releases", cfg.base_url))
        .header("Content-Type", "application/json")
        .header("X-Release-Key", &cfg.write_key)
        .body(body.to_string())
        .send()
        .map_err(unreachable_err)?;
    let (status, body) = read_body(resp);
    if !(200..300).contains(&status) {
        return Err(ApiError::plain(error_text(&body).unwrap_or_else(|| format!("releases.uprough.net returned {status}"))));
    }
    Ok(body)
}

pub fn delete_release_request(cfg: &Config, code: &str) -> ApiRes<Value> {
    let resp = client()
        .delete(format!("{}/api/releases/{}", cfg.base_url, encode_uri_component(code)))
        .header("X-Release-Key", &cfg.write_key)
        .send()
        .map_err(unreachable_err)?;
    let (status, body) = read_body(resp);
    if !(200..300).contains(&status) {
        return Err(ApiError {
            message: error_text(&body).unwrap_or_else(|| format!("releases.uprough.net returned {status}")),
            status: Some(status),
            ..Default::default()
        });
    }
    Ok(body)
}

pub fn publish_to_web_request(cfg: &Config, code: &str, archive: &str, image: &str, embeds: &[String]) -> ApiRes<Value> {
    let file_err = |e: std::io::Error| ApiError::plain(e.to_string());
    let mut form = reqwest::blocking::multipart::Form::new()
        .file("archive", archive)
        .map_err(file_err)?
        .file("image", image)
        .map_err(file_err)?;
    // Matched positionally on the server to this release's stored image-type media embeds.
    for e in embeds {
        form = form.file("embedImage", e).map_err(file_err)?;
    }
    let resp = client()
        .post(format!("{}/api/releases/{}/publish", cfg.base_url, encode_uri_component(code)))
        .header("X-Release-Key", &cfg.write_key)
        .multipart(form)
        .send()
        .map_err(unreachable_err)?;
    let (status, body) = read_body(resp);
    if !(200..300).contains(&status) {
        let text = |k: &str| body.get(k).and_then(Value::as_str).filter(|s| !s.is_empty()).map(str::to_string);
        return Err(ApiError {
            message: error_text(&body).unwrap_or_else(|| format!("releases.uprough.net returned {status}")),
            status: Some(status),
            reason: text("reason"),
            wordpress_post_url: if status == 409 { text("wordpressPostUrl") } else { None },
        });
    }
    Ok(body)
}

// ---------------------------------------------------------------- door-repo-client.ts

pub fn submit_door(archive_path: &Path, url: &str) -> Value {
    let part = reqwest::blocking::multipart::Form::new().file("file", archive_path);
    let form = match part {
        Ok(f) => f,
        Err(e) => return json!({ "ok": false, "message": format!("could not reach the door repository: {e}") }),
    };
    let resp = match client().post(url).multipart(form).send() {
        Ok(r) => r,
        Err(e) => return json!({ "ok": false, "message": format!("could not reach the door repository: {e}") }),
    };
    let (status, body) = read_body(resp);
    if !(200..300).contains(&status) {
        let message = error_text(&body).unwrap_or_else(|| format!("the door repository returned {status}"));
        return json!({ "ok": false, "message": message });
    }
    let mut out = json!({
        "ok": true,
        "message": body.get("message").and_then(Value::as_str).unwrap_or("submitted to the door repository"),
    });
    if body["id"].is_number() {
        out["id"] = body["id"].clone();
    }
    out
}

// ---------------------------------------------------------------- shared helpers

/// members.ts mergeMemberSkills: one entry per member, every skill in one comma-separated role.
pub fn merge_member_skills(members: &[Member]) -> Vec<Member> {
    let mut merged: Vec<(String, Vec<String>)> = vec![];
    for m in members {
        let nick = m.nickname.trim().to_string();
        if nick.is_empty() {
            continue;
        }
        let at = match merged.iter().position(|(n, _)| *n == nick) {
            Some(i) => i,
            None => {
                merged.push((nick, vec![]));
                merged.len() - 1
            }
        };
        for skill in m.role.split(',').map(str::trim).filter(|s| !s.is_empty()) {
            let skills = &mut merged[at].1;
            if !skills.iter().any(|k| k.to_lowercase() == skill.to_lowercase()) {
                skills.push(skill.to_string());
            }
        }
    }
    merged.into_iter().map(|(nickname, skills)| Member { nickname, role: skills.join(", ") }).collect()
}

/// divisions.ts formatReleaseNumber.
pub fn format_release_number(n: i64, division: &str) -> String {
    if division != "xinnovations" {
        return n.to_string();
    }
    format!("{n:0>3}").replace('0', "o")
}

const X_INNOVATIONS_BANNER: &str = r#"    _ _ __ ___    ____  _ _ __ _________
   | | Y  / _ \../ __ \| | Y  / __  | | |
dNo| | | | | | || /_ | | | | | |_ | l | |dS!
|  | | | | |`| || || | | | | | || |___| |  |
|  | | l | |__/'| || | | | | | || |   | |  |
|  | l___| |    | || | | l l | ll | | | |  |
|  `\______|    |_|`\_/'\___/`\__/l_l_|_|  |
|     uP rOUGH /X iNNOVATiONS pRESENTS:    |
|                                          |
`------------------------------------------'"#;

const RELEASE_NAME_ROW: usize = 8;

fn rtrim(s: &str) -> &str {
    s.trim_end_matches([' ', '\t'])
}

fn frame_char(c: char) -> bool {
    !c.is_whitespace() && !c.is_ascii_alphanumeric()
}

/// diz-release-line.ts applyReleaseLineText: only the interior of the row's frame is replaced.
fn apply_release_line(banner: &str, text: &str, row: usize) -> String {
    let mut lines: Vec<String> = banner.split('\n').map(str::to_string).collect();
    let row = row.min(lines.len() - 1);
    let mut line: Vec<char> = rtrim(&lines[row]).chars().collect();
    let head = line.iter().take(3).take_while(|c| frame_char(**c)).count();
    let tail = line.iter().rev().take(3).take_while(|c| frame_char(**c)).count();
    let start = head;
    let tail_start = line.len() - tail;
    let end = if tail_start > start { tail_start } else { line.len() };
    let width = end - start;
    if line.len() < end {
        line.resize(end, ' ');
    }
    let clipped: Vec<char> = text.trim().chars().take(width).collect();
    let pad = width - clipped.len();
    let left = (pad + 1) / 2;
    let mut out: String = line[..start].iter().collect();
    out.push_str(&" ".repeat(left));
    out.extend(clipped);
    out.push_str(&" ".repeat(pad - left));
    out.extend(&line[end..]);
    lines[row] = out;
    lines.join("\n")
}

/// nfo-x-innovations.ts releaseNameInBanner.
pub fn release_name_in_banner(banner: &str) -> String {
    let lines: Vec<&str> = banner.split('\n').collect();
    let announced = lines.iter().position(|l| {
        let l = l.to_lowercase();
        l.contains("presents:")
    });
    let from = announced.map_or(0, |i| i + 1);
    for line in &lines[from..] {
        let text = line.trim_matches(['|', '`', '\'', '\\', '/']).trim();
        if !text.is_empty() && !text.chars().all(|c| "-_=.".contains(c) || c.is_whitespace()) {
            return text.to_string();
        }
    }
    String::new()
}

pub fn list_art_from_banner(banner: &str) -> String {
    apply_release_line(X_INNOVATIONS_BANNER, &release_name_in_banner(banner), RELEASE_NAME_ROW)
}

// ---------------------------------------------------------------- backend-doorserver.ts

pub struct ServerBackend {
    id: String,
    file_prefix: String,
    division: &'static str,
    cfg: Config,
}

impl ServerBackend {
    pub fn from_profile(profile: &Value) -> Result<Self, String> {
        Self::from_profile_with_env(profile, &process_env)
    }

    pub fn from_profile_with_env(profile: &Value, env: &dyn Fn(&str) -> Option<String>) -> Result<Self, String> {
        let id = profile["id"].as_str().unwrap_or("").to_string();
        let division = match profile["renderer"].as_str() {
            Some("uprough") => "uprough",
            Some("xinnovations") => "xinnovations",
            _ => return Err(format!("{id}: the release server needs renderer \"uprough\" or \"xinnovations\"")),
        };
        let cfg = client_config_for(profile, env)?;
        Ok(ServerBackend { id, file_prefix: profile["filePrefix"].as_str().unwrap_or("").to_string(), division, cfg })
    }

    /// The numbering check is a warning, never a precondition. TS runs it in parallel with the seed; here it is sequential.
    pub fn seed(&self, now_seconds: i64) -> Result<Value, String> {
        let seed = fetch_seed(&self.cfg, self.division)?;
        let numbering = fetch_numbering_check(&self.cfg).ok();
        let numbering_warning = numbering.as_ref().and_then(|n| {
            let summary = n["summary"].as_str().filter(|s| !s.is_empty())?;
            (n["checked"].as_bool() == Some(true) && n["ok"].as_bool() == Some(false)).then(|| {
                format!("{summary} Do not build until this is reconciled - the release number goes into the NFO.")
            })
        });
        let drafts: Vec<DraftEntry> = seed["drafts"]
            .as_array()
            .map(|a| {
                a.iter()
                    .map(|d| DraftEntry {
                        code: d["code"].as_str().unwrap_or("").to_string(),
                        release_number: d["releaseNumber"].as_i64().unwrap_or(0),
                        created_at: d["createdAt"].as_i64().unwrap_or(0),
                        entry_title: d["entryTitle"].as_str().unwrap_or("").to_string(),
                    })
                    .collect()
            })
            .unwrap_or_default();
        let members: Vec<Member> = serde_json::from_value(seed["members"].clone()).unwrap_or_default();
        let mut out = Map::new();
        out.insert("members".into(), json!(merge_member_skills(&members)));
        out.insert("boards".into(), if seed["boards"].is_null() { json!([]) } else { seed["boards"].clone() });
        out.insert("nextReleaseNumber".into(), seed["nextReleaseNumber"].clone());
        out.insert("history".into(), seed["history"].clone());
        out.insert(
            "currentDizText".into(),
            if seed["currentDizText"].is_null() { json!("") } else { seed["currentDizText"].clone() },
        );
        out.insert("revision".into(), seed["revision"].clone());
        if let Some(w) = numbering_warning {
            out.insert("numberingWarning".into(), json!(w));
        }
        if let Some(w) = describe_stale_drafts(&drafts, now_seconds) {
            out.insert("staleDraftWarning".into(), json!(w));
        }
        if self.cfg.write_key.is_empty() {
            out.insert(
                "configWarning".into(),
                json!(format!(
                    "The \"{}\" profile has no server write key - publishing a release will fail. Put the key in its profile.json and press Reload profiles.",
                    self.id
                )),
            );
        }
        Ok(Value::Object(out))
    }

    fn history_from(&self, v: &Value) -> Vec<HistoryEntry> {
        serde_json::from_value(v.clone()).unwrap_or_default()
    }
}

fn now_seconds() -> i64 {
    std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map_or(0, |d| d.as_secs() as i64)
}

/// profiles.rs seed_state for a releases-doorserver profile.
pub fn server_seed(profile: &Value) -> Result<Value, String> {
    ServerBackend::from_profile(profile)?.seed(now_seconds())
}

impl NumberedBackend for ServerBackend {
    fn create(&self, form: &Form) -> Result<Numbered, String> {
        let r = publish_release(&self.cfg, &publish_body(form, self.division))?;
        Ok(Numbered { release_number: r["releaseNumber"].as_i64().unwrap_or(0), history: self.history_from(&r["history"]) })
    }

    /// No POST: no number consumed, no catalogue write. The entry a real publish would insert is synthesized.
    fn preview(&self, form: &Form) -> Result<Numbered, String> {
        let seed = fetch_seed(&self.cfg, self.division)?;
        let n = seed["nextReleaseNumber"].as_i64().unwrap_or(0);
        let entry = HistoryEntry {
            release_number: n.to_string(),
            filenames: archive_file_name(&self.file_prefix, &form.code, &form.format).to_lowercase(),
            title: form.entry_title.clone(),
            category: form.entry_category.clone(),
            creators: form.entry_creators.clone(),
        };
        let mut history = vec![entry];
        history.extend(self.history_from(&seed["history"]));
        Ok(Numbered { release_number: n, history })
    }

    fn release_list(&self, form: &Form, release_number: i64) -> Result<Vec<ReleaseListEntry>, String> {
        if self.division != "xinnovations" {
            return Ok(vec![]);
        }
        let manifest = fetch_manifest(&self.cfg, self.division)?;
        let mut list = vec![ReleaseListEntry {
            display_number: format_release_number(release_number, self.division),
            art: list_art_from_banner(&form.diz_banner_text),
        }];
        for e in manifest["history"].as_array().into_iter().flatten() {
            list.push(ReleaseListEntry {
                display_number: e["releaseNumber"].as_str().unwrap_or("").to_string(),
                art: list_art_from_banner(e["dizText"].as_str().unwrap_or("")),
            });
        }
        Ok(list)
    }

    /// A /X release is a door: submit it to the door repository. A failed submission is reported, never thrown.
    fn after_build(&self, archive_path: &Path) -> Result<Option<Value>, String> {
        Ok((self.division == "xinnovations").then(|| submit_door(archive_path, DOOR_REPO_SUBMISSIONS_URL)))
    }
}

// ---------------------------------------------------------------- ipc outcomes (backend.ts publishFailure/deleteFailure)

fn publish_outcome(r: ApiRes<Value>) -> Value {
    match r {
        Ok(body) => {
            let mut out = Map::new();
            out.insert("ok".into(), json!(true));
            if let Some(o) = body.as_object() {
                out.extend(o.clone());
            }
            Value::Object(out)
        }
        Err(e) => {
            let mut out = Map::new();
            out.insert("ok".into(), json!(false));
            out.insert("message".into(), json!(e.message));
            if let Some(s) = e.status {
                out.insert("status".into(), json!(s));
            }
            if let Some(r) = e.reason {
                out.insert("reason".into(), json!(r));
            }
            if let Some(u) = e.wordpress_post_url {
                out.insert("wordpressPostUrl".into(), json!(u));
            }
            Value::Object(out)
        }
    }
}

fn delete_outcome(r: ApiRes<Value>) -> Value {
    match r {
        Ok(body) => {
            let mut out = Map::new();
            out.insert("ok".into(), json!(true));
            for k in [
                "code",
                "releaseNumber",
                "wasPublished",
                "wasDraft",
                "wordpressPostUrl",
                "archiveRemoved",
                "wordpressPostTrashed",
                "wordpressPostError",
            ] {
                if let Some(v) = body.get(k) {
                    out.insert(k.into(), v.clone());
                }
            }
            Value::Object(out)
        }
        Err(e) => {
            let mut out = json!({ "ok": false, "message": e.message });
            if let Some(s) = e.status {
                out["status"] = json!(s);
            }
            out
        }
    }
}

fn failure(message: String) -> Value {
    json!({ "ok": false, "message": message })
}

/// None-backend profiles have no server; otherwise the backend for the profile.
fn server_for(profiles_dir: &Path, profile_id: &str) -> Result<Option<ServerBackend>, String> {
    let profile = crate::profiles::load_profile(profiles_dir, profile_id)?;
    if profile["backend"] == "none" {
        return Ok(None);
    }
    ServerBackend::from_profile(&profile).map(Some)
}

pub fn check_code_core(profiles_dir: &Path, profile_id: &str, code: &str) -> Result<Value, String> {
    let exists = match server_for(profiles_dir, profile_id)? {
        Some(b) => check_code_exists(&b.cfg, code)?,
        None => false,
    };
    Ok(json!({ "exists": exists }))
}

pub fn publish_to_web_core(profiles_dir: &Path, profile_id: &str, code: &str, archive: &str, image: &str, embeds: &[String]) -> Value {
    match server_for(profiles_dir, profile_id) {
        Err(m) => failure(m),
        Ok(None) => failure(NO_SERVER_MESSAGE.into()),
        Ok(Some(b)) => publish_outcome(publish_to_web_request(&b.cfg, code, archive, image, embeds)),
    }
}

pub fn delete_release_core(profiles_dir: &Path, profile_id: &str, code: &str) -> Value {
    match server_for(profiles_dir, profile_id) {
        Err(m) => failure(m),
        Ok(None) => failure(NO_SERVER_MESSAGE.into()),
        Ok(Some(b)) => delete_outcome(delete_release_request(&b.cfg, code)),
    }
}

async fn blocking<T: Send + 'static>(f: impl FnOnce() -> T + Send + 'static) -> Result<T, String> {
    tauri::async_runtime::spawn_blocking(f).await.map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn check_code_availability(app: tauri::AppHandle, profile_id: String, code: String) -> Result<Value, String> {
    let dir = crate::build::profiles_dir(&app)?;
    blocking(move || check_code_core(&dir, &profile_id, &code)).await?
}

#[tauri::command]
pub async fn publish_to_web(
    app: tauri::AppHandle,
    profile_id: String,
    code: String,
    archive_path: String,
    image_path: String,
    embed_image_paths: Vec<String>,
) -> Result<Value, String> {
    let dir = crate::build::profiles_dir(&app)?;
    blocking(move || publish_to_web_core(&dir, &profile_id, &code, &archive_path, &image_path, &embed_image_paths)).await
}

#[tauri::command]
pub async fn upload_to_aminet(request: Value, archive_path: String) -> Result<Value, String> {
    blocking(move || upload_to_aminet_core(&request, Path::new(&archive_path), &curl)).await
}

#[tauri::command]
pub async fn delete_release(app: tauri::AppHandle, profile_id: String, code: String) -> Result<Value, String> {
    let dir = crate::build::profiles_dir(&app)?;
    blocking(move || delete_release_core(&dir, &profile_id, &code)).await
}

// ---------------------------------------------------------------- aminet.ts + shared/aminet-readme.ts

const AMINET_SHORT_MAX: usize = 40;
const AMINET_LINE_MAX: usize = 78;
const AMINET_FILENAME_MAX: usize = 30;

fn utf16_len(s: &str) -> usize {
    s.encode_utf16().count()
}

pub struct AminetNames {
    pub archive: String,
    pub readme: String,
}

pub fn aminet_file_names(archive_base_name: &str) -> AminetNames {
    let archive = archive_base_name.to_lowercase();
    let stem = match archive.rfind('.') {
        Some(i) if i + 1 < archive.len() => &archive[..i],
        _ => &archive,
    };
    AminetNames { readme: format!("{stem}.readme"), archive }
}

pub fn aminet_file_name_problems(names: &AminetNames) -> Vec<String> {
    let mut problems = vec![];
    for name in [&names.archive, &names.readme] {
        let n = utf16_len(name);
        if n > AMINET_FILENAME_MAX {
            problems.push(format!("\"{name}\" is {n} characters; Aminet allows {AMINET_FILENAME_MAX}"));
        }
        if name.is_empty() || !name.chars().all(|c| c.is_ascii_alphanumeric() || "._-".contains(c)) {
            problems.push(format!("\"{name}\" may only contain letters, digits, dots, underscores and hyphens"));
        }
    }
    problems
}

fn header_value(lines: &[&str], field: &str) -> Option<String> {
    let prefix = format!("{field}:");
    lines.iter().find(|l| l.starts_with(&prefix)).map(|l| l[prefix.len()..].trim().to_string())
}

/// `^[^\s@()]+@[^\s@()]+\.[^\s@()]+ \(.+\)$`
fn uploader_ok(s: &str) -> bool {
    let plain = |c: char| !c.is_whitespace() && !"@()".contains(c);
    let Some((local, rest)) = s.split_once('@') else { return false };
    let Some((domain, name)) = rest.split_once(' ') else { return false };
    let dots_ok = domain.char_indices().any(|(i, c)| c == '.' && i > 0 && i + 1 < domain.len());
    !local.is_empty()
        && local.chars().all(plain)
        && domain.chars().all(plain)
        && dots_ok
        && name.starts_with('(')
        && name.ends_with(')')
        && name.chars().count() >= 3
}

pub fn aminet_readme_problems(readme: &str) -> Vec<String> {
    let mut problems = vec![];
    if readme.contains('\r') {
        problems.push("the readme must use LF line ends, not CR LF".to_string());
    }
    let normalized = readme.replace("\r\n", "\n").replace('\r', "\n");
    let lines: Vec<&str> = normalized.split('\n').collect();
    let header = match lines.iter().position(|l| l.is_empty()) {
        Some(i) => &lines[..i],
        None => &lines[..],
    };
    if !lines[0].starts_with("Short:") {
        problems.push("the first line must be \"Short:\"".to_string());
    }
    let short = header_value(header, "Short").unwrap_or_default();
    if short.is_empty() {
        problems.push("Short: is empty".to_string());
    } else if utf16_len(&short) > AMINET_SHORT_MAX {
        problems.push(format!("Short: is {} characters; Aminet allows {AMINET_SHORT_MAX}", utf16_len(&short)));
    }
    if !uploader_ok(&header_value(header, "Uploader").unwrap_or_default()) {
        problems.push("Uploader: needs an email address and a name, as \"you@example.com (Your Name)\"".to_string());
    }
    let kind = header_value(header, "Type").unwrap_or_default();
    let kind_ok = kind.split_once('/').is_some_and(|(a, b)| {
        let ok = |s: &str| !s.is_empty() && s.chars().all(|c| c.is_ascii_lowercase() || c.is_ascii_digit());
        ok(a) && ok(b)
    });
    if !kind_ok {
        problems.push("Type: must be an Aminet directory such as \"demo/aga\"".to_string());
    }
    if header_value(header, "Architecture").unwrap_or_default().is_empty() {
        problems.push("Architecture: is empty".to_string());
    }
    for (i, line) in lines.iter().enumerate() {
        if utf16_len(line) > AMINET_LINE_MAX {
            problems.push(format!("line {} is {} characters; Aminet allows {AMINET_LINE_MAX}", i + 1, utf16_len(line)));
        }
    }
    problems
}

pub fn normalize_readme_text(text: &str) -> String {
    let lf = to_ascii_safe(&text.replace("\r\n", "\n").replace('\r', "\n"));
    if lf.ends_with('\n') {
        lf
    } else {
        format!("{lf}\n")
    }
}

fn curl_hint() -> &'static str {
    if cfg!(target_os = "macos") {
        "it ships with macOS; reinstall the command line tools or \"brew install curl\""
    } else if cfg!(target_os = "linux") {
        "install it with your package manager, e.g. \"sudo apt install curl\""
    } else {
        "it ships with Windows 10 and later; otherwise install it from curl.se"
    }
}

fn curl(bin: &str, args: &[String]) -> Result<(), String> {
    run_tool(bin, args, None, curl_hint())
}

/// Never fails: a failed Aminet upload is reported next to the website's publish result.
pub fn upload_to_aminet_core(request: &Value, archive_path: &Path, run: &dyn Fn(&str, &[String]) -> Result<(), String>) -> Value {
    let base = archive_path.file_name().map(|n| n.to_string_lossy().to_string()).unwrap_or_default();
    let names = aminet_file_names(&base);
    let readme_text = normalize_readme_text(request["readmeText"].as_str().unwrap_or(""));
    let mut problems = aminet_file_name_problems(&names);
    problems.extend(aminet_readme_problems(&readme_text));
    if !problems.is_empty() {
        return failure(format!("not uploaded to Aminet: {}", problems.join("; ")));
    }
    let email = request["uploaderEmail"].as_str().unwrap_or("").trim().to_string();
    if !email.contains('@') {
        return failure("not uploaded to Aminet: the uploader email is missing".into());
    }
    let readme_path = archive_path.parent().unwrap_or(Path::new("")).join(&names.readme);
    if let Err(e) = write_latin1(&readme_path, &readme_text) {
        return failure(format!("not uploaded to Aminet: could not write {}: {e}", readme_path.display()));
    }
    let readme_str = readme_path.display().to_string();
    // Archive first, readme second: Aminet pairs a readme with the file of the same base name.
    for (local, remote) in [(archive_path.display().to_string(), &names.archive), (readme_str.clone(), &names.readme)] {
        let args: Vec<String> = [
            "--silent",
            "--show-error",
            "--fail",
            "--ftp-pasv",
            "--user",
            &format!("anonymous:{email}"),
            "--upload-file",
            &local,
            &format!("{AMINET_UPLOAD_URL}{remote}"),
        ]
        .iter()
        .map(|s| s.to_string())
        .collect();
        if let Err(e) = run("curl", &args) {
            return json!({ "ok": false, "message": format!("Aminet upload of {remote} failed: {e}"), "readmePath": readme_str });
        }
    }
    json!({
        "ok": true,
        "message": format!("uploaded {} and {} to Aminet (it appears once a moderator has looked at it)", names.archive, names.readme),
        "readmePath": readme_str,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::{BufRead, BufReader, Read, Write};
    use std::net::TcpListener;
    use std::sync::{Arc, Mutex};

    struct Req {
        method: String,
        path: String,
        headers: Vec<(String, String)>,
        body: Vec<u8>,
    }

    impl Req {
        fn header(&self, k: &str) -> Option<&str> {
            self.headers.iter().find(|(n, _)| n.eq_ignore_ascii_case(k)).map(|(_, v)| v.as_str())
        }
        fn text(&self) -> String {
            String::from_utf8_lossy(&self.body).to_string()
        }
    }

    type Log = Arc<Mutex<Vec<Req>>>;

    /// A local HTTP server: the handler answers (status, json body) per request; every request is logged.
    fn mock(handler: impl Fn(&Req) -> (u16, String) + Send + Sync + 'static) -> (String, Log) {
        let listener = TcpListener::bind("127.0.0.1:0").unwrap();
        let url = format!("http://{}", listener.local_addr().unwrap());
        let log: Log = Arc::new(Mutex::new(vec![]));
        let handler = Arc::new(handler);
        let seen = log.clone();
        std::thread::spawn(move || {
            for stream in listener.incoming().flatten() {
                let (handler, seen) = (handler.clone(), seen.clone());
                std::thread::spawn(move || serve(stream, &*handler, &seen));
            }
        });
        (url, log)
    }

    fn serve(mut stream: std::net::TcpStream, handler: &dyn Fn(&Req) -> (u16, String), seen: &Log) {
        let mut reader = BufReader::new(stream.try_clone().unwrap());
        let mut first = String::new();
        reader.read_line(&mut first).unwrap();
        let mut parts = first.split_whitespace();
        let (method, path) = (parts.next().unwrap().to_string(), parts.next().unwrap().to_string());
        let mut headers = vec![];
        loop {
            let mut l = String::new();
            reader.read_line(&mut l).unwrap();
            let l = l.trim_end().to_string();
            if l.is_empty() {
                break;
            }
            let (k, v) = l.split_once(':').unwrap();
            headers.push((k.to_string(), v.trim().to_string()));
        }
        let find = |k: &str| headers.iter().find(|(n, _)| n.eq_ignore_ascii_case(k)).map(|(_, v)| v.clone());
        let mut body = vec![];
        if let Some(n) = find("content-length") {
            body = vec![0; n.parse().unwrap()];
            reader.read_exact(&mut body).unwrap();
        } else if find("transfer-encoding").is_some_and(|t| t.contains("chunked")) {
            loop {
                let mut size = String::new();
                reader.read_line(&mut size).unwrap();
                let n = usize::from_str_radix(size.trim(), 16).unwrap();
                let mut chunk = vec![0; n + 2];
                reader.read_exact(&mut chunk).unwrap();
                if n == 0 {
                    break;
                }
                body.extend_from_slice(&chunk[..n]);
            }
        }
        let req = Req { method, path, headers, body };
        let (status, text) = handler(&req);
        seen.lock().unwrap().push(req);
        let _ = write!(
            stream,
            "HTTP/1.1 {status} X\r\nContent-Type: application/json\r\nContent-Length: {}\r\nConnection: close\r\n\r\n{text}",
            text.len()
        );
    }

    fn cfg(url: &str, key: &str) -> Config {
        Config { base_url: url.to_string(), write_key: key.to_string() }
    }

    fn no_env(_: &str) -> Option<String> {
        None
    }

    fn profile(url: &str, key: &str, renderer: &str) -> Value {
        json!({ "id": "up", "label": "Up", "url": "u", "filePrefix": "UP-", "renderer": renderer,
            "backend": "releases-doorserver", "server": { "baseUrl": url, "writeKey": key } })
    }

    fn backend(url: &str, key: &str, renderer: &str) -> ServerBackend {
        ServerBackend::from_profile_with_env(&profile(url, key, renderer), &no_env).unwrap()
    }

    fn form() -> Form {
        Form {
            code: "hypoc".into(),
            format: "LHA".into(),
            release_name: "Hypocrisy".into(),
            platform_tag: "A500".into(),
            release_notes_text: "notes".into(),
            news_text: "news".into(),
            diz_banner_text: "|  a  |\n|  b  |\n".into(),
            entry_title: "Hypocrisy".into(),
            entry_category: "demo".into(),
            entry_creators: "me".into(),
            members: vec![Member { nickname: "a".into(), role: "code".into() }],
            description: "d".into(),
            ..Default::default()
        }
    }

    fn keys(v: &Value) -> Vec<&str> {
        v.as_object().unwrap().keys().map(String::as_str).collect()
    }

    #[test]
    fn config_env_wins_and_missing_server_block_errors() {
        let p = profile("http://file", "filekey", "uprough");
        let c = client_config_for(&p, &no_env).unwrap();
        assert_eq!((c.base_url.as_str(), c.write_key.as_str()), ("http://file", "filekey"));
        let env = |k: &str| match k {
            "RELEASES_API_BASE_URL" => Some("http://env".to_string()),
            "RELEASES_WRITE_KEY" => Some(String::new()), // empty falls back
            _ => None,
        };
        let c = client_config_for(&p, &env).unwrap();
        assert_eq!((c.base_url.as_str(), c.write_key.as_str()), ("http://env", "filekey"));
        let err = client_config_for(&json!({ "id": "x" }), &no_env).err().unwrap();
        assert_eq!(err, "Profile \"x\" has no server block; it does not publish to a releases server.");
        let err = ServerBackend::from_profile_with_env(&profile("u", "k", "placeholder"), &no_env).err().unwrap();
        assert_eq!(err, "up: the release server needs renderer \"uprough\" or \"xinnovations\"");
    }

    #[test]
    fn get_requests_and_error_texts() {
        let (url, log) = mock(|r| match r.path.as_str() {
            "/api/releases/seed?division=xinnovations" => (200, r#"{"nextReleaseNumber":7}"#.into()),
            "/api/releases/a%20b/exists" => (200, r#"{"exists":true}"#.into()),
            "/api/releases/manifest?division=uprough" => (500, r#"{"error":"boom"}"#.into()),
            _ => (404, "not json".into()),
        });
        let c = cfg(&url, "k");
        assert_eq!(fetch_seed(&c, "xinnovations").unwrap()["nextReleaseNumber"], 7);
        assert!(check_code_exists(&c, "a b").unwrap());
        assert_eq!(fetch_manifest(&c, "uprough").unwrap_err().message, "releases.uprough.net returned 500: boom");
        assert_eq!(
            fetch_numbering_check(&c).unwrap_err().message,
            "releases.uprough.net returned 404: unknown error"
        );
        let log = log.lock().unwrap();
        assert!(log.iter().all(|r| r.method == "GET" && r.header("x-release-key").is_none()));
        let down = fetch_seed(&cfg("http://127.0.0.1:1", ""), "uprough").unwrap_err();
        assert!(down.message.starts_with("could not reach releases.uprough.net: "), "{}", down.message);
    }

    #[test]
    fn publish_request_shape_and_errors() {
        let (url, log) = mock(|r| {
            if r.header("x-release-key") == Some("good") {
                (200, r#"{"releaseNumber":9,"revision":3,"history":[]}"#.into())
            } else {
                (401, r#"{"error":"bad key"}"#.into())
            }
        });
        let mut f = form();
        f.boards = Some(vec![]);
        f.media_embeds = Some(json!([]));
        let ok = publish_release(&cfg(&url, "good"), &publish_body(&f, "uprough")).unwrap();
        assert_eq!(ok["releaseNumber"], 9);
        assert_eq!(publish_release(&cfg(&url, "bad"), &publish_body(&f, "uprough")).unwrap_err().message, "bad key");
        let log = log.lock().unwrap();
        assert_eq!((log[0].method.as_str(), log[0].path.as_str()), ("POST", "/api/releases"));
        assert_eq!(log[0].header("content-type"), Some("application/json"));
        let body: Value = serde_json::from_slice(&log[0].body).unwrap();
        assert_eq!(
            keys(&body),
            ["code", "format", "releaseName", "platformTag", "releaseNotes", "news", "dizText", "entryTitle", "entryCategory",
                "entryCreators", "division", "boards", "members", "description", "credits", "mediaEmbeds"]
        );
        assert_eq!(body["releaseNotes"], "notes");
        let mut f = form();
        f.boards = None;
        assert!(!publish_body(&f, "uprough").as_object().unwrap().contains_key("boards"));
        assert!(!publish_body(&f, "uprough").as_object().unwrap().contains_key("mediaEmbeds"));
    }

    #[test]
    fn delete_and_outcome_shapes() {
        let (url, log) = mock(|r| {
            if r.path.ends_with("/gone") {
                (404, r#"{"error":"no such release"}"#.into())
            } else {
                (200, r#"{"deleted":true,"code":"hypoc","releaseNumber":4,"wasPublished":true,"wasDraft":false,"wordpressPostUrl":null,"archiveRemoved":true,"wordpressPostTrashed":false,"wordpressPostError":"x"}"#.into())
            }
        });
        let c = cfg(&url, "k");
        let ok = delete_outcome(delete_release_request(&c, "hypoc"));
        assert_eq!(
            keys(&ok),
            ["ok", "code", "releaseNumber", "wasPublished", "wasDraft", "wordpressPostUrl", "archiveRemoved", "wordpressPostTrashed", "wordpressPostError"]
        );
        assert_eq!(ok["wordpressPostUrl"], Value::Null);
        let bad = delete_outcome(delete_release_request(&c, "gone"));
        assert_eq!(bad, json!({ "ok": false, "message": "no such release", "status": 404 }));
        let log = log.lock().unwrap();
        assert_eq!((log[0].method.as_str(), log[0].path.as_str()), ("DELETE", "/api/releases/hypoc"));
        assert_eq!(log[0].header("x-release-key"), Some("k"));
    }

    #[test]
    fn publish_to_web_multipart_and_409_handling() {
        let dir = tempfile::tempdir().unwrap();
        let files: Vec<String> = ["a.lha", "i.png", "e1.png", "e2.png"]
            .iter()
            .map(|n| {
                let p = dir.path().join(n);
                std::fs::write(&p, n.as_bytes()).unwrap();
                p.display().to_string()
            })
            .collect();
        let (url, log) = mock(|r| match r.path.as_str() {
            "/api/releases/ok/publish" => (200, r#"{"archiveUrl":"https://a","wordpressPostUrl":"https://w"}"#.into()),
            "/api/releases/dup/publish" => (409, r#"{"error":"already","reason":"already-published","wordpressPostUrl":"https://w"}"#.into()),
            _ => (400, r#"{"error":"bad","wordpressPostUrl":"https://w"}"#.into()),
        });
        let c = cfg(&url, "k");
        let ok = publish_outcome(publish_to_web_request(&c, "ok", &files[0], &files[1], &files[2..]));
        assert_eq!(ok, json!({ "ok": true, "archiveUrl": "https://a", "wordpressPostUrl": "https://w" }));
        let dup = publish_outcome(publish_to_web_request(&c, "dup", &files[0], &files[1], &[]));
        assert_eq!(
            dup,
            json!({ "ok": false, "message": "already", "status": 409, "reason": "already-published", "wordpressPostUrl": "https://w" })
        );
        let other = publish_outcome(publish_to_web_request(&c, "x", &files[0], &files[1], &[]));
        assert_eq!(other, json!({ "ok": false, "message": "bad", "status": 400 }));
        let log = log.lock().unwrap();
        let body = log[0].text();
        assert_eq!(log[0].header("x-release-key"), Some("k"));
        assert!(log[0].header("content-type").unwrap().starts_with("multipart/form-data"));
        assert!(body.contains("name=\"archive\"; filename=\"a.lha\""));
        assert!(body.contains("name=\"image\"; filename=\"i.png\""));
        assert_eq!(body.matches("name=\"embedImage\"").count(), 2);
        let missing = publish_to_web_request(&c, "ok", "/no/such/file", &files[1], &[]).unwrap_err();
        assert!(missing.message.contains("No such file"), "{}", missing.message);
    }

    #[test]
    fn door_submission_outcomes() {
        let dir = tempfile::tempdir().unwrap();
        let archive = dir.path().join("x.lha");
        std::fs::write(&archive, b"data").unwrap();
        let (url, log) = mock(|r| match r.path.as_str() {
            "/ok" => (200, r#"{"message":"queued","id":12}"#.into()),
            "/bare" => (200, "{}".into()),
            "/dup" => (409, r#"{"error":"already queued"}"#.into()),
            _ => (500, "".into()),
        });
        assert_eq!(submit_door(&archive, &format!("{url}/ok")), json!({ "ok": true, "message": "queued", "id": 12 }));
        assert_eq!(submit_door(&archive, &format!("{url}/bare")), json!({ "ok": true, "message": "submitted to the door repository" }));
        assert_eq!(submit_door(&archive, &format!("{url}/dup")), json!({ "ok": false, "message": "already queued" }));
        assert_eq!(
            submit_door(&archive, &format!("{url}/x")),
            json!({ "ok": false, "message": "the door repository returned 500" })
        );
        let down = submit_door(&archive, "http://127.0.0.1:1/");
        assert!(down["message"].as_str().unwrap().starts_with("could not reach the door repository: "));
        let log = log.lock().unwrap();
        assert!(log[0].text().contains("name=\"file\"; filename=\"x.lha\""));
        assert!(log[0].header("x-release-key").is_none());
    }

    #[test]
    fn seed_shape_warnings_and_numbering_failure_ignored() {
        let seed = r#"{"members":[{"nickname":"a","role":"code"},{"nickname":"a","role":"Music, code"}],"nextReleaseNumber":5,
            "history":[],"currentDizText":null,"revision":2,"drafts":[{"code":"old","releaseNumber":4,"createdAt":0,"entryTitle":"Old"}]}"#;
        let (url, _) = mock(move |r| match r.path.as_str() {
            p if p.starts_with("/api/releases/seed") => (200, seed.to_string()),
            _ => (200, r#"{"checked":true,"ok":false,"summary":"Site is at 6."}"#.into()),
        });
        let s = backend(&url, "", "uprough").seed(10 * 86400).unwrap();
        assert_eq!(
            keys(&s),
            ["members", "boards", "nextReleaseNumber", "history", "currentDizText", "revision", "numberingWarning", "staleDraftWarning", "configWarning"]
        );
        assert_eq!(s["members"], json!([{ "nickname": "a", "role": "code, Music" }]));
        assert_eq!(s["boards"], json!([]));
        assert_eq!(s["currentDizText"], "");
        assert_eq!(s["numberingWarning"], "Site is at 6. Do not build until this is reconciled - the release number goes into the NFO.");
        assert_eq!(
            s["configWarning"],
            "The \"up\" profile has no server write key - publishing a release will fail. Put the key in its profile.json and press Reload profiles."
        );
        // key present, numbering endpoint down: no warning, seed still loads
        let (url, _) = mock(move |r| if r.path.starts_with("/api/releases/seed") { (200, seed.to_string()) } else { (500, "{}".into()) });
        let s = backend(&url, "k", "uprough").seed(0).unwrap();
        assert_eq!(keys(&s), ["members", "boards", "nextReleaseNumber", "history", "currentDizText", "revision"]);
    }

    #[test]
    fn create_preview_and_release_list() {
        let (url, log) = mock(|r| match (r.method.as_str(), r.path.as_str()) {
            ("POST", "/api/releases") => (200, r#"{"releaseNumber":8,"revision":1,"history":[{"releaseNumber":"8","filenames":"up-hypoc.lha","title":"T","category":"c","creators":"me"}]}"#.into()),
            (_, p) if p.starts_with("/api/releases/seed") => (200, r#"{"nextReleaseNumber":8,"history":[{"releaseNumber":"7","filenames":"f","title":"t","category":"c","creators":"x"}]}"#.into()),
            (_, p) if p.starts_with("/api/releases/manifest") => (200, r#"{"history":[{"releaseNumber":"oo7","filenames":"f","title":"t","dizText":"x\nUp Rough Presents:\n|  OLD NAME  |\n"}]}"#.into()),
            _ => (404, "{}".into()),
        });
        let b = backend(&url, "k", "xinnovations");
        let n = b.create(&form()).unwrap();
        assert_eq!((n.release_number, n.history.len(), n.history[0].filenames.as_str()), (8, 1, "up-hypoc.lha"));
        let p = b.preview(&form()).unwrap();
        assert_eq!(p.release_number, 8);
        assert_eq!((p.history[0].release_number.as_str(), p.history[0].filenames.as_str()), ("8", "up-hypoc.lha"));
        assert_eq!(p.history[1].release_number, "7");
        let mut f = form();
        f.diz_banner_text = "x\nUp Rough Presents:\n|  NEW NAME  |\n".into();
        let list = b.release_list(&f, 8).unwrap();
        assert_eq!((list[0].display_number.as_str(), list[1].display_number.as_str()), ("oo8", "oo7"));
        assert!(list[0].art.contains("NEW NAME") && list[1].art.contains("OLD NAME"));
        let posts = log.lock().unwrap().iter().filter(|r| r.method == "POST").count();
        assert_eq!(posts, 1); // preview never POSTs
        let up = backend(&url, "k", "uprough");
        assert!(up.release_list(&f, 8).unwrap().is_empty());
        assert!(up.after_build(Path::new("/nope")).unwrap().is_none());
    }

    #[test]
    fn after_build_submits_for_x_only() {
        let (url, _) = mock(|_| (200, "{}".into()));
        let dir = tempfile::tempdir().unwrap();
        let a = dir.path().join("a.lha");
        std::fs::write(&a, b"z").unwrap();
        let x = ServerBackend { division: "xinnovations", ..backend(&url, "k", "xinnovations") };
        // The production URL is fixed; here only the shape (a result object comes back) is checked offline-safe via submit_door.
        assert!(submit_door(&a, &url)["ok"].as_bool().unwrap());
        assert_eq!(x.division, "xinnovations");
    }

    #[test]
    fn none_profiles_have_no_server() {
        let dir = tempfile::tempdir().unwrap();
        std::fs::create_dir_all(dir.path().join("solo")).unwrap();
        std::fs::write(
            dir.path().join("solo/profile.json"),
            r#"{"id":"solo","label":"Solo","url":"u","filePrefix":"","renderer":"placeholder","backend":"none"}"#,
        )
        .unwrap();
        assert_eq!(check_code_core(dir.path(), "solo", "x").unwrap(), json!({ "exists": false }));
        let expect = json!({ "ok": false, "message": NO_SERVER_MESSAGE });
        assert_eq!(publish_to_web_core(dir.path(), "solo", "x", "a", "i", &[]), expect);
        assert_eq!(delete_release_core(dir.path(), "solo", "x"), expect);
        let missing = delete_release_core(dir.path(), "nope", "x");
        assert_eq!(missing["ok"], false);
    }

    #[test]
    fn x_banner_list_art() {
        assert_eq!(X_INNOVATIONS_BANNER.split('\n').count(), 10);
        let shipped: String = std::fs::read(concat!(env!("CARGO_MANIFEST_DIR"), "/../../app/test/fixtures/UP-MD15-FILE_ID.DIZ"))
            .unwrap()
            .iter()
            .map(|b| *b as char)
            .collect();
        let art = list_art_from_banner(&shipped);
        assert!(art.contains("mORE dOORS cURSOR mENU 1.5"));
        assert!(!art.contains("uPDATES 4 A bETTER eXPERIENCE!"));
        assert_eq!(art.split('\n').count(), 10);
        assert_eq!(list_art_from_banner(&art), art);
        assert_eq!(format_release_number(7, "xinnovations"), "oo7");
        assert_eq!(format_release_number(100, "xinnovations"), "1oo");
        assert_eq!(format_release_number(7, "uprough"), "7");
    }

    #[test]
    fn member_merge_keeps_order_and_dedupes_skills() {
        let m = |n: &str, r: &str| Member { nickname: n.into(), role: r.into() };
        let out = merge_member_skills(&[m("b", "code"), m(" ", "x"), m("a", "gfx"), m("b", "Code, music")]);
        assert_eq!(out.iter().map(|m| (m.nickname.as_str(), m.role.as_str())).collect::<Vec<_>>(), [("b", "code, music"), ("a", "gfx")]);
    }

    fn readme() -> String {
        "Short:        A demo\nUploader:     me@example.com (Me)\nType:         demo/aga\nArchitecture: m68k-amigaos\n\nbody\n".into()
    }

    #[test]
    fn readme_and_filename_checks() {
        assert!(aminet_readme_problems(&readme()).is_empty());
        let bad = "Name: x\nUploader: nope\nType: Demo\n\n".to_string() + &"x".repeat(79);
        assert_eq!(
            aminet_readme_problems(&bad),
            [
                "the first line must be \"Short:\"",
                "Short: is empty",
                "Uploader: needs an email address and a name, as \"you@example.com (Your Name)\"",
                "Type: must be an Aminet directory such as \"demo/aga\"",
                "Architecture: is empty",
                "line 5 is 79 characters; Aminet allows 78",
            ]
        );
        assert_eq!(aminet_readme_problems("Short: ".to_string().as_str())[0], "Short: is empty");
        assert!(aminet_readme_problems(&format!("Short: {}\nUploader: a@b.c (N)\nType: a/b\nArchitecture: g\n", "s".repeat(41)))[0]
            .starts_with("Short: is 41 characters; Aminet allows 40"));
        assert!(aminet_readme_problems("a\r\nb").contains(&"the readme must use LF line ends, not CR LF".to_string()));
        assert!(!uploader_ok("a@b (N)") && !uploader_ok("a@b.c ()") && uploader_ok("a@b.c (N)"));
        let n = aminet_file_names("UP-Hypoc.LHA");
        assert_eq!((n.archive.as_str(), n.readme.as_str()), ("up-hypoc.lha", "up-hypoc.readme"));
        let long = aminet_file_names(&format!("{}.lha", "a".repeat(30)));
        assert_eq!(
            aminet_file_name_problems(&long),
            [
                format!("\"{}.lha\" is 34 characters; Aminet allows 30", "a".repeat(30)),
                format!("\"{}.readme\" is 37 characters; Aminet allows 30", "a".repeat(30)),
            ]
        );
        assert_eq!(
            aminet_file_name_problems(&aminet_file_names("a b.lha")),
            ["\"a b.lha\" may only contain letters, digits, dots, underscores and hyphens", "\"a b.readme\" may only contain letters, digits, dots, underscores and hyphens"]
        );
        assert_eq!(normalize_readme_text("a\r\nb\rc"), "a\nb\nc\n");
    }

    #[test]
    fn aminet_upload_flow() {
        let dir = tempfile::tempdir().unwrap();
        let archive = dir.path().join("UP-Hypoc.lha");
        let calls: Mutex<Vec<Vec<String>>> = Mutex::new(vec![]);
        let req = json!({ "readmeText": readme().replace('\n', "\r\n"), "uploaderEmail": " me@example.com " });
        let ok = upload_to_aminet_core(&req, &archive, &|bin, args| {
            assert_eq!(bin, "curl");
            calls.lock().unwrap().push(args.to_vec());
            Ok(())
        });
        let readme_path = dir.path().join("up-hypoc.readme").display().to_string();
        assert_eq!(
            ok,
            json!({ "ok": true, "message": "uploaded up-hypoc.lha and up-hypoc.readme to Aminet (it appears once a moderator has looked at it)", "readmePath": readme_path })
        );
        assert_eq!(std::fs::read_to_string(&readme_path).unwrap(), readme());
        let calls = calls.lock().unwrap();
        assert_eq!(
            calls[0],
            ["--silent", "--show-error", "--fail", "--ftp-pasv", "--user", "anonymous:me@example.com", "--upload-file", &archive.display().to_string(), "ftp://main.aminet.net/new/up-hypoc.lha"]
        );
        assert_eq!(calls[1][7], readme_path);
        assert_eq!(calls[1][8], "ftp://main.aminet.net/new/up-hypoc.readme");

        let fail = upload_to_aminet_core(&req, &archive, &|_, _| Err("curl: (67) denied".into()));
        assert_eq!(fail["message"], "Aminet upload of up-hypoc.lha failed: curl: (67) denied");
        assert_eq!(fail["readmePath"], readme_path);

        let no_mail = json!({ "readmeText": readme(), "uploaderEmail": "nobody" });
        let r = upload_to_aminet_core(&no_mail, &archive, &|_, _| panic!("must not run"));
        assert_eq!(r, json!({ "ok": false, "message": "not uploaded to Aminet: the uploader email is missing" }));
        let bad = upload_to_aminet_core(&json!({ "readmeText": "x", "uploaderEmail": "a@b" }), &archive, &|_, _| panic!("must not run"));
        assert!(bad["message"].as_str().unwrap().starts_with("not uploaded to Aminet: the first line must be \"Short:\"; "));
        let unwritable = upload_to_aminet_core(&req, &dir.path().join("no/dir/a.lha"), &|_, _| Ok(()));
        assert!(unwritable["message"].as_str().unwrap().starts_with("not uploaded to Aminet: could not write "));
    }
}
