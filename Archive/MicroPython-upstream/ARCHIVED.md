# Archived: MicroPython (full upstream tree)

Moved here from `External/MicroPython/`.

**Why:** the MicroPython port is not built — `System/CMakeLists.txt:132`
(`#${CMAKE_SOURCE_DIR}/External/MicroPython/libmpy.libc`) is commented out.
This is the complete vendored upstream checkout (~4100 files).

**Note:** distinct from `Archive/MicroPython/`, which holds only the
ExistOS port glue (`py/ extmod/ shared/ mpconfigport.h ...`).

**To revive:** move back to `External/MicroPython/` and re-enable the
`libmpy.libc` link line in `System/CMakeLists.txt`.
