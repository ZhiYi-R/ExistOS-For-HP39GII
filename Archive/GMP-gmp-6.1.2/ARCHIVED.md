# Archived: GMP 6.1.2 source tree

Moved here from `External/GMP/gmp-6.1.2/`.

**Why:** the build only consumes the prebuilt artifacts that remain in
`External/GMP/` — the headers under `External/GMP/include/` and the
precompiled `External/GMP/libgmp.libcpp`. The full autotools source tree
(including `Makefile~` backups) was never referenced by any CMakeLists.

**To revive / rebuild GMP:** move this tree back and configure it against
the ARM toolchain; then refresh `External/GMP/include/` and
`External/GMP/libgmp.libcpp` from the build output.
