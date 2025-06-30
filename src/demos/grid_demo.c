#include "core/components.h"
#include "core/display_config.h"
#include "core/ecs.h"
#include "core/input.h"
#include "core/log.h"
#include "core/renderer.h"
#include "core/window.h"
#include "game/map_system.h"
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Map configuration - using centralized constants
#define GRID_DEMO_MAP_WIDTH MAP_DEFAULT_WIDTH // 5x5 for better testing
#define GRID_DEMO_MAP_HEIGHT MAP_DEFAULT_HEIGHT

// Grid demo state
typedef struct {
  Map map;
  Entity player_entity;
  MapCoord player_pos;
  MapType current_map_type;
  bool show_debug;
} GridDemoState;

// Terrain colors for rendering
static Color terrain_colors[TERRAIN_COUNT] = {
    [TERRAIN_PLAINS] = {0.5f, 0.8f, 0.3f, 1.0f},   // Light green
    [TERRAIN_FOREST] = {0.2f, 0.6f, 0.2f, 1.0f},   // Dark green
    [TERRAIN_WATER] = {0.2f, 0.4f, 0.8f, 1.0f},    // Blue
    [TERRAIN_MOUNTAIN] = {0.6f, 0.5f, 0.4f, 1.0f}, // Brown
    [TERRAIN_DESERT] = {0.9f, 0.8f, 0.4f, 1.0f},   // Yellow
    [TERRAIN_SWAMP] = {0.4f, 0.5f, 0.3f, 1.0f},    // Dark yellow-green
    [TERRAIN_ROAD] = {0.7f, 0.7f, 0.7f, 1.0f},     // Light gray
    [TERRAIN_BRIDGE] = {0.8f, 0.6f, 0.4f, 1.0f},   // Light brown
    [TERRAIN_VOID] = {0.1f, 0.1f, 0.1f, 1.0f}      // Dark gray
};

// Generate a simple test map with different terrains
void generate_test_map(Map *map) {
  // Set up some terrain variety
  for (int y = 0; y < map->height; y++) {
    for (int x = 0; x < map->width; x++) {
      MapCoord coord;
      if (map->type == MAP_GRID) {
        coord = grid_coord(x, y);
      } else {
        // For hex maps, convert offset coordinates to cube coordinates
        coord = hex_offset_to_cube((MapCoord){x, y, 0});
      }

      TerrainType terrain = TERRAIN_PLAINS; // Default

      // Create some patterns for small maps
      if (map->width <= 5 && map->height <= 5) {
        // For small maps, don't use void borders - make them more interesting
        if (x == map->width / 2 && y == map->height / 2) {
          terrain = TERRAIN_ROAD; // Center road
        } else if ((x + y) % 2 == 0) {
          terrain = TERRAIN_FOREST;
        } else if (x == 0 || y == 0) {
          terrain = TERRAIN_WATER;
        } else if (x == map->width - 1 || y == map->height - 1) {
          terrain = TERRAIN_MOUNTAIN;
        } else {
          terrain = TERRAIN_DESERT;
        }
      } else {
        // Original pattern for larger maps
        if (x == 0 || x == map->width - 1 || y == 0 || y == map->height - 1) {
          terrain = TERRAIN_VOID; // Border
        } else if (x == map->width / 2) {
          terrain = TERRAIN_ROAD; // Vertical road
        } else if (y == map->height / 2) {
          terrain = TERRAIN_ROAD; // Horizontal road
        } else if ((x + y) % 5 == 0) {
          terrain = TERRAIN_FOREST;
        } else if ((x * 3 + y * 2) % 7 == 0) {
          terrain = TERRAIN_MOUNTAIN;
        } else if (x > map->width * 0.7f && y < map->height * 0.3f) {
          terrain = TERRAIN_WATER;
        }
      }

      map_set_terrain(map, coord, terrain);
    }
  }
}

