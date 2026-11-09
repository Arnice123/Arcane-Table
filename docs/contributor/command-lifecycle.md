# Command Lifecycle

## 1. Overview

Arcane Table uses a command-based architecture to manage game-state changes.

Every requested state change passes through a central `CommandProcessor`. Commands never modify game state independently. Instead, the processor controls command creation, authorization, validation, application, and notification.

This architecture enforces a **single-writer model**: only the processor's worker thread may access or modify the `Store`.

Key responsibilities:

- **Command:** Defines a requested operation and its behavior.
- **CommandRegistry:** Maps command names to factories.
- **CommandProcessor:** Executes commands sequentially, manages revisions, and broadcasts accepted changes.
- **Store:** Holds authoritative game state.
- **Runtime:** Provides worker threads and execution infrastructure.

The command system serves as the foundation for future game systems, including character data, permissions, inventory, maps, combat, and networking.

## 2. Command Lifecycle

Every submitted command follows the same execution pipeline.

```text
Caller (any thread)
       |
       v
CommandProcessor::submit()
       |
       v
Runtime Worker Thread
       |
       v
+------------------------+
| 1. CREATE               |
| Registry factory       |
+------------------------+
       |
       v
+------------------------+
| 2. AUTHORIZE            |
| Check client permission|
+------------------------+
       |
       v
+------------------------+
| 3. VALIDATE             |
| Check game rules       |
+------------------------+
       |
       v
+------------------------+
| 4. APPLY                |
| Modify Store           |
+------------------------+
       |
       v
+------------------------+
| 5. BROADCAST            |
| Notify listeners       |
+------------------------+
       |
       v
    Delta returned
```

Any rejection during stages 1–3 stops execution immediately. Exceptions during stages 1–4 produce an `Internal` rejection.

Rejected commands never modify the Store or consume a revision.

### 2.1 Submission

Commands enter the system through:

`CommandProcessor::submit(CommandMeta, name, payload)`

Submission accepts calls from any thread.

Arguments:

- `CommandMeta`: Identifies requesting client and its sequence number.
- `name`: Registered command identifier.
- `payload`: Serialized command arguments.

Returns `std::future<CommandResult>`.

The processor schedules execution on its dedicated Runtime worker thread. Submission does not execute command logic on the calling thread.

### 2.2 Create

**Responsible component:** `CommandRegistry`

The registry resolves command names into concrete command objects.

Each registered command type provides a factory:

`Factory = std::function<std::unique_ptr<Command>(const std::string&)>`

The factory receives serialized payload and constructs the appropriate command.

Possible outcomes:

| Outcome | Result |
|---|---|
| Registered name, valid payload | Command object |
| Unknown name | `UnknownCommand` |
| Invalid payload / nullptr factory result | `BadSchema` |
| Factory throws exception | `Internal` |

Factories should handle payload parsing and basic schema checks.

They should not modify game state.

### 2.3 Authorize

**Responsible component:** `Command::authorize()`

Determines whether requesting client may execute command.

Receives:

- `const Store&`: Read-only game state.
- `ClientId`: Requesting client.

Returns `std::optional<Reject>`.

A successful authorization returns `std::nullopt`.

A failed authorization returns `RejectCode::NotPermitted`.

Authorization handles access restrictions, not game-rule validation.

Examples:

- Client attempting GM-only operation.
- Player modifying another player's character.
- Unauthorized inventory modification.
- Client attempting administrative operation.

Future PERMS epic should extend authorization logic without changing the processor's execution pipeline.

Authorization must not modify Store.

### 2.4 Validate

**Responsible component:** `Command::validate()`

Determines whether requested operation complies with current game rules and state.

Receives:

- `const Store&`: Read-only game state.

Returns `std::optional<Reject>`.

A successful validation returns `std::nullopt`.

A failed validation may return:

- `RuleViolation`: Operation violates game rules.
- `Conflict`: Operation conflicts with existing state.

Examples:

