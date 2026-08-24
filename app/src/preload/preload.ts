import { contextBridge, ipcRenderer } from 'electron';
import { BuildResult, MakereleaseApi, PublishToWebOutcome, ReleaseFormData, SeedState } from '../shared/types';

const api: MakereleaseApi = {
  loadSeedState: (): Promise<SeedState> => ipcRenderer.invoke('loadSeedState'),
  pickFilesAndDirs: (): Promise<string[]> => ipcRenderer.invoke('pickFilesAndDirs'),
  checkCodeAvailability: (code: string): Promise<{ exists: boolean }> =>
    ipcRenderer.invoke('checkCodeAvailability', code),
  buildRelease: (form: ReleaseFormData): Promise<BuildResult> => ipcRenderer.invoke('buildRelease', form),
  pickImage: (): Promise<string | null> => ipcRenderer.invoke('pickImage'),
  publishToWeb: (code: string, archivePath: string, imagePath: string): Promise<PublishToWebOutcome> =>
    ipcRenderer.invoke('publishToWeb', code, archivePath, imagePath),
};

contextBridge.exposeInMainWorld('api', api);
