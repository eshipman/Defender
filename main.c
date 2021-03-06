/*
 * Defender Game
 * By Evan Shipman and Jessica Spranger
 * program which demonstrates sprites colliding with tiles
 */

#include <stdlib.h>
#include <stdio.h>

#include "music.h"
#include "player.h"

/* include the background image we are using */
#include "DefenderBackground.h"

/* include the tile map we are using */
#include "space.h"
#include "Landscape2.h"

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 160

/* the tile mode flags needed for display control register */
#define MODE0 0x00
#define BG0_ENABLE 0x100
#define BG1_ENABLE 0x200
#define BG2_ENABLE 0x400
#define BG3_ENABLE 0x800

/* flags to set sprite handling in display control register */
#define SPRITE_MAP_2D 0x0
#define SPRITE_MAP_1D 0x40
#define SPRITE_ENABLE 0x1000


/* the control registers for the four tile layers */
volatile unsigned short* bg0_control = (volatile unsigned short*) 0x4000008;
volatile unsigned short* bg1_control = (volatile unsigned short*) 0x400000a;
volatile unsigned short* bg2_control = (volatile unsigned short*) 0x400000c;
volatile unsigned short* bg3_control = (volatile unsigned short*) 0x400000e;
/* palette is always 256 colors */
#define PALETTE_SIZE 256

/* there are 128 sprites on the GBA */
#define NUM_SPRITES 128

/* the display control pointer points to the gba graphics register */
volatile unsigned long* display_control = (volatile unsigned long*) 0x4000000;

/* the memory location which controls sprite attributes */
volatile unsigned short* sprite_attribute_memory = (volatile unsigned short*) 0x7000000;

/* the memory location which stores sprite image data */
volatile unsigned short* sprite_image_memory = (volatile unsigned short*) 0x6010000;

/* the address of the color palettes used for backgrounds and sprites */
volatile unsigned short* bg_palette = (volatile unsigned short*) 0x5000000;
volatile unsigned short* sprite_palette = (volatile unsigned short*) 0x5000200;

/* the button register holds the bits which indicate whether each button has
 * been pressed - this has got to be volatile as well
 */
volatile unsigned short* buttons = (volatile unsigned short*) 0x04000130;

/* scrolling registers for backgrounds */
volatile short* bg0_x_scroll = (unsigned short*) 0x4000010;
volatile short* bg0_y_scroll = (unsigned short*) 0x4000012;
volatile short* bg1_y_scroll = (unsigned short*) 0x4000016;
volatile short* bg1_x_scroll = (unsigned short*) 0x4000014;
volatile short* bg2_x_scroll = (unsigned short*) 0x4000018;
volatile short* bg2_y_scroll = (unsigned short*) 0x400001a;
/* the bit positions indicate each button - the first bit is for A, second for
 * B, and so on, each constant below can be ANDED into the register to get the
 * status of any one button */
#define BUTTON_A (1 << 0)
#define BUTTON_B (1 << 1)
#define BUTTON_SELECT (1 << 2)
#define BUTTON_START (1 << 3)
#define BUTTON_RIGHT (1 << 4)
#define BUTTON_LEFT (1 << 5)
#define BUTTON_UP (1 << 6)
#define BUTTON_DOWN (1 << 7)
#define BUTTON_R (1 << 8)
#define BUTTON_L (1 << 9)

// timer control registers
volatile unsigned short* timer0_data = (volatile unsigned short*) 0x4000100;
volatile unsigned short* timer0_control = (volatile unsigned short*) 0x4000102;

// bit positions for control registers
#define TIMER_FREQ_1 0x0
#define TIMER_FREQ_64 0x2
#define TIMER_FREQ_256 0x3
#define TIMER_FREQ_1024 0x4
#define TIMER_ENABLE 0x80

// fixed clock speed
#define CLOCK 16777216
#define CYCLES_PER_BLANK 280806


/* the scanline counter is a memory cell which is updated to indicate how
 * much of the screen has been drawn */
volatile unsigned short* scanline_counter = (volatile unsigned short*) 0x4000006;

