#!/usr/bin/env python
# -*- coding: utf-8 -*-

from distutils.core import setup, Extension
from Cython.Build import cythonize


setup(ext_modules = cythonize(Extension(
    name="pydarts",                        # the extension name
    sources=["pydarts.pyx", "darts.cpp"],  # the Cython and C++ source files
    language="c++",                        # generate and compile C++ code
)))
