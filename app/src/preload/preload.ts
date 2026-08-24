import { contextBridge, ipcRenderer } from 'electron';
import { BuildResult, MakereleaseApi, ReleaseFormData, SeedState } from '../shared/types';

const api: MakereleaseApi = {
  loadSeedState: (): Promise<SeedState> => ipcRenderer.invoke('loadSeedState'),
  pickFilesAndDirs: (): Promise<string[]> => ipcRenderer.invoke('pickFilesAndDirs'),
  checkCodeAvailability: (code: string): Promise<{ exists: boolean }> =>
    ipcRenderer.invoke('checkCodeAvailability', code),
  buildRelease: (form: ReleaseFormData): Promise<BuildResult> => ipcRenderer.invoke('buildRelease', form),
};

contextBridge.exposeInMainWorld('api', api);