/* wait for the screen to be fully drawn so we can do something during vblank */
void wait_vblank() {
    /* wait until all 160 lines have been updated */
    while (*scanline_counter < 160) { }
}

/* this function checks whether a particular button has been pressed */
unsigned char button_pressed(unsigned short button) {
    /* and the button register with the button constant we want */
    unsigned short pressed = *buttons & button;

    /* if this value is zero, then it's not pressed */
    if (pressed == 0) {
        return 1;
    } else {
        return 0;
    }
}

/* return a pointer to one of the 4 character blocks (0-3) */
volatile unsigned short* char_block(unsigned long block) {
    /* they are each 16K big */
    return (volatile unsigned short*) (0x6000000 + (block * 0x4000));
}

/* return a pointer to one of the 32 screen blocks (0-31) */
volatile unsigned short* screen_block(unsigned long block) {
    /* they are each 2K big */
    return (volatile unsigned short*) (0x6000000 + (block * 0x800));
}

/* flag for turning on DMA */
#define DMA_ENABLE 0x80000000

/* flags for the sizes to transfer, 16 or 32 bits */
#define DMA_16 0x00000000
#define DMA_32 0x04000000

// write to the same destination
#define DMA_DEST_FIXED 0x400000

// repeat DMA interval
#define DMA_REPEAT 0x2000000

// sync the repetitions with timer 0
#define DMA_SYNC_TO_TIMER 0x30000000

/* pointer to the DMA source location */
volatile unsigned int* dma_source = (volatile unsigned int*) 0x40000D4;

/* pointer to the DMA destination location */
volatile unsigned int* dma_destination = (volatile unsigned int*) 0x40000D8;

/* pointer to the DMA count/control */
volatile unsigned int* dma_count = (volatile unsigned int*) 0x40000DC;

// Control, source, and destination for DMA channels one and two
volatile unsigned int* dma1_source = (volatile unsigned int*) 0x40000BC;
volatile unsigned int* dma1_destination = (volatile unsigned int*) 0x40000C0;
volatile unsigned int* dma1_control = (volatile unsigned int*) 0x40000C4;

volatile unsigned int* dma2_source = (volatile unsigned int*) 0x40000C8;
volatile unsigned int* dma2_destination = (volatile unsigned int*) 0x40000CC;
volatile unsigned int* dma2_control = (volatile unsigned int*) 0x40000D0;

volatile unsigned short* interrupt_enable = (unsigned short*) 0x4000208;
volatile unsigned short* interrupt_selection = (unsigned short*) 0x4000200;
volatile unsigned short* interrupt_state = (unsigned short*) 0x4000202;
volatile unsigned int* interrupt_callback = (unsigned int*) 0x3007FFC;
volatile unsigned short* display_interrupts = (unsigned short*) 0x4000004;

#define INTERRUPT_VBLANK 0x1

volatile unsigned short* master_sound = (volatile unsigned short*) 0x4000084;
#define SOUND_MASTER_ENABLE 0x80

volatile unsigned short* sound_control = (volatile unsigned short*) 0x4000082;

// defines for sound control registers
#define SOUND_A_RIGHT_CHANNEL   0x100
#define SOUND_A_LEFT_CHANNEL    0x200
#define SOUND_A_FIFO_RESET      0x800
#define SOUND_B_RIGHT_CHANNEL   0x1000
#define SOUND_B_LEFT_CHANNEL    0x2000
#define SOUND_B_FIFO_RESET      0x8000

volatile unsigned char* fifo_buffer_a = (volatile unsigned char*) 0x40000A0;
volatile unsigned char* fifo_buffer_b = (volatile unsigned char*) 0x40000A4;

unsigned int channel_a_vblanks_remaining = 0;
unsigned int channel_b_vblanks_remaining = 0;

