#include "wasm4.h"
#include "gfx.h"

enum struct Direction {
    Right,
    Left,
};

enum struct Tile {
    None,
    Brick,
    BrickHard,
    BrickFake,
    Ladder,
    LadderWin,
    Rope,
    Gold,
};

struct Jeff {
    u8 x;
    u8 y;
    Direction direction = Direction::Right;
};

struct Level {
    static const usize GRID_HEIGHT = 18;
    static const usize GRID_WIDTH = 20;

    Tile terrain[GRID_WIDTH][GRID_HEIGHT];
    Jeff player;
};

enum struct State {
    Menu,
    Level,
    LevelTransition,
    Editor,
};

auto draw(const Tile &t, usize row, usize col) {
    i32 x = (i32)(col * 8);
    i32 y = (i32)(row * 8);
    switch (t) {
    case Tile::None:
        break;
    case Tile::LadderWin:
        break;
    case Tile::Brick:
        tiles::brick.draw(x, y, 0);
        break;
    case Tile::BrickFake:
        break;
    case Tile::BrickHard:
        *DRAW_COLORS = 0x13;
        tiles::brick_hard.draw(x, y, 0);
        break;
    case Tile::Ladder:
        *DRAW_COLORS = 0x20;
        tiles::ladder.draw(x, y, 0);
        break;
    case Tile::Rope:
        *DRAW_COLORS = 0x10;
        tiles::rope.draw(x, y, 0);
        break;
    case Tile::Gold:
        *DRAW_COLORS = 0x210;
        tiles::gold.draw(x, y, 0);
        break;
    }
}

void draw(const Jeff jeff) {
    switch (jeff.direction) {
    case Direction::Right:
        break;
    case Direction::Left:
        break;
    }
}

void draw(const Level &l) {
    for (usize row = 0; row < Level::GRID_HEIGHT; row++) {
        for (usize col = 0; col < Level::GRID_WIDTH; col++) {
            draw(l.terrain[row][col], row, col);
        }
    }
    draw(l.player);
}

void update () {
    *DRAW_COLORS = 2;
    text("Hello from C!", 10, 10);

    u8 gamepad = *GAMEPAD1;
    if (gamepad & BUTTON_1) {
        *DRAW_COLORS = 4;
    }

    text("Press X to blink", 16, 90);
}
