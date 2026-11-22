// Port of app/src/main/profiles.ts, example-profile.ts, output-root.ts, app/src/shared/profile.ts
// and the profile/dialog handlers in ipc-handlers.ts. JSON shapes match the TypeScript handlers.
use serde_json::{json, Map, Value};
use std::fs;
use std::path::{Path, PathBuf};
use tauri::{AppHandle, Manager};
use tauri_plugin_dialog::DialogExt;

const PROFILE_FILE: &str = "profile.json";
const NFO_FILE: &str = "release.nfo";
const DIZ_FILE: &str = "file_id.diz";
const EXAMPLE_ID: &str = "example";

const EXAMPLE_NFO: &str = "            {{group}}  -  {{url}}\n\n  Release ..: {{release_name}} ({{platform}})\n  Code .....: {{code}} ({{format}})\n  File .....: {{filename}}\n  Number ...: {{release_number}}\n  Date .....: {{date}}\n\n  {{description}}\n  By {{creators}}\n\n  -- notes -------------------------------------------------------------\n  {{notes}}\n\n  -- news --------------------------------------------------------------\n  {{news}}\n\n  -- members -----------------------------------------------------------\n  {{members}}\n\n  -- credits -----------------------------------------------------------\n  {{credits}}\n\n                 {{group}} - {{url}}\n";
const EXAMPLE_DIZ: &str = " ____________________________________________\n|                                            |\n|  {{group                                }} |\n|  {{release_name                         }} |\n|  {{platform            }} {{date       }}  |\n|____________________________________________|\n";

type Res<T> = Result<T, String>;

pub fn is_profile_id(s: &str) -> bool {
    !s.is_empty() && s.bytes().all(|b| b.is_ascii_lowercase() || b.is_ascii_digit() || b == b'-')
}

fn bad_id(id: &str) -> String {
    format!("the folder name \"{id}\" must use only a-z, 0-9 and \"-\"")
}

// ---- shared/profile.ts ----

/// Validates raw profile.json content; the error text names the file and field like parseProfile.
pub fn parse_profile(raw: &Value, id: &str, file: &str) -> Res<Value> {
    let fail = |field: &str, problem: &str| -> String { format!("{file}: \"{field}\" {problem}") };
    if !is_profile_id(id) {
        return Err(format!("{file}: {}", bad_id(id)));
    }
    let obj = raw.as_object().ok_or_else(|| format!("{file}: must be a JSON object"))?;
    let text = |field: &str, required: bool| -> Res<String> {
        match obj.get(field) {
            None => if required { Err(fail(field, "is missing")) } else { Ok(String::new()) },
            Some(Value::String(s)) => {
                if required && s.trim().is_empty() { Err(fail(field, "must not be empty")) } else { Ok(s.clone()) }
            }
            Some(_) => Err(fail(field, "must be a string")),
        }
    };
    let label = text("label", true)?;
    let url = text("url", true)?;
    let file_prefix = text("filePrefix", false)?;
    if file_prefix.chars().any(|c| c.is_whitespace() || "\\/:*?\"<>|".contains(c)) {
        return Err(fail("filePrefix", "must not contain spaces or any of \\ / : * ? \" < > |"));
    }
    let mut code_length = 5i64;
    if let Some(v) = obj.get("codeLength") {
        match v.as_f64() {
            Some(n) if v.is_number() && n.fract() == 0.0 && (1.0..=32.0).contains(&n) => code_length = n as i64,
            _ => return Err(fail("codeLength", "must be a whole number from 1 to 32")),
        }
    }
    let renderer = match obj.get("renderer") {
        Some(Value::String(s)) if ["uprough", "xinnovations", "placeholder"].contains(&s.as_str()) => s.clone(),
        _ => return Err(fail("renderer", "must be one of \"uprough\", \"xinnovations\", \"placeholder\"")),
    };
    let backend = match obj.get("backend") {
        None | Some(Value::Null) => "none".to_string(),
        Some(Value::String(s)) if ["none", "releases-doorserver"].contains(&s.as_str()) => s.clone(),
        _ => return Err(fail("backend", "must be one of \"none\", \"releases-doorserver\"")),
    };
    let mut server: Option<Value> = None;
    if backend == "releases-doorserver" {
        if renderer == "placeholder" {
            return Err(fail("backend", "can be \"releases-doorserver\" only with renderer \"uprough\" or \"xinnovations\""));
        }
        let s = obj.get("server").and_then(Value::as_object)
            .ok_or_else(|| fail("server", "is required when backend is \"releases-doorserver\""))?;
        let base = match s.get("baseUrl") {
            Some(Value::String(b)) if !b.trim().is_empty() => b.clone(),
            _ => return Err(fail("server.baseUrl", "must be a non-empty string")),
        };
        let key = match s.get("writeKey") {
            None | Some(Value::Null) => String::new(),
            Some(Value::String(k)) => k.clone(),
            _ => return Err(fail("server.writeKey", "must be a string")),
        };
        server = Some(json!({ "baseUrl": base.trim_end_matches('/'), "writeKey": key }));
    } else if obj.contains_key("server") {
        return Err(fail("server", "is allowed only when backend is \"releases-doorserver\""));
    }
    let mut members = Vec::new();
    if let Some(m) = obj.get("members") {
        let list = m.as_array().ok_or_else(|| fail("members", "must be a list"))?;
        for (i, e) in list.iter().enumerate() {
            match (e.get("nickname").and_then(Value::as_str), e.get("role").and_then(Value::as_str)) {
                (Some(n), Some(r)) if e.is_object() => members.push(json!({ "nickname": n, "role": r })),
                _ => return Err(fail(&format!("members[{i}]"), "must be an object with string \"nickname\" and \"role\"")),
            }
        }
    }
    let mut out = Map::new();
    out.insert("id".into(), json!(id));
    out.insert("label".into(), json!(label));
    out.insert("url".into(), json!(url));
    out.insert("filePrefix".into(), json!(file_prefix));
    out.insert("codeLength".into(), json!(code_length));
    out.insert("renderer".into(), json!(renderer));
    out.insert("backend".into(), json!(backend));
    if let Some(s) = server {
        out.insert("server".into(), s);
    }
    out.insert("members".into(), Value::Array(members));
    Ok(Value::Object(out))
}