- Moving token into an invalid location.
- Exceeding inventory capacity.
- Performing an action without sufficient resources.
- Attempting to occupy an unavailable space.

Validation must not modify Store.

### 2.5 Apply

**Responsible component:** `Command::apply()`

Executes accepted command and modifies authoritative game state.

Receives:

- `Store&`: Mutable game state.

Returns a human-readable description of the change.

Only called after successful creation, authorization, and validation.

The processor assigns revision numbers, not individual commands.

**Important guarantees:**

- Apply must not run for rejected commands.
- Store mutations must happen on processor worker thread.
- Apply exceptions produce `RejectCode::Internal`.
- Apply must not leave partially modified state when throwing.

The final point is an important requirement for future command implementations: the processor cannot automatically roll back arbitrary mutations.

### 2.6 Revision Assignment

**Responsible component:** `CommandProcessor`

Every successfully applied command receives a monotonically increasing revision.

First accepted command receives revision `1`.

Example:

| Command | Result | Revision |
|---|---|---|
| Add 5 | Accepted | 1 |
| Invalid payload | Rejected | None |
| Unauthorized operation | Rejected | None |
| Add 3 | Accepted | 2 |
| Rule violation | Rejected | None |
| Add 2 | Accepted | 3 |

Revisions represent authoritative ordering of accepted state changes.

Revision counter belongs to each CommandProcessor instance. It must not use global or static state.

### 2.7 Broadcast

**Responsible component:** `CommandProcessor`

After successful application, processor creates a `Delta` and notifies registered listeners.

Listener signature:

`std::function<void(const Delta&, const CommandMeta&)>`

Listeners receive:

- Accepted command's Delta.
- Original requesting client's metadata.

Listeners execute in revision order.

Potential future consumers:

- Networking layer broadcasting changes to clients.
- Logging and audit systems.
- Event tracking.
- Debugging tools.

Rejected commands never generate listener notifications.

Listener exceptions must be caught independently. A failing listener must not terminate the processor or prevent future commands from executing.

Promises are fulfilled and listeners called without holding locks.

**Re-entrancy:** Listeners may submit additional commands. However, they must not synchronously wait for those commands to finish, since they execute on the same worker thread.

## 3. Core Components

### 3.1 Command

Abstract interface implemented by individual command types.

Responsibilities:

- Define authorization rules.
- Define validation rules.
- Apply state changes.
- Provide a description of resulting changes.

Commands should contain only information necessary to execute their requested operation.

Commands must not independently manage revisions, worker threads, or listener notifications.

### 3.2 CommandRegistry

Maps command identifiers to command factories.

Responsibilities:

- Register command types.
- Prevent duplicate registrations.
- Construct commands from serialized payloads.
- Report unknown commands and invalid payloads.

Registration occurs during startup, before command submission begins.

Registry is not intended for concurrent modification.

Adding new command types should require registration, not modifications to a central switch statement.

### 3.3 CommandProcessor

Central coordinator for command execution.

Responsibilities:

- Accept submissions from arbitrary threads.
- Schedule work on Runtime's worker thread.
- Execute pipeline stages in order.
- Maintain authoritative Store.
- Maintain revision counter.
- Fulfil submission promises.
- Notify listeners.
- Handle exceptions without terminating processing.

Each processor owns its own Store and revision counter.

### 3.4 Store

Authoritative game-state container.

Currently provides basic key/value storage for demonstrating command execution.

Store is deliberately not thread-safe.

Only processor worker thread may access Store directly.

Other threads must use `CommandProcessor::inspect()` to read state safely.

### 3.5 Runtime

Provides execution infrastructure shared by application systems.

CommandProcessor receives Runtime through its constructor:

`CommandProcessor(CommandRegistry reg, Runtime& rt)`

Runtime provides the worker thread used for command processing.

Runtime must outlive CommandProcessor.

Shutdown order must account for dependencies. CommandProcessor stops after `net` and before `rules`, as specified by Runtime's lifecycle.

## 4. Threading and Execution Guarantees

Command processing follows a single-writer model.

