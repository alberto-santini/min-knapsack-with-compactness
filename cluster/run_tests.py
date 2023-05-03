from os import makedirs, path, chmod
from argparse import ArgumentParser
from glob import glob
from typing import List, Dict
from hashlib import md5
import subprocess
import textwrap
import stat
import os


sandbox_dir = '/homes/users/asantini/scratch/kplink'

all_params = {
    'Labelling':              '-a labelling',
    'Compact MIP':            '-a compact_mip',
    'Compact MIP lift CC':    '-a compact_mip -f',
    'Compact MIP no presolve':'-a compact_mip -s',
    'Branch-and-cut':         '-a bc',
    'Branch-and-cut with VI': '-a bc -v',
    'Branch-and-cut lift CC': '-a bc -f'
}

unit_profit_params = {
    'Greedy':                 '-a greedy',
    'Unit Profit DP':         '-a unit_dp'
}


def fetch_commit(commit: str) -> str:
    commit_path = path.realpath(path.join(sandbox_dir, commit))

    try:
        makedirs(commit_path)
    except FileExistsError:
        print(f"Commit {commit} already downloaded. Skipping git clone!")
    else:
        res = subprocess.run(['git', 'clone', 'git@github.com:alberto-santini/kplink.git'], cwd=commit_path)

        if res.returncode != 0:
            raise RuntimeError(f"Cannot clone repo in folder {commit_path}")

        res = subprocess.run(['git', 'checkout', commit], cwd=path.join(commit_path, 'kplink'))

        if res.returncode != 0:
            raise RuntimeError(f"Cannot checkout commit {commit}")

    return path.join(commit_path, 'kplink')

def build_project(base_path: str) -> None:
    build_path = path.realpath(path.join(base_path, 'build'))

    try:
        makedirs(build_path)
    except FileExistsError:
        print(f"Build path {build_path} already exists. Skipping build!")
        return

    res = subprocess.run([
        'cmake',
        '-DCMAKE_BUILD_TYPE=Release',
        '..'], cwd=build_path)
    
    if res.returncode != 0:
        print('Cmake failed! Aborting...')
        raise RuntimeError(f"Could not run cmake for {base_path}")

    res = subprocess.run(['make', '-j', '20'], cwd=build_path)

    if res.returncode != 0:
        print('Make failed! Aborting...')
        raise RuntimeError(f"Could not run make for {base_path}")

    exe = path.realpath(path.join(build_path, 'kplink'))

    if not path.exists(exe):
        print('Executable not created! Aborting...')
        raise RuntimeError(f"Executable not created for {base_path}")
    else:
        chmod(exe, os.stat(exe).st_mode | stat.S_IXUSR)

def get_instances(base_path: str, use_unit_profit: bool, new_only: bool) -> List[str]:
    all_instances = list()
    
    folders = ['synthetic-instances']
    if use_unit_profit:
        folders.append('cappello-instances')

    for instance_type_dir in folders:
        instances_dir = path.realpath(path.join(base_path, 'data', instance_type_dir))
        instances = glob(path.join(instances_dir, '*.json'))

        if len(instances) == 0:
            raise RuntimeError(f"Could not find any instance in {instances_dir}")
        else:
            all_instances += list(instances)
    
    if new_only:
        with open(path.join(base_path), 'data', 'new_synthetic_instances.txt') as f:
            new_instances = [line.rstrip() for line in f]
        all_instances = [inst for inst in all_instances if inst in new_instances]

    return all_instances

def initialise_dirs(base_path: str) -> Dict[str, str]:
    dirs = dict(
        scripts_dir=path.realpath(path.join(base_path, 'cluster-scripts')),
        output_dir=path.realpath(path.join(base_path, 'cluster-output')),
        results_dir=path.realpath(path.join(base_path, 'cluster-results'))
    )

    for dir in dirs.values():
        makedirs(dir, exist_ok=True)

    return dirs

