# Haunted Village Tycoon

Haunted Village Tycoon is a C++ colony-management and simulation game prototype built with Raylib.
The player acts as a “god-like” observer and builder, indirectly shaping a growing village by placing structures, managing resources, and letting autonomous entities live, work, rest, reproduce, and survive in a procedurally generated world.

The project is currently focused on building a modular simulation foundation: data-driven entities, world generation, AI behaviors, village systems, storage logistics, day/night cycles, seasons, professions, needs, and basic social mechanics.

---

## Current Features

### Procedural World Generation

The world is generated from data-driven biome definitions.

Current world-generation features include:

- finite rectangular maps;
- biome-based terrain generation using climate properties;
- climate biomes driven by temperature and humidity;
- patch biomes such as oasis, volcanoes, and cursed graveyards;
- data-driven flora spawning;
- support for walkable and non-walkable tiles;
- black/void map borders replacing the previous island-style generation.

The system currently uses `.stv` configuration files to control biome properties such as:

- biome type;
- temperature range;
- humidity range;
- base tile;
- flora spawning rules;
- patch instance count;
- patch radius.

---

### Entity Component System

The project uses a custom ECS-style architecture based on a Structure of Arrays layout.

Entities are backed by component arrays such as:

- identity and tags;
- transforms;
- sprites;
- health;
- needs;
- inventory;
- storage;
- rest spots;
- behavior;
- profession;
- construction;
- rooms;
- village membership;
- family links;
- loot;
- equipment.

The `EntityManager` supports entity creation, slot recycling, and centralized component flag reset logic.

---

### Data-Driven Gameplay

Most gameplay definitions are stored in `.stv` files.

Current data-driven assets include:

- entities;
- furniture;
- constructions;
- resources;
- weapons;
- behaviors;
- professions;
- structures;
- biomes;
- environment objects;
- names.

This allows new gameplay entries to be added without hardcoding every object in C++.

---

### Entities and Needs

Entities currently support:

- health;
- hunger;
- fatigue;
- age;
- gender;
- species;
- profession;
- inventory;
- behavior rules;
- action radius.

Humanoid entities such as villagers are currently diurnal and work only during their configured activity window.

Cannibals can be configured as nocturnal entities.

---

### AI Behavior System

Entities use simple behavior rules loaded from data files.

Current supported behaviors include:

- wandering;
- seeking food;
- hunting;
- harvesting;
- building;
- dismantling;
- storing resources;
- resting.

The AI uses spatial queries to avoid scanning the entire entity list when searching for nearby targets, storages, rest spots, or jobs.

Behavior execution currently follows a priority-based model:

1. satisfy survival needs such as hunger;
2. deposit resources when needed;
3. rest when tired or outside working hours;
4. perform productive work during active hours;
5. wander when no higher-priority task exists.

---

### Pathfinding and Navigation

Entities use pathfinding to move toward:

- construction blueprints;
- harvestable resources;
- hunting targets;
- storage containers;
- rest spots.

The current pathfinding system supports moving to adjacent tiles when interacting with blocking objects such as trees, furniture, or constructions.

Door interaction is supported during movement.

---

### Storage and Logistics

Storage containers can hold resources and may define filters.

Examples:

- generic chests can accept all resources;
- gathering baskets can accept only berries and similar gathering resources;
- sawmill benches can accept only wood;
- village cores can act as central storage.

Workers can deposit gathered resources into compatible storages.

Construction can consume resources from:

- the worker inventory;
- the village core;
- nearby accessible storages.

This allows villagers to build from settlement resources rather than requiring all materials to be carried directly by the worker.

---

### Professions and Workplaces

Structures can provide profession slots.

Example structures include:

- lumberjack sawmill;
- gathering tent;
- carpentry workshop;
- cannibal tent;
- forsaken ruin.

The profession system assigns available villagers to workplace slots based on:

- required species;
- minimum age;
- available profession slot;
- current profession status.

The profession system also cleans stale or invalid job assignments when rooms are recalculated, workers die, or workplace slots disappear.

---

### Room and Structure Detection

The room system detects enclosed areas using walls and doors.

Detected rooms are matched against structure definitions based on:

- minimum area;
- maximum area;
- required furniture;
- job slots.

When a room matches a structure definition, it can provide workplace slots for professions.

---

### Village System

The game now supports the concept of a village.

At world initialization, the game creates:

- a `Village Core`;
- starting resources;
- several adult villagers;
- village membership links for villagers.

The village core acts as:

- a rally point;
- central storage;
- population reference;
- future anchor for ownership and family logic.

The village system currently tracks:

- total population;
- adult population;
- child population;
- population limit.

Seasonal reproduction is also supported in a first simple version.

A child can be spawned when:

- the village has available population capacity;
- at least one adult male and one adult female exist;
- enough food nutrition is available in the village core.

Children are spawned as villagers with age `0` and linked to their village and parents.

---

### Family System Foundation

A lightweight family component exists.

It currently supports:

- partner ID;
- parent A;
- parent B;
- children list.

This is currently used during reproduction to connect children to parents.

Future work will extend this into family ownership, private rooms, beds, and household-level behavior.

---

### Time, Seasons, and Day/Night Logic

