// Internal
#include "movement.h"
#include "figure.h"

// Advances the figure during an attack animation
void figure_movement_advance_attack(figure *f)
{
    if (f->progress_on_tile <= 5) {
        f->progress_on_tile++;
        advance_mov(f);
    }
}
// Animates Tower Sentry movement (sub-coordinates within a tile boundary only)
void figure_movement_tower_sentry(figure *f, int num_ticks)
{
    while (num_ticks-- > 0) {
        f->progress_on_tile++;
        // If we haven't reached the tile edge, continue animating sub-tile movement.
        if (f->progress_on_tile < MOV_PER_TILE) {
            advance_mov(f);
        } else {
            // Reached the end of the tile walk (e.g., end of the tower wall).
            // Clamp the progress to prevent overflow and stop movement.
            f->progress_on_tile = MOV_PER_TILE;
            // Note: No call to move_to_next_tile(f); the figure remains on its grid tile.
        }
    }
}