import { contextBridge, ipcRenderer } from 'electron';
import type { DivisionId } from '../shared/divisions';
import {
  AminetUploadRequest,
  BuildResult,
  DeleteReleaseOutcome,
  MakereleaseApi,
  PublishToWebOutcome,
  ReleaseFormData,
  SeedState,
} from '../shared/types';

const api: MakereleaseApi = {
  loadSeedState: (division: DivisionId): Promise<SeedState> => ipcRenderer.invoke('loadSeedState', division),
  pickFilesAndDirs: (): Promise<string[]> => ipcRenderer.invoke('pickFilesAndDirs'),
  checkCodeAvailability: (code: string): Promise<{ exists: boolean }> =>
    ipcRenderer.invoke('checkCodeAvailability', code),
  buildRelease: (form: ReleaseFormData, dryRun: boolean): Promise<BuildResult> =>
    ipcRenderer.invoke('buildRelease', form, dryRun),
  pickImage: (): Promise<string | null> => ipcRenderer.invoke('pickImage'),
  publishToWeb: (
    code: string,
    archivePath: string,
    imagePath: string,
    embedImagePaths: string[],
    aminet: AminetUploadRequest | null,
  ): Promise<PublishToWebOutcome> =>
    ipcRenderer.invoke('publishToWeb', code, archivePath, imagePath, embedImagePaths, aminet),
  deleteRelease: (code: string): Promise<DeleteReleaseOutcome> => ipcRenderer.invoke('deleteRelease', code),
};

contextBridge.exposeInMainWorld('api', api);
