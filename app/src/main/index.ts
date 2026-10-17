import { app, BrowserWindow, dialog } from 'electron';
import * as path from 'path';
import { registerIpcHandlers } from './ipc-handlers';
import { resolvePackagedOutputRoot } from './output-root';
import { ensureExampleProfile } from './profiles';
import type { GroupProfile } from '../shared/profile';

// In dev, __dirname is dist/main/main and resolving 4 levels up reaches the
// repo root; builds then land in releases-out/<profile id>/ there, which is
// ignored by git. In a PACKAGED build (asar, an AppImage's squashfs mount, an
// installed .app/.exe), __dirname sits inside the app's own read-only bundle,
// so the same walk lands somewhere inside the install location instead of
// anywhere writable - on Linux AppImages that showed up as a build failure
// writing into the AppImage's temporary /tmp/.mount_* directory. Packaged
// builds get a real, stable, user-writable folder per group instead.
function outputRootFor(profile: GroupProfile): string {
  if (!app.isPackaged) {
    return path.join(path.resolve(__dirname, '..', '..', '..', '..'), 'releases-out', profile.id);
  }
  return resolvePackagedOutputRoot((name) => app.getPath(name), `${profile.label} Releases`);
}

function createWindow(): void {
  const win = new BrowserWindow({
    width: 900,
    height: 800,
    webPreferences: {
      preload: path.join(__dirname, '..', '..', 'preload', 'preload', 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true,
    },
  });
  win.loadFile(path.join(__dirname, '..', '..', 'renderer', 'renderer', 'index.html'));
  win.webContents.on('console-message', (_event, _level, message, line, sourceId) => {
    console.log(`[renderer] ${sourceId}:${line} ${message}`);
  });
  win.webContents.on('did-fail-load', (_event, errorCode, errorDescription, validatedURL) => {
    console.log(`[did-fail-load] ${errorCode} ${errorDescription} ${validatedURL}`);
  });
  win.webContents.on('preload-error', (_event, preloadPath, error) => {
    console.log(`[preload-error] ${preloadPath} ${error.message}\n${error.stack}`);
  });
}

app.whenReady().then(async () => {
  const profilesDir = path.join(app.getPath('userData'), 'profiles');
  try {
    // First run: a working example group, so the window has something to
    // show and the maintainer has a template to copy.
    await ensureExampleProfile(profilesDir);
  } catch (err) {
    // Startup failure with no window yet - a plain throw here would exit
    // silently, so say what went wrong before quitting.
    dialog.showErrorBox(
      'makerelease',
      `Could not prepare the group profiles folder ${profilesDir}.\n\n${(err as Error).message}`,
    );
    app.quit();
    return;
  }
  registerIpcHandlers(profilesDir, outputRootFor);
  createWindow();

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow();
  });
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') app.quit();
});
