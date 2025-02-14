#include "engine/wasm4.h"
#include "engine/gfx.h"

enum struct Direction {
    Right,
    Left,
};

struct Tile {
    // 3bit
    enum Tag {
        None,
        Brick,
        BrickHard,
        BrickFake,
        Ladder,
        LadderWin,
        Rope,
        Gold,
    } tag;

    Tile(Tag tag) : tag(tag) {}
    Tile() : tag(None) {}

    operator Tag() const {
        return tag;
    }

    void draw(usize row, usize col, w4::draw::Framebuffer &fb) const {
        i32 x = (i32)(col * 8);
        i32 y = (i32)(row * 8);

        switch (tag) {
        case None:
            break;
        case Brick:
            // TODO: breakage
        case BrickFake:
            fb.blit(gfx::tiles::brick, {x, y}, {});
            break;
        case BrickHard:
            fb.blit(gfx::tiles::brick, {x, y}, {});
            fb.hline({x, y}, 8, w4::draw::DrawIndex::First);
            break;
        case LadderWin:
            // NOTE: if !win break;
            break;
        case Ladder: {
            auto color = w4::draw::DrawIndex::First;
            fb.vline({x + 1, y}, 8, color);
            fb.vline({x + 6, y}, 8, color);
            fb.hline({x + 1, y + 2}, 6, color);
            fb.hline({x + 1, y + 5}, 6, color);
        } break;
        case Rope:
            fb.hline({x, y + 1}, 8, w4::draw::DrawIndex::First);
            break;
        case Gold:
            fb.blit(gfx::tiles::gold, {x, y}, {});
            break;
        }
    }
};

struct Jeff {
    u8 x;
    u8 y;
    Direction direction = Direction::Right;

    enum class State {
        Normal,
        Climbing,
        Roping,
    };

    void draw(w4::draw::Framebuffer &fb, State state) const {
        w4::draw::BlitTransform flags;

        if (direction == Direction::Left) {
            flags |= w4::draw::BlitTransform::FLIP_X;
        }

        (void)state;
        fb.blit(gfx::tiles::jeff[0], {x, y}, flags);
    }
};

struct Level {
    static constexpr usize GRID_HEIGHT = 18;
    static constexpr usize GRID_WIDTH = 20;

    // 360 * 3 = 1080bit = 135 bytes
    Tile terrain[GRID_HEIGHT][GRID_WIDTH];
    Jeff player;

    void draw(w4::draw::Framebuffer &fb) const {
        fb.rect({0, 0}, {160, 160}, w4::draw::DrawIndex::Fourth, w4::draw::DrawIndex::Fourth);
        fb.hline({0, 18 * 8}, 160, w4::draw::DrawIndex::First);
        // terrain
        usize y = 0;
        for (const auto &row : terrain) {
            usize x = 0;
            for (const auto &tile : row) {
                tile.draw((usize)y, (usize)x, fb);
                x++;
            }
            y++;
        }
        // player
        player.draw(fb, Jeff::State::Normal);
    }
};

enum struct State {
    Menu,
    Level,
    LevelTransition,
    Editor,
};

#pragma GCC diagnostic ignored "-Wc99-designator"
struct Loderunner {
    w4::control::Gamepad gamepad;
    w4::draw::Framebuffer fb;

    Level level;

    static auto start(w4::rt::Resources resources) -> Loderunner {
        return {
            .gamepad = resources.controls.gamepads[0],
            .fb = resources.framebuffer,
            .level = {
                .terrain = { [17] = { [19] = Tile::Brick }, [0] = { [0] = Tile::Brick } },
                .player = { .x = 50, .y = 50, .direction = {} },
            }
        };
    }
    void update() {
        level.draw(fb);
    }
};

main(Loderunner)
