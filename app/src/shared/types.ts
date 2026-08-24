export interface MemberEntry {
  nickname: string;
  role: string;
}

export interface ReleaseHistoryEntry {
  releaseNumber: number;
  filenames: string;
  title: string;
  category: string;
  creators: string;
}

export type ReleaseFormat = 'LHA' | 'ZIP';

export interface ReleaseFormData {
  code: string;
  format: ReleaseFormat;
  releaseName: string;
  platformTag: string;
  releaseNotesText: string;
  newsText: string;
  dizBannerText: string;
  members: MemberEntry[];
  entryTitle: string;
  entryCategory: string;
  entryCreators: string;
  selectedPaths: string[];
}

export interface GeneratedOutputs {
  nfoText: string;
  dizText: string;
  releaseNumber: number;
}

export interface SeedState {
  members: MemberEntry[];
  nextReleaseNumber: number;
  history: ReleaseHistoryEntry[];
  currentDizText: string;
  revision: number;
}

export interface BuildResult {
  archivePath: string;
  releaseNumber: number;
}

export interface MakereleaseApi {
  loadSeedState(): Promise<SeedState>;
  pickFilesAndDirs(): Promise<string[]>;
  checkCodeAvailability(code: string): Promise<{ exists: boolean }>;
  buildRelease(form: ReleaseFormData): Promise<BuildResult>;
}