// Render a single map tile
void render_map_tile(const Map *map, Renderer *renderer, MapCoord coord) {
  const MapNode *node = map_get_node_const(map, coord);
  if (!node)
    return;

  Vec3 world_pos = map_coord_to_world(map, coord);
  Color tile_color = terrain_colors[node->terrain];

  // Tile rendering is working correctly

  // Create a temporary renderable for the tile
  Renderable tile_renderable = {0};

  if (map->type == MAP_GRID) {
    tile_renderable.shape = SHAPE_QUAD;
    // Make tiles slightly smaller to create visible borders
    float border_size = 2.0f;
    tile_renderable.data.quad.width = map->tile_size - border_size;
    tile_renderable.data.quad.height = map->tile_size - border_size;
  } else {
    // For hex, use a circle approximation for now
    tile_renderable.shape = SHAPE_CIRCLE;
    tile_renderable.data.circle.radius = (map->tile_size - 2.0f) * 0.5f;
  }

  tile_renderable.color = tile_color;
  tile_renderable.visible = true;

  // Temporarily set transform for rendering
  Transform tile_transform = {0};
  tile_transform.position = world_pos;
  tile_transform.scale = vec3_one();

  // Render based on shape
  if (tile_renderable.shape == SHAPE_QUAD) {
    renderer_render_quad(renderer, &tile_transform, &tile_renderable);
  } else {
    renderer_render_circle(renderer, &tile_transform, &tile_renderable);
  }
}

// Render the entire map
void render_map(const Map *map, Renderer *renderer) {
  static int debug_render_count = 0;
  int tiles_rendered = 0;

  for (int y = 0; y < map->height; y++) {
    for (int x = 0; x < map->width; x++) {
      MapCoord coord;
      if (map->type == MAP_GRID) {
        coord = grid_coord(x, y);
      } else {
        // For hex maps, convert offset coordinates to cube coordinates
        coord = hex_offset_to_cube((MapCoord){x, y, 0});
      }

      if (map_coord_valid(map, coord)) {
        render_map_tile(map, renderer, coord);
        tiles_rendered++;
      }
    }
  }

  if (debug_render_count < 3) {
    printf("Rendered %d/%d tiles in frame %d\\n", tiles_rendered,
           map->width * map->height, debug_render_count);
  }
  debug_render_count++;
}

// Render the player
void render_player(GridDemoState *state, Renderer *renderer) {
  Vec3 player_world_pos = map_coord_to_world(&state->map, state->player_pos);

  Transform player_transform = {0};
  player_transform.position = player_world_pos;
  player_transform.scale = vec3_one();

  Renderable player_renderable = {0};
  player_renderable.shape = SHAPE_CIRCLE;
  player_renderable.data.circle.radius = state->map.tile_size * 0.3f;
  player_renderable.color = (Color){1.0f, 0.0f, 0.0f, 1.0f}; // Red
  player_renderable.visible = true;

  renderer_render_circle(renderer, &player_transform, &player_renderable);
}

