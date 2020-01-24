#pragma once

#include "types.h"
#include "mm.hpp"

#include <map>
#include <map>
#include <string>
#include <functional>

class CP
{
  struct Op
  {
    uint8_t               const code;
    std::string           const desc;
    std::function<void()> const fn;
  };

public:
  CP(MM& mm)
    : mm_(mm)
  {}

  void power_on()
  {
    cycle_  = 0;
    cycles_ = 0;
    halted_ = false;
    ime_    = false;
    // flag_   = 0;
    sp_     = 0xFFFF;
    pc_     = 0x0100;

    mm_.write(0xFF0F, 0x00); // interrupt flag
    mm_.write(0xFFFF, 0xFF); // interrupt enable

    // check op code configuration
    for (int i = 0; i < ops_.size(); ++i) {
      if (ops_[i].code != i) {
        printf("ERROR: opcode does not match: %02x\n", i);
        exit(5);
      }
    }
  }

  wide_reg_t pc() const { return pc_; }
  wide_reg_t sp() const { return sp_; }
  void sp(wide_reg_t value) { sp_ = value; }

  bool zero_flag() const { return f() & (1 << 7); }
  void zero_flag(bool b) { set_bit_(7, b); }

  bool substract_flag() const { return f() & (1 << 6); }
  void substract_flag(bool b) { set_bit_(6, b); }

  bool half_carry_flag() const { return f() & (1 << 5); }
  void half_carry_flag(bool b) { set_bit_(5, b); }

  bool carry_flag() const { return f() & (1 << 4); }
  void carry_flag(bool b) { set_bit_(4, b); }

  reg_t& a() { return a_; }
  reg_t& b() { return b_; }
  reg_t& c() { return c_; }
  reg_t& d() { return d_; }
  reg_t& e() { return e_; }
  reg_t& f() { return f_; }
  reg_t& h() { return h_; }
  reg_t& l() { return l_; }

  reg_t const& a() const { return a_; }
  reg_t const& b() const { return b_; }
  reg_t const& c() const { return c_; }
  reg_t const& d() const { return d_; }
  reg_t const& e() const { return e_; }
  reg_t const& f() const { return f_; }
  reg_t const& h() const { return h_; }
  reg_t const& l() const { return l_; }

  wide_reg_t af() const { return wide_(a(), f()); }
  wide_reg_t bc() const { return wide_(b(), c()); }
  wide_reg_t de() const { return wide_(d(), e()); }
  wide_reg_t hl() const { return wide_(h(), l()); }

  void af(wide_reg_t value) { return wide_(a(), f(), value & 0xFFF0); }
  void bc(wide_reg_t value) { return wide_(b(), c(), value); }
  void de(wide_reg_t value) { return wide_(d(), e(), value); }
  void hl(wide_reg_t value) { return wide_(h(), l(), value); }

  reg_t op() const { return mm_.read(pc_); }
  reg_t b1() const { return mm_.read(pc_ + 1); }
  reg_t b2() const { return mm_.read(pc_ + 2); }
  wide_reg_t nn() const { return (b2() << 8) | b1(); }

  bool tick()
  {
    if (cycles_ > 0) {
      --cycles_;
      return false;
    }

    process_interrupt_();

    if (halted_)
      return false;

#if DEBUG_CPU
    dbg();
#endif

    process_opcode_();

    return true;
  }

  void dbg()
  {
    printf(
      "pc:%04x sp:%04x op:%02x,%02x,%02x af:%02x%02x bc:%02x%02x de:%02x%02x hl:%02x%02x %c%c%c%c LCDC:%02x %s\n",
      pc_, sp_, mm_.read(pc_), mm_.read(pc_+1), mm_.read(pc_+2), a_, f_, b_, c_, d_, e_, h_, l_,
      (zero_flag() ? 'Z' : '_'),
      (substract_flag() ? 'S' : '_'),
      (half_carry_flag() ? 'H' : '_'),
      (carry_flag() ? 'C' : '_'),
      mm_.read(0xFF40),
      ops_[ mm_.read(pc_)].desc.c_str());
  }

private:
  void process_interrupt_()
  {
    if (not ime_)
      return;

    auto fn_is_enabled = [this] (reg_t val) -> bool {
      return (mm_.read(0xFFFF) & val) and (mm_.read(0xFF0F) & val);
    };

    if (fn_is_enabled(0x01)) { // vblank
       // printf("INT: VBLANK\n");
       process_interrupt_(0x0040);
       mm_.write(0xFF0F, mm_.read(0xFF0F) ^ 0x01);
       return;
    }

    if (fn_is_enabled(0x02)) { // lcdc
      // printf("INT: LCDC\n");
      process_interrupt_(0x0048);
      mm_.write(0xFF0F, mm_.read(0xFF0F) ^ 0x02);

      return;
    }

    if (fn_is_enabled(0x04)) { // timer
      // printf("INT: TIMER\n");
      process_interrupt_(0x0050);
      mm_.write(0xFF0F, mm_.read(0xFF0F) ^ 0x04);
      return;
    }

    if (fn_is_enabled(0x08)) { // S IO
      // printf("INT: SIO\n");
      process_interrupt_(0x0058);
      mm_.write(0xFF0F, mm_.read(0xFF0F) ^ 0x08);
      return;
    }

    if (fn_is_enabled(0x10)) { // high->low P10-P13
      // printf("INT: P10-13\n");
      process_interrupt_(0x0060);
      mm_.write(0xFF0F, mm_.read(0xFF0F) ^ 0x10);
      return;
    }
  }

  void process_interrupt_(wide_reg_t addr) {
    ime_ = false;
    push_(pc_);
    pc_ = addr; //0x0040;
    halted_ = false; // FIXME where to put this?
  }

  void process_opcode_()
  {
    auto const op_code = op();

    if (op_code == 0xCB) {
      auto fn = pref_.find(b1());
      if (fn != pref_.end()) {
        fn->second();
        return;
      }
      exit(1);
    }

    ops_[op_code].fn();
  }

  wide_reg_t wide_(reg_t const& high, reg_t const& low) const
  {
    return (high << 8) | low;
  }

  void wide_(reg_t& high, reg_t& low, wide_reg_t value)
  {
    high = value >> 8;
    low  = value;
  }

  void set_bit_(int n, bool val) // FIXME: rename
  {
    f() ^= (-static_cast<unsigned long>(val) ^ f()) & (1UL << n);
  }

  inline void push_(wide_reg_t val) // FIXME: dirty
  {
    push_stack_(val >> 8);
    push_stack_(val);
  }

  inline wide_reg_t pop_() // FIXME: dirty
  {
    auto l = pop_stack_();
    auto h = pop_stack_();
    return h << 8 | l;
  }

  void push_stack_(reg_t value) // FIXME: dirty
  {
    --sp_;
    mm_.write(sp_, value);
  }

  reg_t pop_stack_() // FIXME: dirty
  {
    auto const v = mm_.read(sp_);
    ++sp_;
    return v;
  }

  inline void ld_8(reg_t const& src, reg_t& dst)
  {
    dst = src;
  }

  inline void ld_hl_spn_(uint8_t n)
  {
    auto const sp_old = sp();
    sp(static_cast<int8_t>(sp_old) + static_cast<int8_t>(n)); // FIXME wtf

    zero_flag(false);
    substract_flag(false);

    carry_flag(false); // FIXME ???
    half_carry_flag(false); // FIXME ???
  }

  inline void add_8(reg_t n, reg_t& dst)
  {
    half_carry_flag(((dst & 0xF) + (n & 0xF)) > 0x0F);
    carry_flag((static_cast<uint16_t>(dst) + static_cast<uint16_t>(n)) > 0xFF);

    dst += n;

    zero_flag(dst == 0);
    substract_flag(false);
  }

  inline void inc_(reg_t& dst)
  {
    half_carry_flag(((dst & 0xF) + (1 & 0xF)) > 0x0F);

    dst += 1;

    zero_flag(dst == 0);
    substract_flag(false);
  }


  inline wide_reg_t add_16(wide_reg_t n, wide_reg_t dst)
  {
    half_carry_flag(((dst & 0x0FFF) + (n & 0x0FFF)) > 0x0FFF);
    carry_flag((static_cast<uint32_t>(dst) + static_cast<uint32_t>(n)) > 0xFFFF);

    dst += n;

    substract_flag(false);

    return dst;
  }

  inline void adc_8(uint32_t n, reg_t& dst)
  {
    // add_8(carry_flag() ? n + 1 : n, dst); // orig

    // n += static_cast<int>(carry_flag());

    // half_carry_flag(((dst & 0xF) + (n & 0xF)) & 0x10);
    // carry_flag((static_cast<uint16_t>(dst) + static_cast<uint16_t>(n)) & 0x10000);

    // dst += n;

    // zero_flag(dst == 0);
    // substract_flag(false);

    uint32_t result = dst + n;

    if (carry_flag())
      result += 1;

    half_carry_flag(((dst & 0xF) + (n & 0xF) + carry_flag()) > 0x0F);
    carry_flag(result > 0xFF);

    dst = result;

    zero_flag(dst == 0);
    substract_flag(false);
  }

  inline void sub_8(reg_t n, reg_t& dst)
  {
    // half_carry_flag(((dst & 0xF) - (n & 0xF)) < 0);
    // carry_flag((static_cast<uint16_t>(dst) - static_cast<uint16_t>(n)) < 0);

    // dst -= n;

    // zero_flag(dst == 0);
    // substract_flag(true);
    int result = dst - n;

    half_carry_flag(((dst & 0xF) - (n & 0xF)) < 0);
    carry_flag(result < 0);

    dst = result;

    zero_flag(dst == 0);
    substract_flag(true);
  }

  inline void dec_(reg_t& dst)
  {
    int result = dst - 1;

    half_carry_flag(((dst & 0xF) - (1 & 0xF)) < 0);

    dst = result;

    zero_flag(dst == 0);
    substract_flag(true);
  }

  inline void sbc_8(reg_t n, reg_t& dst)
  {
    int result = dst - n - carry_flag();

    half_carry_flag(((dst & 0xF) - (n & 0xF) - carry_flag()) < 0);
    carry_flag(result < 0);

    dst = result;

    zero_flag(dst == 0);
    substract_flag(true);
  }

