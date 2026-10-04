# Memory-safety-critical parsing (SHA-256, image decoding, the contributor
# feed, audio decoding, Live2D expression files) lives in the bongo-safe Rust
# crate and is linked statically through Corrosion. The Rust toolchain is a
# build requirement; install it from https://rustup.rs (CI runners ship it
# preinstalled).
find_program(BONGO_CAT_CARGO cargo)
if(NOT BONGO_CAT_CARGO)
  message(FATAL_ERROR
    "The Rust toolchain (cargo) is required to build the bongo-safe crate "
    "(memory-safety-critical parsers).\n"
    "Install it from https://rustup.rs and re-run the configure step.")
endif()
if(BONGO_CAT_FETCH_DEPS)
  FetchContent_Declare(Corrosion
    GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
    GIT_TAG v0.5.2)
  FetchContent_MakeAvailable(Corrosion)
else()
  find_package(Corrosion REQUIRED)
endif()
# Corrosion appends its cmake directory to CMAKE_MODULE_PATH inside its own
# (sub-)scope; find_package(Rust) here needs it in this scope.
list(APPEND CMAKE_MODULE_PATH "${corrosion_SOURCE_DIR}/cmake")
find_package(Rust REQUIRED)
corrosion_import_crate(MANIFEST_PATH
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rust/bongo-safe/Cargo.toml"
  CRATE_TYPES staticlib)
