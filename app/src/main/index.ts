import { app, BrowserWindow } from 'electron';
import * as path from 'path';
import { registerIpcHandlers } from './ipc-handlers';
import { loadReleasesClientConfig } from './config';

// __dirname is dist/main/main (tsc preserves the src/main/ subfolder under outDir dist/main).
const MAKERELEASE_ROOT = path.resolve(__dirname, '..', '..', '..', '..');

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

app.whenReady().then(() => {
  const releasesConfig = loadReleasesClientConfig();
  registerIpcHandlers(MAKERELEASE_ROOT, releasesConfig);
  createWindow();

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow();
  });
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') app.quit();
});