// Handle player movement input
void handle_player_input(GridDemoState *state, InputState *input) {
  MapCoord new_pos = state->player_pos;
  bool moved = false;

  if (state->map.type == MAP_GRID) {
    // Grid movement (4 directions)
    if (input_key_pressed(input, GLFW_KEY_W) ||
        input_key_pressed(input, GLFW_KEY_UP)) {
      new_pos.y += 1;
      moved = true;
    }
    if (input_key_pressed(input, GLFW_KEY_S) ||
        input_key_pressed(input, GLFW_KEY_DOWN)) {
      new_pos.y -= 1;
      moved = true;
    }
    if (input_key_pressed(input, GLFW_KEY_A) ||
        input_key_pressed(input, GLFW_KEY_LEFT)) {
      new_pos.x -= 1;
      moved = true;
    }
    if (input_key_pressed(input, GLFW_KEY_D) ||
        input_key_pressed(input, GLFW_KEY_RIGHT)) {
      new_pos.x += 1;
      moved = true;
    }
  } else {
    // Hex movement (6 directions using cube coordinates)
    // Using clear directional mapping: W/S for N/S, Q/E for NW/NE, A/D for
    // SW/SE
    if (input_key_pressed(input, GLFW_KEY_S)) {
      // North: decrease r (up in visual grid)
      new_pos = hex_coord(new_pos.x, new_pos.y - 1);
      moved = true;
    }
    if (input_key_pressed(input, GLFW_KEY_W)) {
      // South: increase r (down in visual grid)
      new_pos = hex_coord(new_pos.x, new_pos.y + 1);
      moved = true;
    }
    if (input_key_pressed(input, GLFW_KEY_Q)) {
      // Northwest: decrease q
      new_pos = hex_coord(new_pos.x - 1, new_pos.y);
      moved = true;
    }
    if (input_key_pressed(input, GLFW_KEY_E)) {
      // Northeast: increase q
      new_pos = hex_coord(new_pos.x + 1, new_pos.y);
      moved = true;
    }
    if (input_key_pressed(input, GLFW_KEY_A)) {
      // Southwest: decrease q, increase r
      new_pos = hex_coord(new_pos.x - 1, new_pos.y + 1);
      moved = true;
    }
    if (input_key_pressed(input, GLFW_KEY_D)) {
      // Southeast: increase q, decrease r
      new_pos = hex_coord(new_pos.x + 1, new_pos.y - 1);
      moved = true;
    }
  }

  // Validate movement
  if (moved && map_coord_valid(&state->map, new_pos)) {
    if (map_can_move_to(&state->map, state->player_pos, new_pos)) {
      // Update occupancy
      map_set_occupant(&state->map, state->player_pos, 0); // Clear old position
      map_set_occupant(&state->map, new_pos,
                       state->player_entity); // Set new position
      state->player_pos = new_pos;

      printf("Player moved to (%d, %d) - Terrain: %s (Cost: %d)\\n", new_pos.x,
             new_pos.y,
             terrain_type_to_string(
                 map_get_node_const(&state->map, new_pos)->terrain),
             map_get_movement_cost(&state->map, new_pos));
    } else {
      printf("Cannot move to (%d, %d) - blocked!\\n", new_pos.x, new_pos.y);
    }
  }
}

// Switch between map modes
void switch_map_mode(GridDemoState *state) {
  // Clean up current map
  map_cleanup(&state->map);

  // Switch mode and determine appropriate tile size
  float tile_size;
  bool is_hex;

  if (state->current_map_type == MAP_GRID) {
    state->current_map_type = MAP_HEX_POINTY;
    tile_size = TILE_SIZE_HEX;
    is_hex = true;
    printf("Switched to hexagonal map mode\\n");
  } else {
    state->current_map_type = MAP_GRID;
    tile_size = TILE_SIZE_GRID;
    is_hex = false;
    printf("Switched to grid map mode\\n");
  }

  // Calculate appropriate tile size for current window
  tile_size = calculate_tile_size_for_window(
      GRID_DEMO_MAP_WIDTH, GRID_DEMO_MAP_HEIGHT, WINDOW_DEFAULT_WIDTH,
      WINDOW_DEFAULT_HEIGHT, is_hex);

  // Initialize new map
  map_init(&state->map, state->current_map_type, GRID_DEMO_MAP_WIDTH,
           GRID_DEMO_MAP_HEIGHT, tile_size);

  // Calculate actual map bounds for centering
  float map_width_world =
      calculate_map_world_width(GRID_DEMO_MAP_WIDTH, tile_size, is_hex);
  float map_height_world =
      calculate_map_world_height(GRID_DEMO_MAP_HEIGHT, tile_size, is_hex);

  // Center the map in the world coordinate system
  state->map.origin =
      (Vec3){-map_width_world * 0.5f +
                 MAP_PADDING_WORLD, // Center horizontally with padding
             -map_height_world * 0.5f +
                 MAP_PADDING_WORLD, // Center vertically with padding
             0.0f};

  generate_test_map(&state->map);

  // Reset player position using proper coordinate system
  if (state->current_map_type == MAP_GRID) {
    state->player_pos =
        grid_coord(GRID_DEMO_MAP_WIDTH / 2, GRID_DEMO_MAP_HEIGHT / 2);
  } else {
    // For hex, convert offset coordinates to cube coordinates
    MapCoord offset_center = {GRID_DEMO_MAP_WIDTH / 2, GRID_DEMO_MAP_HEIGHT / 2,
                              0};
    state->player_pos = hex_offset_to_cube(offset_center);
  }

  map_set_occupant(&state->map, state->player_pos, state->player_entity);

  // Debug output
  printf("Map bounds: width=%.1f height=%.1f tile_size=%.1f\\n",
         map_width_world, map_height_world, tile_size);
  printf("Map fits in bounds: %s\\n",
         map_fits_in_world_bounds(GRID_DEMO_MAP_WIDTH, GRID_DEMO_MAP_HEIGHT,
                                  tile_size, is_hex)
             ? "YES"
             : "NO");
}

