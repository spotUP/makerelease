import { MakereleaseApi } from '../shared/types';

declare global {
  interface Window {
    api: MakereleaseApi;
  }
}
