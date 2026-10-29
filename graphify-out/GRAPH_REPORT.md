# Graph Report - skyrim-together-tweaks  (2026-10-29)

## Corpus Check
- 22 files · ~3,559 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 10 file(s) not represented in the graph (top: (none) 5, .in 2, .ini 1)

## Summary
- 121 nodes · 140 edges · 12 communities (10 shown, 2 thin omitted)
- Extraction: 99% EXTRACTED · 1% INFERRED · 0% AMBIGUOUS · INFERRED: 2 edges (avg confidence: 0.85)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `b14696c0`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- PCH.h
- main.cpp
- Tweaks.h
- MH_CreateHookTyped
- UIActivationFix.cpp
- MemoryScan.cpp
- build.yml
- steps
- ChangeUIActivationKeys.cpp
- vcpkg.json
- ScopedMemoryWrite
- Skyrim Together Tweaks

## God Nodes (most connected - your core abstractions)
1. `steps` - 6 edges
2. `ScopedMemoryWrite` - 6 edges
3. `ParsePattern()` - 5 edges
4. `Skyrim Together Tweaks` - 5 edges
5. `with` - 4 edges
6. `ScanRegion()` - 4 edges
7. `StringToAOB()` - 4 edges
8. `memscan::FindPattern()` - 4 edges
9. `MH_CreateHookTyped()` - 4 edges
10. `RealInitialize()` - 4 edges

## Surprising Connections (you probably didn't know these)
- `AppendVersionInCrashLogs::Install()` --calls--> `MH_CreateHookTyped()`  [INFERRED]
  src/tweaks/AppendVersionInCrashLogs.cpp → src/PCH.h
- `UIActivationFix::Install()` --calls--> `MH_CreateHookTyped()`  [INFERRED]
  src/tweaks/UIActivationFix.cpp → src/PCH.h

## Import Cycles
- None detected.

## Communities (12 total, 2 thin omitted)

### Community 1 - "main.cpp"
Cohesion: 0.18
Nodes (6): Initialize(), InitializeLog(), OnInit(), RealInitialize(), SKSEPlugin_Load(), SKSEPlugin_Preload()

### Community 2 - "Tweaks.h"
Cohesion: 0.18
Nodes (3): EnableConsole::Install(), SpawnSkyrimTogetherConsole(), TiltedOnlineApp

### Community 3 - "MH_CreateHookTyped"
Cohesion: 0.20
Nodes (5): MH_CreateHookTyped(), AppendVersionInCrashLogs::Install(), CrashLoggerLogHeaderInfo, real, UIActivationFix::Install()

### Community 4 - "UIActivationFix.cpp"
Cohesion: 0.20
Nodes (4): ApplyNvidiaFix, GetSecondsElapsed(), IsDllBlocked, UiFixVectoredExceptionHandler()

### Community 5 - "MemoryScan.cpp"
Cohesion: 0.35
Nodes (6): memscan::FindPattern(), memscan::FindPatternInST(), memscan::FindString(), ParsePattern(), ScanRegion(), StringToAOB()

### Community 6 - "build.yml"
Cohesion: 0.22
Nodes (8): runs-on, jobs, build, name, on, push, workflow_dispatch, tags

### Community 7 - "steps"
Cohesion: 0.22
Nodes (9): steps, name, run, shell, uses, with, name, path (+1 more)

### Community 9 - "vcpkg.json"
Cohesion: 0.25
Nodes (7): builtin-baseline, dependencies, description, homepage, license, name, version-string

### Community 10 - "ScopedMemoryWrite"
Cohesion: 0.33
Nodes (4): ScopedMemoryWrite, address_, length_, old_protect_

### Community 11 - "Skyrim Together Tweaks"
Cohesion: 0.33
Nodes (5): Building, Credits, License, Requirements, Skyrim Together Tweaks

## Knowledge Gaps
- **3 isolated node(s):** `address_`, `length_`, `old_protect_`
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 65 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **2 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `MH_CreateHookTyped()` connect `MH_CreateHookTyped` to `PCH.h`?**
  _High betweenness centrality (0.076) - this node is a cross-community bridge._
- **What connects `address_`, `length_`, `old_protect_` to the rest of the system?**
  _3 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `PCH.h` be split into smaller, more focused modules?**
  _Cohesion score 0.14285714285714285 - nodes in this community are weakly interconnected._
- **Why does `ScopedMemoryWrite` connect `ScopedMemoryWrite` to `Tweaks.h`?**
  _High betweenness centrality (0.060) - this node is a cross-community bridge._