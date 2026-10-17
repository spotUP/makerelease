import { describe, it, expect } from 'vitest';
import { divisionIdOf, isProfileId, parseProfile, serializeProfile, slugifyProfileId, summarize } from '../../src/shared/profile';

const FILE = 'profiles/demo/profile.json';
const MIN = { label: 'Demo', url: 'https://demo.net', renderer: 'placeholder' };
const SERVER = {
  ...MIN,
  renderer: 'uprough',
  backend: 'releases-doorserver',
  server: { baseUrl: 'https://releases.demo.net///', writeKey: 'secret' },
};

const parse = (raw: unknown, id = 'demo') => parseProfile(raw, id, FILE);

describe('parseProfile', () => {
  it('fills the defaults for a minimal profile', () => {
    expect(parse(MIN)).toEqual({
      id: 'demo',
      label: 'Demo',
      url: 'https://demo.net',
      filePrefix: '',
      codeLength: 5,
      renderer: 'placeholder',
      backend: 'none',
      members: [],
    });
  });

  it('keeps a server block and strips trailing slashes from its base URL', () => {
    expect(parse(SERVER).server).toEqual({ baseUrl: 'https://releases.demo.net', writeKey: 'secret' });
  });

  it('treats a missing write key as empty, so the window can warn instead of failing to start', () => {
    const profile = parse({ ...SERVER, server: { baseUrl: 'https://x' } });
    expect(profile.server).toEqual({ baseUrl: 'https://x', writeKey: '' });
  });

  it('reads members', () => {
    expect(parse({ ...MIN, members: [{ nickname: 'a', role: 'b' }] }).members).toEqual([{ nickname: 'a', role: 'b' }]);
  });

  it.each([
    ['a folder name outside a-z, 0-9 and "-"', MIN, 'Demo_1', /folder name "Demo_1"/],
    ['a value that is not an object', [], 'demo', /must be a JSON object/],
    ['a missing label', { ...MIN, label: undefined }, 'demo', /"label" is missing/],
    ['an empty label', { ...MIN, label: '  ' }, 'demo', /"label" must not be empty/],
    ['a label that is not a string', { ...MIN, label: 7 }, 'demo', /"label" must be a string/],
    ['a missing url', { ...MIN, url: undefined }, 'demo', /"url" is missing/],
    ['a prefix with a space', { ...MIN, filePrefix: 'EX ' }, 'demo', /"filePrefix" must not contain spaces/],
    ['a prefix with a slash', { ...MIN, filePrefix: 'EX/' }, 'demo', /"filePrefix" must not contain/],
    ['a code length of 0', { ...MIN, codeLength: 0 }, 'demo', /"codeLength" must be a whole number from 1 to 32/],
    ['a code length of 33', { ...MIN, codeLength: 33 }, 'demo', /"codeLength"/],
    ['a fractional code length', { ...MIN, codeLength: 4.5 }, 'demo', /"codeLength"/],
    ['an unknown renderer', { ...MIN, renderer: 'fancy' }, 'demo', /"renderer" must be one of "uprough", "xinnovations", "placeholder"/],
    ['an unknown backend', { ...MIN, backend: 'ftp' }, 'demo', /"backend" must be one of "none", "releases-doorserver"/],
    ['a server backend with the placeholder renderer', { ...SERVER, renderer: 'placeholder' }, 'demo', /"backend" can be "releases-doorserver" only with/],
    ['a server backend without a server block', { ...SERVER, server: undefined }, 'demo', /"server" is required/],
    ['an empty server base URL', { ...SERVER, server: { baseUrl: '' } }, 'demo', /"server.baseUrl" must be a non-empty string/],
    ['a write key that is not a string', { ...SERVER, server: { baseUrl: 'https://x', writeKey: 1 } }, 'demo', /"server.writeKey" must be a string/],
    ['a server block without the server backend', { ...MIN, server: { baseUrl: 'https://x' } }, 'demo', /"server" is allowed only when/],
    ['members that are not a list', { ...MIN, members: {} }, 'demo', /"members" must be a list/],
    ['a member without a role', { ...MIN, members: [{ nickname: 'a' }] }, 'demo', /"members\[0\]" must be an object/],
  ])('rejects %s, naming the file', (_name, raw, id, message) => {
    expect(() => parse(raw, id)).toThrow(message);
    expect(() => parse(raw, id)).toThrow(FILE);
  });
});

describe('profile helpers', () => {
  it('isProfileId accepts only lower-case letters, digits and "-"', () => {
    expect(isProfileId('up-rough2')).toBe(true);
    expect(isProfileId('Up')).toBe(false);
    expect(isProfileId('')).toBe(false);
  });

  it('divisionIdOf maps the code renderers to their division and the placeholder to null', () => {
    expect(divisionIdOf('uprough')).toBe('uprough');
    expect(divisionIdOf('xinnovations')).toBe('xinnovations');
    expect(divisionIdOf('placeholder')).toBeNull();
  });

  it('summarize leaves out the server block, so the write key never reaches the window', () => {
    const summary = summarize(parse(SERVER));
    expect(summary).toEqual({
      id: 'demo', label: 'Demo', url: 'https://demo.net', filePrefix: '', codeLength: 5, renderer: 'uprough', backend: 'releases-doorserver',
    });
    expect(JSON.stringify(summary)).not.toContain('secret');
  });
});

describe('serializeProfile', () => {
  const minimal = parseProfile({ label: 'Min', url: 'https://min.net', renderer: 'placeholder' }, 'min', 'min/profile.json');
  const withServer = parseProfile(
    {
      label: 'Up Rough', url: 'https://uprough.net', filePrefix: 'UP-', codeLength: 5, renderer: 'uprough',
      backend: 'releases-doorserver', server: { baseUrl: 'https://x', writeKey: 'k' },
      members: [{ nickname: 'a', role: 'code' }, { nickname: 'b', role: 'gfx' }],
    },
    'uprough',
    'uprough/profile.json',
  );

  it.each([['minimal', minimal], ['server and members', withServer]])('round-trips through parseProfile: %s', (_name, profile) => {
    expect(parseProfile(JSON.parse(serializeProfile(profile)), profile.id, 'x')).toEqual(profile);
  });

  it('writes keys in a fixed order, no id, and a trailing newline', () => {
    const text = serializeProfile(withServer);
    expect(Object.keys(JSON.parse(text))).toEqual(['label', 'url', 'filePrefix', 'codeLength', 'renderer', 'backend', 'server', 'members']);
    expect(text.endsWith('}\n')).toBe(true);
    expect(Object.keys(JSON.parse(serializeProfile(minimal)))).not.toContain('server');
  });
});

describe('slugifyProfileId', () => {
  it('lower-cases and joins runs of other characters with a dash', () => {
    expect(slugifyProfileId('  Up Rough /X!  ', [])).toBe('up-rough-x');
  });
  it('falls back to "group" when nothing usable is left', () => {
    expect(slugifyProfileId('???', [])).toBe('group');
  });
  it('adds a numeric suffix on collision', () => {
    expect(slugifyProfileId('Demo', ['demo'])).toBe('demo-2');
    expect(slugifyProfileId('Demo', ['demo', 'demo-2'])).toBe('demo-3');
  });
});
