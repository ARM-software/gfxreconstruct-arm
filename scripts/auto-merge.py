#!/usr/bin/env python3

import argparse
import subprocess
import sys


# Success if a merge commit has been generated. Error otherwise.
# Success has most significant bit set to 0
# True success (0) is if the desired commit has entirely been merge without any issue.
SUCCESS_ALL_MERGED       = 0x00
SUCCESS_PARTIALLY_MERGED = 0x01
ERROR_NOTHING_TO_MERGE   = 0x80
ERROR_MERGE_CONFLICT     = 0x81
ERROR_OTHER              = 0xFF


def is_auto_mergeable(commit: str) -> bool:
    out = subprocess.run(['git', 'merge', '--no-edit', commit], capture_output=True).stdout.decode().strip().split('\n')
    if out[-1] == 'Automatic merge failed; fix conflicts and then commit the result.':
        subprocess.run(['git', 'merge', '--abort'], capture_output=True)
        return False
    else:
        if out != ['Already up to date.']:
            subprocess.run(['git', 'reset', '--hard', 'HEAD~1'], capture_output=True)
        return True

def main() -> int:

    arg_parser = argparse.ArgumentParser()
    arg_parser.add_argument(
        'commit',
        help='Commit hash or alias (like FETCH_HEAD, a tag or the tip of a local branch) to merge into the current branch.',
        metavar='<commit>'
    )
    args = arg_parser.parse_args()

    if subprocess.run(['git', 'cat-file', '-t', args.commit], capture_output=True).stdout.decode().strip() != 'commit':
        print(args.commit, 'is not a valid commit. Aborting.')
        return ERROR_OTHER

    if subprocess.run(['git', 'diff'], capture_output=True).stdout.decode().strip():
        print('The working directory has uncomitted changes. Aborting.')
        return ERROR_OTHER

    out = subprocess.run(['git', 'merge', '--no-edit', args.commit], capture_output=True).stdout.decode().strip().split('\n')
    if out[-1] == 'Automatic merge failed; fix conflicts and then commit the result.':
        subprocess.run(['git', 'merge', '--abort'], capture_output=True)
    elif out == ['Already up to date.']:
        print(args.commit, 'has already been merged in the current working directory. Aborting.')
        return ERROR_NOTHING_TO_MERGE
    else:
        print('Successfully merged all commits from', args.commit)
        return SUCCESS_ALL_MERGED

    upstream_commits = subprocess.run(['git', 'rev-list', args.commit], capture_output=True).stdout.decode().strip().split('\n')
    current_merge_commits = subprocess.run(['git', 'rev-list', '--merges', 'HEAD'], capture_output=True).stdout.decode().strip().split('\n')

    last_common_commit = str()
    for merge_commit in current_merge_commits:
        parent_commits = subprocess.run(['git', 'rev-parse', f'{merge_commit}^@'], capture_output=True).stdout.decode().strip().split('\n')
        if parent_commits[1] in upstream_commits:
            last_common_commit = parent_commits[1]
            break

    commits_to_merge = subprocess.run(['git', 'rev-list', last_common_commit + '..' + args.commit], capture_output=True).stdout.decode().strip().split('\n')
    commits_to_merge.reverse()

    if not is_auto_mergeable(commits_to_merge[0]):
        print(f'Cannot merge any commit automatically ({len(commits_to_merge)} commits to merge). Next commit needs to be merged manually: {commits_to_merge[0]}')
        return ERROR_MERGE_CONFLICT

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

    return SUCCESS_PARTIALLY_MERGED


if __name__ == '__main__':
    sys.exit(main())
