#pragma once

#include <cstdint>
#include <string>
#include <variant>

using ClientId = std::uint32_t;

// counter for ordering
using Seq = std::uint64_t;

// hosts own counter for accepted changes
using Revision = std::uint64_t;

enum class RejectCode {
	UnknownCommand, // no command by that name
	BadSchema,      // payload couldn't be parsed
	NotPermitted,   // the client is not allowed to do this
	RuleViolation,  // the rules say no
	Conflict,       // clashes with game state (e.g. square taken)
	Internal,       // internal exception
	ClosedProcessor // processor is closed
};

struct Reject {
	RejectCode Code;
	std::string detail;
};

// what changed (text placeholder)
struct Delta { 
	Revision rev;
	std::string description;
};

// who send it
struct CommandMeta {
	ClientId client;
	Seq seq;
};

// the possible outcomes of a submit
using CommandResult = std::variant<Delta, Reject>;