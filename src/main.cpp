#include "engine/wasm4.h"
#include "engine/gfx.h"
#include <array>
#include <bit>
#include <cstring>
#include <optional>
#include <span>
#include <utility>

enum struct Direction {
    Right,
    Left,
    Down,
    Up,
};

const char *DEBUG = "Normal";

struct Tile {
    // 3bit
    enum Tag : u8 {
        None,
        Brick,
        BrickHard,
        BrickFake,
        Ladder,
        LadderWin,
        Rope,
        Gold,
    } tag;
    // if it's a brick that's broken this acts as a timer
    u16 brick_state = 0;

    Tile(Tag tag) : tag(tag) {}
    Tile() : tag(None) {}

    operator Tag() const {
        return tag;
    }

    void draw(i32 row, i32 col, w4::draw::Framebuffer &fb) const {
        i32 x = col * 8;
        i32 y = row * 8;

        switch (tag) {
        case None:
        case LadderWin:
            break;
        case Brick:
            if (brick_state) {
                if (brick_state > 295 || brick_state < 10) {
                    fb.blit(gfx::tiles::brick, {x, y}, {});
                    fb.rect({x, y}, {8, 4}, w4::draw::DrawIndex::Fourth, w4::draw::DrawIndex::Transparent);
                }
                break;
            }
            // TODO: breakage
        case BrickFake:
            fb.blit(gfx::tiles::brick, {x, y}, {});
            break;
        case BrickHard:
            fb.blit(gfx::tiles::brick, {x, y}, {});
            fb.hline({x, y}, 8, w4::draw::DrawIndex::First);
            break;
        case Ladder: {
            auto color = w4::draw::DrawIndex::Second;
            fb.vline({x + 1, y}, 8, color);
            fb.vline({x + 6, y}, 8, color);
            fb.hline({x + 1, y + 1}, 6, color);
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

    auto prevents_fall() const -> bool {
        switch (tag) {
        case Brick:
            if (brick_state) return false;
        case BrickHard:
        case Ladder:
            return true;
        case LadderWin:
        case BrickFake:
        case Rope:
        case Gold:
        case None:
            return false;
        }
    }

    auto solid() const -> bool {
        switch (tag) {
        case None:
        case BrickFake:
        case Ladder:
        case LadderWin:
        case Rope:
        case Gold:
            return false;
        case Brick:
            if (brick_state) return false;
        case BrickHard:
            return true;
        }
    }
};

namespace compress {
// TODO: unpacking
#define PACKED __attribute__((packed))
struct Chunk {
    bool is_rle : 1;
    u16 data : 15;

    static auto non_rle(std::array<Tile, 5> batch) -> Chunk {
        struct Batch {
            u8 a : 3;
            u8 b : 3;
            u8 c : 3;
            u8 d : 3;
            u8 e : 3;
        } PACKED data;
        static_assert(sizeof(Batch) == 2);

        data.a = batch[0];
        data.b = batch[1];
        data.c = batch[2];
        data.d = batch[3];
        data.e = batch[4];

        return {
            .is_rle = false,
            .data = std::bit_cast<u16>(data),
        };
    }

    // Input is checked to only contain compatible tiles
    static auto rle(std::array<std::pair<u8, Tile>, 3> batch) -> Chunk {
        enum Tag : u8 {
            None = 0b00,
            Brick = 0b01,
            BrickHard = 0b10,
            Rope = 0b11,
        };
        struct Subchunk {
            u8 length : 3;
            u8 tag : 2;
        } PACKED;

        Subchunk subchunks[3];
        for (usize i = 0; i < 3; i++) {
            subchunks[i].length = batch[i].first;
            switch (batch[i].second) {
            case Tile::None:
                subchunks[i].tag = Tag::None;
                break;
            case Tile::Brick:
                subchunks[i].tag = Tag::Brick;
                break;
            case Tile::BrickHard:
                subchunks[i].tag = Tag::BrickHard;
                break;
            case Tile::Rope:
                subchunks[i].tag = Tag::Rope;
                break;
            default:
                std::unreachable();
            }
        }

        return {
            .is_rle = true,
            .data = static_cast<u16>(
                std::bit_cast<u8>(subchunks[0])
              | std::bit_cast<u8>(subchunks[1]) << 5
              | std::bit_cast<u8>(subchunks[2]) << 10
            ),
        };
    }
} PACKED;
static_assert(sizeof(Chunk) == 2);
#undef PACKED
}


struct Jeff {
    u8 x;
    u8 y;
    Direction direction = Direction::Right;

    enum class State {
        Normal,
        Climbing,
        Roping,
        Falling,
    };

    void draw(w4::draw::Framebuffer &fb, State state) const {
        w4::draw::BlitTransform flags;

        usize index;
        switch(state) {
        case State::Falling:
        case State::Normal:
            index = x / 8 % 2 ? 1 : 0;
            if (direction == Direction::Left) {
                flags |= w4::draw::BlitTransform::FLIP_X;
            }
            break;
        case State::Roping:
            index = x / 8 % 2 ? 3 : 2;
            if (direction == Direction::Left) {
                flags |= w4::draw::BlitTransform::FLIP_X;
            }
            break;
        case State::Climbing:
            index = 4;
            if (y / 8 % 2 == 1) {
                flags |= w4::draw::BlitTransform::FLIP_X;
            }
            break;
        }
        this->blit(
            gfx::tiles::jeff[index],
            w4::draw::DrawIndex::First,
            flags,
            fb
        );
    }

    void blit(
        w4::draw::Blit auto sprite,
        w4::draw::DrawIndex color,
        w4::draw::BlitTransform flags,
        w4::draw::Framebuffer &fb
    ) const {
        sprite.blit_with_colors(
            {x, y},
            {w4::draw::DrawIndex::Transparent, color},
            flags,
            fb
        );
    }

    void snapX() {
        x = getX() * 8;
    }
    void snapY() {
        y = getY() * 8;
    }

    auto alignedX() const -> bool {
        return x % 8 == 0;
    }
    auto alignedY() const -> bool {
        return y % 8 == 0;
    }

    /// Ignores collision. Check in the caller
    void move(Direction direction) {
        switch (direction) {
        case Direction::Right:
            snapY();
            x++;
            break;
        case Direction::Left:
            snapY();
            x--;
            break;
        case Direction::Down:
            snapX();
            y++;
            break;
        case Direction::Up:
            snapX();
            y--;
            break;
        }
    }

    auto getX() const -> u8 {
        u8 x = this->x / 8;

        u8 dx = this->x % 8;
        if (0 <= dx && dx <= 3) return x;
        if (4 <= dx && dx <= 7) return x + 1;
        std::unreachable();
    }

    auto getY() const -> u8 {
        u8 y = this->y / 8;

        u8 dy = this->y % 8;
        if (0 <= dy && dy <= 3) return y;
        if (4 <= dy && dy <= 7) return y + 1;
        std::unreachable();
    }
};

struct Enemy : Jeff {
    u8 spawn_x;
    u8 spawn_y;
    void draw(w4::draw::Framebuffer &fb, State state) const {
        w4::draw::BlitTransform flags;

        if (direction == Direction::Left) {
            flags |= w4::draw::BlitTransform::FLIP_X;
        }

        (void)state;
        gfx::tiles::jeff[0].blit_with_colors(
            {x, y},
            {w4::draw::DrawIndex::Transparent, w4::draw::DrawIndex::Third},
            flags,
            fb
        );
    }

    template<typename ...Ts>
    Enemy(Ts ...ts) : Jeff(ts...) {}
};

struct Level {
    static constexpr usize GRID_HEIGHT = 18;
    static constexpr usize GRID_WIDTH = 20;

    // 360 * 3 = 1080bit = 135 bytes
    std::array<Tile, GRID_HEIGHT * GRID_WIDTH> terrain;
    Jeff player;
    std::array<std::optional<Enemy>, 4> enemies;

    bool win = false;

    enum class State {
        Play,
        Win,
        Lose
    };

    // returns win
    auto update(w4::control::Gamepad::State input) -> State {
        // Emplace wincon if there's no gold left and stop checking
        for (auto &tile : terrain) {
            if (tile == Tile::Brick && tile.brick_state != 0) tile.brick_state--;
        }
        if (!win) {
            win = true;
            for (const auto &tile : terrain) {
                if (tile == Tile::Gold) {
                    win = false;
                }
            }
            if (win) {
                for (auto &tile : terrain) {
                    if (tile == Tile::LadderWin) {
                        tile = Tile::Ladder;
                    }
                }
            }
        }

        // update player
        usize player_pos = player.getX() + player.getY() * GRID_WIDTH;
        auto &current_tile = terrain[player_pos];
        if (current_tile == Tile::Brick && !current_tile.brick_state) {
            return State::Lose;
        }
        if (current_tile == Tile::Gold) {
            current_tile = Tile::None;
        }
        if (jeff_state(player) == Jeff::State::Falling) {
            player.move(Direction::Down);
        } else {
            // TODO: prioritize latest input instead of an elseif cascade
            if (input.dpad.left) {
                player.direction = Direction::Left;
                if (!collides(player, Direction::Left)) {
                    player.move(Direction::Left);
                }
            } else if (input.dpad.right) {
                player.direction = Direction::Right;
                if (!collides(player, Direction::Right)) {
                    player.move(Direction::Right);
                }
            } else if (input.dpad.up) {
                if (jeff_state(player) == Jeff::State::Climbing
                && !collides(player, Direction::Up)) {
                    player.move(Direction::Up);
                }
            } else if (input.dpad.down) {
                if (((jeff_state(player) == Jeff::State::Climbing
                   || jeff_state(player) == Jeff::State::Roping)
                && !collides(player, Direction::Down))
                || terrain[player_pos + GRID_WIDTH] == Tile::Ladder) {
                    player.move(Direction::Down);
                }
            }
        }

        do {
            usize break_pos;
            if (input.buttons[1]) {
                break_pos = player_pos + GRID_WIDTH - 1;
            } else if (input.buttons[0]) {
                break_pos = player_pos + GRID_WIDTH + 1;
            } else {
                break;
            }
            if (terrain[break_pos] == Tile::Brick
                && terrain[break_pos].brick_state == 0
                && (terrain[break_pos - GRID_WIDTH] == Tile::None
                    || terrain[break_pos - GRID_WIDTH == Tile::Gold])) {
                terrain[break_pos].brick_state = 300;
            }
        } while(0);

        // wincon
        if (player.alignedY() && player.y == 0) {
            return State::Win;
        } else {
            return State::Play;
        }

        for (auto &enemy : enemies) {
            if (enemy) {
                // TODO: enemy logic
            }
        }
    }

    void draw(w4::draw::Framebuffer &fb) const {
        fb.hline({0, 18 * 8}, 160, w4::draw::DrawIndex::First);
        // terrain
        for (usize i = 0; i < terrain.size(); i++) {
            terrain[i].draw(i32(i / GRID_WIDTH), i % GRID_WIDTH, fb);
        }

        // player
        player.draw(fb, jeff_state(player));

        fb.text(DEBUG, {3, 160 - 13}, w4::draw::DrawIndex::First, w4::draw::DrawIndex::Fourth);

        // enemies
        for(const auto &enemy : enemies) {
            if (enemy) {
                enemy->draw(fb, jeff_state(*enemy));
            }
        }
    }

    auto collides(const Jeff &jeff, Direction direction) -> bool {
        i32 diff;
        bool aligned;
        switch (direction) {
        case Direction::Right:
            if (jeff.x == (GRID_WIDTH - 1) * 8) return true;
            diff = 1;
            aligned = jeff.alignedX();
            break;
        case Direction::Left:
            if (jeff.x == 0) return true;
            diff = -1;
            aligned = jeff.alignedX();
            break;
        case Direction::Down:
            if (jeff.y == (GRID_HEIGHT - 1) * 8) return true;
            diff = GRID_WIDTH;
            aligned = jeff.alignedY();
            break;
        case Direction::Up:
            if (jeff.y == 0) return true;
            diff = -i32(GRID_WIDTH);
            aligned = jeff.alignedY();
            break;
        }
        i32 jeff_pos = jeff.getX() + jeff.getY() * GRID_WIDTH;
        // if (jeff_pos + diff < 0 || jeff_pos + diff > i32(GRID_WIDTH * GRID_HEIGHT)) {
        //     return true;
        // }
        return terrain[usize(jeff_pos + diff)].solid() && aligned;
    }

    auto jeff_state(const Jeff &jeff) const -> Jeff::State {
        usize jeff_pos = jeff.getX() + jeff.getY() * GRID_WIDTH;
        if (terrain[jeff_pos] == Tile::Ladder
        || (terrain[jeff_pos + GRID_WIDTH] == Tile::Ladder && !jeff.alignedY())) {
            DEBUG = "Climbing";
            return Jeff::State::Climbing;
        }
        if (terrain[jeff_pos] == Tile::Rope && player.alignedY()) {
            DEBUG = "Roping";
            return Jeff::State::Roping;
        }
        if (!((terrain[jeff_pos + GRID_WIDTH].prevents_fall() || player.y == GRID_HEIGHT * 8 - 8) && jeff.alignedY())) {
            DEBUG = "Falling";
            return Jeff::State::Falling;
        }

        DEBUG = "Normal";
        return Jeff::State::Normal;
    }

    // 3 bytes. One of the less annoying ways to bitpack imo
    struct Batch {
        u8 a : 3;
        u8 b : 3;
        u8 c : 3;
        u8 d : 3;
        u8 e : 3;
        u8 f : 3;
        u8 g : 3;
        u8 h : 3;
    } __attribute__((packed));

    auto dump() const -> std::array<u8, 137> {
        std::array<u8, 137> out;

        std::array<Batch, 45> terrain_container;

        for (usize i = 0; i < terrain_container.size(); i++) {
            terrain_container[i] = {
                terrain[i * 8],
                terrain[i * 8 + 1],
                terrain[i * 8 + 2],
                terrain[i * 8 + 3],
                terrain[i * 8 + 4],
                terrain[i * 8 + 5],
                terrain[i * 8 + 6],
                terrain[i * 8 + 7],
            };
        }
        std::memcpy(out.data(), terrain_container.data(), 135);
        out[135] = player.x;
        out[136] = player.y;

        return out;
    }

    static auto from_bytes(std::array<u8, 137> bytes) -> Level {
        Level self;
        std::array<Batch, 45> terrain_container;
        std::memcpy(terrain_container.data(), bytes.data(), 135);
        for (usize i = 0; i < terrain_container.size(); i++) {
            self.terrain[i * 8] = Tile::Tag(terrain_container[i].a);
            self.terrain[i * 8 + 1] = Tile::Tag(terrain_container[i].b);
            self.terrain[i * 8 + 2] = Tile::Tag(terrain_container[i].c);
            self.terrain[i * 8 + 3] = Tile::Tag(terrain_container[i].d);
            self.terrain[i * 8 + 4] = Tile::Tag(terrain_container[i].e);
            self.terrain[i * 8 + 5] = Tile::Tag(terrain_container[i].f);
            self.terrain[i * 8 + 6] = Tile::Tag(terrain_container[i].g);
            self.terrain[i * 8 + 7] = Tile::Tag(terrain_container[i].h);
        }
        self.player.x = bytes[135];
        self.player.y = bytes[136];

        return self;
    }
};

struct Loderunner {
    w4::control::Gamepad::State prev_gamepad;
    w4::control::Gamepad gamepad;
    w4::control::Mouse mouse;
    w4::draw::Framebuffer fb;

    // TODO: replace with a scene ref
    Level level;

    static auto start(w4::rt::Resources resources) -> Loderunner {
        std::array<Tile, Level::GRID_WIDTH * Level::GRID_HEIGHT> terrain{
            Tile::None, Tile::None, Tile::Brick,
        };

        terrain[5 * 20] = Tile::BrickHard;
        terrain[5 * 20 + 1] = Tile::BrickFake;
        terrain[5 * 20 + 2] = Tile::Brick;
        terrain[5 * 20 + 3] = Tile::Brick;

        for (usize i = 5; i < 17; i++) {
            terrain[i * 20 + 4] = Tile::Ladder;
        }

        for (usize i = 0; i < 5; i++) {
            terrain[i * 20 + 4] = Tile::LadderWin;
        }

        for (auto &i : std::span{ &terrain[5 * 20 + 5], 15 }) {
            i = Tile::Rope;
        }

        for(auto &i : std::span{ terrain.end() - 20, 20 }) {
            i = Tile::Brick;
        }

        for(auto &i : std::span{ terrain.end() - 27, 4 }) {
            i = Tile::Gold;
        }

        *(terrain.end() - 21) = Tile::Brick;
        // *(terrain.end() - 20 - 20) = Tile::Brick;

        auto gamepad = resources.controls.gamepads[0];

        std::array<u8, 137> level_bytes;
        w4::sys::diskr(level_bytes.data(), 137);

        return {
            .prev_gamepad = gamepad.state(),
            .gamepad = gamepad,
            .mouse = resources.controls.mouse,
            .fb = resources.framebuffer,
            // .level = Level::from_bytes(level_bytes)
            .level = {
                .terrain = terrain,
                .player = { .x = 3 * 8, .y = 0 },
                .enemies = {
                    Enemy { (u8)0, (u8)0 }
                },
            }
        };
    }
    void update() {
        // fill background. Maybe replace the palette instead idk
        std::memset(fb.span().data(), 0xFF, 6400);

        switch (level.update(gamepad.state().clean())) {
        case Level::State::Play:
           level.draw(fb);
           break;
        case Level::State::Win:
           // TODO: load next level
           level = {};
           break;
        case Level::State::Lose:
           // TODO: reload level
           level = {};
           break;
        }

        prev_gamepad = gamepad.state();
    }
};

main(Loderunner)
