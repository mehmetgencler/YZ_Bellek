# nlohmann-json placeholder

This directory is where the nlohmann/json single-header should live for offline builds.

You can either:

- Run the provided script to download it now:

  ./scripts/download_json.sh

- Or let CMake attempt to download it at configure time (it will try automatically if missing).

After the header is available, it must be at:

third_party/nlohmann-json/single_include/nlohmann/json.hpp
