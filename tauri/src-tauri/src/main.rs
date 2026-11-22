// Tauri shell for makerelease. Each module mirrors one slice of app/src/main (Electron).
// Command names are the snake_case of the preload bridge calls (app/src/preload).
mod build;
mod net;
mod profiles;

fn main() {
    tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .setup(|app| {
            profiles::prepare_profiles(app.handle()).map_err(|e| format!("Could not prepare the group profiles folder: {e}"))?;
            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            profiles::list_profiles,
            profiles::open_profiles_folder,
            profiles::new_profile_draft,
            profiles::load_profile_for_edit,
            profiles::save_profile,
            profiles::delete_profile,
            profiles::import_template,
            profiles::load_seed_state,
            profiles::pick_files_and_dirs,
            profiles::pick_image,
            build::build_release,
            build::preview_outputs,
            net::check_code_availability,
            net::publish_to_web,
            net::upload_to_aminet,
            net::delete_release,
        ])
        .run(tauri::generate_context!())
        .expect("error while running makerelease");
}
