#pragma once

#include "types.h"
#include "mm.hpp"

#include <array>

class Timer
{
public:
  Timer(MM& mm)
    : mm_(mm)
  {}

  void power_on()
  {
    cnt_ = 0;
    cnt_2 = 0;
  }

  void tick()
  {
    ++cnt_2;
    if (cnt_2 > cls_[1]) { // DIV FIXME move into sep. method
      cnt_2 = 0;
      auto div = mm_.read(0xFF04) + 1;
      mm_.write(0xFF04, div, true);
    }

    // FIXME move into sep. method
    auto const tac = mm_.read(0xFF07);
    auto const cls = tac & 0x3;

    if (not (tac & 0x04))
      return;

    ++cnt_;

    if (cnt_ >= cls_[cls]) {
      cnt_ = 0;

      auto tima = mm_.read(0xFF05) + 1;

      if (tima == 0x00) {
        tima = mm_.read(0xFF06);
        mm_.write(0xFF0F, mm_.read(0xFF0F) | 0x04);
      }

      mm_.write(0xFF05, tima);
    }
  }

private:
  MM& mm_;

  int cnt_;
  int cnt_2; // FIXME rename into div_cnt or something

  std::array<int, 4> const cls_ = {{ 1024, 16, 64, 256 }};
};