/// The one on-disk format of profile.json: fixed key order, two-space indent, trailing newline, no id.
pub fn serialize_profile(p: &Value) -> String {
    let mut body = Map::new();
    for k in ["label", "url", "filePrefix", "codeLength", "renderer", "backend"] {
        if let Some(v) = p.get(k) {
            body.insert(k.into(), v.clone());
        }
    }
    if let Some(s) = p.get("server").filter(|s| !s.is_null()) {
        body.insert("server".into(), json!({ "baseUrl": s["baseUrl"], "writeKey": s["writeKey"] }));
    }
    let members: Vec<Value> = p.get("members").and_then(Value::as_array).map(|m| {
        m.iter().map(|e| json!({ "nickname": e["nickname"], "role": e["role"] })).collect()
    }).unwrap_or_default();
    body.insert("members".into(), Value::Array(members));
    format!("{}\n", serde_json::to_string_pretty(&Value::Object(body)).unwrap())
}

pub fn summarize(p: &Value) -> Value {
    let mut m = Map::new();
    for k in ["id", "label", "url", "filePrefix", "codeLength", "renderer", "backend"] {
        m.insert(k.into(), p[k].clone());
    }
    Value::Object(m)
}

pub fn slugify_profile_id(label: &str, taken: &[String]) -> String {
    let mut base = String::new();
    let mut dash = false;
    for c in label.to_lowercase().chars() {
        if c.is_ascii_lowercase() || c.is_ascii_digit() {
            if dash && !base.is_empty() { base.push('-'); }
            dash = false;
            base.push(c);
        } else {
            dash = true;
        }
    }
    if base.is_empty() { base = "group".into(); }
    let mut id = base.clone();
    let mut n = 2;
    while taken.contains(&id) { id = format!("{base}-{n}"); n += 1; }
    id
}

// ---- profiles.ts ----

fn latin1_to_string(b: &[u8]) -> String { b.iter().map(|&c| c as char).collect() }

fn write_latin1(file: &Path, text: &str) -> Res<()> {
    let mut bytes = Vec::with_capacity(text.len());
    for (pos, c) in text.chars().enumerate() {
        if c as u32 > 0xff {
            return Err(format!(
                "Text contains \"{c}\" (U+{:04X}) at character {pos}, which is outside the Latin-1 range (0-255) and would be silently corrupted if written. Replace it with a plain Latin-1 character (e.g. a straight quote ' instead of a curly one) and try again.",
                c as u32
            ));
        }
        bytes.push(c as u32 as u8);
    }
    fs::write(file, bytes).map_err(|e| e.to_string())
}

fn normalise_line_endings(s: &str) -> String { s.replace("\r\n", "\n").replace('\r', "\n") }