// plays a sound using the number of samples, sample rate, and channel 'A' or 'B'
void play_sound(const signed char* sound, int total_samples, int sample_rate, char channel) {
    /* start by disabling the timer and dma controller (to reset a previous sound) */
    *timer0_control = 0;
    if (channel == 'A') {
        *dma1_control = 0;
    } else if (channel == 'B') {
        *dma2_control = 0;
    }

    /* output to both sides and reset the FIFO */
    if (channel == 'A') {
        *sound_control |= SOUND_A_RIGHT_CHANNEL | SOUND_A_LEFT_CHANNEL | SOUND_A_FIFO_RESET;
    } else if (channel == 'B') {
        *sound_control |= SOUND_B_RIGHT_CHANNEL | SOUND_B_LEFT_CHANNEL | SOUND_B_FIFO_RESET;
    }

    /* enable all sound */
    *master_sound = SOUND_MASTER_ENABLE;

    /* set the dma channel to transfer from the sound array to the sound buffer */
    if (channel == 'A') {
        *dma1_source = (unsigned int) sound;
        *dma1_destination = (unsigned int) fifo_buffer_a;
        *dma1_control = DMA_DEST_FIXED | DMA_REPEAT | DMA_32 | DMA_SYNC_TO_TIMER | DMA_ENABLE;
    } else if (channel == 'B') {
        *dma2_source = (unsigned int) sound;
        *dma2_destination = (unsigned int) fifo_buffer_b;
        *dma2_control = DMA_DEST_FIXED | DMA_REPEAT | DMA_32 | DMA_SYNC_TO_TIMER | DMA_ENABLE;
    }

    /* set the timer so that it increments once each time a sample is due
     * we divide the clock (ticks/second) by the sample rate (samples/second)
     * to get the number of ticks/samples */
    unsigned short ticks_per_sample = CLOCK / sample_rate;

    /* the timers all count up to 65536 and overflow at that point, so we count up to that
     * now the timer will trigger each time we need a sample, and cause DMA to give it one! */
    *timer0_data = 65536 - ticks_per_sample;

    /* determine length of playback in vblanks
     * this is the total number of samples, times the number of clock ticks per sample,
     * divided by the number of machine cycles per vblank (a constant) */
    if (channel == 'A') {
        channel_a_vblanks_remaining = total_samples * ticks_per_sample * (1.0 / CYCLES_PER_BLANK);
    } else if (channel == 'B') {
        channel_b_vblanks_remaining = total_samples * ticks_per_sample * (1.0 / CYCLES_PER_BLANK);
    }

    /* enable the timer */
    *timer0_control = TIMER_ENABLE | TIMER_FREQ_1;
}

// called each vblank to time the sounds right
void on_vblank() {
    //disable interrupts temporarily
    *interrupt_enable = 0;
    unsigned short temp = *interrupt_state;

    // look for vertical refresh
    if ((*interrupt_state & INTERRUPT_VBLANK) == INTERRUPT_VBLANK) {
        // update channel A
        if (channel_a_vblanks_remaining == 0) {
            // restart the sound
            play_sound(music, music_bytes, 16000, 'A');
        } else {
            channel_a_vblanks_remaining--;
        }
        //update channel B
        if (channel_b_vblanks_remaining == 0) {
            //disable sound and DMA transfer on channel B
            *sound_control &= ~(SOUND_B_RIGHT_CHANNEL | SOUND_B_LEFT_CHANNEL | SOUND_B_FIFO_RESET);
            *dma2_control = 0;
        } else {
            channel_b_vblanks_remaining--;
        }
    }
    // restore/enable interrupts
    *interrupt_state = temp;
    *interrupt_enable = 1;
}

/* copy data using DMA */
void memcpy16_dma(unsigned short* dest, unsigned short* source, int amount) {
    *dma_source = (unsigned int) source;
    *dma_destination = (unsigned int) dest;
    *dma_count = amount | DMA_16 | DMA_ENABLE;
}

