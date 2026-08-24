export interface MemberEntry {
  nickname: string;
  role: string;
}

export interface ReleaseHistoryEntry {
  releaseNumber: string;
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
  /**
   * Set when this build is running with the placeholder RELEASES_WRITE_KEY
   * (i.e. no key was baked in at build time and none was supplied via the
   * dev-time env var override). Publishing a release will 401. The renderer
   * surfaces this to the user; see main.ts's init().
   */
  configWarning?: string;
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
