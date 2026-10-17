import { serializeProfile } from '../shared/profile';

/**
 * The profile written on first launch, so a new group sees a working setup
 * to copy instead of an empty dropdown. Every placeholder appears once in
 * the NFO; the DIZ shows the fixed-width form that keeps a 44-column frame
 * intact. Kept as string constants so a test can assert the exact bytes.
 */
export const EXAMPLE_PROFILE_ID = 'example';

export const EXAMPLE_PROFILE_JSON = serializeProfile({
  id: EXAMPLE_PROFILE_ID,
  label: 'Example Group',
  url: 'https://example.net',
  filePrefix: 'EX-',
  codeLength: 5,
  renderer: 'placeholder',
  backend: 'none',
  members: [{ nickname: 'coder', role: 'code' }],
});

export const EXAMPLE_NFO_TEMPLATE = [
  '            {{group}}  -  {{url}}',
  '',
  '  Release ..: {{release_name}} ({{platform}})',
  '  Code .....: {{code}} ({{format}})',
  '  File .....: {{filename}}',
  '  Number ...: {{release_number}}',
  '  Date .....: {{date}}',
  '',
  '  {{description}}',
  '  By {{creators}}',
  '',
  '  -- notes -------------------------------------------------------------',
  '  {{notes}}',
  '',
  '  -- news --------------------------------------------------------------',
  '  {{news}}',
  '',
  '  -- members -----------------------------------------------------------',
  '  {{members}}',
  '',
  '  -- credits -----------------------------------------------------------',
  '  {{credits}}',
  '',
  '                 {{group}} - {{url}}',
  '',
].join('\n');

export const EXAMPLE_DIZ_TEMPLATE = [
  ' ____________________________________________',
  '|                                            |',
  '|  {{group                                }} |',
  '|  {{release_name                         }} |',
  '|  {{platform            }} {{date       }}  |',
  '|____________________________________________|',
  '',
].join('\n');
