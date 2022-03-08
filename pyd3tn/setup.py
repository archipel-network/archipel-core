#!/usr/bin/env python
# SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0

import setuptools

with open('README.md', 'r') as file:
    long_description = file.read()

setuptools.setup(
    name='pyD3TN',
    version='0.11.0',
    author='D3TN GmbH',
    author_email='contact@d3tn.com',
    description='Collection of DTN protocol implementations',
    long_description=long_description,
    long_description_content_type='text/markdown',
    url='https://gitlab.com/d3tn/ud3tn',
    packages=['pyd3tn'],
    install_requires=['cbor'],
    python_requires='>=3.7',
)
