# Copilot guidance for llm_router

## Architecture overview
- `src/main.cpp` boots the app: load `config/router.json` with `config::ConfigLoader`, create the `routing::Router`, and hand the router + port to `http::Server`. Everything routes through that chain.
- `config::ConfigLoader` (`src/config/loader.cpp`) parses the JSON into `Config`/`Target`/`Route` structs so routing decisions stay data-driven. Keep any structural changes aligned with that file.
- `http::Server` (`src/http/server.cpp`) registers a single `svr_.Post(".*", …)` handler (httplib) that turns each request into a `providers::RequestContext` and asks `Router` for a target before delegating to a provider client.
- `routing::Router` (`src/routing/router.cpp`) iterates `config_.routes`, matches `path` prefixes + optional `modelPrefix`, and currently only executes round-robin via `StrategyType::RoundRobin`. Targets are looked up by name in `targets_`; clients live in `clients_` keyed by `TargetType`.
- Only `providers::OpenAIClient` (`src/providers/openai_client.*`) is wired into `Router::getClient`; the other `TargetType` values are placeholders.

## Key patterns & conventions
- Add new provider behavior by subclassing `providers::BaseClient` (see `src/providers/base_client.h`), implementing `sendRequest`, and registering the client in `routing::Router::Router` so it’s available for target routing.
- Route definitions must include `match.path` (prefix match) and/or `match.modelPrefix`. The router stops at the first matching route and applies the listed strategy/targets (see config/router.json for the current sample).
- Config files are parsed via `vendor/vjson`. When editing, follow the structure in `config/router.json`; every `Target` can optionally declare `apiKeyEnv` + `models`, and every `Route` defines `strategy.targets` (strings matching target names).
- `http::Server` uses `providers::RequestContext` and `RequestContext.method` even though only POST handlers exist today. Preserve that shape when sending mock requests or adding metadata.
- Logging is very simple (`src/logging/logger.*`) and always prints to stdout. `Config.logging` values are stored but not consumed yet; update logger usage deliberately.

## Workflows
- Build with CMake (Windows): from repo root run `cmake -S . -B build` once, then `cmake --build build --config Release`. Re-run `cmake --build` after source changes.
- The binary reads `config/router.json` relative to its working directory, so run `build/llm_router.exe` (or the exe under `build/Release`) with the repo root as cwd or pass a path via the hard-coded string if you refactor it.
- There is no automated test suite; the only tests live under `vendor/vjson/test` but are not wired into the top-level build. Verify behavior manually by curling the running server.
- `providers::OpenAIClient` shells out to `curl.exe` and writes a temporary JSON file (`temp_req_*.json`). Keep `curl.exe` on the PATH, clean up any lingering temp files, and ensure `OPENAI_API_KEY` (or other env var named in the target) is set before hitting OpenAI routes.
- The OpenAI client's URL inflation removes duplicate `/v1` segments (if both base URL and request path include `/v1`). Mirror the same logic if adding other clients so route normalization stays consistent.

## Dependencies & external integration notes
- VS only: CMake lists `ws2_32` and `crypt32` under `LIBS` (CMakeLists.txt). No other third-party packages are required beyond the included `vendor/httplib.h` and `vendor/vjson/*` files, which are compiled directly.
- HTTP hosting is single-threaded via httplib; the `config.server` fields (`maxConcurrentRequests`, `requestTimeoutMs`) are parsed but not enforced anywhere—treat them as future knobs.
- Add new providers incrementally: copy `src/providers/openai_client` patterns for temp file handling, logging, and `curl` invocation, then wire the new `TargetType` and update `config/router.json` routes.

## Suggested reference points
- `CMakeLists.txt` for include dirs and the single executable target.
- `config/router.json` for the canonical runtime config and route/target examples.
- `src/http/server.cpp` to understand request handling + response flow.
- `src/routing/router.cpp` for matching + strategy logic.
- `src/providers/openai_client.cpp` for how API keys/environment variables and curl invocations are structured.
- `vendor/httplib.h` and `vendor/vjson/*` for third-party parsing/HTTP behavior that can’t be modified.

## Questions for you
- Should the build instructions mention multiple configurations (Debug/Release) or is Release the preferred default?
- Are there additional routes or providers we should prioritize documenting as we expand this router?

Please flag any missing context or unclear items so I can iterate on these instructions.