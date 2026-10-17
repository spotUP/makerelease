import { describe, it, expect } from 'vitest';
import { formToRequest, profileToForm } from '../../src/renderer/profile-editor';
import type { ProfileEditState } from '../../src/shared/types';

const state: ProfileEditState = {
  profile: {
    id: 'up', label: 'Up Rough', url: 'https://uprough.net', filePrefix: 'UP-', codeLength: 5, renderer: 'uprough',
    backend: 'releases-doorserver', server: { baseUrl: 'https://x', writeKey: 'k' },
    members: [{ nickname: 'b', role: 'gfx' }, { nickname: 'a', role: 'code' }],
  },
  templates: { nfo: 'NFO\n', diz: 'DIZ\n' },
};

describe('profile editor form mapping', () => {
  it('keeps member order and drops rows with a blank nickname', () => {
    const form = profileToForm(state, 'edit', ['up']);
    form.members.splice(1, 0, { nickname: '   ', role: 'x' });
    expect(formToRequest(form).profile.members.map((m) => m.nickname)).toEqual(['b', 'a']);
  });

  it('sends server only for the doorserver backend', () => {
    const form = profileToForm(state, 'edit', ['up']);
    expect(formToRequest(form).profile.server).toEqual({ baseUrl: 'https://x', writeKey: 'k' });
    expect(formToRequest({ ...form, backend: 'none' }).profile).not.toHaveProperty('server');
  });

  it('sends codeLength as a number and omits templates for the code renderers', () => {
    const form = profileToForm(state, 'edit', ['up']);
    const request = formToRequest(form);
    expect(request.profile.codeLength).toBe(5);
    expect(request).not.toHaveProperty('templates');
    expect(formToRequest({ ...form, renderer: 'placeholder', backend: 'none' }).templates).toEqual(state.templates);
  });

  it('duplicate copies the fields, adds copy to the name and picks a fresh id', () => {
    const form = profileToForm(state, 'duplicate', ['up', 'up-rough-copy']);
    expect(form).toMatchObject({ label: 'Up Rough copy', id: 'up-rough-copy-2', isNew: true, writeKey: 'k' });
    expect(profileToForm(state, 'edit', ['up'])).toMatchObject({ id: 'up', isNew: false });
  });
});