pub fn load_profile(dir: &Path, id: &str) -> Res<Value> {
    let file = dir.join(id).join(PROFILE_FILE);
    let fs_ = file.display().to_string();
    if !is_profile_id(id) {
        return Err(format!("{fs_}: {}", bad_id(id)));
    }
    let text = fs::read_to_string(&file).map_err(|e| format!("{fs_}: could not be read ({e})"))?;
    let raw: Value = serde_json::from_str(text.trim_start_matches('\u{feff}'))
        .map_err(|e| format!("{fs_}: is not valid JSON ({e})"))?;
    parse_profile(&raw, id, &fs_)
}

fn folder_ids(dir: &Path) -> Option<Vec<String>> {
    let mut ids: Vec<String> = fs::read_dir(dir).ok()?.flatten()
        .filter(|e| e.path().is_dir())
        .filter_map(|e| e.file_name().into_string().ok())
        .filter(|n| !n.starts_with('.')).collect();
    ids.sort();
    Some(ids)
}

/// Every folder under dir, sorted; a broken profile is `{id, error}` in place.
pub fn list_profile_entries(dir: &Path) -> Vec<Value> {
    folder_ids(dir).unwrap_or_default().into_iter()
        .map(|id| load_profile(dir, &id).unwrap_or_else(|error| json!({ "id": id, "error": error })))
        .collect()
}

pub fn read_template_file(file: &Path, optional: bool) -> Res<Option<String>> {
    match fs::read(file) {
        Ok(b) => Ok(Some(normalise_line_endings(&latin1_to_string(&b)))),
        Err(e) if optional && e.kind() == std::io::ErrorKind::NotFound => Ok(None),
        Err(e) => Err(format!("{}: could not be read ({e})", file.display())),
    }
}

fn write_profile_file(file: &Path, text: &str) -> Res<()> {
    let tmp = PathBuf::from(format!("{}.tmp", file.display()));
    fs::write(&tmp, text).map_err(|e| e.to_string())?;
    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        fs::set_permissions(&tmp, fs::Permissions::from_mode(0o600)).map_err(|e| e.to_string())?;
    }
    fs::rename(&tmp, file).map_err(|e| e.to_string())
}

fn outcome(r: Res<()>) -> Value {
    match r { Ok(()) => json!({ "ok": true }), Err(error) => json!({ "ok": false, "error": error }) }
}

pub fn save_profile_in(dir: &Path, req: &Value) -> Value {
    outcome((|| {
        let id = req["id"].as_str().unwrap_or("undefined");
        let d = dir.join(id);
        let file = d.join(PROFILE_FILE);
        if !is_profile_id(id) {
            return Err(format!("{}: {}", file.display(), bad_id(id)));
        }
        let is_new = req["isNew"].as_bool().unwrap_or(false);
        if is_new && d.exists() { return Err(format!("A group with id {id} already exists")); }
        if !is_new && !d.exists() { return Err(format!("No group with id {id} exists")); }
        let text = serialize_profile(&req["profile"]);
        parse_profile(&serde_json::from_str(&text).unwrap(), id, &file.display().to_string())?;
        fs::create_dir_all(&d).map_err(|e| e.to_string())?;
        write_profile_file(&file, &text)?;
        if let Some(t) = req.get("templates").filter(|t| t.is_object()) {
            write_latin1(&d.join(DIZ_FILE), t["diz"].as_str().unwrap_or(""))?;
            match t["nfo"].as_str() {
                None => { let _ = fs::remove_file(d.join(NFO_FILE)); }
                Some(n) => write_latin1(&d.join(NFO_FILE), n)?,
            }
        }
        Ok(())
    })())
}

pub fn delete_profile_in(dir: &Path, id: &str, trash: impl Fn(&Path) -> Res<()>) -> Value {
    outcome((|| {
        if !is_profile_id(id) { return Err(bad_id(id)); }
        let d = dir.join(id);
        if !d.exists() { return Err(format!("No group with id {id} exists")); }
        if folder_ids(dir).map_or(0, |v| v.len()) <= 1 { return Err("The last group cannot be deleted".into()); }
        trash(&d)
    })())
}

fn copy_dir(from: &Path, to: &Path) -> std::io::Result<()> {
    fs::create_dir_all(to)?;
    for e in fs::read_dir(from)? {
        let e = e?;
        let t = to.join(e.file_name());
        if e.path().is_dir() { copy_dir(&e.path(), &t)?; } else { fs::copy(e.path(), t)?; }
    }
    Ok(())
}

