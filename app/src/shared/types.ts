export interface MemberEntry {
  nickname: string;
  role: string;
}

export interface ReleaseHistoryEntry {
  releaseNumberRaw: string;
  releaseNumberValue: number | null;
  filenames: string;
  title: string;
  category: string;
  creators: string;
  rawBlock: string[];
}

export interface ParsedNfo {
  sourceFilePath: string;
  currentReleaseNotesTitleLine: string;
  currentReleaseNotesText: string;
  currentNewsText: string;
  members: MemberEntry[];
  history: ReleaseHistoryEntry[];
  maxReleaseNumber: number;
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
  sourceNfoPath: string;
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
