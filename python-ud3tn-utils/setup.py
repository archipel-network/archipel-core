#!/usr/bin/env python
# SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0

import setuptools

with open('README.md', 'r') as file:
    long_description = file.read()

setuptools.setup(
    name='ud3tn-utils',
    version='0.12.0',
    author='D3TN GmbH',
    author_email='contact@d3tn.com',
    description='μD3TN Utilities',
    long_description=long_description,
    long_description_content_type='text/markdown',
    url='https://gitlab.com/d3tn/ud3tn',
    packages=[
        'ud3tn_utils',
        'ud3tn_utils.aap',
        'ud3tn_utils.aap2',
        'ud3tn_utils.aap2.generated',
    ],
    install_requires=['protobuf==4.21.12'],
    python_requires='>=3.6',
)
