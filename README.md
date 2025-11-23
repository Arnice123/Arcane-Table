# Arcane Table

Arcane Table is a modern, modular, high-performance Virtual Tabletop (VTT) platform built with C++20, Qt, OpenGL, Boost.Asio, SQLite, and Lua.
It’s designed to be fast, extensible, and fully customizable — an alternative to Roll20, Foundry VTT, and Owlbear Rodeo, with a strong emphasis on engine-level flexibility and plugin support.

Arcane Table aims to be both a lightweight tactical map tool and a full rules-aware RPG engine, depending on how much automation you enable.

```
YourProject/
│
├── cmake/                # CMake modules, custom find scripts
├── docs/                 # Design docs, architecture notes
├── tools/                # Internal dev tools (asset processor, etc.)
├── scripts/              # Build scripts, dev utilities
│
├── external/             # Third-party libs (Lua, tinyxml2, stb, etc.)
│
├── engine/               # Core game engine (shared by client & server)
│
├── client/               # Full client application
│
├── server/               # Dedicated server
│
├── plugins/              # Built-in or example plugins
│
├── assets/               # Default assets: icons, UI themes, test maps
│
├── test/                 # Unit tests / integration tests
│
└── CMakeLists.txt

engine/
│
├── core/                 # Core data structures & utilities
│   ├── types/            # Token, Character, Map, DiceResult, etc.
│   ├── events/           # Event bus system
│   ├── logging/          # Logging system
│   ├── config/           # Config loader/saver
│   └── serialization/    # JSON/binary serialization helpers
│
├── data/                 # Persistent storage layer
│   ├── sqlite/           # SQLite wrapper classes
│   ├── migrations/       # Schema versioning
│   └── models/           # DB models (Campaign, Map, Character, etc.)
│
├── scripting/            # Lua engine integration
│   ├── lua_runtime/      # Lua state mgmt
│   ├── bindings/         # Bindings to engine objects
│   └── scripts/          # Built-in Lua scripts
│
├── rules/                # Rules engine and automation logic
│   ├── dice/             # Dice parsing, expression evaluation
│   ├── conditions/       # Condition/Status definitions
│   ├── initiative/       # Turn order engine
│   └── encounters/       # Encounter automation
│
├── permissions/          # Permissions and access control
│
└── plugin_api/           # API exposed to plugins

client/
│
├── ui/                     # Qt UI code
│   ├── windows/            # Main window, dialogs
│   ├── panels/             # Dockable panels (chat, sheets, journal)
│   ├── widgets/            # Custom Qt widgets
│   └── themes/             # Stylesheets, icons
│
├── rendering/              # OpenGL rendering engine
│   ├── gl/                 # Raw GL wrappers
│   ├── shaders/            # GLSL shader files
│   ├── map/                # Map renderer
│   ├── tokens/             # Token renderer
│   ├── lighting/           # Dynamic lighting system
│   └── fog/                # Fog-of-war brushing & masks
│
├── systems/                # Client-side game systems
│   ├── input/              # Mouse/keyboard input handling
│   ├── token_manager/      # Client-side token logic
│   ├── map_manager/        # Local mapview logic
│   ├── chat/               # Chat GUI + engine connector
│   ├── character_sheets/   # UI + engine integration
│   ├── journal/            # Panel + DB integration
│   ├── bestiary/           # Panel + DB integration
│   └── gm_tools/           # GM-only interactions
│
├── networking/             # Networking client
│   ├── protocol/           # Message types, encoding
│   ├── handlers/           # Handlers for incoming messages
│   └── connection/         # Client session (Boost.Asio)
│
└── app/                    # Client entry point
server/
│
├── network/                # Boost.Asio server implementation
│   ├── session/            # Client sessions
│   ├── dispatcher/         # Routes messages to handlers
│   └── protocol/           # Shared message definitions
│
├── systems/                # Server-side game systems
│   ├── campaign_manager/
│   ├── token_manager/
│   ├── initiative/
│   ├── encounter_engine/
│   ├── status_manager/
│   └── fog_lighting/       # Server-side vision logic
│
├── permissions/            # Server-side permission enforcement
│
├── scripting/              # Lua engine on server
│
├── plugin_host/            # Plugin loader for server-side plugins
│
└── app/                    # Main server entry point

plugins/
│
├── examples/
│   ├── basic_ruleset/           # Example D&D-like plugin
│   ├── custom_dice/             # Example dice extension
│   └── ui_panel/                # Example UI plugin
│
└── api_headers/                 # Headers required by plugin devs

external/
│
├── lua/
├── glm/
├── stb/
├── sqlite3/
├── rapidjson/
└── implot/ (optional visualization)

assets/
│
├── icons/
├── ui/
├── fonts/
├── example_maps/
├── example_tokens/
└── shaders/

tools/
│
├── asset_importer/
├── migration_tool/
├── map_processor/
└── script_debugger/

test/
│
├── engine/
├── client/
├── server/
└── plugins/
```
