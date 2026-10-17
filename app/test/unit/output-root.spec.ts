import { describe, it, expect } from 'vitest';
import * as path from 'path';
import { resolvePackagedOutputRoot, type OutputRootLocation } from '../../src/main/output-root';

/**
 * Electron's app.getPath() throws for locations it cannot resolve. On a Linux
 * box without xdg-user-dirs configured that can include 'documents' — and this
 * runs during startup, so an escaping throw kills the app before it can show a
 * window or an error.
 */
function getPathStub(available: Partial<Record<OutputRootLocation, string>>) {
  return (name: OutputRootLocation): string => {
    const value = available[name];
    if (!value) throw new Error(`Failed to get '${name}' path`);
    return value;
  };
}

describe('resolvePackagedOutputRoot', () => {
  const FOLDER = 'Example Group Releases';

  it('puts the output folder under Documents when Documents resolves', () => {
    const root = resolvePackagedOutputRoot(getPathStub({ documents: '/home/varin/Documents' }), FOLDER);
    expect(root).toBe(path.join('/home/varin/Documents', FOLDER));
  });

  it('falls back to the home directory when Documents cannot be resolved', () => {
    const root = resolvePackagedOutputRoot(
      getPathStub({ home: '/home/varin', userData: '/home/varin/.config/makerelease' }),
      FOLDER,
    );
    expect(root).toBe(path.join('/home/varin', FOLDER));
  });

  it('falls back to userData when neither Documents nor home can be resolved', () => {
    const root = resolvePackagedOutputRoot(getPathStub({ userData: '/home/varin/.config/makerelease' }), FOLDER);
    expect(root).toBe(path.join('/home/varin/.config/makerelease', FOLDER));
  });

  it('throws a describable error only when every location fails', () => {
    expect(() => resolvePackagedOutputRoot(getPathStub({}), FOLDER)).toThrow(/documents, home, userData/);
  });

  it('skips a location that resolves to an empty string', () => {
    const root = resolvePackagedOutputRoot(getPathStub({ documents: '', home: '/home/varin' }), FOLDER);
    expect(root).toBe(path.join('/home/varin', FOLDER));
  });
});