/// Copies bundled groups missing from `dir`; ids are recorded in `.seeded` so a deleted group stays deleted.
pub fn seed_private_profiles(bundled: &Path, dir: &Path) -> Res<Vec<String>> {
    let Some(ids) = folder_ids(bundled) else { return Ok(vec![]) };
    let marker = dir.join(".seeded");
    let mut done: std::collections::BTreeSet<String> = fs::read_to_string(&marker).unwrap_or_default()
        .lines().filter(|l| !l.is_empty()).map(String::from).collect();
    let mut seeded = vec![];
    for id in ids {
        if !is_profile_id(&id) {
            return Err(format!("{}: {}", bundled.join(&id).display(), bad_id(&id)));
        }
        if done.contains(&id) || dir.join(&id).exists() { done.insert(id); continue; }
        load_profile(bundled, &id)?;
        copy_dir(&bundled.join(&id), &dir.join(&id)).map_err(|e| e.to_string())?;
        done.insert(id.clone());
        seeded.push(id);
    }
    fs::create_dir_all(dir).map_err(|e| e.to_string())?;
    let body: Vec<String> = done.into_iter().collect();
    fs::write(&marker, format!("{}\n", body.join("\n"))).map_err(|e| e.to_string())?;
    Ok(seeded)
}

/// Writes the example profile on first launch only (the folder not existing yet).
pub fn ensure_example_profile(dir: &Path) -> Res<()> {
    if dir.exists() { return Ok(()); }
    let d = dir.join(EXAMPLE_ID);
    fs::create_dir_all(&d).map_err(|e| e.to_string())?;
    let profile = json!({ "label": "Example Group", "url": "https://example.net", "filePrefix": "EX-", "codeLength": 5,
        "renderer": "placeholder", "backend": "none", "members": [{ "nickname": "coder", "role": "code" }] });
    write_profile_file(&d.join(PROFILE_FILE), &serialize_profile(&profile))?;
    write_latin1(&d.join(NFO_FILE), EXAMPLE_NFO)?;
    write_latin1(&d.join(DIZ_FILE), EXAMPLE_DIZ)
}

// ---- output-root.ts ----

/// First of documents, home, userData that resolves non-empty, joined with folder_name.
pub fn resolve_packaged_output_root(lookup: impl Fn(&str) -> Option<PathBuf>, folder_name: &str) -> Res<PathBuf> {
    for loc in ["documents", "home", "userData"] {
        if let Some(base) = lookup(loc).filter(|b| !b.as_os_str().is_empty()) {
            return Ok(base.join(folder_name));
        }
    }
    Err(format!("Could not resolve any of documents, home, userData to put the \"{folder_name}\" folder in"))
}

pub fn output_root_for(app: &AppHandle, profile: &Value) -> Res<PathBuf> {
    let id = profile["id"].as_str().unwrap_or_default();
    if cfg!(debug_assertions) {
        // Dev: <repo root>/releases-out/<id>; this crate sits at <repo>/tauri/src-tauri.
        return Ok(Path::new(env!("CARGO_MANIFEST_DIR")).join("..").join("..").join("releases-out").join(id));
    }
    let p = app.path();
    resolve_packaged_output_root(
        |name| match name {
            "documents" => p.document_dir().ok(),
            "home" => p.home_dir().ok(),
            _ => user_data_dir(app).ok(),
        },
        &format!("{} Releases", profile["label"].as_str().unwrap_or_default()),
    )
}

/// Electron's userData: Application Support / %APPDATA% / ~/.config, joined with "makerelease".
fn user_data_dir(app: &AppHandle) -> Res<PathBuf> {
    let p = app.path();
    let base = if cfg!(target_os = "linux") { p.config_dir() } else { p.data_dir() };
    base.map(|b| b.join("makerelease")).map_err(|e| e.to_string())
}

pub fn profiles_dir(app: &AppHandle) -> Res<PathBuf> { Ok(user_data_dir(app)?.join("profiles")) }

/// First-launch seeding, same as index.ts: private bundle when present, else the example.
pub fn prepare_profiles(app: &AppHandle) -> Res<()> {
    let dir = profiles_dir(app)?;
    let bundled = std::env::var_os("PRIVATE_PROFILES_DIR").map(PathBuf::from)
        .or_else(|| app.path().resource_dir().ok().map(|r| r.join("private-profiles")))
        .unwrap_or_default();
    if bundled.exists() { seed_private_profiles(&bundled, &dir).map(|_| ()) } else { ensure_example_profile(&dir) }
}