def get_latest_commit() -> str:
    res = subprocess.run(['git', 'rev-parse', 'HEAD'], capture_output=True, text=True)

    if res.returncode != 0:
        raise RuntimeError('Failed to run git rev-parse to get hash of latest commit')

    return res.stdout.strip()

def get_instance_size(instance_base: str) -> int:
    fields = instance_base.split('_')

    if 'gen' in instance_base:
        return int(fields[-5])
    else:
        return int(fields[1])

def create_script(commit: str, base_path: str, dirs: Dict[str, str], params: str, instance: str) -> str:
    instance_base = path.basename(instance)
    sz = get_instance_size(instance_base)
    params_hash = md5(params.encode('utf-8')).hexdigest()[:6]
    script_base = f"{commit[:6]}_{params_hash}_{instance_base}"

    script_file = path.join(dirs['scripts_dir'], f"launch_{script_base}.sh")
    out_file = path.join(dirs['output_dir'], f"out_{script_base}.txt")
    err_file = path.join(dirs['output_dir'], f"err_{script_base}.txt")
    results_file = path.join(dirs['results_dir'], f"res_{script_base}.csv")

    exe = path.realpath(path.join(base_path, 'build', 'kplink'))

    mem = '4GB'
    if sz > 500:
        mem = '8GB'

    script = f"""
        #!/bin/bash
        #SBATCH --partition=normal
        #SBATCH --time=01:30:00
        #SBATCH --cpus-per-task=1
        #SBATCH --mem-per-cpu={mem}
        #SBATCH -o {out_file}
        #SBATCH -e {err_file}

        module load GCC/9.3.0
        module load Boost/1.72.0-gompi-2020a
        module load Gurobi/9.0.0-lic-GCC-9.3.0

        {exe} -p {instance} -o {results_file} {params}
    """
    script = '\n'.join(script.split('\n', 1)[1:])

    with open(script_file, 'w') as f:
        f.write(textwrap.dedent(script) + '\n')

    return script_file

def launch_script(script_file: str) -> None:
    res = subprocess.run(['sbatch', path.realpath(script_file)])

    if res.returncode != 0:
        raise RuntimeError(f"Could not launch (sbatch) {script_file}")


if __name__ == '__main__':
    makedirs(sandbox_dir, exist_ok=True)

    parser = ArgumentParser(description='Download, build and run kplink')
    parser.add_argument(
        '--commit',
        type=str,
        help='Commit to test or "latest" for the latest commit',
        action='store',
        required=True)
    parser.add_argument(
        '--params',
        type=str,
        help='Command-line parameters to pass to kplink; use "all" to try a list of hard-coded params',
        action='store',
        default='')
    parser.add_argument(
        '--sbatch',
        help='Automatically launch scripts on the cluster (otherwise it just creates them)',
        action='store_true')
    parser.add_argument(
        '--unit-profit-instances',
        help='Also run tests on Unit Profit instances. Otherwise only run on generated instances.',
        action='store_true'
    )
    parser.add_argument(
        '--new-only',
        help='Only run tests on instances listed in data/new_synthetic_instances.txt',
        action='store_true'
    )

    args = parser.parse_args()

    commit = args.commit
    if commit == "latest":
        commit = get_latest_commit()

    base_path = fetch_commit(commit=commit)
    build_project(base_path=base_path)
    dirs = initialise_dirs(base_path=base_path)

    for instance in get_instances(base_path=base_path, use_unit_profit=args.unit_profit_instances, new_only=args.new_only):
        if args.params == 'all':
            params = all_params.values()

            if args.unit_profit_instances:
                params += list(unit_profit_params.values())
        else:
            params = [args.params]

        for cl_params in params:
            script_file = create_script(
                commit=commit,
                base_path=base_path,
                dirs=dirs,
                params=cl_params,
                instance=instance)

            if args.sbatch:
                launch_script(script_file=script_file)
