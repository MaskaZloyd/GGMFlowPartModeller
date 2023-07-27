import os
import subprocess


def build_config() -> None:
    current_directory = os.getcwd()
    build_dir = os.path.join(current_directory, 'build')

    if not os.path.exists(build_dir):
        os.makedirs(build_dir)

    subprocess.run(
        ['cmake', '--preset=win_default'],
        check=True,
    )
    subprocess.run(
        ['cmake', '--build', '--preset=debug'],
        check=True,
    )


if __name__ == '__main__':
    build_config()
