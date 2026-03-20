# ChillFarmGame

A chill, point-and-click farm game built in Unreal Engine 5.7 with C++. The core idea is simple: plant crops, wait for them to grow, harvest, sell, buy better seeds. The world is procedurally generated at runtime so each session feels slightly different.

> **Work in Progress.** The project is in active early development. Core systems are being built and iterated on. Expect rough edges, missing features, and evolving architecture.

---

## Current Branch: TerrainGenerationMVP

All active development is happening on the `TerrainGenerationMVP` branch. This branch focuses on building the foundation — procedural world generation, the tile system, and the editor tooling needed to debug and inspect the generated world. Gameplay systems (planting, harvesting, economy) come after this foundation is solid.

---

## What Exists So Far

### Procedural World Generation
The world is built on a tile grid generated at runtime using Perlin noise. Each tile gets assigned a biome and tile type based on noise values for height and moisture. The generation runs through several post-processing passes (shore detection, neighbor recalculation) before the world is displayed.

The world is rendered using Hierarchical Instanced Static Mesh Components (HISMC) organized into chunks. This is a preview/editor approach — production rendering will move to ProceduralMeshComponent.

### Tile System
Each tile stores its type, biome, fertility, moisture, and other properties. Tiles are mutated through a central `FTileMutationHelper` which also records the history of every change for debugging purposes.

### Tile Inspector (Editor Tool)
A custom editor tool for inspecting and debugging the generated world. This is the main focus of the current branch.

Features:
- Click any tile in the editor viewport to select it
- Selected tile gets highlighted with a yellow outline (rendered via custom SceneProxy)
- A docked Tile Inspector panel shows tile coordinates, type, biome, and full mutation history — every change made to the tile during generation, what caused it, and in what order

This tool exists entirely in the editor module and has zero impact on runtime or shipping builds.

---

## Project Structure

```
ChillFarmGame/
├── Source/
│   ├── ChillFarmGame/           # Runtime module
│   │   ├── WorldGeneration/     # Tile grid, generator, post-processor, biome data
│   │   │   └── TileMutationHistory/  # Mutation tracking system
│   │   ├── Crops/               # Crop actors (early stage)
│   │   ├── Player/              # Player controller
│   │   └── Persistent/          # Game state, save system stubs
│   └── ChillFarmGameEditor/     # Editor-only module
│       ├── TileDebugRender/     # SceneProxy, highlight component, highlight subsystem
│       ├── TileDebugSelection/  # Viewport click notifier, selection subsystem
│       └── Widgets/             # Slate UI — Tile Inspector window, snapshot registry
```

---

## Architecture Notes

- Runtime and editor modules are strictly separated. The editor module depends on runtime, never the other way around.
- Tile mutation history is collected in the runtime module and consumed by the editor tool. It is guarded by a subsystem null-check so it has no effect in shipping builds.
- Tile snapshots in the inspector are copies, not live pointers — this keeps the tool stable across world regeneration.
- Biome smoothing is currently disabled. It was causing stripe artifacts along biome boundaries and will be revisited with a better algorithm.

---

## Planned (Not Yet Started)
- Planting and crop growth system
- Harvest and inventory
- Store and economy loop
- Save/load
- Production-quality world rendering via ProceduralMeshComponent
