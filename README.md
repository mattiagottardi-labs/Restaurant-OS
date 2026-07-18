# 🍽️ Restaurant Simulation — OS Lab Project 2026-5

A multithreaded restaurant simulation written in **C** and **Bash**, implementing concurrent customers, waiters, and cooks communicating via shared data structures and synchronization primitives.

---

## Table of Contents

- [Project Overview](#project-overview)
- [Development Steps](#development-steps)
- [File Structure](#file-structure)
- [Configuration](#configuration)
- [Building & Running](#building--running)
- [Input File Formats](#input-file-formats)
- [Architecture](#architecture)
- [Scoring System](#scoring-system)
- [Edge Cases & Concurrency Notes](#edge-cases--concurrency-notes)

---

## Project Overview

The simulation models a restaurant with:
- **Customers** — spawned randomly, place orders, have a patience timer
- **Waiters** — take orders, assign dishes to cooks, serve food, entertain waiting customers
- **Cooks** — prepare dishes sequentially, acquire/release kitchen resources, optionally clean them at the sink

Performance is tracked via a **global score** updated in a thread-safe manner by multiple parties.

---

## Development Steps

Follow this order to build the project incrementally, testing each layer before proceeding.

### Step 1 — Project Skeleton & Build System

- [ ] Create the directory structure (`code/`, `data/`, `report.pdf` placeholder)
- [ ] Write a `Makefile` (or `build.sh`) supporting `build`, `clean`, and `run` targets
- [ ] Create a minimal `main.c` that just prints "Restaurant starting" and exits cleanly
- [ ] Verify `make build` and `make clean` work on Ubuntu 24.04

### Step 2 — Configuration & Bootstrapping (`bootstrap.sh`)

- [ ] Write `bootstrap.sh` to load and validate the `.env` file
- [ ] Validate all required parameters are present and have correct types/ranges:
  - `NUM_COOKS`, `NUM_WAITERS`, `MAX_CUSTOMERS`, `TOTAL_CUSTOMERS` → positive integers
  - `GAME_SPEED` → positive number
  - `RANDOM_SEED` → integer
  - `MENU_FILE`, `RESOURCES_FILE` → paths to readable files
- [ ] Support `--env-file=<path>` argument to override default `.env`
- [ ] Support `--<param>=<value>` argument overrides (applied after `.env` validation)
- [ ] Print clear error messages and exit with a non-zero code on any failure
- [ ] On success, export variables and exec the main binary (`./restaurant`)

### Step 3 — Input Parsing (C)

- [ ] Parse `MENU_FILE` CSV: `name,price,time,requirements`
  - Parse `requirements` field: split on `;`, then on `:` for optional quantity
  - Store as a struct array; validate all fields (non-empty name, numeric price/time)
- [ ] Parse `RESOURCES_FILE` CSV: `resource,quantity,clean_time`
  - Store as a struct array; validate fields
- [ ] Define and use error codes: `FILE_NOT_FOUND`, `MALFORMED_CSV`, `INVALID_PARAMETER`, etc.
- [ ] Write unit-style tests (simple `main()` test driver) for both parsers

### Step 4 — Shared Data Structures & Synchronization Primitives

- [ ] Define the **kitchen resource pool** structure:
  - Available count, dirty count, consecutive dirty uses per resource
  - Mutex + condition variable for resource acquisition
- [ ] Define the **sink** as a mutex-guarded shared resource (one cook at a time)
- [ ] Define the **global score** as an `int` protected by a mutex
- [ ] Define the **cook queue** per cook: a thread-safe queue of dishes (mutex + condvar)
- [ ] Define the **customer state**: order list, patience level, dishes served count — all mutex-protected

### Step 5 — Cook Threads

- [ ] Spawn `NUM_COOKS` cook threads at startup, each initialized with a derived RNG seed
- [ ] Implement the cook loop:
  1. Dequeue next dish from own queue (block if empty)
  2. Attempt to acquire required kitchen resources
     - If unavailable: either wait (condvar), pick another queued dish, or go clean dirty resources
  3. `usleep(cooking_time / GAME_SPEED)` — **no global locks held during sleep**
  4. Release resources; notify waiter the dish is ready
  5. Decide whether to clean resources or leave dirty (strategy of your choice)
- [ ] Implement **sink cleaning**:
  - Acquire the sink mutex
  - For each dirty resource: `usleep(clean_time / GAME_SPEED)`, mark resource clean
  - Release sink mutex; wake waiting cooks
- [ ] Apply dirty-use score penalty:  `2^(consecutive_dirty_uses) * log2(1 + clean_time)`
- [ ] Handle deadlock prevention (e.g., resource ordering, timeout + backoff)

### Step 6 — Customer Threads

- [ ] Spawn customers from the main thread at random intervals (up to `TOTAL_CUSTOMERS`, capped at `MAX_CUSTOMERS` concurrently)
- [ ] On spawn: randomly select dishes from the menu; assign patience level as a random integer strictly greater than the total cooking time of the order
- [ ] Decrement patience over time using `usleep` + a countdown loop (thread-safe updates)
- [ ] On full service: add score `total_price * (1 - time_to_serve / patience_level)`
- [ ] On patience exceeded: subtract score `total_price * log2(1 + patience_level / (1 + dishes_served))`
- [ ] Signal the main thread when leaving (decrement active customer count)

### Step 7 — Waiter Threads

- [ ] Spawn `NUM_WAITERS` waiter threads at startup
- [ ] Implement order intake: pick up a customer's order (from a shared orders queue)
- [ ] Implement **dish scheduling**: assign dishes to cooks considering:
  - Cooking time vs. customer patience
  - Dish price
  - Current queue length of each cook
  - (Document your chosen algorithm in the report)
- [ ] When a cook signals a dish is ready, the responsible waiter serves it to the customer
- [ ] When idle: attempt to entertain a waiting customer
  - Entertainment fires with configurable probability
  - Adjusts customer patience by a random amount (up or down) — thread-safe

### Step 8 — Main Thread & Periodic Status

- [ ] Write PID to `/tmp/restaurant.pid` on startup
- [ ] Initialize all shared structures and seed the global RNG
- [ ] Spawn cook and waiter threads (pass per-thread RNG seeds)
- [ ] Spawn customer threads at random intervals; enforce `MAX_CUSTOMERS` cap with a semaphore or condvar
- [ ] Print a periodic status update (e.g., every 2 seconds of sim time) containing:
  - Current score
  - Customers currently in restaurant
  - % of total customers spawned
- [ ] Wait for all customers to be processed, then signal cooks/waiters to terminate cleanly
- [ ] Join all threads; free all resources; remove `/tmp/restaurant.pid`

### Step 9 — Signal Handling & `status.sh`

- [ ] Register a `SIGUSR1` handler in the main process
- [ ] On `SIGUSR1`, print a full state snapshot:
  - Current score
  - Customers in restaurant / unserved count
  - Spawned vs. total customers
  - Each cook's queue length
  - Each kitchen resource's current availability
- [ ] Write `status.sh`: read PID from `/tmp/restaurant.pid` and send `SIGUSR1`
- [ ] Handle missing PID file gracefully

### Step 10 — Error Handling Audit

- [ ] Every `pthread_*`, `malloc`, file I/O, and system call must check return values
- [ ] All error codes defined consistently in C (`#define`) — document them in the report
- [ ] `bootstrap.sh` must validate `.env` before passing anything to the binary
- [ ] Malformed CSV → descriptive error + non-zero exit, no crash

### Step 11 — Edge Case Testing

Test each scenario explicitly:

- [ ] All resources occupied → no deadlock; cooks eventually proceed
- [ ] Two cooks race for the same resource → only one proceeds, other waits
- [ ] Customer patience runs out mid-cook → partial order handled, score updated correctly
- [ ] Sink occupied → cook blocks or chooses alternative (no busy-wait)
- [ ] All cooks at sink → only one proceeds; others block without starvation
- [ ] `GAME_SPEED=100` → simulation runs fast without crashes or race conditions
- [ ] Missing/invalid `.env` → `bootstrap.sh` rejects gracefully
- [ ] Malformed CSV → binary prints error and exits

### Step 12 — Report (`report.pdf`)

Write the report (≤ 5 pages, PDF format) covering:

- [ ] High-level architecture and thread model
- [ ] Synchronization strategy (which mutexes/condvars protect what)
- [ ] Waiter scheduling algorithm and rationale
- [ ] Cook decision logic (wait vs. cook another dish vs. clean)
- [ ] Deadlock prevention approach
- [ ] Known limitations or deviations from spec (if any)

### Step 13 — Packaging & Submission

- [ ] Ensure `make clean` removes all binaries, object files, named pipes, and IPC resources
- [ ] Confirm `make build && make run` works on a clean Ubuntu 24.04 machine
- [ ] Package: `tar -czf Surname1_Surname2_Surname3.tar.gz report.pdf code/`
- [ ] Verify archive contents before uploading to Moodle
- [ ] Press **Submit** in Moodle only when the group is ready — submission is final

---

## File Structure

```
.
├── Makefile              # or build.sh
├── bootstrap.sh          # env validation + launcher
├── status.sh             # sends SIGUSR1 to running process
├── .env                  # simulation parameters (not committed to VCS)
├── report.pdf            # ≤5 page project report
├── resources/
│   ├── menu.csv          # dish definitions
│   ├── resources.csv     # kitchen resource definitions
|   └── 2026-project-5    # reference pdf
└── code/
    ├── main.c
    ├── cook.c / cook.h
    ├── waiter.c / waiter.h
    ├── customer.c / customer.h
    ├── kitchen.c / kitchen.h   # resource pool + sink
    ├── score.c / score.h
    ├── parser.c / parser.h     # CSV + .env parsing
    ├── errors.h                # error code definitions
    └── utils.c / utils.h       # RNG helpers, logging, etc.
```

---

## Configuration

Create a `.env` file in the project root:

```env
NUM_COOKS=4
NUM_WAITERS=2
MAX_CUSTOMERS=10
TOTAL_CUSTOMERS=30
MENU_FILE=data/menu.csv
RESOURCES_FILE=data/resources.csv
GAME_SPEED=5
RANDOM_SEED=42
```

| Parameter         | Type            | Description                                              |
|-------------------|-----------------|----------------------------------------------------------|
| `NUM_COOKS`       | positive int    | Number of cook threads                                   |
| `NUM_WAITERS`     | positive int    | Number of waiter threads                                 |
| `MAX_CUSTOMERS`   | positive int    | Max concurrent customers in the restaurant               |
| `TOTAL_CUSTOMERS` | positive int    | Total customers that will visit during the simulation    |
| `MENU_FILE`       | path            | Path to menu CSV file                                    |
| `RESOURCES_FILE`  | path            | Path to kitchen resources CSV file                       |
| `GAME_SPEED`      | positive number | Simulation speed multiplier (1 = real-time, >1 = faster)|
| `RANDOM_SEED`     | integer         | Seed for reproducible randomization                      |

---

## Building & Running

```bash
# Build the project
make build

# Run the default scenario
make run

# Run with argument overrides
make run ARGS="--num-cooks=6 --game-speed=10"

# Run manually via bootstrap
./bootstrap.sh --num-cooks=5 --game-speed=3

# Check status of a running simulation
./status.sh

# Clean all build artifacts and IPC resources
make clean
```

---

## Input File Formats

### `menu.csv`

```csv
name,price,time,requirements
Carbonara,12,15,pot;pan;burner:2
Margherita,10,20,burner:3;pan
Tiramisu,8,5,bowl
```

- `requirements`: semicolon-separated resource names, with optional `:<quantity>` suffix (default quantity = 1)

### `resources.csv`

```csv
resource,quantity,clean_time
pot,3,2
pan,4,1
burner,6,3
bowl,5,1
```

---

## Architecture

```
Main Thread
  ├── Spawns NUM_COOKS cook threads
  ├── Spawns NUM_WAITERS waiter threads
  ├── Randomly spawns customer threads (up to MAX_CUSTOMERS at once)
  └── Prints periodic status; handles SIGUSR1 for full dump

Customer Thread
  └── Places order → waits for all dishes → updates score → exits

Waiter Thread
  └── Takes order → schedules dishes to cook queues → serves dishes → entertains customers

Cook Thread
  └── Dequeues dish → acquires resources → cooks → releases → optionally cleans at sink
```

**Shared resources:** kitchen resource pool, sink, score, cook queues, customer states, orders queue — all protected by mutexes and condition variables.

---

## Scoring System

| Event | Formula |
|-------|---------|
| Customer fully served | `total_price × (1 − time_to_serve / patience_level)` |
| Customer leaves unserved | `−total_price × log2(1 + patience_level / (1 + dishes_served))` |
| Cook uses dirty resource | `−2^(consecutive_dirty_uses) × log2(1 + clean_time)` |

---

## Edge Cases & Concurrency Notes

- **No busy-waiting**: all blocking uses `pthread_cond_wait` or blocking dequeue, never a spin loop
- **No global locks during sleep**: `usleep()` calls for cooking/cleaning are always made with only local or no locks held
- **Deadlock prevention**: document your chosen strategy (e.g., total resource ordering, try-lock with backoff)
- **RNG reproducibility**: each thread receives its own seed derived from the main thread's RNG; given the same `RANDOM_SEED`, the simulation is fully reproducible
- **SIGUSR1 safety**: the signal handler only sets a flag; the actual status print happens in the main thread loop to avoid async-signal-safety issues
