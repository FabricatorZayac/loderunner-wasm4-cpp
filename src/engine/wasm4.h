#pragma once

#include <bitset>
#include <cstdint>
#include <array>
#include <cstddef>
#include <cstring>
#include <optional>
#include <span>
#include <stdint.h>

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;

using i16 = int16_t;
using i32 = int32_t;

using usize = size_t;

namespace w4 {
namespace sys {
#include "engine/wasm4-sys.h"
}

namespace rt {
    struct Resources;
}
}

namespace w4 {

namespace control {
    struct Mouse {
        struct Buttons {
            bool left;
            bool right;
            bool middle;
        };
        struct State {
            i16 x;
            i16 y;
            Buttons buttons;
        };

        State state() const {
            return {
                .x = *MOUSE_X,
                .y = *MOUSE_Y,
                .buttons = {
                    .left = (*MOUSE_BUTTONS & MOUSE_LEFT) != 0,
                    .right = (*MOUSE_BUTTONS & MOUSE_RIGHT) != 0,
                    .middle = (*MOUSE_BUTTONS & MOUSE_MIDDLE) != 0,
                }
            };
        }
    };

    struct Gamepad {
        struct Directions {
            bool up;
            bool down;
            bool left;
            bool right;
        };
        struct State {
            std::array<bool, 2> buttons;
            Directions dpad;

            State clean() const {
                return {
                    buttons,
                    {
                        .up = this->dpad.up && !this->dpad.down,
                        .down = this->dpad.down && !this->dpad.up,
                        .left = this->dpad.left && !this->dpad.right,
                        .right = this->dpad.right && !this->dpad.left,
                    }
                };
            }
        };

        State state() const {
            return {
                .buttons = {
                    (*gamepad & BUTTON_1) != 0,
                    (*gamepad & BUTTON_2) != 0,
                },
                .dpad = {
                    .up = (*gamepad & BUTTON_UP) != 0,
                    .down = (*gamepad & BUTTON_DOWN) != 0,
                    .left = (*gamepad & BUTTON_LEFT) != 0,
                    .right = (*gamepad & BUTTON_RIGHT) != 0,
                }
            };
        }

        friend struct rt::Resources;
    private:
        Gamepad(const u8 *gamepad) : gamepad(gamepad) {}
        const u8 *gamepad;
    };

    struct Controls {
        Mouse mouse;
        std::array<Gamepad, 4> gamepads;
    };
}

namespace draw {
    struct Framebuffer;
    struct DrawIndices;

    using BlitFlags = std::bitset<4>;

    struct BlitTransform : BlitFlags {
        enum Tag {
            FLIP_X = BLIT_FLIP_X,
            FLIP_Y = BLIT_FLIP_Y,
            ROTATE = BLIT_ROTATE,
        };
    };

    struct BitsPerPixel : BlitFlags {
        enum Tag {
            One = BLIT_1BPP,
            Two = BLIT_2BPP,
        };

        BitsPerPixel(Tag tag) : BlitFlags(tag) {}
    };

    template<typename T>
    concept Blit = requires(
        T self,
        std::array<i32, 2> start,
        BlitTransform transform,
        Framebuffer &fb,
        DrawIndices colors
    ) {
        self.blit(start, transform, fb);
        self.blit_with_colors(start, colors, transform, fb);
    };

    enum class DrawIndex : u16 {
        Transparent,
        First,
        Second,
        Third,
        Fourth,
    };
    struct DrawIndices {
        static auto from_array(std::array<DrawIndex, 4> array) -> DrawIndices {
            return u16((u16)array[0]
                     | (u16)array[1] << 4
                     | (u16)array[2] << 8
                     | (u16)array[3] << 16);
        }

        operator u16() const {
            return data;
        }

        template<typename ...Ts>
        DrawIndices(Ts... ts) {
            *this = from_array({ts...});
        }
    private:
        DrawIndices(u16 data) : data(data) {}
        u16 data;
    };

    using Color = u32;

    template<usize N>
    struct Sprite {
        void blit_with_colors(
            std::array<i32, 2> start,
            DrawIndices colors,
            BlitTransform transform,
            Framebuffer &framebuffer
        ) const {
            BlitFlags flags = bpp | transform;

            *DRAW_COLORS = colors;
            (void)framebuffer;
            // framebuffer.set_indices(indices);

            sys::blit(bytes.data(), start[0], start[1], shape[0], shape[1], flags.to_ulong());
        }

        void blit(
            std::array<i32, 2> start,
            BlitTransform transform,
            Framebuffer &framebuffer
        ) const {
            blit_with_colors(start, indices, transform, framebuffer);
        }

        static auto from_byte_array(
            std::array<u8, N> bytes,
            std::array<u32, 2> shape,
            BitsPerPixel bpp,
            DrawIndices draw_colors
        ) -> std::optional<Sprite> {
            usize resoltuion = shape[0] * shape[1];
            usize capacity = N * (1 << (3 - bpp.to_ulong())); // in bits

            if (resoltuion <= capacity) {
                return Sprite(bytes, shape, bpp, draw_colors);
            }
            return {};
        }

