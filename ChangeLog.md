Changelog
=====
This changelog documents all notable changes in the project.

4.3.2 (2020-09-21)
-----
- Added support for processing files larger than 2 GB.
- Further optimization of bspatch (~ 4 % gain).
- Fixed stack overflow in bsdiff for edge cases.

4.3.1 (2020-09-19)
-----
- Added support for Clang and MSVC compilers.

4.3.0 (2020-09-15)
-----
- Added header compatibility with C++.
- Added control buffer optimization that generates a bit smaller patch files.
- Fixes CVE-2014-9862 (heap overflow in bspatch).
- Switched to CMake build process.
- Read and write library callbacks now specify which buffer is being read/written.
