"""Simple installation script for Python modules in the tools/ directory of the
µPCN project.

Usage:
    Simply execute this script with the Python interpret of your choice. The
    tools/ directory will be added to the system paths.

    .. code:: bash

        python tools/pyupcn/install.py
"""
import site
import os

tools_dir = os.path.abspath(os.path.join(__file__, '../..'))
site_dir = site.getsitepackages()[0]


def install():
    """Adds a ``.pth`` file to the site-packages directory of the Python
    interpreter executing this script. The .pth file contains the path to the
    ``tools/`` directory of the µPCN project.
    """
    with open(os.path.join(site_dir, 'pyupcn.pth'), 'w') as fd:
        fd.write(tools_dir)


if __name__ == '__main__':
    install()
