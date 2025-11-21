// Internal
#include "movement.h"
// External
#include "core/calc.h"
#include "map/terrain.h"

// Handles variable speed accumulator for snake followers
void figure_movement_follow_ticks_with_percentage(figure *f, int num_ticks, int tick_percentage)
{
    // 1. Handle Sub-Tick Accumulator (Variable Speed)
    // Adds the percentage variance. If it crosses 100, we gain/lose a whole tick.
    int progress = f->progress_to_next_movement + tick_percentage;

    if (progress >= 100) {
        progress -= 100;
        num_ticks++;
    } else if (progress <= -100) {
        progress += 100;
        num_ticks--;
    }
    // Cast back to the storage type (likely char or short)
    f->progress_to_next_movement = (char) progress;

    // 2. Get Leader with Safety Check
    const figure *leader = figure_get(f->leading_figure_id);
    if (!leader) {
        // If the leader is gone, the follower should probably stop or vanish.
        // For now, we just return to prevent a crash.
        return;
    }

    // 3. Handle "Spawn Ghosting"
    // If at the source coordinate, remain a ghost (prevents clipping at spawn structures)
    if (f->x == f->source_x && f->y == f->source_y) {
        f->is_ghost = 1;
    }

    // 4. Highway Rubber-Banding
    // If the leader is speeding on a highway, the follower matches speed to stay connected.
    if (map_terrain_is(map_grid_offset(leader->x, leader->y), TERRAIN_HIGHWAY)) {
        num_ticks *= 2;
    }

    // 5. Movement Loop
    while (num_ticks-- > 0) {
        f->progress_on_tile++;

        // Mid-tile movement
        if (f->progress_on_tile < TICKS_PER_TILE) {
            advance_tick(f);
        }
        // End of tile reached
        else {
            // SNAKE LOGIC: Target the tile the leader JUST left
            direction_type next_dir = calc_general_direction(f->x, f->y,
                leader->previous_tile_x, leader->previous_tile_y);

            // Stop if the direction is invalid (e.g., Status Flag or No Move)
            if (next_dir > DIR_MAX_MOVEMENT) {
                f->progress_on_tile = TICKS_PER_TILE; // Clamp progress
                break;
            }

            // Apply Movement
            f->direction = next_dir;
            f->previous_tile_direction = f->direction;
            f->progress_on_tile = 0;

            move_to_next_tile(f);

            // Advance the sub-coordinate tick immediately after entering the new tile
            advance_tick(f);
        }
    }
}

// --- WRAPPERS --
/* Followers without speed variation */
void figure_movement_follow_ticks(figure *f, int num_ticks)
{
    figure_movement_follow_ticks_with_percentage(f, num_ticks, 0); // 0 represents 0% speed variation.
}