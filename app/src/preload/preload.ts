import { contextBridge, ipcRenderer } from 'electron';
import type { ProfileListEntry } from '../shared/profile';
import {
  AminetUploadOutcome,
  AminetUploadRequest,
  BuildProgress,
  BuildResult,
  DeleteReleaseOutcome,
  MakereleaseApi,
  ArchivePreview,
  OutputsPreview,
  ProfileEditOutcome,
  ProfileEditRequest,
  ProfileEditState,
  PublishToWebOutcome,
  ReleaseFormData,
  SeedState,
} from '../shared/types';

const api: MakereleaseApi = {
  listProfiles: (): Promise<ProfileListEntry[]> => ipcRenderer.invoke('listProfiles'),
  openProfilesFolder: (): Promise<void> => ipcRenderer.invoke('openProfilesFolder'),
  newProfileDraft: (): Promise<ProfileEditState> => ipcRenderer.invoke('newProfileDraft'),
  loadProfileForEdit: (id: string): Promise<ProfileEditState> => ipcRenderer.invoke('loadProfileForEdit', id),
  saveProfile: (request: ProfileEditRequest): Promise<ProfileEditOutcome> => ipcRenderer.invoke('saveProfile', request),
  deleteProfile: (id: string): Promise<ProfileEditOutcome> => ipcRenderer.invoke('deleteProfile', id),
  importTemplate: (): Promise<{ text: string; name: string } | null> => ipcRenderer.invoke('importTemplate'),
  loadSeedState: (profileId: string): Promise<SeedState> => ipcRenderer.invoke('loadSeedState', profileId),
  pickFilesAndDirs: (): Promise<string[]> => ipcRenderer.invoke('pickFilesAndDirs'),
  checkCodeAvailability: (profileId: string, code: string): Promise<{ exists: boolean }> =>
    ipcRenderer.invoke('checkCodeAvailability', profileId, code),
  buildRelease: (form: ReleaseFormData, dryRun: boolean): Promise<BuildResult> =>
    ipcRenderer.invoke('buildRelease', form, dryRun),
  onBuildProgress: (callback: (progress: BuildProgress) => void): (() => void) => {
    const listener = (_event: Electron.IpcRendererEvent, progress: BuildProgress): void => callback(progress);
    ipcRenderer.on('buildProgress', listener);
    return () => ipcRenderer.removeListener('buildProgress', listener);
  },
  previewOutputs: (form: ReleaseFormData): Promise<OutputsPreview | null> =>
    ipcRenderer.invoke('previewOutputs', form),
  previewArchive: (form: ReleaseFormData): Promise<ArchivePreview> =>
    ipcRenderer.invoke('previewArchive', form),
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