        Sprite(
            std::array<u8, N> bytes,
            std::array<u32, 2> shape,
            BitsPerPixel bpp,
            DrawIndices draw_colors
        ) : shape(shape), bpp(bpp), indices(draw_colors), bytes(bytes) {}

    protected:
        std::array<u32, 2> shape;
        BitsPerPixel bpp;
        DrawIndices indices;
        std::array<u8, N> bytes;
    };

    // TODO: SpriteView

    struct Framebuffer {
        constexpr static auto WIDTH = 160;
        constexpr static auto HEIGHT = 160;
        constexpr static auto BYTE_LENGTH = WIDTH * HEIGHT / 4; // 2bpp

        auto span() -> std::span<u8, BYTE_LENGTH> {
            return std::span<u8, BYTE_LENGTH>(FRAMEBUFFER, BYTE_LENGTH);
        }
        void set_palette(std::array<Color, 4> palette) {
            std::memcpy(PALETTE, palette.data(), palette.size() * sizeof(Color));
        }
        void set_indices(DrawIndices colors) {
            *DRAW_COLORS = colors;
        }

        void line(std::array<i32, 2> start, std::array<i32, 2> end, DrawIndex color) {
            set_indices(DrawIndices::from_array({color}));
            sys::line(start[0], start[1], end[0], end[1]);
        }
        void hline(std::array<i32, 2> start, u32 len, DrawIndex color) {
            set_indices(DrawIndices::from_array({color}));
            sys::hline(start[0], start[1], len);
        }
        void vline(std::array<i32, 2> start, u32 len, DrawIndex color) {
            set_indices(DrawIndices::from_array({color}));
            sys::vline(start[0], start[1], len);
        }
        void rect(
            std::array<i32, 2> start,
            std::array<u32, 2> shape,
            DrawIndex fill,
            DrawIndex outline
        ) {
            set_indices(DrawIndices::from_array({fill, outline}));
            sys::rect(start[0], start[1], shape[0], shape[1]);
        }
        void oval(
            std::array<i32, 2> start,
            std::array<u32, 2> shape,
            DrawIndex fill,
            DrawIndex outline
        ) {
            set_indices(DrawIndices({fill, outline}));
            sys::oval(start[0], start[1], shape[0], shape[1]);
        }
        void text(const char *text, std::array<i32, 2> pos, DrawIndex color, DrawIndex bg) {
            set_indices(DrawIndices({color, bg}));
            sys::text(text, pos[0], pos[1]);
        }
        void blit(const Blit auto &sprite, std::array<i32, 2> start, BlitTransform transform) {
            sprite.blit(start, transform, *this);
        }
    };
}

namespace rt {
    struct Resources {
        draw::Framebuffer framebuffer;
        control::Controls controls;

        friend void w4::sys::start();
    private:
        Resources() :
            framebuffer(),
            controls{
                .mouse = control::Mouse(),
                .gamepads = {
                    control::Gamepad(GAMEPAD1),
                    control::Gamepad(GAMEPAD2),
                    control::Gamepad(GAMEPAD3),
                    control::Gamepad(GAMEPAD4),
                }
            } {}
    };
}
}

#define main(Rt)                                     \
static u8 RUNTIME[sizeof(Rt)];                       \
void w4::sys::start() {                              \
    *(Rt *)RUNTIME = Rt::start(w4::rt::Resources()); \
}                                                    \
void w4::sys::update() {                             \
    ((Rt *)RUNTIME)->update();                       \
}

#undef SCREEN_SIZE
#undef PALETTE
#undef DRAW_COLORS
#undef GAMEPAD1
#undef GAMEPAD2
#undef GAMEPAD3
#undef GAMEPAD4
#undef MOUSE_X
#undef MOUSE_Y
#undef MOUSE_BUTTONS
#undef SYSTEM_FLAGS
#undef NETPLAY
#undef FRAMEBUFFER

#undef BUTTON_1
#undef BUTTON_2
#undef BUTTON_LEFT
#undef BUTTON_RIGHT
#undef BUTTON_UP
#undef BUTTON_DOWN

#undef MOUSE_LEFT
#undef MOUSE_RIGHT
#undef MOUSE_MIDDLE

#undef SYSTEM_PRESERVE_FRAMEBUFFER
#undef SYSTEM_HIDE_GAMEPAD_OVERLAY

#undef TONE_PULSE1
#undef TONE_PULSE2
#undef TONE_TRIANGLE
#undef TONE_NOISE
#undef TONE_MODE1
#undef TONE_MODE2
#undef TONE_MODE3
#undef TONE_MODE4
#undef TONE_PAN_LEFT
#undef TONE_PAN_RIGHT
#undef TONE_NOTE_MODE