### Single-writer rule

Only one worker thread executes commands for a given processor.

Consequences:

- Commands execute sequentially.
- Store requires no mutex.
- Validation observes consistent state.
- Revisions cannot be assigned concurrently.
- Listener notifications follow accepted revision order.

Multiple threads may submit simultaneously. Their execution order follows the order in which submissions enter the processor's work queue.

This does not guarantee ordering based on client sequence numbers or wall-clock submission timestamps.

### Inspecting game state

Other threads must not directly access Store.

Use:

`CommandProcessor::inspect(std::function<void(const Store&)>)`

Inspection schedules a read operation on the same worker thread.

This guarantees inspection occurs in sequence with command processing.

The returned future completes once inspection finishes.

Inspection callbacks must not modify Store.

### Exception handling

Exceptions from command creation, authorization, validation, or application become `RejectCode::Internal`.

Listener exceptions are isolated from command execution.

The worker continues processing subsequent jobs after handled failures.

## 5. Extending the Command System

Future epics should implement new functionality through additional command types.

Adding a command should generally require three steps.

### Step 1: Implement command

Create a class inheriting from `Command`.

Implement:

- `authorize()`
- `validate()`
- `apply()`

Keep responsibilities separated:

| Method | Responsibility |
|---|---|
| `authorize()` | Determine whether client may perform operation |
| `validate()` | Determine whether operation is valid |
| `apply()` | Perform state mutation |

### Step 2: Register command factory

Register unique command identifier and corresponding factory with `CommandRegistry`.

Factory should parse payload and construct command object.

Malformed payloads should return `nullptr`.

### Step 3: Add tests

Every new command should test:

- Valid execution.
- Invalid payload.
- Authorization failures.
- Validation failures.
- Correct Store mutations.
- Correct returned Delta.
- Rejection without Store mutation.
- Revision behavior.

Additional tests should cover command-specific edge cases.

## 6. Guidelines for Future Epics

### DATA

Commands modifying character data, inventories, maps, and other persistent game information should use the existing pipeline.

Data validation belongs in command validation.

Store mutations belong in apply.

### PERMS

Authorization should handle permissions consistently across command types.

Avoid duplicating permission logic inside individual commands when shared permission infrastructure becomes available.

### Networking

Network requests should submit commands through CommandProcessor rather than modifying game state directly.

Listeners provide the initial integration point for broadcasting accepted changes.

Clients should receive authoritative revisions to maintain synchronized state.

### Combat and Rules

Combat actions, movement restrictions, spell effects, and other game mechanics should implement validation through command types.

Rules should inspect current Store state before allowing mutations.

### Persistence

Database operations should respect Runtime's execution model.

Persistence integration must not introduce direct concurrent access to Store.

## 7. Architectural Invariants

All future implementations must preserve these guarantees:

1. Only processor worker thread accesses its Store.
2. Every command follows Create → Authorize → Validate → Apply → Broadcast.
3. Rejected commands never modify Store.
4. Rejected commands never consume revisions.
5. Revisions increase monotonically without gaps.
6. Only accepted commands generate listener notifications.
7. Listeners execute in revision order.
8. Listener exceptions cannot terminate processing.
9. Command exceptions cannot terminate processing.
10. Inspection executes on processor worker thread.
11. Runtime outlives CommandProcessor.
12. Shutdown drains already queued commands before worker termination.

Changes to these invariants require deliberate architectural review.

## 8. Current Limitations

The current command system provides the foundation for authoritative game-state changes, not a complete multiplayer synchronization protocol.

Not yet covered:

- Client sequence-number deduplication.
- Command replay and rollback.
- Persistent revision history.
- Network acknowledgements.
- Transactional rollback after partial application.
- Multi-processor shared-state coordination.
- Dynamic command registration after startup.

These concerns should be addressed by their respective future epics without weakening the single-writer model.

---

**Design principle:** All authoritative game-state changes pass through CommandProcessor. Individual systems define operations; processor controls execution, ordering, and acceptance.