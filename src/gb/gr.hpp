#pragma once

#include "types.h"

#include "mm.hpp"

class GR
{
  static const reg_t WIDTH  = 160;
  static const reg_t HEIGHT = 144;

public:
  typedef std::array<reg_t, WIDTH*HEIGHT> screen_t;

  GR(MM& mm)
    : mm_(mm)
    , lx_(0)
  {}

  void power_on()
  {
    lx_ = 0;
    screen_  = screen_t();
  }

  screen_t screen() const
  {
    return screen_;
  }

  reg_t width() const
  {
    return WIDTH;
  }

  reg_t height() const
  {
    return HEIGHT;
  }

  reg_t lcdc()    const { return mm_.read(0xFF40); }
  reg_t scy()     const { return mm_.read(0xFF42); }
  reg_t scx()     const { return mm_.read(0xFF43); }
  reg_t wy()      const { return mm_.read(0xFF4A); }
  reg_t wx()      const { return mm_.read(0xFF4B); }
  reg_t ly()      const { return mm_.read(0xFF44); }
  reg_t lyc()     const { return mm_.read(0xFF45); }
  wide_reg_t lx() const { return lx_; }

  void ly(reg_t val)
  {
    mm_.write(0xFF44, val, true);
  }

  void tick()
  {
    auto v_ly = ly();
    auto const v_ly_orig = v_ly;

    ++lx_;

    if (lx_ == 450) {
      ++v_ly;
      lx_ = 0;
    }

    if (v_ly == 154) {
      v_ly = 0;
    }

    bool mode_entered = false;

    reg_t mode = 0x00;
    if (v_ly >= 144) {
      mode = 0x01;
    }
    else if (lx() == 360) {
      mode = 0x02;
      render_scanline_(v_ly);
      mode_entered = true;
    }
    else if (lx() > 360) {
      mode = 0x02;
    }
    else if (lx() >= 160) {
      mode = 0x00;
    }
    else if (lx() == 0) {
      mode = 0x03;
      mode_entered = true;
      render_background_(lx(), v_ly);
    }
    else {
      mode = 0x03;
      render_background_(lx(), v_ly);
    }

    reg_t ly_lyc = (v_ly == lyc()) << 2;
    reg_t stat = mm_.read(0xFF41) & 0xF8;
    mm_.write(0xFF41, stat | ly_lyc | mode);

    auto const int_00 = stat & 0x08;
    auto const int_01 = stat & 0x10;
    auto const int_10 = stat & 0x20;
    auto const int_ly = stat & 0x40;

    bool interrupt =
      (mode == 0x00 and int_00) or
      (mode == 0x01 and int_01) or
      (mode == 0x10 and int_10) or
      (ly_lyc       and int_ly);

    if (mode_entered and interrupt) {
      mm_.write(0xFF0F, mm_.read(0xFF0F) | 0x02); // LCDC int
    }

    if (v_ly == 144 and lx() == 0) {
      mm_.write(0xFF0F, mm_.read(0xFF0F) | 0x01); // vblank
    }

    if (v_ly != v_ly_orig)
      ly(v_ly);
  }

private:
  reg_t pixel_tile_(
    reg_t index,
    int x,
    int y,
    bool tds = false,
    bool x_flip = false,
    bool y_flip = false) const
  {
    reg_t tile_byte_1 = 0;
    reg_t tile_byte_2 = 0;

    int const tile_y = y_flip ? (7-y) : y;
    if (tds) {
      tile_byte_1 = mm_.read(0x8000 + (index*16) + tile_y*2 + 0);
      tile_byte_2 = mm_.read(0x8000 + (index*16) + tile_y*2 + 1);
    }
    else {
      tile_byte_1 = mm_.read(0x9000 + (static_cast<int8_t>(index)*16) + y*2 + 0);
      tile_byte_2 = mm_.read(0x9000 + (static_cast<int8_t>(index)*16) + y*2 + 1);
    }

    int   const bit         = x_flip ? x : (7 - x);
    reg_t const pixel_bit_1 = (tile_byte_1 & (1 << bit)) > 0;
    reg_t const pixel_bit_2 = (tile_byte_2 & (1 << bit)) > 0;

    return (pixel_bit_2 << 1) | pixel_bit_1;
  }