/* function to setup background 0 for this program */
void setup_background() {

    /* load the palette from the image into palette memory*/
    memcpy16_dma((unsigned short*) bg_palette, (unsigned short*) DefenderBackground_palette, PALETTE_SIZE);

    /* load the image into char block 0 */
    memcpy16_dma((unsigned short*) char_block(0), (unsigned short*) DefenderBackground_data,
            (DefenderBackground_width * DefenderBackground_height) / 2);

    /* set all control the bits in this register */
    *bg0_control = 3 |    /* priority, 0 is highest, 3 is lowest */
        (0 << 2)  |       /* the char block the image data is stored in */
        (0 << 6)  |       /* the mosaic flag */
        (1 << 7)  |       /* color mode, 0 is 16 colors, 1 is 256 colors */
        (16 << 8) |       /* the screen block the tile data is stored in */
        (1 << 13) |       /* wrapping flag */
        (0 << 14);        /* bg size, 0 is 256x256 */

    /*set the control bits for the landscape register*/
    *bg1_control = 2 |
        (0 << 2) |
        (0 << 6) |
        (1 << 7) | 
        (18 << 8) |
        (1 << 13) |
        (0 << 14);

    /* load the tile data into screen block 16 */
    memcpy16_dma((unsigned short*) screen_block(16), (unsigned short*) space, space_width * space_height);

    //Load landscape map into screen black 18
    memcpy16_dma((unsigned short*) screen_block(18), (unsigned short*) Landscape2, Landscape2_width * Landscape2_height);

}
/* just kill time */
void delay(unsigned int amount) {
    for (int i = 0; i < amount * 10; i++);
}

/* a sprite is a moveable image on the screen */
struct Sprite {
    unsigned short attribute0;
    unsigned short attribute1;
    unsigned short attribute2;
    unsigned short attribute3;
};

/* array of all the sprites available on the GBA */
struct Sprite sprites[NUM_SPRITES];
int next_sprite_index = 0;

/* the different sizes of sprites which are possible */
enum SpriteSize {
    SIZE_8_8,
    SIZE_16_16,
    SIZE_32_32,
    SIZE_64_64,
    SIZE_16_8,
    SIZE_32_8,
    SIZE_32_16,
    SIZE_64_32,
    SIZE_8_16,
    SIZE_8_32,
    SIZE_16_32,
    SIZE_32_64
};

/* function to initialize a sprite with its properties, and return a pointer */
struct Sprite* sprite_init(int x, int y, enum SpriteSize size,
        int horizontal_flip, int vertical_flip, int tile_index, int priority) {

    /* grab the next index */
    int index = next_sprite_index++;

    /* setup the bits used for each shape/size possible */
    int size_bits, shape_bits;
    switch (size) {
        case SIZE_8_8:   size_bits = 0; shape_bits = 0; break;
        case SIZE_16_16: size_bits = 1; shape_bits = 0; break;
        case SIZE_32_32: size_bits = 2; shape_bits = 0; break;
        case SIZE_64_64: size_bits = 3; shape_bits = 0; break;
        case SIZE_16_8:  size_bits = 0; shape_bits = 1; break;
        case SIZE_32_8:  size_bits = 1; shape_bits = 1; break;
        case SIZE_32_16: size_bits = 2; shape_bits = 1; break;
        case SIZE_64_32: size_bits = 3; shape_bits = 1; break;
        case SIZE_8_16:  size_bits = 0; shape_bits = 2; break;
        case SIZE_8_32:  size_bits = 1; shape_bits = 2; break;
        case SIZE_16_32: size_bits = 2; shape_bits = 2; break;
        case SIZE_32_64: size_bits = 3; shape_bits = 2; break;
    }

    int h = horizontal_flip ? 1 : 0;
    int v = vertical_flip ? 1 : 0;

    /* set up the first attribute */
    sprites[index].attribute0 = y |             /* y coordinate */
        (0 << 8) |          /* rendering mode */
        (0 << 10) |         /* gfx mode */
        (0 << 12) |         /* mosaic */
        (1 << 13) |         /* color mode, 0:16, 1:256 */
        (shape_bits << 14); /* shape */

    /* set up the second attribute */
    sprites[index].attribute1 = x |             /* x coordinate */
        (0 << 9) |          /* affine flag */
        (h << 12) |         /* horizontal flip flag */
        (v << 13) |         /* vertical flip flag */
        (size_bits << 14);  /* size */

    /* setup the second attribute */
    sprites[index].attribute2 = tile_index |   // tile index */
        (priority << 10) | // priority */
        (0 << 12);         // palette bank (only 16 color)*/

    /* return pointer to this sprite */
    return &sprites[index];
}

