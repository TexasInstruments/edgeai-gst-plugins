#!/usr/bin/env python3

# Copyright (c) [2021] Texas Instruments Incorporated
#
# All rights reserved not granted herein.
#
# Limited License.
#
# Texas Instruments Incorporated grants a world-wide, royalty-free,
# non-exclusive license under copyrights and patents it now or hereafter
# owns or controls to make, have made, use, import, offer to sell and sell
# ("Utilize") this software subject to the terms herein.  With respect to
# the foregoing patent license, such license is granted  solely to the extent
# that any such patent is necessary to Utilize the software alone.
# The patent license shall not apply to any combinations which include
# this software, other than combinations with devices manufactured by or
# for TI (“TI Devices”).  No hardware patent is licensed hereunder.
#
# Redistributions must preserve existing copyright notices and reproduce
# this license (including the above copyright notice and the disclaimer
# and (if applicable) source code license limitations below) in the
# documentation and/or other materials provided with the distribution
#
# Redistribution and use in binary form, without modification, are permitted
# provided that the following conditions are met:
#
# *	No reverse engineering, decompilation, or disassembly of this software
#      is permitted with respect to any software provided in binary form.
#
# *	Any redistribution and use are licensed by TI for use only with TI Devices.
#
# *	Nothing shall obligate TI to provide you with source code for the
#      software licensed and provided to you in object code.
#
# If software source code is provided to you, modification and redistribution
# of the source code are permitted provided that the following conditions are met:
#
# *	Any redistribution and use of the source code, including any resulting
#      derivative works, are licensed by TI for use only with TI Devices.
#
# *	Any redistribution and use of any object code compiled from the source
#      code and any resulting derivative works, are licensed by TI for use
#      only with TI Devices.
#
# Neither the name of Texas Instruments Incorporated nor the names of its
# suppliers may be used to endorse or promote products derived from this
# software without specific prior written permission.
#
# DISCLAIMER.
#
# THIS SOFTWARE IS PROVIDED BY TI AND TI’S LICENSORS "AS IS" AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL TI AND TI’S LICENSORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
# EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from codecs import open
import logging
import os
from setuptools import find_packages
from setuptools import setup
from setuptools.command.develop import develop
import shlex
from subprocess import check_call
import unittest

name = 'example-gsttiovx'
version = '0.1.0'
author = 'RidgeRun'
description = 'TI GstTIOVXMux plugin app'
long_description = 'Example AppSink/Src app for GstTIOVXMux'
author_email = 'support@ridgerun.com'

# Default dependencies
install_requires = ["setuptools", "wheel"]
# Optional dependencies
extra_requires = []
# Test dependencies
test_requires = ["unittest"]
# Dev dependencies
dev_requires = ["pre-commit"]


class PostDevelopCommand(develop):
    def run(self):
        try:
            check_call(shlex.split("pre-commit install"))
        except Exception as e:
            logging.warning("Unable to run 'pre-commit install'")
        develop.run(self)


setup(
    name=name,
    version=version,
    description=description,
    long_description=long_description,
    author=author,
    author_email=author_email,
    long_description_content_type='text/markdown',

    packages=find_packages(
        exclude=[
            "*.tests",
            "*.tests.*",
            "tests.*",
            "tests"]),
    scripts=[
        'test_mux_app.py',
    ],
    classifiers=[
        'Development Status :: 4 - Beta',
        'Programming Language :: Python :: 3.8',
    ],
    python_requires='>=3',
    install_requires=install_requires,
    command_options={
        'build_sphinx': {
            'project': ('setup.py', name),
            'version': ('setup.py', version),
            'release': ('setup.py', version),
            'source_dir': ('setup.py', 'doc')}},

    extras_require={
        "test": test_requires,
        "extra": extra_requires,
        "dev": test_requires + extra_requires + dev_requires,
    },
    cmdclass={"develop": PostDevelopCommand},
)
