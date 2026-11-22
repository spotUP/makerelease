// Port of releases-client.ts, backend*.ts, door-repo-client.ts, aminet.ts and the network handlers.

#[tauri::command]
pub fn check_code_availability(profile_id: String, code: String) -> Result<serde_json::Value, String> {
    Err("not ported: check_code_availability".into())
}

#[tauri::command]
pub fn publish_to_web(profile_id: String, code: String, archive_path: String, image_path: String, embed_image_paths: Vec<String>) -> Result<serde_json::Value, String> {
    Err("not ported: publish_to_web".into())
}

#[tauri::command]
pub fn upload_to_aminet(request: serde_json::Value, archive_path: String) -> Result<serde_json::Value, String> {
    Err("not ported: upload_to_aminet".into())
}

#[tauri::command]
pub fn delete_release(profile_id: String, code: String) -> Result<serde_json::Value, String> {
    Err("not ported: delete_release".into())
}