  inline void and_(reg_t n, reg_t& dst)
  {
    dst = dst & n;

    zero_flag(dst == 0);
    substract_flag(false);
    half_carry_flag(true);
    carry_flag(false);
  }

  inline void or_(reg_t n, reg_t& dst)
  {
    dst = dst | n;

    zero_flag(dst == 0);
    substract_flag(false);
    half_carry_flag(false);
    carry_flag(false);
  }

  inline void xor_(reg_t n, reg_t& dst)
  {
    dst = dst ^ n;

    zero_flag(dst == 0);
    substract_flag(false);
    half_carry_flag(false);
    carry_flag(false);
  }

  inline void cp_(reg_t n)
  {
    zero_flag(a() == n);
    substract_flag(true);
    half_carry_flag(((a() & 0xF) - (n & 0xF)) < 0);
    carry_flag(a() < n);
  }

  inline void rlc_(reg_t& dst, bool zero = false)
  {
    auto const carry = dst & 0x80;
    dst <<= 1;
    dst |= carry >> 7;
    carry_flag(carry);

    if (zero)
      zero_flag(dst == 0);
    else
      zero_flag(false);

    substract_flag(false);
    half_carry_flag(false);
  }

  inline void rrc_(reg_t& dst, bool zero = false)
  {
    auto const carry =  dst & 0x01;
    dst >>= 1;
    dst |= carry << 7;
    carry_flag(carry);

    if (zero)
      zero_flag(dst == 0);
    else
      zero_flag(false);

    substract_flag(false);
    half_carry_flag(false);
  }

  inline void rl_(reg_t& dst, bool zero = false)
  {
    reg_t const old_carry = carry_flag();
    carry_flag(dst & 0x80);
    dst <<= 1;
    dst |= old_carry;

    if (zero)
      zero_flag(dst == 0);
    else
      zero_flag(false);

    substract_flag(false);
    half_carry_flag(false);
  }

  inline void rr_(reg_t& dst, bool zero = false)
  {
    reg_t const old_carry = carry_flag();
    carry_flag(dst & 0x01);
    dst >>= 1;
    dst |= old_carry << 7;

    if (zero)
      zero_flag(dst == 0);
    else
      zero_flag(false);

    substract_flag(false);
    half_carry_flag(false);
  }

  inline void sla_(reg_t& dst)
  {
    carry_flag(dst & 0x80);
    dst <<= 1;

    zero_flag(dst == 0);
    substract_flag(false);
    half_carry_flag(false);
  }

  inline void sra_(reg_t& dst)
  {
    reg_t const old_msb = dst & 0x80;
    carry_flag(dst & 0x01);

    dst >>= 1;
    dst |= old_msb;

    zero_flag(dst == 0);
    substract_flag(false);
    half_carry_flag(false);
  }

  inline void srl_(reg_t& dst)
  {
    carry_flag(dst & 0x01);

    dst >>= 1;

    zero_flag(dst == 0);
    substract_flag(false);
    half_carry_flag(false);
  }

  inline void bit_(reg_t dst, reg_t bit)
  {
    zero_flag((dst & (1 << bit)) == 0);
    substract_flag(false);
    half_carry_flag(true);
  }

  inline void set_(reg_t& dst, reg_t bit)
  {
    dst |= (1 << bit);
  }

  inline void res_(reg_t& dst, reg_t bit)
  {
    dst &= ~(1 << bit);
  }

  inline void call_(wide_reg_t nn, reg_t offset = 3)
  {
    auto const next_pc = pc_ + offset;
    push_(next_pc);
    pc_ = nn;
  }

  inline void swap_(reg_t& dst)
  {
    dst = ((dst & 0xF0) >> 4) | ((dst & 0x0F) << 4);
    zero_flag(dst == 0);
    carry_flag(false);
    substract_flag(false);
    half_carry_flag(false);
  }

private:
  MM&        mm_;

  reg_t      a_;
  reg_t      b_;
  reg_t      c_;
  reg_t      d_;
  reg_t      e_;
  reg_t      f_;
  reg_t      g_;
  reg_t      h_;
  reg_t      l_;
  // reg_t      flag_;
  wide_reg_t sp_;
  wide_reg_t pc_;

  bool       ime_;
  bool       halted_;

  uint8_t    cycles_; // FIXME: rename to busy_cycles
  uint64_t   cycle_;