/* update all of the spries on the screen */
void sprite_update_all() {
    /* copy them all over */
    memcpy16_dma((unsigned short*) sprite_attribute_memory, (unsigned short*) sprites, NUM_SPRITES * 4);
}

/* setup all sprites */
void sprite_clear() {
    /* clear the index counter */
    next_sprite_index = 0;

    /* move all sprites offscreen to hide them */
    for(int i = 0; i < NUM_SPRITES; i++) {
        sprites[i].attribute0 = SCREEN_HEIGHT;
        sprites[i].attribute1 = SCREEN_WIDTH;
    }
}

/* set a sprite postion */
void sprite_position(struct Sprite* sprite, int x, int y) {
    /* clear out the y coordinate */
    sprite->attribute0 &= 0xff00;

    /* set the new y coordinate */
    sprite->attribute0 |= (y & 0xff);

    /* clear out the x coordinate */
    sprite->attribute1 &= 0xfe00;

    /* set the new x coordinate */
    sprite->attribute1 |= (x & 0x1ff);
}

/* move a sprite in a direction */
void sprite_move(struct Sprite* sprite, int dx, int dy) {
    /* get the current y coordinate */
    int y = sprite->attribute0 & 0xff;

    /* get the current x coordinate */
    int x = sprite->attribute1 & 0x1ff;

    /* move to the new location */
    sprite_position(sprite, x + dx, y + dy);
}

/* change the vertical flip flag */
void sprite_set_vertical_flip(struct Sprite* sprite, int vertical_flip) {
    if (vertical_flip) {
        /* set the bit */
        sprite->attribute1 |= 0x2000;
    } else {
        /* clear the bit */
        sprite->attribute1 &= 0xdfff;
    }
}

/* change the vertical flip flag */
void sprite_set_horizontal_flip(struct Sprite* sprite, int horizontal_flip) {
    if (horizontal_flip) {
        /* set the bit */
        sprite->attribute1 |= 0x1000;
    } else {
        /* clear the bit */
        sprite->attribute1 &= 0xefff;
    }
}

/* change the tile offset of a sprite */
void sprite_set_offset(struct Sprite* sprite, int offset) {
    /* clear the old offset */
    sprite->attribute2 &= 0xfc00;

    /* apply the new one */
    sprite->attribute2 |= (offset & 0x03ff);
}

/* setup the sprite image and palette */
void setup_sprite_image() {
    memcpy16_dma((unsigned short*) sprite_palette, (unsigned short*) player_palette, PALETTE_SIZE);
    memcpy16_dma((unsigned short*) sprite_image_memory, (unsigned short*) player_data, (player_width * player_height) / 2);
}

struct Player {
    struct Sprite* sprite;

    int x, y;
    int frame;
    int animation_delay;
    int counter;
    int move;
    int border;

};

void enemy_init(struct Player* enemy, int x, int y) {
    enemy->x = x << 8;
    enemy->y = y << 8;
    enemy->frame = 4;
    enemy->animation_delay = 2147483647;    //hot fix for flashing sprite
    enemy->counter = 0;                     //need to modify the update player
    enemy->move = 0;                        //function for cleaner fix
    enemy->border = 32;
    enemy->sprite = sprite_init(enemy->x >> 8, enemy->y >> 8, SIZE_16_8, 0, 0, enemy->frame, 0);
}

void player_init(struct Player* player) {
    player->x = 100 << 8;
    player->y = 113 << 8;
    player->frame = 0;
    player->animation_delay = 8;
    player->counter = 0;
    player->move = 0;
    player->border = 32;

    player->sprite = sprite_init(player->x >> 8, player->y >> 8, SIZE_16_8, 0, 0, player->frame, 0);
}

int player_left(struct Player* player) {
    sprite_set_horizontal_flip(player->sprite, 1);
    player->move = 1;

    if ((player->x >> 8) < player->border)
        return 1;
    else {
        player->x -= 256*2;
        //player->x--;
        return 0;
    }
}

