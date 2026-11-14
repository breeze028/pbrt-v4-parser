// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <cstdint>
#include <span>
#include <vector>
namespace pbrt4::web {
struct ImageData {std::uint32_t width=0,height=0;bool alpha=false;std::vector<std::uint8_t> rgba;};
struct Mip {std::uint32_t width=0,height=0,offset=0,bytes=0;};
struct ImageMips {std::vector<Mip> levels;std::vector<std::uint8_t> bytes;};
ImageData decode_tga(std::span<const std::uint8_t>);
ImageMips make_mips(ImageData,bool srgb,bool scalar);
}
