// Port of app/src/main/profiles.ts, example-profile.ts, output-root.ts and the profile/dialog handlers in ipc-handlers.ts.

#[tauri::command]
pub fn list_profiles() -> Result<serde_json::Value, String> {
    Err("not ported: list_profiles".into())
}

#[tauri::command]
pub fn open_profiles_folder() -> Result<serde_json::Value, String> {
    Err("not ported: open_profiles_folder".into())
}

#[tauri::command]
pub fn new_profile_draft() -> Result<serde_json::Value, String> {
    Err("not ported: new_profile_draft".into())
}

#[tauri::command]
pub fn load_profile_for_edit(id: String) -> Result<serde_json::Value, String> {
    Err("not ported: load_profile_for_edit".into())
}

#[tauri::command]
pub fn save_profile(request: serde_json::Value) -> Result<serde_json::Value, String> {
    Err("not ported: save_profile".into())
}

#[tauri::command]
pub fn delete_profile(id: String) -> Result<serde_json::Value, String> {
    Err("not ported: delete_profile".into())
}

#[tauri::command]
pub fn import_template() -> Result<serde_json::Value, String> {
    Err("not ported: import_template".into())
}

#[tauri::command]
pub fn load_seed_state(profile_id: String) -> Result<serde_json::Value, String> {
    Err("not ported: load_seed_state".into())
}

#[tauri::command]
pub fn pick_files_and_dirs() -> Result<serde_json::Value, String> {
    Err("not ported: pick_files_and_dirs".into())
}

#[tauri::command]
pub fn pick_image() -> Result<serde_json::Value, String> {
    Err("not ported: pick_image".into())
}