  std::array<Op, 0x100> ops_ = {{
    {
      0x00,
      "NOP",
      [this]() { pc_ += 1; cycles_ = 4; }
    },
    {
      0x01,
      "LD BC,nn",
      [this]() { bc(nn()); pc_ += 3; cycles_ = 12; }
    },
    {
      0x02,
      "LD (BC),A",
      [this]() { reg_t i; ld_8(a(), i); mm_.write(bc(), i); pc_ += 1; cycles_ = 8; }
    },
    {
      0x03,
      "INC BC",
      [this]() { bc(bc() + 1); pc_ += 1; cycles_ = 8; }
    },
    {
      0x04,
      "INC B",
      [this]() { inc_(b()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x05,
      "DEC B",
      [this]() { dec_(b()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x06,
      "LD B,n",
      [this]() { ld_8(b1(), b()); pc_ += 2; cycles_ = 8; }
    },
    {
      0x07,
      "RLCA",
      [this]() { rlc_(a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x08,
      "LD (nn),SP",
      [this]()
      {
        mm_.write(nn()    ,  sp() & 0x00FF      );
        mm_.write(nn() + 1, (sp() & 0xFF00) >> 8);
        pc_ += 3;
        cycles_ = 20;
      }
    },
    {
      0x09,
      "ADD HL,BC",
      [this]() { hl(add_16(bc(), hl())); pc_ += 1; cycles_ = 8; }
    },
    {
      0x0A, "LD A,(BC)",
      [this]() { ld_8(mm_.read(bc()), a()); pc_ += 1; cycles_ = 8; }
    },
    {
      0x0B,
      "DEC BC",
      [this]() { bc(bc() - 1); pc_ += 1; cycles_ = 8; }
    },
    {
      0x0C,
      "INC C",
      [this]() { inc_(c()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x0D,
      "DEC C",
      [this]() { dec_(c()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x0E,
      "LD C,n",
      [this]() { ld_8(b1(), c()); pc_ += 2; cycles_ = 8; }
    },
    {
      0x0F,
      "RRCA",
      [this]() { rrc_(a()); pc_ += 1; cycles_ = 4; }
    },

    { 0x10,
      "STOP",
      [this]() { halted_ = true; pc_ += 2; cycles_ = 4; }
    },
    {
      0x11,
      "LD DE,nn",
      [this]() { de(nn()); pc_ += 3; cycles_ = 12; }
    },
    {
      0x12,
      "LD (DE),A",
      [this]() { reg_t i; ld_8(a(), i); mm_.write(de(), i); pc_ += 1; cycles_ = 8; }
    },
    {
      0x13,
      "INC DE",
      [this]() { de(de() + 1); pc_ += 1; cycles_ = 8; }
    },
    {
      0x14,
      "INC D",
      [this]() { inc_(d()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x15,
      "DEC D",
      [this]() { dec_(d()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x16,
      "LD D,n",
      [this]() { ld_8(b1(), d()); pc_ += 2; cycles_ = 8; }
    },
    {
      0x17,
      "RLA",
      [this]() { rl_(a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x18,
      "JR n",
      [this]() { pc_ += static_cast<int8_t>(b1()) + 2; cycles_ = 8; }
    },
    {
      0x19,
      "ADD HL,DE",
      [this]() { hl(add_16(de(), hl())); pc_ += 1; cycles_ = 8; }
    },
    {
      0x1A,
      "LD A,(DE)",
      [this]() { ld_8(mm_.read(de()), a()); pc_ += 1; cycles_ = 8; }
    },
    {
      0x1B,
      "DEC DE",
      [this]() { de(de() - 1); pc_ += 1; cycles_ = 8; }
    },
    {
      0x1C,
      "INC E",
      [this]() { inc_(e()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x1D,
      "DEC E",
      [this]() { dec_(e()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x1E,
      "LD E,n",
      [this]() { ld_8(b1(), e()); pc_ += 2; cycles_ = 8; }
    },
    {
      0x1F,
      "RRA",
      [this]() { rr_(a()); pc_ += 1; cycles_ = 4; }
    },

    { 0x20,
      "JR NZ,n",
      [this]()
	  {
		int8_t r = b1();
		if (zero_flag())
          r = 0;
		pc_ += r + 2;
		cycles_ = 8;
	  }
    },
    {
      0x21,
      "LD HL,nn",
      [this]() { hl(nn()); pc_ += 3; cycles_ = 12; }
    },
    {
      0x22,
      "LD (HL+),A",
      [this]() { reg_t i = 0; ld_8(a(), i); mm_.write(hl(), i); hl(hl()+1); pc_ += 1; cycles_ = 8; }
    },
    {
      0x23,
      "INC HL",
      [this]() { hl(hl() + 1); pc_ += 1; cycles_ = 8; }
    },
    {
      0x24,
      "INC H",
      [this]() { inc_(h()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x25,
      "DEC H",
      [this]() { dec_(h()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x26,
      "LD H,n",
      [this]() { ld_8(b1(), h()); pc_ += 2; cycles_ = 8; }
    },
    {
      0x27,
      "DAA",
      // [this]() { a() = ((a()/10%10)<<4)|((a()%10)&0xF); pc_ += 1; cycles_ = 4; }
      [this]()
      {
        int va = a();
        if (not substract_flag()) {
          if (half_carry_flag() or (va & 0x0F) > 0x09)
            va += 0x06;

          if (carry_flag() or va > 0x9F) {
            va += 0x60;
          }
        }
        else {
          if (half_carry_flag())
            va = (va - 0x06) & 0xFF;

          if (carry_flag())
            va -= 0x60;
        }

        if ((va & 0x100) == 0x100) {
          carry_flag(true);
        }

        half_carry_flag(false);
        zero_flag((va & 0xFF) == 0);
        a() = va & 0xFF;

        pc_ += 1; cycles_ = 4;
      }
    },
    {
      0x28,
      "JR Z,n",
      [this]() { auto const d = (zero_flag())      ? static_cast<int8_t>(b1()) : 0; pc_ += d + 2; cycles_ = 8; }
    },
    {
      0x29,
      "ADD HL,HL",
      [this]() { hl(add_16(hl(), hl())); pc_ += 1; cycles_ = 8; }
    },
    {
      0x2A,
      "LD A,(HL+)",
      [this]() { ld_8(mm_.read(hl()), a()); hl(hl()+1); pc_ += 1; cycles_ = 8; }
    },
    {
      0x2B,
      "DEC HL",
      [this]() { hl(hl() - 1); pc_ += 1; cycles_ = 8; }
    },
    {
      0x2C,
      "INC L",
      [this]() { inc_(l()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x2D,
      "DEC L",
      [this]() { dec_(l()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x2E,
      "LD L,n",
      [this]() { ld_8(b1(), l()); pc_ += 2; cycles_ = 8; }
    },
    {
      0x2F,
      "CPL",
      [this]() { a() = ~a(); substract_flag(true); half_carry_flag(true); pc_ += 1; cycles_ = 4; }
    },

    { 0x30,
      "JR NC,n",
      [this]() { auto const d = (not carry_flag()) ? static_cast<int8_t>(b1()) : 0; pc_ += d + 2; cycles_ = 8; }
    },
    {
      0x31,
      "LD SP,nn",
      [this]() { sp(nn()); pc_ += 3; cycles_ = 12; }
    },
    {
      0x32,
      "LD (HL-),A",
      [this]() { reg_t i = 0; ld_8(a(), i); mm_.write(hl(), i); hl(hl()-1); pc_ += 1; cycles_ = 8; }
    },
    {
      0x33,
      "INC SP",
      [this]() { sp(sp() + 1); pc_ += 1; cycles_ = 8; }
    },
    {
      0x34,
      "INC (HL)",
      [this]() { reg_t i = mm_.read(hl()); inc_(i); mm_.write(hl(), i); pc_ += 1; cycles_ = 12; }
    },
    {
      0x35,
      "DEC (HL)",
      [this]() { reg_t i = mm_.read(hl()); dec_(i); mm_.write(hl(), i); pc_ += 1; cycles_ = 12; }
    },
    {
      0x36,
      "LD (HL),n",
      [this]() { reg_t i = 0; ld_8(b1(), i); mm_.write(hl(), i); pc_ += 2; cycles_ = 12; }
    },
    {
      0x37,
      "SCF",
      [this]() { carry_flag(true); substract_flag(false); half_carry_flag(false); pc_ += 1; cycles_ = 4; }
    },
    {
      0x38,
      "JR C,n",
      [this]() { auto const d = (carry_flag())     ? static_cast<int8_t>(b1()) : 0; pc_ += d + 2; cycles_ = 8; }
    },
    {
      0x39,
      "ADD HL,SP",
      [this]() { hl(add_16(sp(), hl())); pc_ += 1; cycles_ = 8; }
    },
    {
      0x3A,
      "LD A,(HL-)",
      [this]() { ld_8(mm_.read(hl()), a()); hl(hl()-1); pc_ += 1; cycles_ = 8; }
    },
    {
      0x3B,
      "DEC SP",
      [this]() { sp(sp() - 1); pc_ += 1; cycles_ = 8; }
    },
    {
      0x3C,
      "INC A",
      [this]() { inc_(a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x3D,
      "DEC A",
      [this]() { dec_(a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x3E,
      "LD A,n",
      [this]() { ld_8(b1(), a()); pc_ += 2; cycles_ = 8; }
    },
    {
      0x3F,
      "CCF",
      [this]() { substract_flag(false); half_carry_flag(false); carry_flag(carry_flag()?false:true); pc_ += 1; cycles_ = 4; }
    },

    { 0x40,
      "LD B,B",
      [this]() { ld_8(b(), b()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x41,
      "LD B,C",
      [this]() { ld_8(c(), b()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x42,
      "LD B,D",
      [this]() { ld_8(d(), b()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x43,
      "LD B,E",
      [this]() { ld_8(e(), b()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x44,
      "LD B,H",
      [this]() { ld_8(h(), b()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x45,
      "LD B,L",
      [this]() { ld_8(l(), b()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x46,
      "LD B,(HL)",
      [this]() { ld_8(mm_.read(hl()), b()); pc_ += 1; cycles_ = 8; }
    },
    {
      0x47,
      "LD B,A",
      [this]() { ld_8(a(), b()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x48,
      "LD C,B",
      [this]() { ld_8(b(), c()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x49,
      "LD C,C",
      [this]() { ld_8(c(), c()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x4A,
      "LD C,D",
      [this]() { ld_8(d(), c()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x4B,
      "LD C,E",
      [this]() { ld_8(e(), c()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x4C,
      "LD C,H",
      [this]() { ld_8(h(), c()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x4D,
      "LD C,L",
      [this]() { ld_8(l(), c()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x4E,
      "LD C,(HL)",
      [this]() { ld_8(mm_.read(hl()), c()); pc_ += 1; cycles_ = 8; }
    },
    {
      0x4F,
      "LD C,A",
      [this]() { ld_8(a(), c()); pc_ += 1; cycles_ = 4; }
    },
    { 0x50,
      "LD D,B",
      [this]() { ld_8(b(), d()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x51,
      "LD D,C",
      [this]() { ld_8(c(), d()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x52,
      "LD D,D",
      [this]() { ld_8(d(), d()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x53,
      "LD D,E",
      [this]() { ld_8(e(), d()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x54,
      "LD D,H",
      [this]() { ld_8(h(), d()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x55,
      "LD D,L",
      [this]() { ld_8(l(), d()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x56,
      "LD D,(HL)",
      [this]() { ld_8(mm_.read(hl()), d()); pc_ += 1; cycles_ = 8; }
    },
    {
      0x57,
      "LD D,A",
      [this]() { ld_8(a(), d()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x58,
      "LD E,B",
      [this]() { ld_8(b(), e()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x59,
      "LD E,C",
      [this]() { ld_8(c(), e()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x5A,
      "LD E,D",
      [this]() { ld_8(d(), e()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x5B,
      "LD E,E",
      [this]() { ld_8(e(), e()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x5C,
      "LD E,H",
      [this]() { ld_8(h(), e()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x5D,
      "LD E,L",
      [this]() { ld_8(l(), e()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x5E,
      "LD E,(HL)",
      [this]() { ld_8(mm_.read(hl()), e()); pc_ += 1; cycles_ = 8; }
    },
    {
      0x5F,
      "LD E,A",
      [this]() { ld_8(a(), e()); pc_ += 1; cycles_ = 4; }
    },

    { 0x60,
      "LD H,B",
      [this]() { ld_8(b(), h()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x61,
      "LD H,C",
      [this]() { ld_8(c(), h()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x62,
      "LD H,D",
      [this]() { ld_8(d(), h()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x63,
      "LD H,E",
      [this]() { ld_8(e(), h()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x64,
      "LD H,H",
      [this]() { ld_8(h(), h()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x65,
      "LD H,l",
      [this]() { ld_8(l(), h()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x66,
      "LD H,(HL)",
      [this]() { ld_8(mm_.read(hl()), h()); pc_ += 1; cycles_ = 8; }
    },
    {
      0x67,
      "LD H,A",
      [this]() { ld_8(a(), h()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x68,
      "LD L,B",
      [this]() { ld_8(b(), l()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x69,
      "LD L,C",
      [this]() { ld_8(c(), l()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x6A,
      "LD L,D",
      [this]() { ld_8(d(), l()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x6B,
      "LD L,E",
      [this]() { ld_8(e(), l()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x6C,
      "LD L,H",
      [this]() { ld_8(h(), l()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x6D,
      "LD L,L",
      [this]() { ld_8(l(), l()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x6E,
      "LD L,(HL)",
      [this]() { ld_8(mm_.read(hl()), l()); pc_ += 1; cycles_ = 8; }
    },
    {
      0x6F,
      "LD L,A",
      [this]() { ld_8(a(), l()); pc_ += 1; cycles_ = 4; }
    },
    { 0x70,
      "LD (HL),B",
      [this]() { reg_t i = 0; ld_8(b(), i); mm_.write(hl(), i); pc_ += 1; cycles_ = 8; }
    },
    {
      0x71,
      "LD (HL),C",
      [this]() { reg_t i = 0; ld_8(c(), i); mm_.write(hl(), i); pc_ += 1; cycles_ = 8; }
    },
    {
      0x72,
      "LD (HL),D",
      [this]() { reg_t i = 0; ld_8(d(), i); mm_.write(hl(), i); pc_ += 1; cycles_ = 8; }
    },
    {
      0x73,
      "LD (HL),E",
      [this]() { reg_t i = 0; ld_8(e(), i); mm_.write(hl(), i); pc_ += 1; cycles_ = 8; }
    },
    {
      0x74,
      "LD (HL),H",
      [this]() { reg_t i = 0; ld_8(h(), i); mm_.write(hl(), i); pc_ += 1; cycles_ = 8; }
    },
    {
      0x75,
      "LD (HL),L",
      [this]() { reg_t i = 0; ld_8(l(), i); mm_.write(hl(), i); pc_ += 1; cycles_ = 8; }
    },
    {
      0x76,
      "HALT",
      [this]() { halted_ = true; pc_ += 1; cycles_ = 4; }
    },
    {
      0x77,
      "LD (HL),A",
      [this]() { reg_t i = 0; ld_8(a(), i); mm_.write(hl(), i); pc_ += 1; cycles_ = 8; }
    },
    {
      0x78,
      "LD A,B",
      [this]() { ld_8(b(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x79,
      "LD A,C",
      [this]() { ld_8(c(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x7A,
      "LD A,D",
      [this]() { ld_8(d(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x7B,
      "LD A,E",
      [this]() { ld_8(e(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x7C,
      "LD A,H",
      [this]() { ld_8(h(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x7D,
      "LD A,L",
      [this]() { ld_8(l(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x7E,
      "LD A,(HL)",
      [this]() { ld_8(mm_.read(hl()), a()); pc_ += 1; cycles_ = 8; }
    },
    {
      0x7F,
      "LD A,A",
      [this]() { ld_8(a(), a()); pc_ += 1; cycles_ = 4; }
    },

    { 0x80,
      "ADD A,B",
      [this]() { add_8(b(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x81,
      "ADD A,C",
      [this]() { add_8(c(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x82,
      "ADD A,D",
      [this]() { add_8(d(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x83,
      "ADD A,E",
      [this]() { add_8(e(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x84,
      "ADD A,H",
      [this]() { add_8(h(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x85,
      "ADD A,L",
      [this]() { add_8(l(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x86,
      "ADD A,(HL)",
      [this]() { add_8(mm_.read(hl()), a()); pc_ += 1; cycles_ = 8; }
    },
    {
      0x87,
      "ADD A,A",
      [this]() { add_8(a(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x88,
      "ADC A,B",
      [this]() { adc_8(b(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x89,
      "ADC A,C",
      [this]() { adc_8(c(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x8A,
      "ADC A,D",
      [this]() { adc_8(d(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x8B,
      "ADC A,E",
      [this]() { adc_8(e(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x8C,
      "ADC A,H",
      [this]() { adc_8(h(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x8D,
      "ADC A,L",
      [this]() { adc_8(l(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x8E,
      "ADC A,(HL)",
      [this]() { adc_8(mm_.read(hl()), a()); pc_ += 1; cycles_ = 8; }
    },
    {
      0x8F,
      "ADC A,A",
      [this]() { adc_8(a(), a()); pc_ += 1; cycles_ = 4; }
    },
    { 0x90,
      "SUB B",
      [this]() { sub_8(b(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x91,
      "SUB C",
      [this]() { sub_8(c(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x92,
      "SUB D",
      [this]() { sub_8(d(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x93,
      "SUB E",
      [this]() { sub_8(e(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x94,
      "SUB H",
      [this]() { sub_8(h(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x95,
      "SUB L",
      [this]() { sub_8(l(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x96,
      "SUB (HL)",
      [this]() { sub_8(mm_.read(hl()), a()); pc_ += 1; cycles_ = 8; }
    },
    {
      0x97,
      "SUB A",
      [this]() { sub_8(a(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x98,
      "SBC A,B",
      [this]() { sbc_8(b(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x99,
      "SBC A,C",
      [this]() { sbc_8(c(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x9A,
      "SBC A,D",
      [this]() { sbc_8(d(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x9B,
      "SBC A,E",
      [this]() { sbc_8(e(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x9C,
      "SBC A,H",
      [this]() { sbc_8(h(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x9D,
      "SBC A,L",
      [this]() { sbc_8(l(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0x9E,
      "SBC A,(HL)",
      [this]() { sbc_8(mm_.read(hl()), a()); pc_ += 1; cycles_ = 8; }
    },
    {
      0x9F,
      "SBC A,A",
      [this]() { sbc_8(a(), a()); pc_ += 1; cycles_ = 4; }
    },
    { 0xA0,
      "AND B",
      [this]() { and_(b(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0xA1,
      "AND C",
      [this]() { and_(c(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0xA2,
      "AND D",
      [this]() { and_(d(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0xA3,
      "AND E",
      [this]() { and_(e(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0xA4,
      "AND H",
      [this]() { and_(h(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0xA5,
      "AND L",
      [this]() { and_(l(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0xA6,
      "AND (HL)",
      [this]() { and_(mm_.read(hl()), a()); pc_ += 1; cycles_ = 8; }
    },
    {
      0xA7,
      "AND A",
      [this]() { and_(a(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0xA8,
      "XOR B",
      [this]() { xor_(b(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0xA9,
      "XOR C",
      [this]() { xor_(c(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0xAA,
      "XOR D",
      [this]() { xor_(d(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0xAB,
      "XOR E",
      [this]() { xor_(e(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0xAC,
      "XOR H",
      [this]() { xor_(h(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0xAD,
      "XOR L",
      [this]() { xor_(l(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0xAE,
      "XOR (HL)",
      [this]() { xor_(mm_.read(hl()), a()); pc_ += 1; cycles_ = 8; }
    },
    {
      0xAF,
      "XOR A",
      [this]() { xor_(a(), a()); pc_ += 1; cycles_ = 4; }
    },

    { 0xB0,
      "OR B",
      [this]() { or_(b(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0xB1,
      "OR C",
      [this]() { or_(c(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0xB2,
      "OR D",
      [this]() { or_(d(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0xB3,
      "OR E",
      [this]() { or_(e(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0xB4,
      "OR H",
      [this]() { or_(h(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0xB5,
      "OR L",
      [this]() { or_(l(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0xB6,
      "OR (HL)",
      [this]() { or_(mm_.read(hl()), a()); pc_ += 1; cycles_ = 8; }
    },
    {
      0xB7,
      "OR A",
      [this]() { or_(a(), a()); pc_ += 1; cycles_ = 4; }
    },
    {
      0xB8,
      "CP B",
      [this]() { cp_(b()); pc_ += 1; cycles_ = 4; }
    },
    {
      0xB9,
      "CP C",
      [this]() { cp_(c()); pc_ += 1; cycles_ = 4; }
    },
    {
      0xBA,
      "CP D",
      [this]() { cp_(d()); pc_ += 1; cycles_ = 4; }
    },
    {
      0xBB,
      "CP E",
      [this]() { cp_(e()); pc_ += 1; cycles_ = 4; }
    },
    {
      0xBC,
      "CP H",
      [this]() { cp_(h()); pc_ += 1; cycles_ = 4; }
    },
    {
      0xBD,
      "CP L",
      [this]() { cp_(l()); pc_ += 1; cycles_ = 4; }
    },
    {
      0xBE,
      "CP (HL)",
      [this]() { cp_(mm_.read(hl())); pc_ += 1; cycles_ = 8; }
    },
    {
      0xBF,
      "CP A",
      [this]() { cp_(a()); pc_ += 1; cycles_ = 4; }
    },

    { 0xC0,
      "RET NZ",
      [this]() { if (not zero_flag())  pc_ = pop_(); else pc_ += 1; cycles_ = 12; }
    },
    {
      0xC1,
      "POP BC",
      [this]() { bc(pop_()); pc_ += 1; cycles_ = 12; }
    },
    {
      0xC2,
      "JP NZ,nn",
      [this]() { pc_ = (not zero_flag()) ? nn() : pc_ + 3; cycles_ = 12; }
    },
    {
      0xC3,
      "JP nn",
      [this]() { pc_ = nn(); cycles_ = 12; }
    },
    {
      0xC4,
      "CALL NZ,nn",
      [this]() { if (not zero_flag())  call_(nn()); else pc_ += 3; cycles_ = 12; }
    },
    {
      0xC5,
      "PUSH BC",
      [this]() { push_(bc()); pc_ += 1; cycles_ = 16; }
    },
    {
      0xC6,
      "ADD A,#",
      [this]() { add_8(b1(), a()); pc_ += 2; cycles_ = 8; }
    },
    {
      0xC7,
      "RST 00H",
      [this]() { call_(0x0000, 1); cycles_ = 32; }
    },
    {
      0xC8,
      "RET Z",
      [this]() { if (zero_flag())      pc_ = pop_(); else pc_ += 1; cycles_ = 12; }
    },
    {
      0xC9,
      "RET",
      [this]() { pc_ = pop_(); cycles_ = 8; }
    },
    {
      0xCA,
      "JP Z,nn",
      [this]() { pc_ = (zero_flag()) ? nn() : pc_ + 3; cycles_ = 12; }
    },
    {
      0xCB,
      "PREF",
      []() {}
    },
    {
      0xCC,
      "CALL Z,nn",
      [this]() { if (zero_flag())      call_(nn()); else pc_ += 3; cycles_ = 12; }
    },
    {
      0xCD,
      "CALL nn",
      [this]() { call_(nn()); cycles_ = 12; }
    },
    {
      0xCE,
      "ADC A,#",
      [this]() { adc_8(b1(), a()); pc_ += 2; cycles_ = 8; }
    },
    {
      0xCF,
      "RST 08H",
      [this]() { call_(0x0008, 1); cycles_ = 32; }
    },

    { 0xD0,
      "RET NC",
      [this]() { if (not carry_flag()) pc_ = pop_(); else pc_ += 1; cycles_ = 12; }
    },
    {
      0xD1,
      "POP DE",
      [this]() { de(pop_()); pc_ += 1; cycles_ = 12; }
    },
    {
      0xD2,
      "JP NC,nn",
      [this]() { pc_ = (not carry_flag()) ? nn() : pc_ + 3; cycles_ = 12; }
    },
    {
      0xD3,
      "---",
      []() {}
    },
    {
      0xD4,
      "CALL NC,nn",
      [this]() { if (not carry_flag()) call_(nn()); else pc_ += 3; cycles_ = 12; }
    },
    {
      0xD5,
      "PUSH DE",
      [this]() { push_(de()); pc_ += 1; cycles_ = 16; }
    },
    {
      0xD6,
      "SUB #",
      [this]() { sub_8(b1(), a()); pc_ += 2; cycles_ = 8; }
    },
    {
      0xD7,
      "RST 10H",
      [this]() { call_(0x0010, 1); cycles_ = 32; }
    },
    {
      0xD8,
      "RET C",
      [this]() { if (carry_flag())     pc_ = pop_(); else pc_ += 1; cycles_ = 12; }
    },
    {
      0xD9,
      "RETI",
      [this]() { pc_ = pop_(); ime_ = true; cycles_ = 8; }
    },
    {
      0xDA,
      "JP C,nn",
      [this]() { pc_ = (carry_flag()) ? nn() : pc_ + 3; cycles_ = 12; }
    },
    {
      0xDB,
      "---",
      []() {}
    },
    {
      0xDC,
      "CALL C,nn",
      [this]() { if (carry_flag())     call_(nn()); else pc_ += 3; cycles_ = 12; }
    },
    {
      0xDD,
      "---",
      []() {}
    },
    {
      0xDE,
      "SBC A,#",
      [this]() { sbc_8(b1(), a()); pc_ += 2; cycles_ = 4; }
    },
    {
      0xDF,
      "RST 18H",
      [this]() { call_(0x0018, 1); cycles_ = 32; }
    },

    { 0xE0,
      "LD (n),A",
      [this]() { reg_t i = 0; ld_8(a(), i); mm_.write(0xFF00 + b1(), i); pc_ += 2; cycles_ = 12; }
    },
    {
      0xE1,
      "POP HL",
      [this]() { hl(pop_()); pc_ += 1; cycles_ = 12; }
    },
    {
      0xE2,
      "LD (C),A",
      [this]() { reg_t i = 0; ld_8(a(), i); mm_.write(0xFF00 + c(), i); pc_ += 1; cycles_ = 8; }
    },
    {
      0xE3,
      "---",
      []() {}
    },
    {
      0xE4,
      "---",
      []() {}
    },
    {
      0xE5,
      "PUSH HL",
      [this]() { push_(hl()); pc_ += 1; cycles_ = 16; }
    },
    {
      0xE6,
      "AND #",
      [this]() { and_(b1(), a()); pc_ += 2; cycles_ = 8; }
    },
    {
      0xE7,
      "RST 20H",
      [this]() { call_(0x0020, 1); cycles_ = 32; }
    },
    {
      0xE8,
      "ADD SP,#",
      [this]()
      {
        // int arg = b1();
        // int src = sp();
        // int result = src + arg;

        // half_carry_flag(((sp() & 0x00FF) + (arg & 0x00FF)) > 0x00FF);
        // carry_flag(result > 0xFFFF);

        // FIXME SHAMELESS COPY
        wide_reg_t reg = sp();
        int8_t   value = b1();

        int result = static_cast<int>(reg + value);

        zero_flag(false);
        substract_flag(false);
        half_carry_flag(((reg ^ value ^ (result & 0xFFFF)) & 0x10) == 0x10);
        carry_flag(((reg ^ value ^ (result & 0xFFFF)) & 0x100) == 0x100);

        sp(static_cast<wide_reg_t>(result));

        // sp(add_16(b1(), sp()));

        // substract_flag(false);
        // zero_flag(false);
        pc_ += 2;
        cycles_ = 16;
      }
    },
    {
      0xE9,
      "JP (HL)",
      [this]() { pc_ = hl(); cycles_ = 4; }
    },
    {
      0xEA,
      "LD (nn),A",
      [this]() { reg_t i = 0; ld_8(a(), i); mm_.write(nn(), i); pc_ += 3; cycles_ = 8; }
    },
    {
      0xEB,
      "---",
      []() {}
    },
    {
      0xEC,
      "---",
      []() {}
    },
    {
      0xED,
      "---",
      []() {}
    },
    {
      0xEE,
      "XOR #",
      [this]() { xor_(b1(), a()); pc_ += 2; cycles_ = 8; }
    },
    {
      0xEF,
      "RST 28H",
      [this]() { call_(0x0028, 1); cycles_ = 32; }
    },
    { 0xF0,
      "LD A,(n)",
      [this]() { ld_8(mm_.read(0xFF00 + b1()), a()); pc_ += 2; cycles_ = 12; }
    },
    {
      0xF1,
      "POP AF",
      [this]() { af(pop_()); pc_ += 1; cycles_ = 12; }
    },
    {
      0xF2,
      "LD A,(C)",
      [this]() { ld_8(mm_.read(0xFF00 + c()), a()); pc_ += 1; cycles_ = 8; }
    },
    {
      0xF3,
      "DI",
      [this]() { ime_ = false; pc_ += 1; cycles_ = 4; }
    },
    {
      0xF4,
      "---",
      []() {}
    },
    {
      0xF5,
      "PUSH AF",
      [this]() { push_(af()); pc_ += 1; cycles_ = 16; }
    },
    {
      0xF6,
      "OR #",
      [this]() { or_(b1(), a()); pc_ += 2; cycles_ = 8; }
    },
    {
      0xF7,
      "RST 30H",
      [this]() { call_(0x0030, 1); cycles_ = 32; }
    },
    {
      0xF8,
      "LD HL,SP+n",
      [this]()
      {
        // FIXME SHAMELESS COPY
        // ld_hl_spn_(b1());
        wide_reg_t reg = sp();
        int8_t   value = b1();

        int result = static_cast<int>(reg + value);

        zero_flag(false);
        substract_flag(false);
        half_carry_flag(((reg ^ value ^ (result & 0xFFFF)) & 0x10) == 0x10);
        carry_flag(((reg ^ value ^ (result & 0xFFFF)) & 0x100) == 0x100);

        hl(static_cast<wide_reg_t>(result));
        pc_ += 2;
        cycles_ = 12;
      }
    },
    {
      0xF9,
      "LD SP,HL",
      [this]() { sp(hl()); pc_ += 1; cycles_ = 8; }
    },
    {
      0xFA,
      "LD A,(nn)",
      [this]() { ld_8(mm_.read(nn()), a()); pc_ += 3; cycles_ = 16; }
    },
    {
      0xFB,
      "EI",
      [this]() { ime_ = true; pc_ += 1; cycles_ = 4; }
    },
    {
      0xFC,
      "---",
      []() {}
    },
    {
      0xFD,
      "---",
      []() {}
    },
    {
      0xFE,
      "CP #",
      [this]() { cp_(b1()); pc_ += 2; cycles_ = 8; }
    },
    {
      0xFF,
      "RST 38H",
      [this]() { call_(0x0038, 1); cycles_ = 32; }
    }
  }};

  std::map<reg_t, std::function<void()>> pref_ {
    { 0x00, /* RLC B   */ [this]() { rlc_(b(), true); pc_ += 2; cycles_ = 8; } },
    { 0x01, /* RLC C   */ [this]() { rlc_(c(), true); pc_ += 2; cycles_ = 8; } },
    { 0x02, /* RLC D   */ [this]() { rlc_(d(), true); pc_ += 2; cycles_ = 8; } },
    { 0x03, /* RLC E   */ [this]() { rlc_(e(), true); pc_ += 2; cycles_ = 8; } },
    { 0x04, /* RLC H   */ [this]() { rlc_(h(), true); pc_ += 2; cycles_ = 8; } },
    { 0x05, /* RLC L   */ [this]() { rlc_(l(), true); pc_ += 2; cycles_ = 8; } },
    { 0x06, /* RLC (HL)*/ [this]() { reg_t i = mm_.read(hl()); rlc_(i, true); mm_.write(hl(), i); pc_ += 2; cycles_ = 16; } },
    { 0x07, /* RLC A   */ [this]() { rlc_(a(), true); pc_ += 2; cycles_ = 8; } },

    { 0x08, /* RRC B   */ [this]() { rrc_(b(), true); pc_ += 2; cycles_ = 8; } },
    { 0x09, /* RRC C   */ [this]() { rrc_(c(), true); pc_ += 2; cycles_ = 8; } },
    { 0x0A, /* RRC D   */ [this]() { rrc_(d(), true); pc_ += 2; cycles_ = 8; } },
    { 0x0B, /* RRC E   */ [this]() { rrc_(e(), true); pc_ += 2; cycles_ = 8; } },
    { 0x0C, /* RRC H   */ [this]() { rrc_(h(), true); pc_ += 2; cycles_ = 8; } },
    { 0x0D, /* RRC L   */ [this]() { rrc_(l(), true); pc_ += 2; cycles_ = 8; } },
    { 0x0E, /* RRC (HL)*/ [this]() { reg_t i = mm_.read(hl()); rrc_(i, true); mm_.write(hl(), i); pc_ += 2; cycles_ = 16; } },
    { 0x0F, /* RRC A   */ [this]() { rrc_(a(), true); pc_ += 2; cycles_ = 8; } },

    { 0x10, /* RL B    */ [this]() { rl_(b(), true); pc_ += 2; cycles_ = 8; } },
    { 0x11, /* RL C    */ [this]() { rl_(c(), true); pc_ += 2; cycles_ = 8; } },
    { 0x12, /* RL D    */ [this]() { rl_(d(), true); pc_ += 2; cycles_ = 8; } },
    { 0x13, /* RL E    */ [this]() { rl_(e(), true); pc_ += 2; cycles_ = 8; } },
    { 0x14, /* RL H    */ [this]() { rl_(h(), true); pc_ += 2; cycles_ = 8; } },
    { 0x15, /* RL L    */ [this]() { rl_(l(), true); pc_ += 2; cycles_ = 8; } },
    { 0x16, /* RL (HL) */ [this]() { reg_t i = mm_.read(hl()); rl_(i, true); mm_.write(hl(), i); pc_ += 2; cycles_ = 16; } },
    { 0x17, /* RL A    */ [this]() { rl_(a(), true); pc_ += 2; cycles_ = 8; } },

    { 0x18, /* RR B    */ [this]() { rr_(b(), true); pc_ += 2; cycles_ = 8; } },
    { 0x19, /* RR C    */ [this]() { rr_(c(), true); pc_ += 2; cycles_ = 8; } },
    { 0x1A, /* RR D    */ [this]() { rr_(d(), true); pc_ += 2; cycles_ = 8; } },
    { 0x1B, /* RR E    */ [this]() { rr_(e(), true); pc_ += 2; cycles_ = 8; } },
    { 0x1C, /* RR H    */ [this]() { rr_(h(), true); pc_ += 2; cycles_ = 8; } },
    { 0x1D, /* RR L    */ [this]() { rr_(l(), true); pc_ += 2; cycles_ = 8; } },
    { 0x1E, /* RR (HL) */ [this]() { reg_t i = mm_.read(hl()); rr_(i, true); mm_.write(hl(), i); pc_ += 2; cycles_ = 16; } },
    { 0x1F, /* RR A    */ [this]() { rr_(a(), true); pc_ += 2; cycles_ = 8; } },

    { 0x20, /* SLA B    */ [this]() { sla_(b()); pc_ += 2; cycles_ = 8; } },
    { 0x21, /* SLA C    */ [this]() { sla_(c()); pc_ += 2; cycles_ = 8; } },
    { 0x22, /* SLA D    */ [this]() { sla_(d()); pc_ += 2; cycles_ = 8; } },
    { 0x23, /* SLA E    */ [this]() { sla_(e()); pc_ += 2; cycles_ = 8; } },
    { 0x24, /* SLA H    */ [this]() { sla_(h()); pc_ += 2; cycles_ = 8; } },
    { 0x25, /* SLA L    */ [this]() { sla_(l()); pc_ += 2; cycles_ = 8; } },
    { 0x26, /* SLA (HL) */ [this]() { reg_t i = mm_.read(hl()); sla_(i); mm_.write(hl(), i); pc_ += 2; cycles_ = 16; } },
    { 0x27, /* SLA A    */ [this]() { sla_(a()); pc_ += 2; cycles_ = 8; } },

    { 0x28, /* SRA B    */ [this]() { sra_(b()); pc_ += 2; cycles_ = 8; } },
    { 0x29, /* SRA C    */ [this]() { sra_(c()); pc_ += 2; cycles_ = 8; } },
    { 0x2A, /* SRA D    */ [this]() { sra_(d()); pc_ += 2; cycles_ = 8; } },
    { 0x2B, /* SRA E    */ [this]() { sra_(e()); pc_ += 2; cycles_ = 8; } },
    { 0x2C, /* SRA H    */ [this]() { sra_(h()); pc_ += 2; cycles_ = 8; } },
    { 0x2D, /* SRA L    */ [this]() { sra_(l()); pc_ += 2; cycles_ = 8; } },
    { 0x2E, /* SRA (HL) */ [this]() { reg_t i = mm_.read(hl()); sra_(i); mm_.write(hl(), i); pc_ += 2; cycles_ = 16; } },
    { 0x2F, /* SRA A    */ [this]() { sra_(a()); pc_ += 2; cycles_ = 8; } },

    { 0x30, /* SWAP B  */ [this]() { swap_(b()); pc_ += 2; cycles_ = 8; } },
    { 0x31, /* SWAP C  */ [this]() { swap_(c()); pc_ += 2; cycles_ = 8; } },
    { 0x32, /* SWAP D  */ [this]() { swap_(d()); pc_ += 2; cycles_ = 8; } },
    { 0x33, /* SWAP E  */ [this]() { swap_(e()); pc_ += 2; cycles_ = 8; } },
    { 0x34, /* SWAP H  */ [this]() { swap_(h()); pc_ += 2; cycles_ = 8; } },
    { 0x35, /* SWAP L  */ [this]() { swap_(l()); pc_ += 2; cycles_ = 8; } },
    { 0x36, /* SWAP(HL)*/ [this]() { reg_t i = mm_.read(hl()); swap_(i); mm_.write(hl(), i); pc_ += 2; cycles_ = 16; } },
    { 0x37, /* SWAP A  */ [this]() { swap_(a()); pc_ += 2; cycles_ = 8; } },

    { 0x38, /* SRL B    */ [this]() { srl_(b()); pc_ += 2; cycles_ = 8; } },
    { 0x39, /* SRL C    */ [this]() { srl_(c()); pc_ += 2; cycles_ = 8; } },
    { 0x3A, /* SRL D    */ [this]() { srl_(d()); pc_ += 2; cycles_ = 8; } },
    { 0x3B, /* SRL E    */ [this]() { srl_(e()); pc_ += 2; cycles_ = 8; } },
    { 0x3C, /* SRL H    */ [this]() { srl_(h()); pc_ += 2; cycles_ = 8; } },
    { 0x3D, /* SRL L    */ [this]() { srl_(l()); pc_ += 2; cycles_ = 8; } },
    { 0x3E, /* SRL (HL) */ [this]() { reg_t i = mm_.read(hl()); srl_(i); mm_.write(hl(), i); pc_ += 2; cycles_ = 16; } },
    { 0x3F, /* SRL A    */ [this]() { srl_(a()); pc_ += 2; cycles_ = 8; } },

    { 0x40, /* BIT 0,B    */ [this]() { bit_(b(), 0); pc_ += 2; cycles_ = 8; } },
    { 0x41, /* BIT 0,C    */ [this]() { bit_(c(), 0); pc_ += 2; cycles_ = 8; } },
    { 0x42, /* BIT 0,D    */ [this]() { bit_(d(), 0); pc_ += 2; cycles_ = 8; } },
    { 0x43, /* BIT 0,E    */ [this]() { bit_(e(), 0); pc_ += 2; cycles_ = 8; } },
    { 0x44, /* BIT 0,H    */ [this]() { bit_(h(), 0); pc_ += 2; cycles_ = 8; } },
    { 0x45, /* BIT 0,L    */ [this]() { bit_(l(), 0); pc_ += 2; cycles_ = 8; } },
    { 0x46, /* BIT 0,(HL) */ [this]() { reg_t i = mm_.read(hl()); bit_(i, 0); mm_.write(hl(), i); pc_ += 2; cycles_ = 16; } },
    { 0x47, /* BIT 0,A    */ [this]() { bit_(a(), 0); pc_ += 2; cycles_ = 8; } },

    { 0x48, /* BIT 1,B    */ [this]() { bit_(b(), 1); pc_ += 2; cycles_ = 8; } },
    { 0x49, /* BIT 1,C    */ [this]() { bit_(c(), 1); pc_ += 2; cycles_ = 8; } },
    { 0x4A, /* BIT 1,D    */ [this]() { bit_(d(), 1); pc_ += 2; cycles_ = 8; } },
    { 0x4B, /* BIT 1,E    */ [this]() { bit_(e(), 1); pc_ += 2; cycles_ = 8; } },
    { 0x4C, /* BIT 1,H    */ [this]() { bit_(h(), 1); pc_ += 2; cycles_ = 8; } },
    { 0x4D, /* BIT 1,L    */ [this]() { bit_(l(), 1); pc_ += 2; cycles_ = 8; } },
    { 0x4E, /* BIT 1,(HL) */ [this]() { reg_t i = mm_.read(hl()); bit_(i, 1); mm_.write(hl(), i); pc_ += 2; cycles_ = 16; } },
    { 0x4F, /* BIT 1,A    */ [this]() { bit_(a(), 1); pc_ += 2; cycles_ = 8; } },

    { 0x50, /* BIT 2,B    */ [this]() { bit_(b(), 2); pc_ += 2; cycles_ = 8; } },
    { 0x51, /* BIT 2,C    */ [this]() { bit_(c(), 2); pc_ += 2; cycles_ = 8; } },
    { 0x52, /* BIT 2,D    */ [this]() { bit_(d(), 2); pc_ += 2; cycles_ = 8; } },
    { 0x53, /* BIT 2,E    */ [this]() { bit_(e(), 2); pc_ += 2; cycles_ = 8; } },
    { 0x54, /* BIT 2,H    */ [this]() { bit_(h(), 2); pc_ += 2; cycles_ = 8; } },
    { 0x55, /* BIT 2,L    */ [this]() { bit_(l(), 2); pc_ += 2; cycles_ = 8; } },
    { 0x56, /* BIT 2,(HL) */ [this]() { reg_t i = mm_.read(hl()); bit_(i, 2); mm_.write(hl(), i); pc_ += 2; cycles_ = 16; } },
    { 0x57, /* BIT 2,A    */ [this]() { bit_(a(), 2); pc_ += 2; cycles_ = 8; } },

    { 0x58, /* BIT 3,B    */ [this]() { bit_(b(), 3); pc_ += 2; cycles_ = 8; } },
    { 0x59, /* BIT 3,C    */ [this]() { bit_(c(), 3); pc_ += 2; cycles_ = 8; } },
    { 0x5A, /* BIT 3,D    */ [this]() { bit_(d(), 3); pc_ += 2; cycles_ = 8; } },
    { 0x5B, /* BIT 3,E    */ [this]() { bit_(e(), 3); pc_ += 2; cycles_ = 8; } },
    { 0x5C, /* BIT 3,H    */ [this]() { bit_(h(), 3); pc_ += 2; cycles_ = 8; } },
    { 0x5D, /* BIT 3,L    */ [this]() { bit_(l(), 3); pc_ += 2; cycles_ = 8; } },
    { 0x5E, /* BIT 3,(HL) */ [this]() { bit_(mm_.read(hl()), 3); pc_ += 2; cycles_ = 16; } },
    { 0x5F, /* BIT 3,A    */ [this]() { bit_(a(), 3); pc_ += 2; cycles_ = 8; } },

    { 0x60, /* BIT 4,B    */ [this]() { bit_(b(), 4); pc_ += 2; cycles_ = 8; } },
    { 0x61, /* BIT 4,C    */ [this]() { bit_(c(), 4); pc_ += 2; cycles_ = 8; } },
    { 0x62, /* BIT 4,D    */ [this]() { bit_(d(), 4); pc_ += 2; cycles_ = 8; } },
    { 0x63, /* BIT 4,E    */ [this]() { bit_(e(), 4); pc_ += 2; cycles_ = 8; } },
    { 0x64, /* BIT 4,H    */ [this]() { bit_(h(), 4); pc_ += 2; cycles_ = 8; } },
    { 0x65, /* BIT 4,L    */ [this]() { bit_(l(), 4); pc_ += 2; cycles_ = 8; } },
    { 0x66, /* BIT 4,(HL) */ [this]() { bit_(mm_.read(hl()), 4); pc_ += 2; cycles_ = 16; } },
    { 0x67, /* BIT 4,A    */ [this]() { bit_(a(), 4); pc_ += 2; cycles_ = 8; } },

    { 0x68, /* BIT 5,B    */ [this]() { bit_(b(), 5); pc_ += 2; cycles_ = 8; } },
    { 0x69, /* BIT 5,C    */ [this]() { bit_(c(), 5); pc_ += 2; cycles_ = 8; } },
    { 0x6A, /* BIT 5,D    */ [this]() { bit_(d(), 5); pc_ += 2; cycles_ = 8; } },
    { 0x6B, /* BIT 5,E    */ [this]() { bit_(e(), 5); pc_ += 2; cycles_ = 8; } },
    { 0x6C, /* BIT 5,H    */ [this]() { bit_(h(), 5); pc_ += 2; cycles_ = 8; } },
    { 0x6D, /* BIT 5,L    */ [this]() { bit_(l(), 5); pc_ += 2; cycles_ = 8; } },
    { 0x6E, /* BIT 5,(HL) */ [this]() { bit_(mm_.read(hl()), 5); pc_ += 2; cycles_ = 16; } },
    { 0x6F, /* BIT 5,A    */ [this]() { bit_(a(), 5); pc_ += 2; cycles_ = 8; } },

    { 0x70, /* BIT 6,B    */ [this]() { bit_(b(), 6); pc_ += 2; cycles_ = 8; } },
    { 0x71, /* BIT 6,C    */ [this]() { bit_(c(), 6); pc_ += 2; cycles_ = 8; } },
    { 0x72, /* BIT 6,D    */ [this]() { bit_(d(), 6); pc_ += 2; cycles_ = 8; } },
    { 0x73, /* BIT 6,E    */ [this]() { bit_(e(), 6); pc_ += 2; cycles_ = 8; } },
    { 0x74, /* BIT 6,H    */ [this]() { bit_(h(), 6); pc_ += 2; cycles_ = 8; } },
    { 0x75, /* BIT 6,L    */ [this]() { bit_(l(), 6); pc_ += 2; cycles_ = 8; } },
    { 0x76, /* BIT 6,(HL) */ [this]() { bit_(mm_.read(hl()), 6); pc_ += 2; cycles_ = 16; } },
    { 0x77, /* BIT 6,A    */ [this]() { bit_(a(), 6); pc_ += 2; cycles_ = 8; } },

    { 0x78, /* BIT 7,B    */ [this]() { bit_(b(), 7); pc_ += 2; cycles_ = 8; } },
    { 0x79, /* BIT 7,C    */ [this]() { bit_(c(), 7); pc_ += 2; cycles_ = 8; } },
    { 0x7A, /* BIT 7,D    */ [this]() { bit_(d(), 7); pc_ += 2; cycles_ = 8; } },
    { 0x7B, /* BIT 7,E    */ [this]() { bit_(e(), 7); pc_ += 2; cycles_ = 8; } },
    { 0x7C, /* BIT 7,H    */ [this]() { bit_(h(), 7); pc_ += 2; cycles_ = 8; } },
    { 0x7D, /* BIT 7,L    */ [this]() { bit_(l(), 7); pc_ += 2; cycles_ = 8; } },
    { 0x7E, /* BIT 7,(HL) */ [this]() { bit_(mm_.read(hl()), 7); pc_ += 2; cycles_ = 16; } },
    { 0x7F, /* BIT 7,A    */ [this]() { bit_(a(), 7); pc_ += 2; cycles_ = 8; } },

    { 0x80, /* RES 0,B    */ [this]() { res_(b(), 0); pc_ += 2; cycles_ = 8; } },
    { 0x81, /* RES 0,C    */ [this]() { res_(c(), 0); pc_ += 2; cycles_ = 8; } },
    { 0x82, /* RES 0,D    */ [this]() { res_(d(), 0); pc_ += 2; cycles_ = 8; } },
    { 0x83, /* RES 0,E    */ [this]() { res_(e(), 0); pc_ += 2; cycles_ = 8; } },
    { 0x84, /* RES 0,H    */ [this]() { res_(h(), 0); pc_ += 2; cycles_ = 8; } },
    { 0x85, /* RES 0,L    */ [this]() { res_(l(), 0); pc_ += 2; cycles_ = 8; } },
    { 0x86, /* RES 0,(HL) */ [this]() { reg_t i = mm_.read(hl()); res_(i, 0); mm_.write(hl(), i); pc_ += 2; cycles_ = 16; } },
    { 0x87, /* RES 0,A    */ [this]() { res_(a(), 0); pc_ += 2; cycles_ = 8; } },

    { 0x88, /* RES 1,B    */ [this]() { res_(b(), 1); pc_ += 2; cycles_ = 8; } },
    { 0x89, /* RES 1,C    */ [this]() { res_(c(), 1); pc_ += 2; cycles_ = 8; } },
    { 0x8A, /* RES 1,D    */ [this]() { res_(d(), 1); pc_ += 2; cycles_ = 8; } },
    { 0x8B, /* RES 1,E    */ [this]() { res_(e(), 1); pc_ += 2; cycles_ = 8; } },
    { 0x8C, /* RES 1,H    */ [this]() { res_(h(), 1); pc_ += 2; cycles_ = 8; } },
    { 0x8D, /* RES 1,L    */ [this]() { res_(l(), 1); pc_ += 2; cycles_ = 8; } },
    { 0x8E, /* RES 1,(HL) */ [this]() { reg_t i = mm_.read(hl()); res_(i, 1); mm_.write(hl(), i); pc_ += 2; cycles_ = 16; } },
    { 0x8F, /* RES 1,A    */ [this]() { res_(a(), 1); pc_ += 2; cycles_ = 8; } },

    { 0x90, /* RES 2,B    */ [this]() { res_(b(), 2); pc_ += 2; cycles_ = 8; } },
    { 0x91, /* RES 2,C    */ [this]() { res_(c(), 2); pc_ += 2; cycles_ = 8; } },
    { 0x92, /* RES 2,D    */ [this]() { res_(d(), 2); pc_ += 2; cycles_ = 8; } },
    { 0x93, /* RES 2,E    */ [this]() { res_(e(), 2); pc_ += 2; cycles_ = 8; } },
    { 0x94, /* RES 2,H    */ [this]() { res_(h(), 2); pc_ += 2; cycles_ = 8; } },
    { 0x95, /* RES 2,L    */ [this]() { res_(l(), 2); pc_ += 2; cycles_ = 8; } },
    { 0x96, /* RES 2,(HL) */ [this]() { reg_t i = mm_.read(hl()); res_(i, 2); mm_.write(hl(), i); pc_ += 2; cycles_ = 16; } },
    { 0x97, /* RES 2,A    */ [this]() { res_(a(), 2); pc_ += 2; cycles_ = 8; } },

    { 0x98, /* RES 3,B    */ [this]() { res_(b(), 3); pc_ += 2; cycles_ = 8; } },
    { 0x99, /* RES 3,C    */ [this]() { res_(c(), 3); pc_ += 2; cycles_ = 8; } },
    { 0x9A, /* RES 3,D    */ [this]() { res_(d(), 3); pc_ += 2; cycles_ = 8; } },
    { 0x9B, /* RES 3,E    */ [this]() { res_(e(), 3); pc_ += 2; cycles_ = 8; } },
    { 0x9C, /* RES 3,H    */ [this]() { res_(h(), 3); pc_ += 2; cycles_ = 8; } },
    { 0x9D, /* RES 3,L    */ [this]() { res_(l(), 3); pc_ += 2; cycles_ = 8; } },
    { 0x9E, /* RES 3,(HL) */ [this]() { reg_t i = mm_.read(hl()); res_(i, 3); mm_.write(hl(), i); pc_ += 2; cycles_ = 16; } },
    { 0x9F, /* RES 3,A    */ [this]() { res_(a(), 3); pc_ += 2; cycles_ = 8; } },

    { 0xA0, /* RES 4,B    */ [this]() { res_(b(), 4); pc_ += 2; cycles_ = 8; } },
    { 0xA1, /* RES 4,C    */ [this]() { res_(c(), 4); pc_ += 2; cycles_ = 8; } },
    { 0xA2, /* RES 4,D    */ [this]() { res_(d(), 4); pc_ += 2; cycles_ = 8; } },
    { 0xA3, /* RES 4,E    */ [this]() { res_(e(), 4); pc_ += 2; cycles_ = 8; } },
    { 0xA4, /* RES 4,H    */ [this]() { res_(h(), 4); pc_ += 2; cycles_ = 8; } },
    { 0xA5, /* RES 4,L    */ [this]() { res_(l(), 4); pc_ += 2; cycles_ = 8; } },
    { 0xA6, /* RES 4,(HL) */ [this]() { reg_t i = mm_.read(hl()); res_(i, 4); mm_.write(hl(), i); pc_ += 2; cycles_ = 16; } },
    { 0xA7, /* RES 4,A    */ [this]() { res_(a(), 4); pc_ += 2; cycles_ = 8; } },

    { 0xA8, /* RES 5,B    */ [this]() { res_(b(), 5); pc_ += 2; cycles_ = 8; } },
    { 0xA9, /* RES 5,C    */ [this]() { res_(c(), 5); pc_ += 2; cycles_ = 8; } },
    { 0xAA, /* RES 5,D    */ [this]() { res_(d(), 5); pc_ += 2; cycles_ = 8; } },
    { 0xAB, /* RES 5,E    */ [this]() { res_(e(), 5); pc_ += 2; cycles_ = 8; } },
    { 0xAC, /* RES 5,H    */ [this]() { res_(h(), 5); pc_ += 2; cycles_ = 8; } },
    { 0xAD, /* RES 5,L    */ [this]() { res_(l(), 5); pc_ += 2; cycles_ = 8; } },
    { 0xAE, /* RES 5,(HL) */ [this]() { reg_t i = mm_.read(hl()); res_(i, 5); mm_.write(hl(), i); pc_ += 2; cycles_ = 16; } },
    { 0xAF, /* RES 5,A    */ [this]() { res_(a(), 5); pc_ += 2; cycles_ = 8; } },

    { 0xB0, /* RES 6,B    */ [this]() { res_(b(), 6); pc_ += 2; cycles_ = 8; } },
    { 0xB1, /* RES 6,C    */ [this]() { res_(c(), 6); pc_ += 2; cycles_ = 8; } },
    { 0xB2, /* RES 6,D    */ [this]() { res_(d(), 6); pc_ += 2; cycles_ = 8; } },
    { 0xB3, /* RES 6,E    */ [this]() { res_(e(), 6); pc_ += 2; cycles_ = 8; } },
    { 0xB4, /* RES 6,H    */ [this]() { res_(h(), 6); pc_ += 2; cycles_ = 8; } },
    { 0xB5, /* RES 6,L    */ [this]() { res_(l(), 6); pc_ += 2; cycles_ = 8; } },
    { 0xB6, /* RES 6,(HL) */ [this]() { reg_t i = mm_.read(hl()); res_(i, 6); mm_.write(hl(), i); pc_ += 2; cycles_ = 16; } },
    { 0xB7, /* RES 6,A    */ [this]() { res_(a(), 6); pc_ += 2; cycles_ = 8; } },

    { 0xB8, /* RES 7,B    */ [this]() { res_(b(), 7); pc_ += 2; cycles_ = 8; } },
    { 0xB9, /* RES 7,C    */ [this]() { res_(c(), 7); pc_ += 2; cycles_ = 8; } },
    { 0xBA, /* RES 7,D    */ [this]() { res_(d(), 7); pc_ += 2; cycles_ = 8; } },
    { 0xBB, /* RES 7,E    */ [this]() { res_(e(), 7); pc_ += 2; cycles_ = 8; } },
    { 0xBC, /* RES 7,H    */ [this]() { res_(h(), 7); pc_ += 2; cycles_ = 8; } },
    { 0xBD, /* RES 7,L    */ [this]() { res_(l(), 7); pc_ += 2; cycles_ = 8; } },
    { 0xBE, /* RES 7,(HL) */ [this]() { reg_t i = mm_.read(hl()); res_(i, 7); mm_.write(hl(), i); pc_ += 2; cycles_ = 16; } },
    { 0xBF, /* RES 7,A    */ [this]() { res_(a(), 7); pc_ += 2; cycles_ = 8; } },

    { 0xC0, /* SET 0,B    */ [this]() { set_(b(), 0); pc_ += 2; cycles_ = 8; } },
    { 0xC1, /* SET 0,C    */ [this]() { set_(c(), 0); pc_ += 2; cycles_ = 8; } },
    { 0xC2, /* SET 0,D    */ [this]() { set_(d(), 0); pc_ += 2; cycles_ = 8; } },
    { 0xC3, /* SET 0,E    */ [this]() { set_(e(), 0); pc_ += 2; cycles_ = 8; } },
    { 0xC4, /* SET 0,H    */ [this]() { set_(h(), 0); pc_ += 2; cycles_ = 8; } },
    { 0xC5, /* SET 0,L    */ [this]() { set_(l(), 0); pc_ += 2; cycles_ = 8; } },
    { 0xC6, /* SET 0,(HL) */ [this]() { reg_t i = mm_.read(hl()); set_(i, 0); mm_.write(hl(), i); pc_ += 2; cycles_ = 16; } },
    { 0xC7, /* SET 0,A    */ [this]() { set_(a(), 0); pc_ += 2; cycles_ = 8; } },

    { 0xC8, /* SET 1,B    */ [this]() { set_(b(), 1); pc_ += 2; cycles_ = 8; } },
    { 0xC9, /* SET 1,C    */ [this]() { set_(c(), 1); pc_ += 2; cycles_ = 8; } },
    { 0xCA, /* SET 1,D    */ [this]() { set_(d(), 1); pc_ += 2; cycles_ = 8; } },
    { 0xCB, /* SET 1,E    */ [this]() { set_(e(), 1); pc_ += 2; cycles_ = 8; } },
    { 0xCC, /* SET 1,H    */ [this]() { set_(h(), 1); pc_ += 2; cycles_ = 8; } },
    { 0xCD, /* SET 1,L    */ [this]() { set_(l(), 1); pc_ += 2; cycles_ = 8; } },
    { 0xCE, /* SET 1,(HL) */ [this]() { reg_t i = mm_.read(hl()); set_(i, 1); mm_.write(hl(), i); pc_ += 2; cycles_ = 16; } },
    { 0xCF, /* SET 1,A    */ [this]() { set_(a(), 1); pc_ += 2; cycles_ = 8; } },

    { 0xD0, /* SET 2,B    */ [this]() { set_(b(), 2); pc_ += 2; cycles_ = 8; } },
    { 0xD1, /* SET 2,C    */ [this]() { set_(c(), 2); pc_ += 2; cycles_ = 8; } },
    { 0xD2, /* SET 2,D    */ [this]() { set_(d(), 2); pc_ += 2; cycles_ = 8; } },
    { 0xD3, /* SET 2,E    */ [this]() { set_(e(), 2); pc_ += 2; cycles_ = 8; } },
    { 0xD4, /* SET 2,H    */ [this]() { set_(h(), 2); pc_ += 2; cycles_ = 8; } },
    { 0xD5, /* SET 2,L    */ [this]() { set_(l(), 2); pc_ += 2; cycles_ = 8; } },
    { 0xD6, /* SET 2,(HL) */ [this]() { reg_t i = mm_.read(hl()); set_(i, 2); mm_.write(hl(), i); pc_ += 2; cycles_ = 16; } },
    { 0xD7, /* SET 2,A    */ [this]() { set_(a(), 2); pc_ += 2; cycles_ = 8; } },

    { 0xD8, /* SET 3,B    */ [this]() { set_(b(), 3); pc_ += 2; cycles_ = 8; } },
    { 0xD9, /* SET 3,C    */ [this]() { set_(c(), 3); pc_ += 2; cycles_ = 8; } },
    { 0xDA, /* SET 3,D    */ [this]() { set_(d(), 3); pc_ += 2; cycles_ = 8; } },
    { 0xDB, /* SET 3,E    */ [this]() { set_(e(), 3); pc_ += 2; cycles_ = 8; } },
    { 0xDC, /* SET 3,H    */ [this]() { set_(h(), 3); pc_ += 2; cycles_ = 8; } },
    { 0xDD, /* SET 3,L    */ [this]() { set_(l(), 3); pc_ += 2; cycles_ = 8; } },
    { 0xDE, /* SET 3,(HL) */ [this]() { reg_t i = mm_.read(hl()); set_(i, 3); mm_.write(hl(), i); pc_ += 2; cycles_ = 16; } },
    { 0xDF, /* SET 3,A    */ [this]() { set_(a(), 3); pc_ += 2; cycles_ = 8; } },

    { 0xE0, /* SET 4,B    */ [this]() { set_(b(), 4); pc_ += 2; cycles_ = 8; } },
    { 0xE1, /* SET 4,C    */ [this]() { set_(c(), 4); pc_ += 2; cycles_ = 8; } },
    { 0xE2, /* SET 4,D    */ [this]() { set_(d(), 4); pc_ += 2; cycles_ = 8; } },
    { 0xE3, /* SET 4,E    */ [this]() { set_(e(), 4); pc_ += 2; cycles_ = 8; } },
    { 0xE4, /* SET 4,H    */ [this]() { set_(h(), 4); pc_ += 2; cycles_ = 8; } },
    { 0xE5, /* SET 4,L    */ [this]() { set_(l(), 4); pc_ += 2; cycles_ = 8; } },
    { 0xE6, /* SET 4,(HL) */ [this]() { reg_t i = mm_.read(hl()); set_(i, 4); mm_.write(hl(), i); pc_ += 2; cycles_ = 16; } },
    { 0xE7, /* SET 4,A    */ [this]() { set_(a(), 4); pc_ += 2; cycles_ = 8; } },

    { 0xE8, /* SET 5,B    */ [this]() { set_(b(), 5); pc_ += 2; cycles_ = 8; } },
    { 0xE9, /* SET 5,C    */ [this]() { set_(c(), 5); pc_ += 2; cycles_ = 8; } },
    { 0xEA, /* SET 5,D    */ [this]() { set_(d(), 5); pc_ += 2; cycles_ = 8; } },
    { 0xEB, /* SET 5,E    */ [this]() { set_(e(), 5); pc_ += 2; cycles_ = 8; } },
    { 0xEC, /* SET 5,H    */ [this]() { set_(h(), 5); pc_ += 2; cycles_ = 8; } },
    { 0xED, /* SET 5,L    */ [this]() { set_(l(), 5); pc_ += 2; cycles_ = 8; } },
    { 0xEE, /* SET 5,(HL) */ [this]() { reg_t i = mm_.read(hl()); set_(i, 5); mm_.write(hl(), i); pc_ += 2; cycles_ = 16; } },
    { 0xEF, /* SET 5,A    */ [this]() { set_(a(), 5); pc_ += 2; cycles_ = 8; } },

    { 0xF0, /* SET 6,B    */ [this]() { set_(b(), 6); pc_ += 2; cycles_ = 8; } },
    { 0xF1, /* SET 6,C    */ [this]() { set_(c(), 6); pc_ += 2; cycles_ = 8; } },
    { 0xF2, /* SET 6,D    */ [this]() { set_(d(), 6); pc_ += 2; cycles_ = 8; } },
    { 0xF3, /* SET 6,E    */ [this]() { set_(e(), 6); pc_ += 2; cycles_ = 8; } },
    { 0xF4, /* SET 6,H    */ [this]() { set_(h(), 6); pc_ += 2; cycles_ = 8; } },
    { 0xF5, /* SET 6,L    */ [this]() { set_(l(), 6); pc_ += 2; cycles_ = 8; } },
    { 0xF6, /* SET 6,(HL) */ [this]() { reg_t i = mm_.read(hl()); set_(i, 6); mm_.write(hl(), i); pc_ += 2; cycles_ = 16; } },
    { 0xF7, /* SET 6,A    */ [this]() { set_(a(), 6); pc_ += 2; cycles_ = 8; } },

    { 0xF8, /* SET 7,B    */ [this]() { set_(b(), 7); pc_ += 2; cycles_ = 8; } },
    { 0xF9, /* SET 7,C    */ [this]() { set_(c(), 7); pc_ += 2; cycles_ = 8; } },
    { 0xFA, /* SET 7,D    */ [this]() { set_(d(), 7); pc_ += 2; cycles_ = 8; } },
    { 0xFB, /* SET 7,E    */ [this]() { set_(e(), 7); pc_ += 2; cycles_ = 8; } },
    { 0xFC, /* SET 7,H    */ [this]() { set_(h(), 7); pc_ += 2; cycles_ = 8; } },
    { 0xFD, /* SET 7,L    */ [this]() { set_(l(), 7); pc_ += 2; cycles_ = 8; } },
    { 0xFE, /* SET 7,(HL) */ [this]() { reg_t i = mm_.read(hl()); set_(i, 7); mm_.write(hl(), i); pc_ += 2; cycles_ = 16; } },
    { 0xFF, /* SET 7,A    */ [this]() { set_(a(), 7); pc_ += 2; cycles_ = 8; } },
  };
};
