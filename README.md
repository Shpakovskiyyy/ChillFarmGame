# ChillFarmGame

A chill, point-and-click farm game built in Unreal Engine 5.7 with C++. The core idea is simple: plant crops, wait for them to grow, harvest, sell, buy better seeds. The world is procedurally generated at runtime so each session feels slightly different.

> **Work in Progress.** The project is in active early development. Core systems are being built and iterated on. Expect rough edges, missing features, and evolving architecture.

---

## Current Branch: TerrainGenerationMVP

All active development is happening on the `TerrainGenerationMVP` branch. This branch focuses on building the foundation — procedural world generation, the tile system, and the editor tooling needed to debug and inspect the generated world. Gameplay systems (planting, harvesting, economy) come after this foundation is solid.

---

## What Exists So Far

### Procedural World Generation
`ATileWorldGenerator` builds the tile map from a seed by running a list of generation passes in order: terrain noise (height, moisture, water, elevation levels), biomes, tile types, terraces, shores, smoothing and fertility. Passes work on a plain `FTileGrid` without touching the world, so the same seed always gives the same map and the pipeline can be tested without a level.

The map is drawn by `ATileVisualGrid` with Hierarchical Instanced Static Mesh components, one per autotile piece per chunk. Raised terraces get cliff sides, stretched from any mesh set on the visual data. The drawn map is not saved with the level; it is regenerated from the seed when the level opens.

### Tile System
Each tile is six bytes of values: type, biome, elevation level, fertility, moisture and flags. `UTileWorldSubsystem` owns the live map and is the only way to change it at runtime (`MutateTile`); visuals and tools react to the changes it broadcasts.

### Tilling and Watering
Right-click a tile within reach of the character: grass and dirt become farmland, farmland gets watered. The rules are pure functions (`TileFarming`), and every change goes through `MutateTile` as a gameplay change.

### Tile Inspector (Editor Tool)
A custom editor tool for inspecting and debugging the generated world.

- Click any tile in the editor viewport to select it; the tile gets a yellow outline (custom SceneProxy)
- The docked Tile Inspector panel shows the tile's data and its full history: every change, which step made it and why, with the changed fields shown as before -> after

History is recorded by the editor module only; the game build contains none of it.

See [Docs/Architecture.md](Docs/Architecture.md) for the full design, the reasoning behind it and the roadmap.

---

## Project Structure

```
ChillFarmGame/
├── Docs/Architecture.md            # World architecture, decisions, roadmap
├── Source/
│   ├── ChillFarmGame/              # Runtime module
│   │   ├── WorldGeneration/        # Tile data, grid, layout, mutations, world subsystem, visuals, biome data
│   │   │   ├── Generation/         # Generation context and passes
│   │   │   └── TileMutationHistory/  # Change source enum
│   │   ├── Tests/                  # Automation specs (ChillFarmGame.TileWorld)
│   │   ├── Crops/                  # Crop actors (early stage)
│   │   ├── Player/                 # Player controller and character
│   │   └── Persistent/             # Game mode
│   └── ChillFarmGameEditor/        # Editor-only module
│       └── TileMutationHistoryEditor/
│           ├── TileDebugRender/    # Highlight component, scene proxy, subsystem
│           ├── TileDebugSelection/ # Viewport click notifier
│           └── Widgets/            # Tile Inspector window
```

---

## Planned (Not Yet Started)
- Planting and crop growth system
- Harvest and inventory
- Store and economy loop
- Save/load
- Ramps between elevation levels, rivers and roads (cliffs have a first version)
- Farm site selection based on generated factors
