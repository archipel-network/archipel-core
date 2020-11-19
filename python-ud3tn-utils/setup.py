#!/usr/bin/env python

import setuptools

with open('README.md', 'r') as file:
    long_description = file.read()

setuptools.setup(
    name='ud3tn-utils',
    version='0.9.0',
    author='D3TN GmbH',
    author_email='contact@d3tn.com',
    description='μD3TN Utilities',
    long_description=long_description,
    long_description_content_type='text/markdown',
    url='https://gitlab.com/d3tn/ud3tn',
    packages=['ud3tn_utils', 'ud3tn_utils.aap'],
    python_requires='>=3.6',
)
