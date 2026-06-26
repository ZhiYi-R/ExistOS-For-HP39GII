# Archived: emu48

Moved here from `System/emu48/`.

**Why:** the HP48 emulator is not part of the active build —
`System/CMakeLists.txt` has `add_subdirectory(emu48)` commented out and
`emu48` is absent from the `aux_source_directory` list.

**To revive:** move back to `System/emu48/` and re-enable the
`add_subdirectory(emu48)` / `target_compile_options(emu48 ...)` lines in
`System/CMakeLists.txt`.
