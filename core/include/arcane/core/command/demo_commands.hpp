#pragma once

#include "arcane/core/command/command_registry.hpp"

// Registers all demonstration commands.
// Currently registers: counter.add
void register_demo_commands(CommandRegistry& registry);