  void render_scanline_(int line)
  {
    auto const v_lcdc = lcdc();
    if (not (v_lcdc & 0x80))
      return;

    if (v_lcdc & 0x02)
       render_sprites_(line);

    if (v_lcdc & 0x20)
      render_window_(line);
  }

  void render_window_(int line)
  {
    int const v_wx = wx() - 7;
    int const v_wy = wy();

    if (wx() > 166 or wy() >= 143 or line < v_wy or (line > v_wy + 144))
      return;

    for (int x = 0; x < 160; ++x) {
      if (x < v_wx or x > (v_wx + 166))
        continue;

      screen_[line * width() + x] = map_palette_(0xFF47, pixel_window_(x, line));
    }
  }

  void render_sprites_(int line)
  {
    bool const small_sprites = not (lcdc() & 0x04);
    reg_t sprite_count = 0;

    for (int i = 0; i < 40 and sprite_count < 11; ++i) {
      auto const oam_addr = 0xFE00 + i*4;
      reg_t const s_y = mm_.read(oam_addr + 0);
      reg_t const s_x = mm_.read(oam_addr + 1);
      reg_t const s_n = mm_.read(oam_addr + 2);
      reg_t const c   = mm_.read(oam_addr + 3);

      bool const xf   =      c & 0x20;
      bool const yf   =      c & 0x40;
      bool const pal  =      c & 0x10;
      bool const prio = not (c & 0x80);

      if (s_y == 0 or s_x == 0) {
        continue;
      }

//      int const right_x = s_x;
      int const left_x  = s_x - 8;

      int const bottom_y = small_sprites ? s_y - 9 : s_y-1;
      int const top_y = s_y - 16;

      if (line < top_y or line > bottom_y)
        continue;

      ++sprite_count;

      for (int x = 0; x < 8; ++x) {
        auto const screen_x = left_x + x;
        if (screen_x < 0 or screen_x >= 160)
          continue;

	auto const dot_color =
	  pixel_tile_(s_n, x, line - top_y, true, xf, yf);
	if (dot_color == 0)
	  continue;

        auto const new_color =
	  map_palette_(0xFF48 + pal, dot_color);

        auto& pixel = screen_[line * width() + left_x+x];
        if (prio or pixel == 0)
          pixel = new_color;
      }
    }
  }

  void render_background_(int x, int y)
  {
    auto const v_lcdc = lcdc();
    if (not (v_lcdc & 0x01) or not (v_lcdc & 0x80))
      return;

    screen_[y * width() + x] = map_palette_(0xFF47, pixel_background_(x, y));
  }

  reg_t pixel_background_(int x, int y) const
  {
    bool tile_data_select = (lcdc() & 0x10);
    bool tile_map_select  = (lcdc() & 0x08);

    int tile_map_start = tile_map_select == false ? 0x9800 : 0x9C00;

    int const background_x = (x + scx()) % 256;
    int const background_y = (y + scy()) % 256;

    int const tile_x = background_x / 8;
    int const tile_y = background_y / 8;

    int const tile_local_x = background_x % 8;
    int const tile_local_y = background_y % 8;

    int const tile_data_table_index = tile_y * 32 + tile_x;
    int const tile_index = mm_.read(tile_map_start + tile_data_table_index);

    return pixel_tile_(tile_index, tile_local_x, tile_local_y, tile_data_select, false);
  }

  reg_t pixel_window_(int x, int y) const
  {
    bool tile_data_select = (lcdc() & 0x10);
    int tile_map_start = ((lcdc() & 0x40) == 0) ? 0x9800 : 0x9C00;

    int const bg_x = (x - wx() + 6);
    int const bg_y = (y - wy());

    int const tile_x = bg_x / 8;
    int const tile_y = bg_y / 8;

    int const tile_local_x = bg_x % 8;
    int const tile_local_y = bg_y % 8;

    int const tile_data_table_index = tile_y * 32 + tile_x;
    int const tile_index = mm_.read(tile_map_start + tile_data_table_index);

    return pixel_tile_(tile_index, tile_local_x, tile_local_y, tile_data_select);
  }

  reg_t map_palette_(wide_reg_t addr, reg_t value)
  {
    return (mm_.read(addr) >> (2*value)) & 0x03;
  }

private:
  MM&       mm_;

  int       lx_;

  screen_t  screen_;
};
