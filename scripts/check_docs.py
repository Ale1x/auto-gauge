#!/usr/bin/env python3
"""Check repository Markdown links and documentation layout; no network or mutation."""
import json
from pathlib import Path
import re
import subprocess
import sys
from urllib.parse import unquote

ROOT = Path(__file__).resolve().parents[1]
# Include new files during local editing, but exclude ignored build output and
# user-owned supplier attachments. In CI, the same inventory is fully tracked.
NAMES = subprocess.check_output(
    ['git', 'ls-files', '--cached', '--others', '--exclude-standard', '-z'],
    cwd=ROOT, text=True).split('\0')
LINK = re.compile(r'!?\[[^\]\n]*\]\((<[^>]+>|[^\s)]+)(?:\s+[^)\n]*)?\)')
REFERENCE = re.compile(r'^\s*\[[^\]]+\]:\s*(<[^>]+>|\S+)', re.M)
FENCE = re.compile(r'^\s*(`{3,}|~{3,})')


def prose(text):
    """Ignore examples inside fenced code blocks when resolving actual links."""
    lines = []
    fence = None
    for line in text.splitlines():
        match = FENCE.match(line)
        if match:
            token = match.group(1)
            if fence is None:
                fence = token
            elif token[0] == fence[0] and len(token) >= len(fence):
                fence = None
            continue
        if fence is None:
            lines.append(line)
    return '\n'.join(lines)


def main():
    errors = []
    checked = 0
    documents = sorted({ROOT / name for name in NAMES
                        if name.endswith('.md')
                        and not name.startswith(('.agents/', 'email-response/'))
                        and (ROOT / name).is_file()})
    for path in documents:
        content = prose(path.read_text())
        targets = [m.group(1) for regex in (LINK, REFERENCE)
                   for m in regex.finditer(content)]
        for target in targets:
            target = target.strip('<>')
            if re.match(r'^[a-zA-Z][\w+.-]*:', target):
                continue
            name = unquote(re.split(r'[#?]', target, maxsplit=1)[0])
            if not name:
                continue
            resolved = (path.parent / name).resolve()
            # External local references (for example /tmp build logs or another
            # checkout) are historical context, not repository dependencies.
            if not resolved.is_relative_to(ROOT):
                continue
            checked += 1
            if not resolved.exists():
                errors.append(f'{path.relative_to(ROOT)}: missing link target {target}')
    mapping_path = ROOT / 'docs/project/DOCUMENTATION_MAP.json'
    mapping = json.loads(mapping_path.read_text())
    destinations = list(mapping['moves'].values())
    if len(destinations) != len(set(destinations)):
        errors.append('Documentation map has duplicate destinations')
    for old, new in mapping['moves'].items():
        if not (ROOT / new).is_file():
            errors.append(f'Documentation map target missing: {old} -> {new}')
        if (ROOT / old).exists():
            errors.append(f'Old documentation path was recreated: {old}')
    stray = [p.name for p in ROOT.glob('*.md') if p.name not in {'README.md', 'AGENTS.md'}]
    if stray:
        errors.append('Project Markdown outside docs/: ' + ', '.join(sorted(stray)))
    if errors:
        print('\n'.join(errors), file=sys.stderr)
        return 1
    print(f'PASS: {len(documents)} Markdown files, {checked} local links, '
          f'{len(destinations)} moved paths; documentation layout intact.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
