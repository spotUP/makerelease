// Replaces app/src/preload: same window.api surface, backed by Tauri commands (src-tauri/src).
const inv = (cmd, args) => window.__TAURI__.core.invoke(cmd, args);
const call = (cmd, args = {}) => inv(cmd, args);
window.api = {
  listProfiles: () => call('list_profiles'),
  openProfilesFolder: () => call('open_profiles_folder'),
  newProfileDraft: () => call('new_profile_draft'),
  loadProfileForEdit: (id) => call('load_profile_for_edit', { id }),
  saveProfile: (request) => call('save_profile', { request }),
  deleteProfile: (id) => call('delete_profile', { id }),
  importTemplate: () => call('import_template'),
  loadSeedState: (profileId) => call('load_seed_state', { profileId }),
  pickPaths: (folders) => call('pick_paths', { folders }),
  onDragDrop: (callback) => {
    const pending = window.__TAURI__.webview.getCurrentWebview().onDragDropEvent((e) => callback(e.payload));
    return () => { void pending.then((unlisten) => unlisten()); };
  },
  checkCodeAvailability: (profileId, code) => call('check_code_availability', { profileId, code }),
  buildRelease: (form, dryRun) => call('build_release', { form, dryRun }),
  onBuildProgress: (callback) => {
    const pending = window.__TAURI__.event.listen('build-progress', (e) => callback(e.payload));
    return () => { void pending.then((unlisten) => unlisten()); };
  },
  previewOutputs: (form) => call('preview_outputs', { form }),
  previewArchive: (form) => call('preview_archive', { form }),
  pickImage: () => call('pick_image'),
  publishToWeb: (profileId, code, archivePath, imagePath, embedImagePaths) =>
    call('publish_to_web', { profileId, code, archivePath, imagePath, embedImagePaths }),
  uploadToAminet: (request, archivePath) => call('upload_to_aminet', { request, archivePath }),
  publishUpkg: (request, archivePath) => call('publish_upkg', { request, archivePath }),
  deleteRelease: (profileId, code) => call('delete_release', { profileId, code }),
};