int enemy_left(struct Player* enemy) {
    sprite_set_horizontal_flip(enemy->sprite, 1);
    enemy->move = 1;

    if ((enemy->x >> 8) <= 0 - enemy->border)
        return 1;
    else {
        enemy->x -= 256;
        //player->x--;
        return 0;
    }
}

//Assembly function

int is_player_right_border(int x, int border, int screen);

int player_right(struct Player* player) {
        sprite_set_horizontal_flip(player->sprite, 0);
        player->move = 1;
/*
        if ((player->x >> 8) > (SCREEN_WIDTH - 16 - player->border))
                return 1;
        else{
                player->x += 256*2;
                return 0;
        }
*/
	if(is_player_right_border(player->x, player->border, SCREEN_WIDTH))
	{
		return 1;
	}
	else
	{
		player->x += 256*2;
		return 0;
	}
}

int enemy_right(struct Player* enemy) {
    sprite_set_horizontal_flip(enemy->sprite, 0);
    enemy->move = 1;

    if ((enemy->x >> 8) >= SCREEN_WIDTH)
        return 1;
    else{
        enemy->x += 256;
        return 0;
    }
}

int player_up(struct Player* player) {
    player->move = 1;

    if ((player->y >> 8) <= 0)
        return 1;
    else
        player->y -= 256*2;
    return 0;

}

int player_down(struct Player* player) {
    player->move = 1;

    if ((player->y >> 8) >= (SCREEN_HEIGHT * 0.75))
        return 1;
    else
        player->y += 256*2;
    return 0;
}

void player_stop(struct Player* player) {
    player->move = 0;
    player->frame = 0;
    player->counter = 7;
    sprite_set_offset(player->sprite, player->frame);
}

// finds which tile a screen coordinate maps to, taking scroll into account
unsigned short tile_lookup(int x, int y, int xscroll, int yscroll,
        const unsigned short* tilemap, int tilemap_w, int tilemap_h) {

    // adjust for the scroll
    x += xscroll;
    y += yscroll;

    // convert from screen coordinates to tile coordinates
    x >>= 3;
    y >>= 3;

    // account for wraparound
    while (x >= tilemap_w) {
        x -= tilemap_w;
    }
    while (y >= tilemap_h) {
        y -= tilemap_h;
    }
    while (x < 0) {
        x += tilemap_w;
    }
    while (y < 0) {
        y += tilemap_h;
    }

    // lookup this tile from the map
    int index = y * tilemap_w + x;

    // return the tile
    return tilemap[index];
}

void player_update(struct Player* player) //player = 0 if enemy
{

    if(player->move)
    {
        player->counter++;
        if(player->counter >= player->animation_delay) {
            player->frame = player->frame + 16;
            if(player->frame > 16) {
//                player->frame = 0;
            }
            sprite_set_offset(player->sprite, player->frame);
            player->counter = 0;
        }

    }

    sprite_position(player->sprite, player->x >> 8, player->y >> 8);

}

void xorshift(unsigned int*);

