# proto
Packet spec shared by firmware and ground software (formerly the `universalproto` repo).

- `packets.jsonc` — packet definitions (id, payload, which boards read/write them).
- `types.jsonc` — payload types and enums referenced by `packets.jsonc`.
- `config_<system>.jsonc` — device IDs and sensor channel mappings per system (`vertical`, `cart`).
- `codegen/` — generates C++ packet headers. PlatformIO projects in this repo use it as a
  pre-build script (`extra_scripts = pre:../proto/codegen/compile.py`) and select the system with
  `custom_system` under `[common]`.
