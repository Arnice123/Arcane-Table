// Spike S10 smoke test: if this builds and prints, the vcpkg dependency chain
// works on this machine/compiler/triplet. Exit code 0 = every check passed.

#include <cstdio>
#include <cstring>

#include <sqlite3.h>
#include <lua.hpp>  // C++ wrapper: wraps the C headers in extern "C"

#if defined(ARCANE_HAVE_SPIKE_DEPS)
#include <benchmark/benchmark.h>
#include <flatbuffers/base.h>
#include <google/protobuf/stubs/common.h>
#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>
#endif

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    std::printf("  [%s] %s\n", ok ? " ok " : "FAIL", what);
    if (!ok) ++failures;
}

void sqlite_checks() {
    std::printf("SQLite %s\n", sqlite3_libversion());
    sqlite3* db = nullptr;
    const bool opened = sqlite3_open(":memory:", &db) == SQLITE_OK;
    check(opened, "open in-memory database");
    if (opened) {
        const int rc = sqlite3_exec(db, "CREATE TABLE t(x); INSERT INTO t VALUES (42);",
                                    nullptr, nullptr, nullptr);
        check(rc == SQLITE_OK, "create table and insert");
    }
    sqlite3_close(db);
}

void lua_checks() {
    lua_State* L = luaL_newstate();
    check(L != nullptr, "create Lua state");
    if (!L) return;
    luaL_openlibs(L);  // fine for a smoke test; the S5 sandbox must NOT do this
    lua_getglobal(L, "_VERSION");
    std::printf("Lua runtime: %s (headers: %s)\n", lua_tostring(L, -1), LUA_RELEASE);
    lua_pop(L, 1);

    const int rc = luaL_dostring(L, "return 1 + 1");
    check(rc == LUA_OK && lua_tointeger(L, -1) == 2, "run 'return 1 + 1'");
    lua_close(L);
}

#if defined(ARCANE_HAVE_SPIKE_DEPS)
void spike_checks() {
    std::printf("Spike libraries:\n");
    std::printf("  Protobuf %s\n",
                google::protobuf::internal::VersionString(GOOGLE_PROTOBUF_VERSION).c_str());
    std::printf("  FlatBuffers %d.%d.%d\n", FLATBUFFERS_VERSION_MAJOR,
                FLATBUFFERS_VERSION_MINOR, FLATBUFFERS_VERSION_REVISION);
    std::printf("  sol2 %s\n", SOL_VERSION_STRING);

    sol::state lua;
    lua.open_libraries(sol::lib::base);
    lua.set_function("twice", [](int x) { return 2 * x; });
    const int result = lua.script("return twice(21)");
    check(result == 42, "sol2 binds a C++ function");
}
#endif

}  // namespace

int main() {
    sqlite_checks();
    lua_checks();
#if defined(ARCANE_HAVE_SPIKE_DEPS)
    spike_checks();
#else
    std::printf("Spike libraries: not built (preset without the 'spikes' feature)\n");
#endif
    std::printf("%s\n", failures == 0 ? "deps_smoke: all checks passed" : "deps_smoke: FAILED");
    return failures == 0 ? 0 : 1;
}
