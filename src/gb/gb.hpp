#pragma once

#include "types.h"
#include "mm.hpp"
#include "gr.hpp"
#include "cp.hpp"
#include "input.hpp"
#include "timer.hpp"

#include <string>

#include <stdio.h>

class GB
{
public:
  typedef uint8_t            reg_t;
  typedef uint16_t           wide_reg_t;
  typedef std::vector<reg_t> cartridge_t;
  typedef std::vector<reg_t> mem_t;

  Error insert_rom(cartridge_t const& cartridge)
  {
    return mm_.insert_rom(cartridge);
  }

  Error load_ram(mem_t const& ram)
  {
    return mm_.load_ram(ram);
  }

  void power_on()
  {
    mm_.power_on();
    cp_.power_on();
    t_.power_on();
    in_.power_on();
  }

  void left(bool down) { in_.left(down); }
  void right(bool down) { in_.right(down); }
  void up(bool down) { in_.up(down); }
  void down(bool down) { in_.down(down); }

  void a(bool down) { in_.a(down); }
  void b(bool down) { in_.b(down); }
  void start(bool down) { in_.start(down); }
  void select(bool down) { in_.select(down); }

  reg_t mem(wide_reg_t addr) const
  {
    return mm_.read(addr);
  }

  mem_t ram() const
  {
    return mm_.ram();
  }

  reg_t screen_width() const
  {
    return gr_.width();
  }

  reg_t screen_height() const
  {
    return gr_.height();
  }

  GR::screen_t screen() const
  {
    return gr_.screen();
  }

  bool is_v_blank_completed() const
  {
    return gr_.lx() == 0 and gr_.ly() == 0;
  }

  void tick()
  {
    if (not mm_.is_rom_verified() and cp_.pc() >= 0x0100) {
      mm_.rom_verified();
    }

    cp_.tick();
    in_.tick();
    t_.tick();
    gr_.tick();

    // FIXME: remove this serial dbg hack
    if (mm_.read(0xFF02)) {
      mm_.write(0xFF02, 0x00);
      printf("SERIAL:%c\n", mm_.read(0xFF01));
    }
  }

  void dbg()
  {
    cp_.dbg();
  }

private:
  MM      mm_;
  CP      cp_      = { mm_ };
  GR      gr_      = { mm_ };
  Timer   t_       = { mm_ };
  Input   in_      = { mm_ };
};