/* the main function */
int main() {
    /* we set the mode to mode 0 with bg0 on */
    *display_control = MODE0 | BG0_ENABLE | BG1_ENABLE | SPRITE_ENABLE | SPRITE_MAP_1D;

    // set up interrupt handler
    *interrupt_enable = 0;
    *interrupt_callback = (unsigned int) &on_vblank;
    *interrupt_selection |= INTERRUPT_VBLANK;
    *display_interrupts |= 0x08;
    *interrupt_enable  = 1;
    // clear the sound control
    *sound_control = 0;

    play_sound(music, music_bytes, 16000, 'A');

    /* setup the background 0 */
    setup_background();

    // setup the sprite image data 
    setup_sprite_image();

    // clear all the sprites on screen now 
    sprite_clear();

    struct Player player;
    player_init(&player);

    struct Player enemy, enemy2;
    enemy_init(&enemy, 0, 0);

    unsigned int seed = 0, score = 0;

    int num_enemies = 1;

    int max_enemies = 128;
    struct Player enemies[max_enemies];
    enemies[0] = enemy;
    int difficulty = 1;

    // set initial scroll to 0 
    int xscroll = 0;
    int yscroll = 0;
    unsigned int vblank_counter = 0;
    
    char done = 0;
    while (!done) {
        if (!seed)
            seed = player.x * player.y;
        int last_x = xscroll;
        if (button_pressed(BUTTON_RIGHT)) {
            if (player_right(&player)) {
                xscroll += 2;
            }
        }
        if (button_pressed(BUTTON_LEFT)) {
            if (player_left(&player)) {
                xscroll -= 2;
            }
        }
        if (button_pressed(BUTTON_DOWN))
            player_down(&player);
        if (button_pressed(BUTTON_UP))
            player_up(&player);
        if (!button_pressed(BUTTON_LEFT | BUTTON_RIGHT | BUTTON_UP | BUTTON_DOWN)) {
            player_stop(&player);
        }

        if (last_x == xscroll)
            for (int i = 0; i < num_enemies; i++)
            {
                enemy_right(&enemies[i]);
                enemies[i].x += 256 * num_enemies;
            }
        else if (last_x < xscroll)
            for (int i = 0; i < num_enemies; i++)
                enemies[i].x += 256 * num_enemies - (384);
        else {
            for (int i = 0; i < num_enemies; i++) {
                enemy_right(&enemies[i]);
                enemies[i].x += 256 * num_enemies;
            }
        }
        for (int i = 0; i < num_enemies; i++) {
            if ((enemies[i].x >> 8) == SCREEN_WIDTH) {
                xorshift(&seed);
                int range = (SCREEN_HEIGHT * 0.75 / 8);
                enemies[i].y = ((abs(seed) % range) * 8) << 8;
                enemies[i].x = 0;
            }
        }
        // Add a new enemy every ~10 seconds
        if (vblank_counter != 0 && vblank_counter % 600 == 0 && num_enemies < max_enemies) {
            for (int i = 0; i < difficulty; i++) {
                xorshift(&seed);
                int range = (SCREEN_HEIGHT * 0.75 / 8);
                struct Player new_enemy;
                enemy_init(&new_enemy, 0, (abs(seed) % range) * 8);
                new_enemy.x -= ((abs(seed)) % 32) << 8;
                enemies[num_enemies] = new_enemy;
                num_enemies++;
                score++;
            }
            difficulty *= 2;

        }
        // wait for vblank before scrolling and moving sprites 
        wait_vblank();
        vblank_counter++;
        *bg0_x_scroll = 0.25 * xscroll;
        *bg1_x_scroll = xscroll;
        player_update(&player);
        for (int i = 0; i < num_enemies; i++) {
            if (abs((enemies[i].x >> 8) - (player.x >> 8)) <= 16 && abs((enemies[i].y >> 8) - (player.y >> 8)) <= 8)
                done = 1;
            player_update(&enemies[i]);
        }
        //player_update(get(list, i));
        sprite_update_all();

        // delay some 
        delay(700);
    }
}

/* the game boy advance uses "interrupts" to handle certain situations
 * for now we will ignore these */
void interrupt_ignore() {
    /* do nothing */
}

/* this table specifies which interrupts we handle which way
 * for now, we ignore all of them */
typedef void (*intrp)();
const intrp IntrTable[13] = {
    interrupt_ignore,   /* V Blank interrupt */
    interrupt_ignore,   /* H Blank interrupt */
    interrupt_ignore,   /* V Counter interrupt */
    interrupt_ignore,   /* Timer 0 interrupt */
    interrupt_ignore,   /* Timer 1 interrupt */
    interrupt_ignore,   /* Timer 2 interrupt */
    interrupt_ignore,   /* Timer 3 interrupt */
    interrupt_ignore,   /* Serial communication interrupt */
    interrupt_ignore,   /* DMA 0 interrupt */
    interrupt_ignore,   /* DMA 1 interrupt */
    interrupt_ignore,   /* DMA 2 interrupt */
    interrupt_ignore,   /* DMA 3 interrupt */
    interrupt_ignore,   /* Key interrupt */
};

