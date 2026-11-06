Box3D, vendored for the Usagi physics backend.

- Upstream: https://github.com/erincatto/box3d (MIT, see LICENSE)
- Commit:   3362633 "Dynamic Mover (#202)"
- Copied:   `include/` and `src/` unmodified (minus `src/CMakeLists.txt` and `src/box3d.pc.in`).

Box3D is built from source as part of the ThirdParty library (`.sources.*`), as C17.
`src/wide8.c` holds the 8-wide AVX2 solver paths and needs `/arch:AVX2` on MSVC
(ThirdParty.vcxproj.erb sets it for that one file); GCC and Clang enable AVX2 for it
with a target pragma. Box3D picks the AVX2 or SSE2 path at run time.

To update: copy `include/` and `src/` from a newer upstream commit over these,
delete the two build files above again, update the commit line here, and rebuild.
Do not edit the sources in place; Usagi hooks in through the public API
(`b3SetAllocator`, `b3SetAssertFcn`, `b3SetLogFcn`, the task callbacks in `b3WorldDef`).
