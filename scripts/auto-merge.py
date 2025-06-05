#!/usr/bin/env python3

import argparse
import subprocess
import sys

def is_auto_mergeable(commit: str) -> bool:
    out = subprocess.run(['git', 'merge', '--no-edit', commit], capture_output=True).stdout.decode().strip().split('\n')
    if out[-1] == 'Automatic merge failed; fix conflicts and then commit the result.':
        subprocess.run(['git', 'merge', '--abort'], capture_output=True)
        return False
    else:
        subprocess.run(['git', 'reset', '--hard', 'HEAD~1'], capture_output=True)
        return True

def main() -> int:

    arg_parser = argparse.ArgumentParser()
    arg_parser.add_argument(
        '-b', '--branch',
        help='Branch to merge into the current branch.',
        required=True,
        dest='branch'
    )

    args = arg_parser.parse_args()

    if subprocess.run(['git', 'diff'], capture_output=True).stdout.decode():
        print('The working directory has uncomitted changes. Aborting auto-merge.')
        return -1

    if is_auto_mergeable(args.branch):
        subprocess.run(['git', 'merge', '--no-edit', args.branch], capture_output=True)
        print('Successfully merged all commits from', args.branch)
        return 0

    upstream_commits = subprocess.run(['git', 'rev-list', args.branch], capture_output=True).stdout.decode().strip().split('\n')
    current_merge_commits = subprocess.run(['git', 'rev-list', '--merges', 'HEAD'], capture_output=True).stdout.decode().strip().split('\n')

    last_common_commit = str()
    for merge_commit in current_merge_commits:
        parent_commits = subprocess.run(['git', 'rev-parse', f'{merge_commit}^@'], capture_output=True).stdout.decode().strip().split('\n')
        if parent_commits[1] in upstream_commits:
            last_common_commit = parent_commits[1]
            break

    commits_to_merge = subprocess.run(['git', 'rev-list', last_common_commit + '..' + args.branch], capture_output=True).stdout.decode().strip().split('\n')
    commits_to_merge.reverse()

    if not is_auto_mergeable(commits_to_merge[0]):
        print(f'Cannot merge any commit automatically ({len(commits_to_merge)} commits to merge). Next commit needs to be merged manually: {commits_to_merge[0]}')
    else:
        a = 0
        b = len(commits_to_merge) - 1
        i = (a + b) // 2
        while i != a:
            if is_auto_mergeable(commits_to_merge[i]):
                a = i
            else:
                b = i
            i = (a + b) // 2
        subprocess.run(['git', 'merge', '--no-edit', commits_to_merge[i]], capture_output=True)
        print(f'Successfully merged {i+1}/{len(commits_to_merge)} commits. Next commit needs to be merged manually: {commits_to_merge[i+1]}')

    return 0


if __name__ == '__main__':
    sys.exit(main())
