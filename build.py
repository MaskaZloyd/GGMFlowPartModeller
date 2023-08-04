import os
import subprocess


def build_config() -> None:
    current_directory = os.getcwd()
    build_dir = os.path.join(current_directory, 'out/build')

    if not os.path.exists(build_dir):
        os.makedirs(build_dir)

    subprocess.run(
        ['cmake', '--preset=win'],
        check=True,
    )
    subprocess.run(
        ['cmake', '--build', '--preset=win-debug'],
        check=True,
    )


if __name__ == '__main__':
    build_config()