// ---- seed ----

/// What noneBackend().seed() returns plus the profile summary and output root.
pub fn seed_state(profile: &Value, output_root: &Path) -> Res<Value> {
    let mut state = json!({ "profile": summarize(profile), "outputRoot": output_root.display().to_string() });
    let rest = if profile["backend"] != "none" {
        crate::net::server_seed(profile)?
    } else {
        json!({ "members": profile["members"], "boards": [], "nextReleaseNumber": 0, "history": [], "currentDizText": "", "revision": 0 })
    };
    state.as_object_mut().unwrap().extend(rest.as_object().unwrap().clone());
    Ok(state)
}

fn edit_state(dir: &Path, id: &str) -> Res<Value> {
    let profile = load_profile(dir, id)?;
    let templates = if profile["renderer"] == "placeholder" {
        let nfo = read_template_file(&dir.join(id).join(NFO_FILE), true)?;
        let diz = read_template_file(&dir.join(id).join(DIZ_FILE), false)?.unwrap_or_default();
        json!({ "nfo": nfo, "diz": diz })
    } else {
        json!({ "nfo": null, "diz": "" })
    };
    Ok(json!({ "profile": profile, "templates": templates }))
}

fn draft_state(dir: &Path) -> Value {
    let taken: Vec<String> = list_profile_entries(dir).iter().filter_map(|e| e["id"].as_str().map(String::from)).collect();
    json!({
        "profile": { "id": slugify_profile_id("new group", &taken), "label": "New Group", "url": "", "filePrefix": "",
            "codeLength": 5, "renderer": "placeholder", "backend": "none", "members": [] },
        "templates": { "nfo": EXAMPLE_NFO, "diz": EXAMPLE_DIZ },
    })
}

fn list_summaries(dir: &Path) -> Vec<Value> {
    list_profile_entries(dir).into_iter().map(|e| if e.get("error").is_some() { e } else { summarize(&e) }).collect()
}

// ---- commands ----

#[tauri::command]
pub fn list_profiles(app: AppHandle) -> Result<Value, String> {
    Ok(Value::Array(list_summaries(&profiles_dir(&app)?)))
}

#[tauri::command]
pub fn open_profiles_folder(app: AppHandle) -> Result<Value, String> {
    let dir = profiles_dir(&app)?;
    fs::create_dir_all(&dir).map_err(|e| e.to_string())?;
    let opener = if cfg!(target_os = "macos") { "open" } else if cfg!(windows) { "explorer" } else { "xdg-open" };
    match std::process::Command::new(opener).arg(&dir).status() {
        // explorer exits 1 even on success.
        Ok(s) if s.success() || cfg!(windows) => Ok(Value::Null),
        Ok(s) => Err(format!("Could not open {}: {s}", dir.display())),
        Err(e) => Err(format!("Could not open {}: {e}", dir.display())),
    }
}

#[tauri::command]
pub fn new_profile_draft(app: AppHandle) -> Result<Value, String> {
    Ok(draft_state(&profiles_dir(&app)?))
}

#[tauri::command]
pub fn load_profile_for_edit(app: AppHandle, id: String) -> Result<Value, String> {
    edit_state(&profiles_dir(&app)?, &id)
}

#[tauri::command]
pub fn save_profile(app: AppHandle, request: Value) -> Result<Value, String> {
    Ok(save_profile_in(&profiles_dir(&app)?, &request))
}

#[tauri::command]
pub fn delete_profile(app: AppHandle, id: String) -> Result<Value, String> {
    Ok(delete_profile_in(&profiles_dir(&app)?, &id, |f| trash::delete(f).map_err(|e| e.to_string())))
}

#[tauri::command]
pub async fn import_template(app: AppHandle) -> Result<Value, String> {
    let picked = app.dialog().file()
        .add_filter("Templates", &["nfo", "diz", "txt"])
        .blocking_pick_file();
    let Some(file) = picked else { return Ok(Value::Null) };
    let path = file.into_path().map_err(|e| e.to_string())?;
    let text = read_template_file(&path, false)?.unwrap_or_default();
    let name = path.file_name().map(|n| n.to_string_lossy().into_owned()).unwrap_or_default();
    Ok(json!({ "text": text, "name": name }))
}

#[tauri::command]
pub async fn load_seed_state(app: AppHandle, profile_id: String) -> Result<Value, String> {
    // The server seed is a blocking HTTP round trip; keep it off the async runtime.
    tauri::async_runtime::spawn_blocking(move || {
        let profile = load_profile(&profiles_dir(&app)?, &profile_id)?;
        seed_state(&profile, &output_root_for(&app, &profile)?)
    })
    .await
    .map_err(|e| e.to_string())?
}

