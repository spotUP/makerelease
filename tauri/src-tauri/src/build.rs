// Port of nfo-*.ts, encoding.ts, archiver.ts, staging.ts, external-tool.ts, stale-drafts.ts and the buildRelease/previewOutputs handlers.

#[tauri::command]
pub fn build_release(form: serde_json::Value, dry_run: bool) -> Result<serde_json::Value, String> {
    Err("not ported: build_release".into())
}

#[tauri::command]
pub fn preview_outputs(form: serde_json::Value) -> Result<serde_json::Value, String> {
    Err("not ported: preview_outputs".into())
}

