from argparse import ArgumentParser
from glob import glob
from tqdm import tqdm
from os import path, makedirs
import pandas as pd
import os


sandbox_dir = '/homes/users/asantini/scratch/kplink'

reports_dir = '/homes/users/asantini/local/src/kplink-reports'


def collect_results(commit: str) -> pd.DataFrame:
    base_dir = path.join(path.realpath(sandbox_dir), commit, 'kplink')

    if not path.exists(base_dir):
        raise RuntimeError(f"Cannot find base directory {base_dir}")

    results_dir = path.join(base_dir, 'cluster-results')

    if not path.exists(results_dir):
        raise RuntimeError(f"Cannot find results directory {results_dir}")

    results = list(glob(path.join(results_dir, 'res_*.csv')))

    if len(results) == 0:
        raise RuntimeError(f"There are no results in {results_dir}")

    dfs = list()

    for res_file in tqdm(results):
        _, _, instance_name = path.splitext(path.basename(res_file))[0].split('_', maxsplit=2)

        cols = pd.read_csv(res_file, nrows=1).columns
        df = pd.read_csv(res_file, usecols=cols)
        df['commit'] = commit
        df['instance'] = instance_name
        dfs.append(df)

    return pd.concat(dfs, axis='index', ignore_index=True)

def check_missing_results(commit: str) -> None:
    base_dir = path.join(path.realpath(sandbox_dir), commit, 'kplink')

    if not path.exists(base_dir):
        raise RuntimeError(f"Cannot find base directory {base_dir}")

    results_dir = path.join(base_dir, 'cluster-results')

    if not path.exists(results_dir):
        raise RuntimeError(f"Cannot find results directory {results_dir}")

    launchers_dir = path.join(base_dir, 'cluster-scripts')

    if not path.exists(launchers_dir):
        raise RuntimeError(f"Cannot find launchers directory {launchers_dir}")

    launchers = list(glob(path.join(launchers_dir, 'launch_*.sh')))

    if len(launchers) == 0:
        raise RuntimeError(f"There are no launchers in {launchers_dir}")

    for launcher in launchers:
        res_file = launcher.replace('launch', 'res')
        res_file = res_file.replace('.sh', '.csv')
        res_file = res_file.replace('cluster-scripts', 'cluster-results')

        if not path.isfile(res_file):
            print(f"sbatch {path.realpath(launcher)}")

def write_results(commit: str, df: pd.DataFrame) -> None:
    makedirs(path.realpath(reports_dir), exist_ok=True)
    reports_file = path.join(path.realpath(reports_dir), f"{commit}.csv")

    print(f"Writing to file {reports_file}")

    df.to_csv(reports_file, index=False)

def get_latest_commit() -> str:
    creation_date = dict()

    for folder in glob(path.join(path.realpath(sandbox_dir), '*')):
        if path.isdir(folder):
            creation_date[folder] = os.stat(folder).st_mtime

    if len(creation_date) == 0:
        raise RuntimeError(f"No commits found in {sandbox_dir}")

    latest = max(creation_date, key=creation_date.get)
    return path.basename(path.normpath(latest))


if __name__ == '__main__':
    parser = ArgumentParser(description='Collect and consolidate cluster results')
    parser.add_argument(
        '--commit',
        type=str,
        help='Commit whose results we collect; skip or use "latest" to use the latest runs',
        action='store',
        default='latest'
    )
    parser.add_argument(
        '--skip-missing-check',
        help='Skip the check for missing results file',
        action='store_true'
    )

    args = parser.parse_args()

    print("Collecting results:")
    print(f"\tSandbox dir: {path.realpath(sandbox_dir)}")
    print(f"\tReports dir: {path.realpath(reports_dir)}")

    commit = get_latest_commit() if args.commit == 'latest' else args.commit

    print(f"Collecting results for commit {commit}")

    df = collect_results(commit=commit)

    print(f"Created a dataframe with shape {df.shape}")

    write_results(commit=commit, df=df)

    if not args.skip_missing_check:
        print('Checking whether results are missing...')
        check_missing_results(commit=commit)
