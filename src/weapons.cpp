/* See weapons.h. */

#include "weapons.h"

u8  bullet_active[N_BULLETS]     = {0, 0};
u8  bullet_frame[N_BULLETS]      = {0, 0};
int bullet_x[N_BULLETS]          = {0, 0};
int bullet_y[N_BULLETS]          = {0, 0};

int bullet_blast_ticks[N_BULLETS] = {0, 0};
int bullet_blast_x[N_BULLETS]     = {0, 0};
int bullet_blast_y[N_BULLETS]     = {0, 0};

namespace {

/* The blast sprite is byte-aligned, so an impact snaps to its column.
 * orig: LA5A3_0 / LAFFC_31 both do `AND $F8` on the x before converting
 * the bullet into a blast. */
void start_blast(int i, int x, int y) {
    bullet_active[i] = 0;
    bullet_blast_x[i] = x & ~7;
    bullet_blast_y[i] = y;
    bullet_blast_ticks[i] = BULLET_BLAST_FRAMES * BULLET_BLAST_TICKS_PER_FRAME;
}

/* An alien is a target only while it is on screen and not already
 * exploding — sprite set $0A is the blast it turns into. */
bool enemy_is_targetable(const Object &e) {
    return (e.sprite_set & 0x7F) != 0
        && (e.sprite_set & 0x80) == 0
        && (e.sprite_set & 0x7F) != 0x0A;
}

bool overlaps_enemy(int i, const Object &e) {
    return bullet_x[i] + BULLET_BODY_W > e.x_coord
        && bullet_x[i] < e.x_coord + e.w_body_px
        && bullet_y[i] + BULLET_BODY_H > e.y_coord
        && bullet_y[i] < e.y_coord + e.h_body_px;
}

}  /* namespace */

bool any_bullet_active() {
    for (int i = 0; i < N_BULLETS; i++) if (bullet_active[i]) return true;
    return false;
}

bool any_bullet_blast() {
    for (int i = 0; i < N_BULLETS; i++) if (bullet_blast_ticks[i]) return true;
    return false;
}

BulletHit bullet_advance(int i, const Object &enemy, const BrickField &field) {
    BulletHit miss = { BulletHit::NOTHING, -1, -1 };
    if (!bullet_active[i]) return miss;

    bullet_frame[i]++;
    bullet_y[i] -= BULLET_SPEED;
    if (bullet_y[i] < 0) {
        bullet_active[i] = 0;             /* left the screen — no blast */
        BulletHit gone = { BulletHit::LEFT_SCREEN, -1, -1 };
        return gone;
    }

    if (enemy_is_targetable(enemy) && overlaps_enemy(i, enemy)) {
        start_blast(i, bullet_x[i], bullet_y[i]);
        BulletHit hit = { BulletHit::ENEMY, -1, -1 };
        return hit;
    }

    /* A bullet is a point against the grid, not a rectangle: the original
     * tests its tip only. */
    if (bullet_y[i] >= FIELD_Y0 && bullet_y[i] < FIELD_Y_END
        && bullet_x[i] >= FIELD_X0
        && bullet_x[i] < FIELD_X0 + FIELD_COLS * BRICK_W_PX) {
        const int col = (bullet_x[i] - FIELD_X0) / BRICK_W_PX;
        const int row = (bullet_y[i] - FIELD_Y0) / BRICK_H_PX;
        if (field.standing(row, col)) {
            start_blast(i, bullet_x[i], bullet_y[i]);
            BulletHit hit = { BulletHit::BRICK, row, col };
            return hit;
        }
    }
    return miss;
}

void bullet_blasts_tick() {
    for (int i = 0; i < N_BULLETS; i++)
        if (bullet_blast_ticks[i]) bullet_blast_ticks[i]--;
}

void bullets_clear() {
    for (int i = 0; i < N_BULLETS; i++) {
        bullet_active[i] = 0;
        bullet_frame[i]  = 0;
        bullet_blast_ticks[i] = 0;
    }
}
