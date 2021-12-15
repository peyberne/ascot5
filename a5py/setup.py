from setuptools import setup, find_packages

setup(name='a5py',
      version='0.0',
      description='ASCOT5 python library',
      url='http://github.com/ascot/python/a5py',
      license='LGPL',
      packages=find_packages(),
      zip_safe=False,
      install_requires=[
          'numpy',
          'h5py',
          'prompt_toolkit',
          'unyt',
          'scipy',
          'matplotlib',
          'scikit-image'
      ],
      scripts=[
        'bin/a5removegroup',
        'bin/a5copygroup',
        'bin/a5editoptions',
        'bin/a5combine',
        'bin/a5ascot4input',
        'bin/a5setactive',
        'bin/a5gui',
        'bin/a5ls',
        'bin/a5makecompatible',
        'bin/a5doxygen',
        'bin/test_ascot.py'
      ],
      include_package_data=True)