The game has an in-game clock and season system.

Current time features:

- day counter;
- hour counter;
- season index;
- day-in-season counter;
- one season lasts five in-game days;
- day/night lighting overlay;
- seasonal phase displayed in the UI.

Time also drives passive needs such as hunger and fatigue.

---

### Rest and Fatigue System

Entities can accumulate fatigue over time.

When fatigue becomes high, or when the entity is outside its work schedule, it can search for a rest spot.

Rest spots include:

- straw beds;
- small beds;
- double beds.

Each rest spot has a capacity:

- straw bed: 1 occupant;
- small bed: 1 occupant;
- double bed: 2 occupants.

Rest spots already include placeholder fields for future ownership logic:

- private/public state;
- owner village ID;
- owner family ID.

This will later support private beds, family houses, and household ownership.

---

### Inspection and Debug UI

The debug inspection system displays rich information when hovering entities.

Current inspection data includes:

- identity;
- species;
- gender;
- age;
- position;
- health;
- hunger;
- fatigue;
- stats;
- current task;
- profession;
- village membership;
- village population;
- food nutrition;
- family links;
- inventory;
- storage capacity;
- rest spot capacity;
- equipment;
- doors;
- harvestable yields.

The inspection tooltip uses structured lines, progress bars, headers, and key-value fields.

---

### Rendering and Optimization

Rendering is partially optimized using spatial indexing.

Current optimization features include:

- visible tile rendering only;
- entity spatial grid;
- viewport-based entity rendering;
- spatial lookup for inspection;
- AI target search using spatial radius;
- configurable action radius per entity.

This avoids many full-map scans and prepares the project for future chunk-based simulation.

---

## Current Architecture Overview

The project is organized around several major systems:

```text
Application
 ├── Registries
 │   ├── EntityRegistry
 │   ├── FurnitureRegistry
 │   ├── ResourceRegistry
 │   ├── BehaviorRegistry
 │   ├── ProfessionRegistry
 │   ├── StructureRegistry
 │   ├── BiomeRegistry
 │   └── TileRegistry
 │
 ├── Core World State
 │   ├── WorldMap
 │   ├── EntityManager
 │   ├── EntitySpatialGrid
 │   ├── Chronicle
 │   └── SettlementMetrics
 │
 ├── Simulation Systems
 │   ├── TimeSystem
 │   ├── VillageSystem
 │   ├── EventSystem
 │   ├── RoomSystem
 │   ├── ProfessionSystem
 │   ├── AISystem
 │   └── BuildPlacementSystem
 │
 ├── Rendering and Debug
 │   ├── WorldRenderSystem
 │   ├── RenderSystem
 │   ├── LightingSystem
 │   ├── ChronicleRenderSystem
 │   └── InspectionSystem
 │
 └── UI / Input
     ├── UIManager
     ├── InputManager
     └── GameCamera
````

***

## Controls

Current controls include:

* opening the build menu;
* selecting entities, furniture, or constructions;
* placing selected objects;
* deleting or marking objects for dismantling;
* inspecting entities with CTRL + hover;
* toggling names display;
* moving and zooming the camera.

Exact key bindings are defined in the input system.

***

## Current Gameplay Loop

The current gameplay loop is:

1. A finite world is generated from data-driven biomes.
2. A village core is spawned near the center of the map.
3. Starting villagers are spawned around the village core.
4. The player places structures, furniture, and constructions.
5. Villagers autonomously:
   * eat when hungry;
   * work during active hours;
   * harvest resources;
   * build blueprints;
   * deposit resources into compatible storage;
   * rest when tired;
   * reproduce seasonally if village conditions allow.
6. Structures create rooms and workplaces.
7. Workplaces assign professions to villagers.
8. The village grows through food, labor, storage, and reproduction.

***

## Known Limitations

The project is still an early prototype.

Current limitations include:

* no save/load system yet;
* no true chunk streaming yet;
* environment objects are still spawned as ECS entities;
* reproduction is simple and not yet tied to homes or beds;
* private ownership is prepared but not implemented;
* fatigue and rest are functional but not yet linked to morale or comfort;
* rooms are recalculated and recreated instead of having persistent room IDs;
* families are lightweight and not yet used for household behavior;
* construction logistics are functional but still simple;
* no advanced job queue or resource request system yet.

***

## Planned Features

Planned or likely future work includes:

* chunk-based simulation and world streaming;
* persistent room identity;
* family-owned homes and beds;
* private property rules;
* more advanced reproduction conditions;
* fatigue, comfort, and morale;
* sleeping animations or state visualization;
* resource request and logistics system;
* village growth levels;
* procedural structures such as cannibal camps, ruins, dungeons, and monster spawns;
* more professions;
* more species and behavior profiles;
* save/load support;
* improved UI panels for village, population, storage, and jobs.

***

## Technical Notes

The codebase is written in modern C++ and uses Raylib for rendering and input.

The current architecture favors:

* data-driven gameplay;
* explicit ECS-style component arrays;
* modular systems;
* simple and readable simulation rules;
* performance-aware spatial queries;
* incremental refactoring over large rewrites.

The goal is to grow the project into a scalable simulation/colony management prototype while keeping systems understandable and easy to extend.