#[tauri::command]
pub async fn pick_files_and_dirs(app: AppHandle) -> Result<Value, String> {
    // tauri-plugin-dialog cannot offer files and folders in one dialog (Electron can): ask which first.
    let files = app
        .dialog()
        .message("Add files or folders to the release?")
        .buttons(tauri_plugin_dialog::MessageDialogButtons::OkCancelCustom("Files".into(), "Folders".into()))
        .blocking_show();
    let picked = if files {
        app.dialog().file().blocking_pick_files().unwrap_or_default()
    } else {
        app.dialog().file().blocking_pick_folders().unwrap_or_default()
    };
    let paths: Result<Vec<String>, String> = picked.into_iter()
        .map(|f| f.into_path().map(|p| p.display().to_string()).map_err(|e| e.to_string())).collect();
    Ok(json!(paths?))
}

#[tauri::command]
pub async fn pick_image(app: AppHandle) -> Result<Value, String> {
    let picked = app.dialog().file().add_filter("Images", &["png", "jpg", "jpeg", "gif"]).blocking_pick_file();
    match picked {
        None => Ok(Value::Null),
        Some(f) => Ok(json!(f.into_path().map_err(|e| e.to_string())?.display().to_string())),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn tmp() -> tempfile::TempDir { tempfile::tempdir().unwrap() }

    #[test]
    fn first_launch_lists_example_and_reads_disk_edits() {
        let t = tmp();
        let dir = t.path().join("profiles");
        ensure_example_profile(&dir).unwrap();
        assert_eq!(list_summaries(&dir), vec![json!({ "id": "example", "label": "Example Group", "url": "https://example.net",
            "filePrefix": "EX-", "codeLength": 5, "renderer": "placeholder", "backend": "none" })]);
        assert_eq!(fs::read(dir.join("example/file_id.diz")).unwrap(), EXAMPLE_DIZ.as_bytes());
        let f = dir.join("example/profile.json");
        let mut j: Value = serde_json::from_str(&fs::read_to_string(&f).unwrap()).unwrap();
        j["filePrefix"] = json!("SENTINEL-");
        fs::write(&f, j.to_string()).unwrap();
        let seed = seed_state(&load_profile(&dir, "example").unwrap(), &t.path().join("out")).unwrap();
        assert_eq!(seed["profile"]["filePrefix"], "SENTINEL-");
        assert_eq!(seed["outputRoot"], t.path().join("out").display().to_string());
        assert_eq!(list_summaries(&dir)[0]["filePrefix"], "SENTINEL-");
        // Existing folder: a deleted example stays deleted.
        fs::remove_dir_all(dir.join("example")).unwrap();
        ensure_example_profile(&dir).unwrap();
        assert!(!dir.join("example").exists());
    }

    #[test]
    fn example_profile_json_bytes() {
        let t = tmp();
        let dir = t.path().join("p");
        ensure_example_profile(&dir).unwrap();
        assert_eq!(fs::read_to_string(dir.join("example/profile.json")).unwrap(),
            "{\n  \"label\": \"Example Group\",\n  \"url\": \"https://example.net\",\n  \"filePrefix\": \"EX-\",\n  \"codeLength\": 5,\n  \"renderer\": \"placeholder\",\n  \"backend\": \"none\",\n  \"members\": [\n    {\n      \"nickname\": \"coder\",\n      \"role\": \"code\"\n    }\n  ]\n}\n");
    }

    #[test]
    fn editor_flow_draft_save_list_seed_delete() {
        let t = tmp();
        let dir = t.path().join("profiles");
        ensure_example_profile(&dir).unwrap();
        let draft = draft_state(&dir);
        assert_eq!(draft["profile"]["id"], "new-group");
        assert_eq!(draft["profile"]["label"], "New Group");
        assert_eq!(draft["templates"]["nfo"], EXAMPLE_NFO);
        let mut profile = draft["profile"].clone();
        profile.as_object_mut().unwrap().remove("id");
        profile["label"] = json!("SENTINEL Group");
        profile["url"] = json!("https://sentinel.net");
        profile["filePrefix"] = json!("SENTINEL-");
        let req = json!({ "id": "sentinel-group", "isNew": true, "profile": profile,
            "templates": { "nfo": null, "diz": "SENTINEL-DIZ {{release_name}}\n" } });
        assert_eq!(save_profile_in(&dir, &req), json!({ "ok": true }));
        // isNew on an existing id is refused; an edit of a missing id too.
        assert_eq!(save_profile_in(&dir, &req)["error"], "A group with id sentinel-group already exists");
        let mut missing = req.clone();
        missing["id"] = json!("nope");
        missing["isNew"] = json!(false);
        assert_eq!(save_profile_in(&dir, &missing)["error"], "No group with id nope exists");
        let ids: Vec<_> = list_summaries(&dir).iter().map(|e| e["id"].clone()).collect();
        assert!(ids.contains(&json!("sentinel-group")));
        assert!(!dir.join("sentinel-group/release.nfo").exists());
        let st = edit_state(&dir, "sentinel-group").unwrap();
        assert_eq!(st["templates"], json!({ "nfo": null, "diz": "SENTINEL-DIZ {{release_name}}\n" }));
        let seed = seed_state(&st["profile"], &t.path().join("out")).unwrap();
        assert_eq!(seed["profile"]["filePrefix"], "SENTINEL-");
        // Delete goes through the trash callback with the group folder.
        let seen = std::cell::RefCell::new(None);
        let r = delete_profile_in(&dir, "sentinel-group", |f| { *seen.borrow_mut() = Some(f.to_path_buf()); fs::remove_dir_all(f).map_err(|e| e.to_string()) });
        assert_eq!(r, json!({ "ok": true }));
        assert_eq!(seen.into_inner().unwrap(), dir.join("sentinel-group"));
        assert!(!list_summaries(&dir).iter().any(|e| e["id"] == "sentinel-group"));
        // The last group cannot be deleted.
        assert_eq!(delete_profile_in(&dir, "example", |_| Ok(()))["error"], "The last group cannot be deleted");
    }

    #[test]
    fn latin1_templates_round_trip_and_reject_wider_chars() {
        let t = tmp();
        let f = t.path().join("a.nfo");
        write_latin1(&f, "Fj\u{e4}lldata\n").unwrap();
        assert_eq!(fs::read(&f).unwrap(), b"Fj\xe4lldata\n");
        assert_eq!(read_template_file(&f, false).unwrap().unwrap(), "Fj\u{e4}lldata\n");
        fs::write(&f, b"a\r\nb\rc").unwrap();
        assert_eq!(read_template_file(&f, false).unwrap().unwrap(), "a\nb\nc");
        assert!(write_latin1(&f, "it\u{2019}s").unwrap_err().contains("U+2019"));
        assert_eq!(read_template_file(&t.path().join("none"), true).unwrap(), None);
    }

    #[test]
    fn parse_errors_name_file_and_field() {
        let f = "/x/profile.json";
        let e = |raw: Value| parse_profile(&raw, "ab", f).unwrap_err();
        assert_eq!(e(json!({})), format!("{f}: \"label\" is missing"));
        assert_eq!(e(json!({ "label": " ", "url": "u" })), format!("{f}: \"label\" must not be empty"));
        assert_eq!(e(json!({ "label": "a", "url": "u", "codeLength": 0, "renderer": "placeholder" })),
            format!("{f}: \"codeLength\" must be a whole number from 1 to 32"));
        assert_eq!(e(json!({ "label": "a", "url": "u", "renderer": "x" })),
            format!("{f}: \"renderer\" must be one of \"uprough\", \"xinnovations\", \"placeholder\""));
        assert_eq!(e(json!({ "label": "a", "url": "u", "renderer": "uprough", "backend": "releases-doorserver" })),
            format!("{f}: \"server\" is required when backend is \"releases-doorserver\""));
        assert_eq!(e(json!({ "label": "a", "url": "u", "renderer": "uprough", "server": {} })),
            format!("{f}: \"server\" is allowed only when backend is \"releases-doorserver\""));
        assert_eq!(e(json!({ "label": "a", "url": "u", "filePrefix": "a b", "renderer": "uprough" })),
            format!("{f}: \"filePrefix\" must not contain spaces or any of \\ / : * ? \" < > |"));
        let ok = parse_profile(&json!({ "label": "a", "url": "u", "renderer": "uprough", "backend": "releases-doorserver",
            "server": { "baseUrl": "http://h//", "writeKey": "k" } }), "ab", f).unwrap();
        assert_eq!(ok["server"], json!({ "baseUrl": "http://h", "writeKey": "k" }));
        assert!(parse_profile(&json!({}), "../x", f).unwrap_err().contains("must use only a-z"));
    }

    #[test]
    fn broken_profile_is_listed_in_place_and_bom_is_accepted() {
        let t = tmp();
        fs::create_dir_all(t.path().join("bad")).unwrap();
        fs::write(t.path().join("bad/profile.json"), "{nope").unwrap();
        fs::create_dir_all(t.path().join("bom")).unwrap();
        fs::write(t.path().join("bom/profile.json"), "\u{feff}{\"label\":\"B\",\"url\":\"u\",\"renderer\":\"placeholder\"}").unwrap();
        fs::create_dir_all(t.path().join(".hidden")).unwrap();
        let l = list_summaries(t.path());
        assert_eq!(l.len(), 2);
        assert_eq!(l[0]["id"], "bad");
        assert!(l[0]["error"].as_str().unwrap().contains("is not valid JSON"));
        assert_eq!(l[1]["label"], "B");
        assert!(list_summaries(&t.path().join("missing")).is_empty());
    }

    #[test]
    fn none_backend_seed_shape_and_server_seed_deferred() {
        let p = json!({ "id": "e", "label": "E", "url": "u", "filePrefix": "", "codeLength": 5, "renderer": "placeholder",
            "backend": "none", "members": [{ "nickname": "coder", "role": "code" }] });
        let s = seed_state(&p, Path::new("/o")).unwrap();
        assert_eq!(s["members"], p["members"]);
        assert_eq!((&s["boards"], &s["history"], &s["currentDizText"], &s["nextReleaseNumber"], &s["revision"]),
            (&json!([]), &json!([]), &json!(""), &json!(0), &json!(0)));
        let mut srv = p.clone();
        srv["backend"] = json!("releases-doorserver");
        srv["server"] = json!({ "baseUrl": "http://127.0.0.1:1", "writeKey": "k" });
        srv["renderer"] = json!("uprough");
        assert!(seed_state(&srv, Path::new("/o")).unwrap_err().starts_with("could not reach releases.uprough.net: "));
    }

    #[test]
    fn private_seeding_never_overwrites_or_resurrects() {
        let t = tmp();
        let (b, d) = (t.path().join("bundled"), t.path().join("profiles"));
        for id in ["alpha", "beta"] {
            fs::create_dir_all(b.join(id)).unwrap();
            fs::write(b.join(id).join("profile.json"), "{\"label\":\"L\",\"url\":\"u\",\"renderer\":\"uprough\"}").unwrap();
        }
        fs::create_dir_all(d.join("alpha")).unwrap();
        assert_eq!(seed_private_profiles(&b, &d).unwrap(), vec!["beta".to_string()]);
        assert_eq!(fs::read_to_string(d.join(".seeded")).unwrap(), "alpha\nbeta\n");
        fs::remove_dir_all(d.join("beta")).unwrap();
        assert!(seed_private_profiles(&b, &d).unwrap().is_empty());
        assert!(!d.join("beta").exists());
        assert!(seed_private_profiles(&t.path().join("none"), &d).unwrap().is_empty());
    }

    #[test]
    fn output_root_fallbacks_match_output_root_spec() {
        let f = |docs: Option<&'static str>, home: Option<&'static str>, ud: Option<&'static str>| {
            let m = [("documents", docs), ("home", home), ("userData", ud)];
            move |n: &str| m.iter().find(|(k, _)| *k == n).and_then(|(_, v)| v.map(PathBuf::from))
        };
        let r = |l| resolve_packaged_output_root(l, "Up Rough Releases");
        assert_eq!(r(f(Some("/h/Documents"), Some("/h"), Some("/h/.config/makerelease"))).unwrap(), PathBuf::from("/h/Documents/Up Rough Releases"));
        assert_eq!(r(f(None, Some("/h"), None)).unwrap(), PathBuf::from("/h/Up Rough Releases"));
        assert_eq!(r(f(None, None, Some("/h/.config/makerelease"))).unwrap(), PathBuf::from("/h/.config/makerelease/Up Rough Releases"));
        assert_eq!(r(f(Some(""), Some("/h"), None)).unwrap(), PathBuf::from("/h/Up Rough Releases"));
        assert!(r(f(None, None, None)).unwrap_err().contains("documents, home, userData"));
    }

    #[test]
    fn slugify_matches_ts() {
        assert_eq!(slugify_profile_id("new group", &[]), "new-group");
        assert_eq!(slugify_profile_id("new group", &["new-group".into(), "new-group-2".into()]), "new-group-3");
        assert_eq!(slugify_profile_id("  !! ", &[]), "group");
    }
}