int main(void) {
  // Initialize logging system
  LogConfig log_cfg = {0};
  log_cfg.min_level = LOG_INFO;
  log_init(&log_cfg);

  LOG_INFO("Starting Grid Demo");

  if (!window_init()) {
    LOG_ERROR("Window init failed");
    return -1;
  }

  Window *window = window_create(WINDOW_DEFAULT_WIDTH, WINDOW_DEFAULT_HEIGHT,
                                 "Grid Demo - Multi-Modal Map System");
  if (!window) {
    LOG_ERROR("Window creation failed");
    window_terminate();
    return -1;
  }

  ECS ecs = {0};
  ecs_init(&ecs);

  Renderer renderer = {0};
  if (!renderer_init(&renderer, &ecs)) {
    LOG_ERROR("Renderer initialization failed");
    window_destroy(window);
    window_terminate();
    return -1;
  }

  InputState input = {0};
  input_init(&input, window->handle);

  // Initialize demo state
  GridDemoState state = {0};
  state.current_map_type = MAP_GRID;
  state.show_debug = false;

  // Calculate appropriate tile size for initial grid mode
  float initial_tile_size = calculate_tile_size_for_window(
      GRID_DEMO_MAP_WIDTH, GRID_DEMO_MAP_HEIGHT, WINDOW_DEFAULT_WIDTH,
      WINDOW_DEFAULT_HEIGHT, false);

  // Initialize map
  map_init(&state.map, state.current_map_type, GRID_DEMO_MAP_WIDTH,
           GRID_DEMO_MAP_HEIGHT, initial_tile_size);

  // Calculate actual map bounds for centering
  float map_width_world =
      calculate_map_world_width(GRID_DEMO_MAP_WIDTH, initial_tile_size, false);
  float map_height_world = calculate_map_world_height(GRID_DEMO_MAP_HEIGHT,
                                                      initial_tile_size, false);

  // Center the map in the world coordinate system
  state.map.origin =
      (Vec3){-map_width_world * 0.5f +
                 MAP_PADDING_WORLD, // Center horizontally with padding
             -map_height_world * 0.5f +
                 MAP_PADDING_WORLD, // Center vertically with padding
             0.0f};

  generate_test_map(&state.map);

  // Debug: Check if map generation worked
  printf("Map validation: width=%d height=%d\\n", state.map.width,
         state.map.height);
  for (int y = 0; y < state.map.height && y < 3; y++) {
    for (int x = 0; x < state.map.width && x < 3; x++) {
      MapCoord coord = grid_coord(x, y);
      const MapNode *node = map_get_node_const(&state.map, coord);
      if (node) {
        printf("Tile (%d,%d): terrain=%s\\n", x, y,
               terrain_type_to_string(node->terrain));
      } else {
        printf("Tile (%d,%d): NULL NODE!\\n", x, y);
      }
    }
  }

  // Debug: Print map bounds and first tile
  printf("Map bounds: origin=(%.1f,%.1f) size=(%.1fx%.1f) tile_size=%.1f\\n",
         state.map.origin.x, state.map.origin.y, map_width_world,
         map_height_world, initial_tile_size);
  printf("Map fits in bounds: %s\\n",
         map_fits_in_world_bounds(GRID_DEMO_MAP_WIDTH, GRID_DEMO_MAP_HEIGHT,
                                  initial_tile_size, false)
             ? "YES"
             : "NO");

  // Debug: Check first tile position
  MapCoord first_tile = grid_coord(0, 0);
  Vec3 first_tile_world = map_coord_to_world(&state.map, first_tile);
  printf("First tile (0,0): world=(%.1f,%.1f) screen=(%.3f,%.3f)\\n",
         first_tile_world.x, first_tile_world.y, first_tile_world.x / 120.0f,
         first_tile_world.y / 120.0f);

  // Debug: Check center tile
  MapCoord center_tile =
      grid_coord(GRID_DEMO_MAP_WIDTH / 2, GRID_DEMO_MAP_HEIGHT / 2);
  Vec3 center_tile_world = map_coord_to_world(&state.map, center_tile);
  printf("Center tile (%d,%d): world=(%.1f,%.1f) screen=(%.3f,%.3f)\\n",
         center_tile.x, center_tile.y, center_tile_world.x, center_tile_world.y,
         center_tile_world.x / 120.0f, center_tile_world.y / 120.0f);

  // Create player entity with proper initial position
  state.player_entity = ecs_create_entity(&ecs);
  state.player_pos =
      grid_coord(GRID_DEMO_MAP_WIDTH / 2, GRID_DEMO_MAP_HEIGHT / 2);
  map_set_occupant(&state.map, state.player_pos, state.player_entity);

  LOG_INFO("Grid Demo Initialized!");
  printf("Controls:\\n");
  printf("  Grid Mode: WASD or Arrow Keys to move (8 directions)\\n");
  printf("  Hex Mode: W/S=N/S, Q/E=NW/NE, A/D=SW/SE (6 directions)\\n");
  printf("  TAB: Switch between Grid and Hex modes\\n");
  printf("  F1: Toggle debug info\\n");
  printf("  ESC: Exit\\n");

  while (!window_should_close(window)) {
    window_poll_events();

    // Handle input BEFORE input_update() clears the pressed states
    if (input_key_pressed(&input, GLFW_KEY_ESCAPE)) {
      glfwSetWindowShouldClose(window->handle, GLFW_TRUE);
    }

    if (input_key_pressed(&input, GLFW_KEY_TAB)) {
      switch_map_mode(&state);
    }

    if (input_key_pressed(&input, GLFW_KEY_F1)) {
      state.show_debug = !state.show_debug;
      printf("Debug info: %s\\n", state.show_debug ? "ON" : "OFF");
    }

    handle_player_input(&state, &input);

    // Clear input state for next frame
    input_update(&input);

    // Render
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    renderer_begin_frame(&renderer);

    // Render map
    render_map(&state.map, &renderer);

    // Render player
    render_player(&state, &renderer);

    // Debug info
    if (state.show_debug) {
      printf("Player: (%d,%d) | Mode: %s | Terrain: %s\\r", state.player_pos.x,
             state.player_pos.y,
             (state.current_map_type == MAP_GRID) ? "Grid" : "Hex",
             terrain_type_to_string(
                 map_get_node_const(&state.map, state.player_pos)->terrain));
      fflush(stdout);
    }

    renderer_end_frame(&renderer);
    window_swap_buffers(window);
  }

  // Cleanup
  map_cleanup(&state.map);
  input_cleanup(&input);
  renderer_cleanup(&renderer);
  ecs_cleanup(&ecs);
  window_destroy(window);
  window_terminate();

  LOG_INFO("Grid Demo shutdown complete");
  return 0;
}
