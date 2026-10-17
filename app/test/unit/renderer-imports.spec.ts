import { describe, it, expect } from 'vitest';
import * as path from 'path';
import * as ts from 'typescript';

// The renderer loads its compiled modules straight from disk as browser ES
// modules: no bundler, no Node resolution. An import written without its
// `.js` extension type-checks, but the browser fails to fetch it, the whole
// module graph aborts and no button in the window gets a handler. Compile
// the renderer exactly as `npm run build:renderer` does, in memory, and hold
// every relative specifier in the emitted JavaScript to a file that exists.
const RELATIVE_SPECIFIER = /^\s*(?:import|export)\b[^;]*?['"](\.{1,2}\/[^'"]+)['"]/gm;

function emitRenderer(): Map<string, string> {
  const configPath = path.resolve(__dirname, '../../tsconfig.renderer.json');
  const { config } = ts.readConfigFile(configPath, ts.sys.readFile);
  const parsed = ts.parseJsonConfigFileContent(config, ts.sys, path.dirname(configPath));
  const emitted = new Map<string, string>();
  const program = ts.createProgram(parsed.fileNames, parsed.options);
  program.emit(undefined, (fileName, text) => {
    if (fileName.endsWith('.js')) emitted.set(path.normalize(fileName), text);
  });
  return emitted;
}

describe('renderer module graph', () => {
  it('loads in the window: every relative import in the emitted JavaScript names a file that exists', () => {
    const emitted = emitRenderer();
    expect(emitted.size).toBeGreaterThan(0);
    const unloadable: string[] = [];
    for (const [file, text] of emitted) {
      for (const [, specifier] of text.matchAll(RELATIVE_SPECIFIER)) {
        if (!emitted.has(path.resolve(path.dirname(file), specifier))) {
          unloadable.push(`${path.basename(file)} -> ${specifier}`);
        }
      }
    }
    expect(unloadable).toEqual([]);
  });
});
