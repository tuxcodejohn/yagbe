#pragma once

#include "types.h"
#include "mm.hpp"

class Input
{
public:
  Input(MM& mm)
    : mm_(mm)
  {}

  void power_on()
  {
    left_           = false;
    right_          = false;
    up_             = false;
    down_           = false;
    a_              = false;
    b_              = false;
    start_          = false;
    select_         = false;
    button_changed_ = true;
    old_p_1         = 100;
  }

  void tick()
  {
    reg_t p1 =mm_.read(0xFF00) & 0x30;

    if (old_p_1 == p1 and not button_changed_)
      return;

    old_p_1 = p1;

    bool p14 = not (p1 & 0x10);
    bool p15 = not (p1 & 0x20);

    reg_t buttons = 0x00;
    buttons |= ((p14 and right_) | (p15 and a_))      << 0;
    buttons |= ((p14 and left_)  | (p15 and b_))      << 1;
    buttons |= ((p14 and up_)    | (p15 and select_)) << 2;
    buttons |= ((p14 and down_)  | (p15 and start_))  << 3;

    mm_.write(0xFF00, p1 | (~buttons & 0x0F));

    if (not button_changed_)
      return;

    if (
        (left_   and not old_left_)   or
        (right_  and not old_right_)  or
        (down_   and not old_down_)   or
        (up_     and not old_up_)     or
        (a_      and not old_a_)      or
        (b_      and not old_b_)      or
        (select_ and not old_select_) or
        (start_  and not old_start_)) {
      mm_.write(0xFF0F, mm_.read(0xFF0F) | 0x10);
    }

    old_left_       = left_;
    old_right_      = right_;
    old_up_         = up_;
    old_down_       = down_;
    old_a_          = a_;
    old_b_          = b_;
    old_start_      = start_;
    old_select_     = select_;
    button_changed_ = false;
  }

  void left(bool down)   { button_changed_ = true; left_ = down;   }
  void right(bool down)  { button_changed_ = true; right_ = down;  }
  void up(bool down)     { button_changed_ = true; up_ = down;     }
  void down(bool down)   { button_changed_ = true; down_ = down;   }

  void a(bool down)      { button_changed_ = true; a_ = down;      }
  void b(bool down)      { button_changed_ = true; b_ = down;      }
  void start(bool down)  { button_changed_ = true; start_ = down;  }
  void select(bool down) { button_changed_ = true; select_ = down; }

private:
  MM&   mm_;

  bool  left_;
  bool  right_;
  bool  up_;
  bool  down_;
  bool  a_;
  bool  b_;
  bool  start_;
  bool  select_;

  bool  old_left_;
  bool  old_right_;
  bool  old_up_;
  bool  old_down_;
  bool  old_a_;
  bool  old_b_;
  bool  old_start_;
  bool  old_select_;

  bool  button_changed_;
  reg_t old_p_1;
};
