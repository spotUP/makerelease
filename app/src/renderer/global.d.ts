import { ReleaseDropperApi } from '../shared/types';

declare global {
  interface Window {
    api: ReleaseDropperApi;
  }
}
