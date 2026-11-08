"""The shared shader list (games/<game>/assets/shader_list.tsv in the refs/MacNativeGames checkout; MIT, see THIRD_PARTY_NOTICES.md) as our tools
read it: every row of one revision, plus the V rows of every older revision that it dropped.

The list drops V rows its producer's runtime no longer needs: default variants, which that runtime translates for every
shader (dropped 2026-10-22 to 2026-10-25), and variants that differ only in vertex input or colour
output types (2026-10-26). Our answers (runtime/gfx_shader.c) still tell those apart, so a dropped row names a variant
our play meets: at 1340bee the list alone has 134 rows of 1.4.3 shaders, its history 24,839, and of 1.2.1 6,771 and
25,238. A row no draw asks for costs one translation. No copy of the list goes into the repository.
"""
import subprocess
import sys

TAIL = '/assets/shader_list.tsv'   # under the checkout's game folder (see list_path)


def list_path(repo, rev):
    """The shader list's path at rev: the one games/<game>/assets/shader_list.tsv of the checkout."""
    for p in git(repo, 'ls-tree', '-r', '--name-only', rev, '--', 'games').splitlines():
        if p.endswith(TAIL) and p.count('/') == 3:
            return p
    raise SystemExit(f'shader_list.py: no games/*{TAIL} at {rev}')


def git(repo, *args):
    return subprocess.run(['git', '-C', str(repo), *args], check=True, capture_output=True, encoding='utf-8').stdout


def history(repo, rev='origin/HEAD'):
    """rev's list, then each V row of an older revision that no newer one has, newest first."""
    if git(repo, 'rev-parse', '--is-shallow-repository').strip() == 'true':
        # a shallow clone has rev's list only: preparing from it skips most variants and shows only as stutter in play
        sys.exit(f'{repo} is a shallow clone without the shader list\'s history; run tools/fetch_refs.sh')
    PATH = list_path(repo, rev)
    lines = git(repo, 'show', f'{rev}:{PATH}').splitlines()
    seen = set(lines)
    for h in git(repo, 'log', '--format=%H', rev, '--', PATH).split()[1:]:
        for line in git(repo, 'show', f'{h}:{PATH}').splitlines():
            if line.startswith('V\t') and line not in seen:
                seen.add(line)
                lines.append(line)
    return '\n'.join(lines) + '\n'
