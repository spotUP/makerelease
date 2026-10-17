"""Derive the /X NFO template from the real UP-MD15.NFO.

The frame is dipswitch's drawing and stays exactly as drawn; only the regions
a release actually varies become markers. Regions were measured from the file
(see the plan) and are 1-based inclusive line numbers.
"""
import os
import json

APP = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
SRC = os.path.join(APP, 'test', 'fixtures', 'UP-MD15.NFO')
OUT = os.path.join(APP, 'src', 'main', 'x-innovations-template.ts')

lines = open(SRC, encoding='utf-8', errors='replace').read().split('\n')

# One paste accident in the source: a 283-column line whose tail is a run of
# dashes and a stray gutter. Everything past the frame's 74th column is junk.
lines[35] = lines[35][:74].rstrip() if len(lines[35]) > 74 else lines[35]

REGIONS = [
    (26, 26, '{{RELEASE_NAME_LINE}}'),
    (29, 29, '{{FIELD_LINE_1}}'),
    (30, 30, '{{FIELD_LINE_2}}'),
    (39, 87, '{{NOTES}}'),
    (94, 98, '{{NEWS}}'),
    (107, 113, '{{ROSTER}}'),
    (123, 127, '{{BOARDS}}'),
    (138, 254, '{{RELEASE_LIST}}'),
]

out = []
i = 0
while i < len(lines):
    n = i + 1
    region = next((r for r in REGIONS if r[0] == n), None)
    if region:
        out.append(region[2])
        i = region[1] - 1
    else:
        out.append(lines[i])
    i += 1

template = '\n'.join(out)

# The two field lines keep their art; only the bracketed values are markers.
template = template.replace(
    '{{FIELD_LINE_1}}',
    lines[28].replace('2026-06-20', '{{DATE}}').replace('zALO/uP!..', '{{CODER}}'),
)
template = template.replace(
    '{{FIELD_LINE_2}}',
    lines[29].replace('aMiEXPRESS', '{{SYSTEM}}').replace('diNO/uP!..', '{{DESIGN}}'),
)
# The whole line, not just the words inside it: a longer or shorter release
# name has to re-centre, or the right-hand gutter moves and the frame breaks.
template = template.replace('{{RELEASE_NAME_LINE}}', '{{RELEASE_NAME_LINE}}')

header = '''// GENERATED from test/fixtures/UP-MD15.NFO - do not edit by hand.
//
// The /X Innovations NFO is a fixed drawing by dipswitch. Everything a
// release varies is a marker; every other byte is the frame exactly as it was
// drawn, including its deliberate asymmetries.
//
// One repair was made to the source: line 36 arrived 283 columns wide, its
// tail a run of dashes and a stray gutter from a paste accident. It is cut
// back to the frame's own width.
//
// Regenerate with scripts/make-x-template.py after changing the fixture.

export const X_INNOVATIONS_NFO_TEMPLATE = %s;
''' % json.dumps(template)

open(OUT, 'w').write(header)
print('markers:', sorted(set(m for m in ['{{DATE}}', '{{CODER}}', '{{SYSTEM}}', '{{DESIGN}}', '{{RELEASE_NAME}}',
                                         '{{NOTES}}', '{{NEWS}}', '{{ROSTER}}', '{{BOARDS}}', '{{RELEASE_LIST}}']
                            if m in template)))
print('template lines:', len(template.split('\n')))
