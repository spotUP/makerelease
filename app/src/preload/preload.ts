import { contextBridge, ipcRenderer } from 'electron';
import type { ProfileListEntry } from '../shared/profile';
import {
  AminetUploadOutcome,
  AminetUploadRequest,
  BuildResult,
  DeleteReleaseOutcome,
  MakereleaseApi,
  OutputsPreview,
  PublishToWebOutcome,
  ReleaseFormData,
  SeedState,
} from '../shared/types';

const api: MakereleaseApi = {
  listProfiles: (): Promise<ProfileListEntry[]> => ipcRenderer.invoke('listProfiles'),
  openProfilesFolder: (): Promise<void> => ipcRenderer.invoke('openProfilesFolder'),
  loadSeedState: (profileId: string): Promise<SeedState> => ipcRenderer.invoke('loadSeedState', profileId),
  pickFilesAndDirs: (): Promise<string[]> => ipcRenderer.invoke('pickFilesAndDirs'),
  checkCodeAvailability: (profileId: string, code: string): Promise<{ exists: boolean }> =>
    ipcRenderer.invoke('checkCodeAvailability', profileId, code),
  buildRelease: (form: ReleaseFormData, dryRun: boolean): Promise<BuildResult> =>
    ipcRenderer.invoke('buildRelease', form, dryRun),
  previewOutputs: (form: ReleaseFormData): Promise<OutputsPreview | null> =>
    ipcRenderer.invoke('previewOutputs', form),
  pickImage: (): Promise<string | null> => ipcRenderer.invoke('pickImage'),
  publishToWeb: (
    profileId: string,
    code: string,
    archivePath: string,
    imagePath: string,
    embedImagePaths: string[],
  ): Promise<PublishToWebOutcome> =>
    ipcRenderer.invoke('publishToWeb', profileId, code, archivePath, imagePath, embedImagePaths),
  uploadToAminet: (request: AminetUploadRequest, archivePath: string): Promise<AminetUploadOutcome> =>
    ipcRenderer.invoke('uploadToAminet', request, archivePath),
  deleteRelease: (profileId: string, code: string): Promise<DeleteReleaseOutcome> =>
    ipcRenderer.invoke('deleteRelease', profileId, code),
};

contextBridge.exposeInMainWorld('api', api);
