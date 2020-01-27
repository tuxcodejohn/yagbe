#pragma once

#include "types.h"
#include "mm.hpp"

#include <map>
#include <map>
#include <string>
#include <functional>

class CP
{

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
		sp_     = 0xffff;
		pc_     = 0x0100;

		mm_.write(0xff0f, 0x00); // interrupt flag
		mm_.write(0xffff, 0xff); // interrupt enable
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

	void af(wide_reg_t value) { return wide_(a(), f(), value & 0xfff0); }
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
				"pc:%04x sp:%04x op:%02x,%02x,%02x af:%02x%02x bc:%02x%02x de:%02x%02x hl:%02x%02x %c%c%c%c lcdc:%02x \n",
				pc_, sp_, mm_.read(pc_), mm_.read(pc_+1), mm_.read(pc_+2), a_, f_, b_, c_, d_, e_, h_, l_,
				(zero_flag() ? 'z' : '_'),
				(substract_flag() ? 's' : '_'),
				(half_carry_flag() ? 'h' : '_'),
				(carry_flag() ? 'c' : '_'),
				mm_.read(0xff40));
	}

private:
	void process_interrupt_()
	{
		if (not ime_)
			return;

		auto fn_is_enabled = [this] (reg_t val) -> bool {
			return (mm_.read(0xffff) & val) and (mm_.read(0xff0f) & val);
		};

		if (fn_is_enabled(0x01)) { // vblank
			// printf("int: vblank\n");
			process_interrupt_(0x0040);
			mm_.write(0xff0f, mm_.read(0xff0f) ^ 0x01);
			return;
		}

		if (fn_is_enabled(0x02)) { // lcdc
			// printf("int: lcdc\n");
			process_interrupt_(0x0048);
			mm_.write(0xff0f, mm_.read(0xff0f) ^ 0x02);

			return;
		}

		if (fn_is_enabled(0x04)) { // timer
			// printf("int: timer\n");
			process_interrupt_(0x0050);
			mm_.write(0xff0f, mm_.read(0xff0f) ^ 0x04);
			return;
		}

		if (fn_is_enabled(0x08)) { // s io
			// printf("int: sio\n");
			process_interrupt_(0x0058);
			mm_.write(0xff0f, mm_.read(0xff0f) ^ 0x08);
			return;
		}

		if (fn_is_enabled(0x10)) { // high->low p10-p13
			// printf("int: p10-13\n");
			process_interrupt_(0x0060);
			mm_.write(0xff0f, mm_.read(0xff0f) ^ 0x10);
			return;
		}
	}

	void process_interrupt_(wide_reg_t addr) {
		ime_ = false;
		push_(pc_);
		pc_ = addr; //0x0040;
		halted_ = false; // fixme where to put this?
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

	void set_bit_(int n, bool val) // fixme: rename
	{
		f() ^= (-static_cast<unsigned long>(val) ^ f()) & (1ul << n);
	}

	inline void push_(wide_reg_t val) // fixme: dirty
	{
		push_stack_(val >> 8);
		push_stack_(val);
	}

	inline wide_reg_t pop_() // fixme: dirty
	{
		auto l = pop_stack_();
		auto h = pop_stack_();
		return h << 8 | l;
	}

	void push_stack_(reg_t value) // fixme: dirty
	{
		--sp_;
		mm_.write(sp_, value);
	}

	reg_t pop_stack_() // fixme: dirty
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
		sp(static_cast<int8_t>(sp_old) + static_cast<int8_t>(n)); // fixme wtf

		zero_flag(false);
		substract_flag(false);

		carry_flag(false); // fixme ???
		half_carry_flag(false); // fixme ???
	}

	inline void add_8(reg_t n, reg_t& dst)
	{
		half_carry_flag(((dst & 0xf) + (n & 0xf)) > 0x0f);
		carry_flag((static_cast<uint16_t>(dst) + static_cast<uint16_t>(n)) > 0xff);

		dst += n;

		zero_flag(dst == 0);
		substract_flag(false);
	}

	inline void inc_(reg_t& dst)
	{
		half_carry_flag(((dst & 0xf) + (1 & 0xf)) > 0x0f);

		dst += 1;

		zero_flag(dst == 0);
		substract_flag(false);
	}


	inline wide_reg_t add_16(wide_reg_t n, wide_reg_t dst)
	{
		half_carry_flag(((dst & 0x0fff) + (n & 0x0fff)) > 0x0fff);
		carry_flag((static_cast<uint32_t>(dst) + static_cast<uint32_t>(n)) > 0xffff);

		dst += n;

		substract_flag(false);

		return dst;
	}

	inline void adc_8(uint32_t n, reg_t& dst)
	{
		// add_8(carry_flag() ? n + 1 : n, dst); // orig

		// n += static_cast<int>(carry_flag());

		// half_carry_flag(((dst & 0xf) + (n & 0xf)) & 0x10);
		// carry_flag((static_cast<uint16_t>(dst) + static_cast<uint16_t>(n)) & 0x10000);

		// dst += n;

		// zero_flag(dst == 0);
		// substract_flag(false);

		uint32_t result = dst + n;

		if (carry_flag())
			result += 1;

		half_carry_flag(((dst & 0xf) + (n & 0xf) + carry_flag()) > 0x0f);
		carry_flag(result > 0xff);

		dst = result;

		zero_flag(dst == 0);
		substract_flag(false);
	}

	inline void sub_8(reg_t n, reg_t& dst)
	{
		// half_carry_flag(((dst & 0xf) - (n & 0xf)) < 0);
		// carry_flag((static_cast<uint16_t>(dst) - static_cast<uint16_t>(n)) < 0);

		// dst -= n;

		// zero_flag(dst == 0);
		// substract_flag(true);
		int result = dst - n;

		half_carry_flag(((dst & 0xf) - (n & 0xf)) < 0);
		carry_flag(result < 0);

		dst = result;

		zero_flag(dst == 0);
		substract_flag(true);
	}

	inline void dec_(reg_t& dst)
	{
		int result = dst - 1;

		half_carry_flag(((dst & 0xf) - (1 & 0xf)) < 0);

		dst = result;

		zero_flag(dst == 0);
		substract_flag(true);
	}

	inline void sbc_8(reg_t n, reg_t& dst)
	{
		int result = dst - n - carry_flag();

		half_carry_flag(((dst & 0xf) - (n & 0xf) - carry_flag()) < 0);
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
		half_carry_flag(((a() & 0xf) - (n & 0xf)) < 0);
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
		dst = ((dst & 0xf0) >> 4) | ((dst & 0x0f) << 4);
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
	reg_t      h_;
	reg_t      l_;
	// reg_t      flag_;
	wide_reg_t sp_;
	wide_reg_t pc_;

	bool       ime_;
	bool       halted_;

	uint8_t    cycles_; // fixme: rename to busy_cycles
	uint64_t   cycle_;

	void process_opcode_() {
		static constexpr void* labels[]={
			&&lab_nop,         // 0x00
			&&lab_ld_bc_d16,   // 0x01
			&&lab_ld_bc_a,     // 0x02
			&&lab_inc_bc,      // 0x03
			&&lab_inc_b,       // 0x04
			&&lab_dec_b,       // 0x05
			&&lab_ld_b_d8,     // 0x06
			&&lab_rlca,        // 0x07
			&&lab_ld_a16_sp,   // 0x08
			&&lab_add_hl_bc,   // 0x09
			&&lab_ld_a_bc,     // 0x0a
			&&lab_dec_bc,      // 0x0b
			&&lab_inc_c,       // 0x0c
			&&lab_dec_c,       // 0x0d
			&&lab_ld_c_d8,     // 0x0e
			&&lab_rrca,        // 0x0f
			&&lab_smop_0,      // 0x10
			&&lab_ld_de_d16,   // 0x11
			&&lab_ld_de_a,     // 0x12
			&&lab_inc_de,      // 0x13
			&&lab_inc_d,       // 0x14
			&&lab_dec_d,       // 0x15
			&&lab_ld_d_d8,     // 0x16
			&&lab_rla,         // 0x17
			&&lab_jr_r8,       // 0x18
			&&lab_add_hl_de,   // 0x19
			&&lab_ld_a_de,     // 0x1a
			&&lab_dec_de,      // 0x1b
			&&lab_inc_e,       // 0x1c
			&&lab_dec_e,       // 0x1d
			&&lab_ld_e_d8,     // 0x1e
			&&lab_rra,         // 0x1f
			&&lab_jr_nz_r8,    // 0x20
			&&lab_ld_hl_d16,   // 0x21
			&&lab_ld_hlp_a,    // 0x22
			&&lab_inc_hl,      // 0x23
			&&lab_inc_h,       // 0x24
			&&lab_dec_h,       // 0x25
			&&lab_ld_h_d8,     // 0x26
			&&lab_daa,         // 0x27
			&&lab_jr_z_r8,     // 0x28
			&&lab_add_hl_hl,   // 0x29
			&&lab_ld_a_hlp,    // 0x2a
			&&lab_dec_hl,      // 0x2b
			&&lab_inc_l,       // 0x2c
			&&lab_dec_l,       // 0x2d
			&&lab_ld_l_d8,     // 0x2e
			&&lab_cpl,         // 0x2f
			&&lab_jr_nc_r8,    // 0x30
			&&lab_ld_sp_d16,   // 0x31
			&&lab_ld_hlm_a,    // 0x32
			&&lab_inc_sp,      // 0x33
			&&lab_inc_Zhl,      // 0x34
			&&lab_dec_Zhl,      // 0x35
			&&lab_ld_hl_d8,    // 0x36
			&&lab_scf,         // 0x37
			&&lab_jr_c_r8,     // 0x38
			&&lab_add_hl_sp,   // 0x39
			&&lab_ld_a_hlm,    // 0x3a
			&&lab_dec_sp,      // 0x3b
			&&lab_inc_a,       // 0x3c
			&&lab_dec_a,       // 0x3d
			&&lab_ld_a_d8,     // 0x3e
			&&lab_ccf,         // 0x3f
			&&lab_ld_b_b,      // 0x40
			&&lab_ld_b_c,      // 0x41
			&&lab_ld_b_d,      // 0x42
			&&lab_ld_b_e,      // 0x43
			&&lab_ld_b_h,      // 0x44
			&&lab_ld_b_l,      // 0x45
			&&lab_ld_b_hl,     // 0x46
			&&lab_ld_b_a,      // 0x47
			&&lab_ld_c_b,      // 0x48
			&&lab_ld_c_c,      // 0x49
			&&lab_ld_c_d,      // 0x4a
			&&lab_ld_c_e,      // 0x4b
			&&lab_ld_c_h,      // 0x4c
			&&lab_ld_c_l,      // 0x4d
			&&lab_ld_c_hl,     // 0x4e
			&&lab_ld_c_a,      // 0x4f
			&&lab_ld_d_b,      // 0x50
			&&lab_ld_d_c,      // 0x51
			&&lab_ld_d_d,      // 0x52
			&&lab_ld_d_e,      // 0x53
			&&lab_ld_d_h,      // 0x54
			&&lab_ld_d_l,      // 0x55
			&&lab_ld_d_hl,     // 0x56
			&&lab_ld_d_a,      // 0x57
			&&lab_ld_e_b,      // 0x58
			&&lab_ld_e_c,      // 0x59
			&&lab_ld_e_d,      // 0x5a
			&&lab_ld_e_e,      // 0x5b
			&&lab_ld_e_h,      // 0x5c
			&&lab_ld_e_l,      // 0x5d
			&&lab_ld_e_hl,     // 0x5e
			&&lab_ld_e_a,      // 0x5f
			&&lab_ld_h_b,      // 0x60
			&&lab_ld_h_c,      // 0x61
			&&lab_ld_h_d,      // 0x62
			&&lab_ld_h_e,      // 0x63
			&&lab_ld_h_h,      // 0x64
			&&lab_ld_h_l,      // 0x65
			&&lab_ld_h_hl,     // 0x66
			&&lab_ld_h_a,      // 0x67
			&&lab_ld_l_b,      // 0x68
			&&lab_ld_l_c,      // 0x69
			&&lab_ld_l_d,      // 0x6a
			&&lab_ld_l_e,      // 0x6b
			&&lab_ld_l_h,      // 0x6c
			&&lab_ld_l_l,      // 0x6d
			&&lab_ld_l_hl,     // 0x6e
			&&lab_ld_l_a,      // 0x6f
			&&lab_ld_hl_b,     // 0x70
			&&lab_ld_hl_c,     // 0x71
			&&lab_ld_hl_d,     // 0x72
			&&lab_ld_hl_e,     // 0x73
			&&lab_ld_hl_h,     // 0x74
			&&lab_ld_hl_l,     // 0x75
			&&lab_halm,        // 0x76
			&&lab_ld_hl_a,     // 0x77
			&&lab_ld_a_b,      // 0x78
			&&lab_ld_a_c,      // 0x79
			&&lab_ld_a_d,      // 0x7a
			&&lab_ld_a_e,      // 0x7b
			&&lab_ld_a_h,      // 0x7c
			&&lab_ld_a_l,      // 0x7d
			&&lab_ld_a_hl,     // 0x7e
			&&lab_ld_a_a,      // 0x7f
			&&lab_add_a_b,     // 0x80
			&&lab_add_a_c,     // 0x81
			&&lab_add_a_d,     // 0x82
			&&lab_add_a_e,     // 0x83
			&&lab_add_a_h,     // 0x84
			&&lab_add_a_l,     // 0x85
			&&lab_add_a_hl,    // 0x86
			&&lab_add_a_a,     // 0x87
			&&lab_adc_a_b,     // 0x88
			&&lab_adc_a_c,     // 0x89
			&&lab_adc_a_d,     // 0x8a
			&&lab_adc_a_e,     // 0x8b
			&&lab_adc_a_h,     // 0x8c
			&&lab_adc_a_l,     // 0x8d
			&&lab_adc_a_hl,    // 0x8e
			&&lab_adc_a_a,     // 0x8f
			&&lab_sub_b,       // 0x90
			&&lab_sub_c,       // 0x91
			&&lab_sub_d,       // 0x92
			&&lab_sub_e,       // 0x93
			&&lab_sub_h,       // 0x94
			&&lab_sub_l,       // 0x95
			&&lab_sub_hl,      // 0x96
			&&lab_sub_a,       // 0x97
			&&lab_sbc_a_b,     // 0x98
			&&lab_sbc_a_c,     // 0x99
			&&lab_sbc_a_d,     // 0x9a
			&&lab_sbc_a_e,     // 0x9b
			&&lab_sbc_a_h,     // 0x9c
			&&lab_sbc_a_l,     // 0x9d
			&&lab_sbc_a_hl,    // 0x9e
			&&lab_sbc_a_a,     // 0x9f
			&&lab_and_b,       // 0xa0
			&&lab_and_c,       // 0xa1
			&&lab_and_d,       // 0xa2
			&&lab_and_e,       // 0xa3
			&&lab_and_h,       // 0xa4
			&&lab_and_l,       // 0xa5
			&&lab_and_hl,      // 0xa6
			&&lab_and_a,       // 0xa7
			&&lab_xor_b,       // 0xa8
			&&lab_xor_c,       // 0xa9
			&&lab_xor_d,       // 0xaa
			&&lab_xor_e,       // 0xab
			&&lab_xor_h,       // 0xac
			&&lab_xor_l,       // 0xad
			&&lab_xor_hl,      // 0xae
			&&lab_xor_a,       // 0xaf
			&&lab_or_b,        // 0xb0
			&&lab_or_c,        // 0xb1
			&&lab_or_d,        // 0xb2
			&&lab_or_e,        // 0xb3
			&&lab_or_h,        // 0xb4
			&&lab_or_l,        // 0xb5
			&&lab_or_hl,       // 0xb6
			&&lab_or_a,        // 0xb7
			&&lab_cp_b,        // 0xb8
			&&lab_cp_c,        // 0xb9
			&&lab_cp_d,        // 0xba
			&&lab_cp_e,        // 0xbb
			&&lab_cp_h,        // 0xbc
			&&lab_cp_l,        // 0xbd
			&&lab_cp_hl,       // 0xbe
			&&lab_cp_a,        // 0xbf
			&&lab_ret_nz,      // 0xc0
			&&lab_pop_bc,      // 0xc1
			&&lab_jp_nz_a16,   // 0xc2
			&&lab_jp_a16,      // 0xc3
			&&lab_call_nz_a16, // 0xc4
			&&lab_push_bc,     // 0xc5
			&&lab_add_a_d8,    // 0xc6
			&&lab_rsm_00h,     // 0xc7
			&&lab_rem_z,       // 0xc8
			&&lab_rem,         // 0xc9
			&&lab_jp_z_a16,    // 0xca
			&&lab_prefix_cb,   // 0xcb
			&&lab_call_z_a16,  // 0xcc
			&&lab_call_a16,    // 0xcd
			&&lab_adc_a_d8,    // 0xce
			&&lab_rsm_08h,     // 0xcf
			&&lab_rem_nc,      // 0xd0
			&&lab_pop_de,      // 0xd1
			&&lab_jp_nc_a16,   // 0xd2
			&&lab_nop,         // 0xd3
			&&lab_call_nc_a16, // 0xd4
			&&lab_push_de,     // 0xd5
			&&lab_sub_d8,      // 0xd6
			&&lab_rsm_10h,     // 0xd7
			&&lab_rem_c,       // 0xd8
			&&lab_remi,        // 0xd9
			&&lab_jp_c_a16,    // 0xda
			&&lab_nop,         // 0xdb
			&&lab_call_c_a16,  // 0xdc
			&&lab_nop,         // 0xdd
			&&lab_sbc_a_d8,    // 0xde
			&&lab_rsm_18h,     // 0xdf
			&&lab_ldh_a8_a,    // 0xe0
			&&lab_pop_hl,      // 0xe1
			&&lab_ld_Zc_a,     // 0xe2
			&&lab_nop,         // 0xe3
			&&lab_nop,         // 0xe4
			&&lab_push_hl,     // 0xe5
			&&lab_and_d8,      // 0xe6
			&&lab_rsm_20h,     // 0xe7
			&&lab_add_sp_r8,   // 0xe8
			&&lab_jp_hl,       // 0xe9
			&&lab_ld_a16_a,    // 0xea
			&&lab_nop,         // 0xeb
			&&lab_nop,         // 0xec
			&&lab_nop,         // 0xed
			&&lab_xor_d8,      // 0xee
			&&lab_rsm_28h,     // 0xef
			&&lab_ldh_a_a8,    // 0xf0
			&&lab_pop_af,      // 0xf1
			&&lab_ld_a_Zc,     // 0xf2
			&&lab_di,          // 0xf3
			&&lab_nop,         // 0xf4
			&&lab_push_af,     // 0xf5
			&&lab_or_d8,       // 0xf6
			&&lab_rsm_30h,     // 0xf7
			&&lab_ld_hl_sppr8, // 0xf8
			&&lab_ld_sp_hl,    // 0xf9
			&&lab_ld_a_a16,    // 0xfa
			&&lab_ei,          // 0xfb
			&&lab_nop,         // 0xfc
			&&lab_nop,         // 0xfd
			&&lab_cp_d8,       // 0xfe
			&&lab_rsm_38h     // 0xff
		};
		static constexpr void* cbpfx_labels[] {
			&&lab_cbpfx_rlc_b   , // 0x00
				&&lab_cbpfx_rlc_c   , // 0x01
				&&lab_cbpfx_rlc_d   , // 0x02
				&&lab_cbpfx_rlc_e   , // 0x03
				&&lab_cbpfx_rlc_h   , // 0x04
				&&lab_cbpfx_rlc_l   , // 0x05
				&&lab_cbpfx_rlc_hl  , // 0x06
				&&lab_cbpfx_rlc_a   , // 0x07
				&&lab_cbpfx_rrc_b   , // 0x08
				&&lab_cbpfx_rrc_c   , // 0x09
				&&lab_cbpfx_rrc_d   , // 0x0a
				&&lab_cbpfx_rrc_e   , // 0x0b
				&&lab_cbpfx_rrc_h   , // 0x0c
				&&lab_cbpfx_rrc_l   , // 0x0d
				&&lab_cbpfx_rrc_hl  , // 0x0e
				&&lab_cbpfx_rrc_a   , // 0x0f
				&&lab_cbpfx_rl_b    , // 0x10
				&&lab_cbpfx_rl_c    , // 0x11
				&&lab_cbpfx_rl_d    , // 0x12
				&&lab_cbpfx_rl_e    , // 0x13
				&&lab_cbpfx_rl_h    , // 0x14
				&&lab_cbpfx_rl_l    , // 0x15
				&&lab_cbpfx_rl_hl   , // 0x16
				&&lab_cbpfx_rl_a    , // 0x17
				&&lab_cbpfx_rr_b    , // 0x18
				&&lab_cbpfx_rr_c    , // 0x19
				&&lab_cbpfx_rr_d    , // 0x1a
				&&lab_cbpfx_rr_e    , // 0x1b
				&&lab_cbpfx_rr_h    , // 0x1c
				&&lab_cbpfx_rr_l    , // 0x1d
				&&lab_cbpfx_rr_hl   , // 0x1e
				&&lab_cbpfx_rr_a    , // 0x1f
				&&lab_cbpfx_sla_b   , // 0x20
				&&lab_cbpfx_sla_c   , // 0x21
				&&lab_cbpfx_sla_d   , // 0x22
				&&lab_cbpfx_sla_e   , // 0x23
				&&lab_cbpfx_sla_h   , // 0x24
				&&lab_cbpfx_sla_l   , // 0x25
				&&lab_cbpfx_sla_hl  , // 0x26
				&&lab_cbpfx_sla_a   , // 0x27
				&&lab_cbpfx_sra_b   , // 0x28
				&&lab_cbpfx_sra_c   , // 0x29
				&&lab_cbpfx_sra_d   , // 0x2a
				&&lab_cbpfx_sra_e   , // 0x2b
				&&lab_cbpfx_sra_h   , // 0x2c
				&&lab_cbpfx_sra_l   , // 0x2d
				&&lab_cbpfx_sra_hl  , // 0x2e
				&&lab_cbpfx_sra_a   , // 0x2f
				&&lab_cbpfx_swap_b  , // 0x30
				&&lab_cbpfx_swap_c  , // 0x31
				&&lab_cbpfx_swap_d  , // 0x32
				&&lab_cbpfx_swap_e  , // 0x33
				&&lab_cbpfx_swap_h  , // 0x34
				&&lab_cbpfx_swap_l  , // 0x35
				&&lab_cbpfx_swaphl_ , // 0x36
				&&lab_cbpfx_swap_a  , // 0x37
				&&lab_cbpfx_srl_b   , // 0x38
				&&lab_cbpfx_srl_c   , // 0x39
				&&lab_cbpfx_srl_d   , // 0x3a
				&&lab_cbpfx_srl_e   , // 0x3b
				&&lab_cbpfx_srl_h   , // 0x3c
				&&lab_cbpfx_srl_l   , // 0x3d
				&&lab_cbpfx_srl_hl  , // 0x3e
				&&lab_cbpfx_srl_a   , // 0x3f
				&&lab_cbpfx_bit_0_b , // 0x40
				&&lab_cbpfx_bit_0_c , // 0x41
				&&lab_cbpfx_bit_0_d , // 0x42
				&&lab_cbpfx_bit_0_e , // 0x43
				&&lab_cbpfx_bit_0_h , // 0x44
				&&lab_cbpfx_bit_0_l , // 0x45
				&&lab_cbpfx_bit_0_hl, // 0x46
				&&lab_cbpfx_bit_0_a , // 0x47
				&&lab_cbpfx_bit_1_b , // 0x48
				&&lab_cbpfx_bit_1_c , // 0x49
				&&lab_cbpfx_bit_1_d , // 0x4a
				&&lab_cbpfx_bit_1_e , // 0x4b
				&&lab_cbpfx_bit_1_h , // 0x4c
				&&lab_cbpfx_bit_1_l , // 0x4d
				&&lab_cbpfx_bit_1_hl, // 0x4e
				&&lab_cbpfx_bit_1_a , // 0x4f
				&&lab_cbpfx_bit_2_b , // 0x50
				&&lab_cbpfx_bit_2_c , // 0x51
				&&lab_cbpfx_bit_2_d , // 0x52
				&&lab_cbpfx_bit_2_e , // 0x53
				&&lab_cbpfx_bit_2_h , // 0x54
				&&lab_cbpfx_bit_2_l , // 0x55
				&&lab_cbpfx_bit_2_hl, // 0x56
				&&lab_cbpfx_bit_2_a , // 0x57
				&&lab_cbpfx_bit_3_b , // 0x58
				&&lab_cbpfx_bit_3_c , // 0x59
				&&lab_cbpfx_bit_3_d , // 0x5a
				&&lab_cbpfx_bit_3_e , // 0x5b
				&&lab_cbpfx_bit_3_h , // 0x5c
				&&lab_cbpfx_bit_3_l , // 0x5d
				&&lab_cbpfx_bit_3_hl, // 0x5e
				&&lab_cbpfx_bit_3_a , // 0x5f
				&&lab_cbpfx_bit_4_b , // 0x60
				&&lab_cbpfx_bit_4_c , // 0x61
				&&lab_cbpfx_bit_4_d , // 0x62
				&&lab_cbpfx_bit_4_e , // 0x63
				&&lab_cbpfx_bit_4_h , // 0x64
				&&lab_cbpfx_bit_4_l , // 0x65
				&&lab_cbpfx_bit_4_hl, // 0x66
				&&lab_cbpfx_bit_4_a , // 0x67
				&&lab_cbpfx_bit_5_b , // 0x68
				&&lab_cbpfx_bit_5_c , // 0x69
				&&lab_cbpfx_bit_5_d , // 0x6a
				&&lab_cbpfx_bit_5_e , // 0x6b
				&&lab_cbpfx_bit_5_h , // 0x6c
				&&lab_cbpfx_bit_5_l , // 0x6d
				&&lab_cbpfx_bit_5_hl, // 0x6e
				&&lab_cbpfx_bit_5_a , // 0x6f
				&&lab_cbpfx_bit_6_b , // 0x70
				&&lab_cbpfx_bit_6_c , // 0x71
				&&lab_cbpfx_bit_6_d , // 0x72
				&&lab_cbpfx_bit_6_e , // 0x73
				&&lab_cbpfx_bit_6_h , // 0x74
				&&lab_cbpfx_bit_6_l , // 0x75
				&&lab_cbpfx_bit_6_hl, // 0x76
				&&lab_cbpfx_bit_6_a , // 0x77
				&&lab_cbpfx_bit_7_b , // 0x78
				&&lab_cbpfx_bit_7_c , // 0x79
				&&lab_cbpfx_bit_7_d , // 0x7a
				&&lab_cbpfx_bit_7_e , // 0x7b
				&&lab_cbpfx_bit_7_h , // 0x7c
				&&lab_cbpfx_bit_7_l , // 0x7d
				&&lab_cbpfx_bit_7_hl, // 0x7e
				&&lab_cbpfx_bit_7_a , // 0x7f
				&&lab_cbpfx_res_0_b , // 0x80
				&&lab_cbpfx_res_0_c , // 0x81
				&&lab_cbpfx_res_0_d , // 0x82
				&&lab_cbpfx_res_0_e , // 0x83
				&&lab_cbpfx_res_0_h , // 0x84
				&&lab_cbpfx_res_0_l , // 0x85
				&&lab_cbpfx_res_0_hl, // 0x86
				&&lab_cbpfx_res_0_a , // 0x87
				&&lab_cbpfx_res_1_b , // 0x88
				&&lab_cbpfx_res_1_c , // 0x89
				&&lab_cbpfx_res_1_d , // 0x8a
				&&lab_cbpfx_res_1_e , // 0x8b
				&&lab_cbpfx_res_1_h , // 0x8c
				&&lab_cbpfx_res_1_l , // 0x8d
				&&lab_cbpfx_res_1_hl, // 0x8e
				&&lab_cbpfx_res_1_a , // 0x8f
				&&lab_cbpfx_res_2_b , // 0x90
				&&lab_cbpfx_res_2_c , // 0x91
				&&lab_cbpfx_res_2_d , // 0x92
				&&lab_cbpfx_res_2_e , // 0x93
				&&lab_cbpfx_res_2_h , // 0x94
				&&lab_cbpfx_res_2_l , // 0x95
				&&lab_cbpfx_res_2_hl, // 0x96
				&&lab_cbpfx_res_2_a , // 0x97
				&&lab_cbpfx_res_3_b , // 0x98
				&&lab_cbpfx_res_3_c , // 0x99
				&&lab_cbpfx_res_3_d , // 0x9a
				&&lab_cbpfx_res_3_e , // 0x9b
				&&lab_cbpfx_res_3_h , // 0x9c
				&&lab_cbpfx_res_3_l , // 0x9d
				&&lab_cbpfx_res_3_hl, // 0x9e
				&&lab_cbpfx_res_3_a , // 0x9f
				&&lab_cbpfx_res_4_b , // 0xa0
				&&lab_cbpfx_res_4_c , // 0xa1
				&&lab_cbpfx_res_4_d , // 0xa2
				&&lab_cbpfx_res_4_e , // 0xa3
				&&lab_cbpfx_res_4_h , // 0xa4
				&&lab_cbpfx_res_4_l , // 0xa5
				&&lab_cbpfx_res_4_hl, // 0xa6
				&&lab_cbpfx_res_4_a , // 0xa7
				&&lab_cbpfx_res_5_b , // 0xa8
				&&lab_cbpfx_res_5_c , // 0xa9
				&&lab_cbpfx_res_5_d , // 0xaa
				&&lab_cbpfx_res_5_e , // 0xab
				&&lab_cbpfx_res_5_h , // 0xac
				&&lab_cbpfx_res_5_l , // 0xad
				&&lab_cbpfx_res_5_hl, // 0xae
				&&lab_cbpfx_res_5_a , // 0xaf
				&&lab_cbpfx_res_6_b , // 0xb0
				&&lab_cbpfx_res_6_c , // 0xb1
				&&lab_cbpfx_res_6_d , // 0xb2
				&&lab_cbpfx_res_6_e , // 0xb3
				&&lab_cbpfx_res_6_h , // 0xb4
				&&lab_cbpfx_res_6_l , // 0xb5
				&&lab_cbpfx_res_6_hl, // 0xb6
				&&lab_cbpfx_res_6_a , // 0xb7
				&&lab_cbpfx_res_7_b , // 0xb8
				&&lab_cbpfx_res_7_c , // 0xb9
				&&lab_cbpfx_res_7_d , // 0xba
				&&lab_cbpfx_res_7_e , // 0xbb
				&&lab_cbpfx_res_7_h , // 0xbc
				&&lab_cbpfx_res_7_l , // 0xbd
				&&lab_cbpfx_res_7_hl, // 0xbe
				&&lab_cbpfx_res_7_a , // 0xbf
				&&lab_cbpfx_set_0_b , // 0xc0
				&&lab_cbpfx_set_0_c , // 0xc1
				&&lab_cbpfx_set_0_d , // 0xc2
				&&lab_cbpfx_set_0_e , // 0xc3
				&&lab_cbpfx_set_0_h , // 0xc4
				&&lab_cbpfx_set_0_l , // 0xc5
				&&lab_cbpfx_set_0_hl, // 0xc6
				&&lab_cbpfx_set_0_a , // 0xc7
				&&lab_cbpfx_set_1_b , // 0xc8
				&&lab_cbpfx_set_1_c , // 0xc9
				&&lab_cbpfx_set_1_d , // 0xca
				&&lab_cbpfx_set_1_e , // 0xcb
				&&lab_cbpfx_set_1_h , // 0xcc
				&&lab_cbpfx_set_1_l , // 0xcd
				&&lab_cbpfx_set_1_hl, // 0xce
				&&lab_cbpfx_set_1_a , // 0xcf
				&&lab_cbpfx_set_2_b , // 0xd0
				&&lab_cbpfx_set_2_c , // 0xd1
				&&lab_cbpfx_set_2_d , // 0xd2
				&&lab_cbpfx_set_2_e , // 0xd3
				&&lab_cbpfx_set_2_h , // 0xd4
				&&lab_cbpfx_set_2_l , // 0xd5
				&&lab_cbpfx_set_2_hl, // 0xd6
				&&lab_cbpfx_set_2_a , // 0xd7
				&&lab_cbpfx_set_3_b , // 0xd8
				&&lab_cbpfx_set_3_c , // 0xd9
				&&lab_cbpfx_set_3_d , // 0xda
				&&lab_cbpfx_set_3_e , // 0xdb
				&&lab_cbpfx_set_3_h , // 0xdc
				&&lab_cbpfx_set_3_l , // 0xdd
				&&lab_cbpfx_set_3_hl, // 0xde
				&&lab_cbpfx_set_3_a , // 0xdf
				&&lab_cbpfx_set_4_b , // 0xe0
				&&lab_cbpfx_set_4_c , // 0xe1
				&&lab_cbpfx_set_4_d , // 0xe2
				&&lab_cbpfx_set_4_e , // 0xe3
				&&lab_cbpfx_set_4_h , // 0xe4
				&&lab_cbpfx_set_4_l , // 0xe5
				&&lab_cbpfx_set_4_hl, // 0xe6
				&&lab_cbpfx_set_4_a , // 0xe7
				&&lab_cbpfx_set_5_b , // 0xe8
				&&lab_cbpfx_set_5_c , // 0xe9
				&&lab_cbpfx_set_5_d , // 0xea
				&&lab_cbpfx_set_5_e , // 0xeb
				&&lab_cbpfx_set_5_h , // 0xec
				&&lab_cbpfx_set_5_l , // 0xed
				&&lab_cbpfx_set_5_hl, // 0xee
				&&lab_cbpfx_set_5_a , // 0xef
				&&lab_cbpfx_set_6_b , // 0xf0
				&&lab_cbpfx_set_6_c , // 0xf1
				&&lab_cbpfx_set_6_d , // 0xf2
				&&lab_cbpfx_set_6_e , // 0xf3
				&&lab_cbpfx_set_6_h , // 0xf4
				&&lab_cbpfx_set_6_l , // 0xf5
				&&lab_cbpfx_set_6_hl, // 0xf6
				&&lab_cbpfx_set_6_a , // 0xf7
				&&lab_cbpfx_set_7_b , // 0xf8
				&&lab_cbpfx_set_7_c , // 0xf9
				&&lab_cbpfx_set_7_d , // 0xfa
				&&lab_cbpfx_set_7_e , // 0xfb
				&&lab_cbpfx_set_7_h , // 0xfc
				&&lab_cbpfx_set_7_l , // 0xfd
				&&lab_cbpfx_set_7_hl, // 0xfe
				&&lab_cbpfx_set_7_a, // 0xff
		};

		auto const op_code = op();
		goto* labels[op_code];
lab_nop:         // 0x00
		pc_ += 1; cycles_ = 4;
		return;
lab_ld_bc_d16:   // 0x01
		bc(nn()); pc_ += 3; cycles_ = 12;
		return;
lab_ld_bc_a:     // 0x02
		reg_t i; ld_8(a(), i); mm_.write(bc(), i); pc_ += 1; cycles_ = 8;
		return;
lab_inc_bc:      // 0x03
		bc(bc() + 1); pc_ += 1; cycles_ = 8;
		return;
lab_inc_b:       // 0x04
		inc_(b()); pc_ += 1; cycles_ = 4;
		return;
lab_dec_b:       // 0x05
		dec_(b()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_b_d8:     // 0x06
		ld_8(b1(), b()); pc_ += 2; cycles_ = 8;
		return;
lab_rlca:        // 0x07
		rlc_(a()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_a16_sp:   // 0x08
		mm_.write(nn()    ,  sp() & 0x00FF      );
		mm_.write(nn() + 1, (sp() & 0xFF00) >> 8);
		pc_ += 3;
		cycles_ = 20;
		return;
lab_add_hl_bc:   // 0x09
		hl(add_16(bc(), hl())); pc_ += 1; cycles_ = 8;
		return;
lab_ld_a_bc:     // 0x0a
		ld_8(mm_.read(bc()), a()); pc_ += 1; cycles_ = 8;
		return;
lab_dec_bc:      // 0x0b
		bc(bc() - 1); pc_ += 1; cycles_ = 8;
		return;
lab_inc_c:       // 0x0c
		inc_(c()); pc_ += 1; cycles_ = 4;
		return;
lab_dec_c:       // 0x0d
		dec_(c()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_c_d8:     // 0x0e
		ld_8(b1(), c()); pc_ += 2; cycles_ = 8;
		return;
lab_rrca:        // 0x0f
		rrc_(a()); pc_ += 1; cycles_ = 4;
		return;
lab_smop_0:      // 0x10
		halted_ = true; pc_ += 2; cycles_ = 4;
		return;
lab_ld_de_d16:   // 0x11
		de(nn()); pc_ += 3; cycles_ = 12;
		return;
lab_ld_de_a:     // 0x12
		{reg_t i; ld_8(a(), i); mm_.write(de(), i);
		}
		pc_ += 1; cycles_ = 8;
		return;
lab_inc_de:      // 0x13
		de(de() + 1); pc_ += 1; cycles_ = 8;
		return;
lab_inc_d:       // 0x14
		inc_(d()); pc_ += 1; cycles_ = 4;
		return;
lab_dec_d:       // 0x15
		dec_(d()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_d_d8:     // 0x16
		ld_8(b1(), d()); pc_ += 2; cycles_ = 8;
		return;
lab_rla:         // 0x17
		rl_(a()); pc_ += 1; cycles_ = 4;
			return;
lab_jr_r8:       // 0x18
		pc_ += static_cast<int8_t>(b1()) + 2; cycles_ = 8;
		return;
lab_add_hl_de:   // 0x19
		hl(add_16(de(), hl())); pc_ += 1; cycles_ = 8;
		return;
lab_ld_a_de:     // 0x1a
		ld_8(mm_.read(de()), a()); pc_ += 1; cycles_ = 8;
		return;
lab_dec_de:      // 0x1b
		de(de() - 1); pc_ += 1; cycles_ = 8;
		return;
lab_inc_e:       // 0x1c
		inc_(e()); pc_ += 1; cycles_ = 4;
		return;
lab_dec_e:       // 0x1d
		dec_(e()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_e_d8:     // 0x1e
		ld_8(b1(), e()); pc_ += 2; cycles_ = 8;
		return;
lab_rra:         // 0x1f
		rr_(a()); pc_ += 1; cycles_ = 4;
		return;
lab_jr_nz_r8:    // 0x20
		{
			int8_t r = b1();
			if (zero_flag()){
				r = 0;
			}
			pc_ += r + 2;
			cycles_ = 8;
		}
		return;
lab_ld_hl_d16:   // 0x21
		hl(nn()); pc_ += 3; cycles_ = 12;
		return;
lab_ld_hlp_a:    // 0x22
		{
			reg_t i = 0;
			ld_8(a(), i);
			mm_.write(hl(), i);
			hl(hl()+1);
		}
		pc_ += 1; cycles_ = 8;
		return;
lab_inc_hl:      // 0x23
		hl(hl() + 1); pc_ += 1; cycles_ = 8;
		return;
lab_inc_h:       // 0x24
		inc_(h()); pc_ += 1; cycles_ = 4;
		return;
lab_dec_h:       // 0x25
		dec_(h()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_h_d8:     // 0x26
		ld_8(b1(), h()); pc_ += 2; cycles_ = 8;
		return;
lab_daa:         // 0x27
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
		}
		pc_ += 1; cycles_ = 4;
		return;
lab_jr_z_r8:     // 0x28
		pc_ += 2 + ((zero_flag())  ? static_cast<int8_t>(b1()) : 0);
		cycles_ = 8;
		return;
lab_add_hl_hl:   // 0x29
		hl(add_16(hl(), hl())); pc_ += 1; cycles_ = 8;
		return;
lab_ld_a_hlp:    // 0x2a
		ld_8(mm_.read(hl()), a()); hl(hl()+1); pc_ += 1; cycles_ = 8;
		return;
lab_dec_hl:      // 0x2b
		hl(hl() - 1); pc_ += 1; cycles_ = 8;
		return;
lab_inc_l:       // 0x2c
		inc_(l()); pc_ += 1; cycles_ = 4;
		return;
lab_dec_l:       // 0x2d
		dec_(l()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_l_d8:     // 0x2e
		ld_8(b1(), l()); pc_ += 2; cycles_ = 8;
		return;
lab_cpl:         // 0x2f
		a() = ~a();
		substract_flag(true);
		half_carry_flag(true);
		pc_ += 1; cycles_ = 4;
		return;
lab_jr_nc_r8:    // 0x30
		pc_ += 2 + ((not carry_flag()) ? static_cast<int8_t>(b1()) : 0);
		cycles_ = 8;
		return;
lab_ld_sp_d16:   // 0x31
		sp(nn()); pc_ += 3; cycles_ = 12;
		return;
lab_ld_hlm_a:    // 0x32
		mm_.write(hl(), a());
		hl(hl()-1);
		pc_ += 1; cycles_ = 8;
		return;
lab_inc_sp:      // 0x33
		sp(sp() + 1); pc_ += 1; cycles_ = 8;
		return;
lab_inc_Zhl:      // 0x34
		{reg_t i = mm_.read(hl()); inc_(i); mm_.write(hl(), i);}
		pc_ += 1; cycles_ = 12;
		return;
lab_dec_Zhl:      // 0x35
		{reg_t i = mm_.read(hl()); dec_(i); mm_.write(hl(), i);}
		pc_ += 1; cycles_ = 12;
		return;
lab_ld_hl_d8:    // 0x36
		{reg_t i = 0; ld_8(b1(), i); mm_.write(hl(), i);}
		pc_ += 2; cycles_ = 12;
		return;
lab_scf:         // 0x37
		carry_flag(true);
		substract_flag(false);
		half_carry_flag(false);
		pc_ += 1; cycles_ = 4;
		return;
lab_jr_c_r8:     // 0x38
		pc_    += 2 + ((carry_flag()) ? static_cast<int8_t>(b1()) : 0);
		cycles_ = 8;
		return;
lab_add_hl_sp:   // 0x39
		hl(add_16(sp(), hl())); pc_ += 1; cycles_ = 8;
		return;
lab_ld_a_hlm:    // 0x3a
		ld_8(mm_.read(hl()), a()); hl(hl()-1);
		pc_ += 1; cycles_ = 8;
		return;
lab_dec_sp:      // 0x3b
		sp(sp() - 1);
		pc_ += 1; cycles_ = 8;
		return;
lab_inc_a:       // 0x3c
		inc_(a());
		pc_ += 1; cycles_ = 4;
		return;
lab_dec_a:       // 0x3d
		dec_(a()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_a_d8:     // 0x3e
		ld_8(b1(), a()); pc_ += 2; cycles_ = 8;
		return;
lab_ccf:         // 0x3f
		substract_flag(false);
		half_carry_flag(false);
		carry_flag(carry_flag()?false:true);
		pc_ += 1; cycles_ = 4;
		return;
lab_ld_b_b:      // 0x40
		ld_8(b(), b());
		pc_ += 1; cycles_ = 4;
		return;
lab_ld_b_c:      // 0x41
		ld_8(c(), b()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_b_d:      // 0x42
		ld_8(d(), b()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_b_e:      // 0x43
		ld_8(e(), b()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_b_h:      // 0x44
		ld_8(h(), b()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_b_l:      // 0x45
		ld_8(l(), b()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_b_hl:     // 0x46
		ld_8(mm_.read(hl()), b()); pc_ += 1; cycles_ = 8;
		return;
lab_ld_b_a:      // 0x47
		ld_8(a(), b()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_c_b:      // 0x48
		ld_8(b(), c()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_c_c:      // 0x49
		ld_8(c(), c()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_c_d:      // 0x4a
		ld_8(d(), c()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_c_e:      // 0x4b
		ld_8(e(), c()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_c_h:      // 0x4c
		ld_8(h(), c()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_c_l:      // 0x4d
		ld_8(l(), c()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_c_hl:     // 0x4e
		ld_8(mm_.read(hl()), c()); pc_ += 1; cycles_ = 8;
		return;
lab_ld_c_a:      // 0x4f
		ld_8(a(), c()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_d_b:      // 0x50
		ld_8(b(), d()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_d_c:      // 0x51
		ld_8(c(), d()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_d_d:      // 0x52
		ld_8(d(), d()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_d_e:      // 0x53
		ld_8(e(), d()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_d_h:      // 0x54
		ld_8(h(), d()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_d_l:      // 0x55
		ld_8(l(), d()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_d_hl:     // 0x56
		ld_8(mm_.read(hl()), d()); pc_ += 1; cycles_ = 8;
		return;
lab_ld_d_a:      // 0x57
		ld_8(a(), d()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_e_b:      // 0x58
		ld_8(b(), e()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_e_c:      // 0x59
		ld_8(c(), e()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_e_d:      // 0x5a
		ld_8(d(), e()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_e_e:      // 0x5b
		ld_8(e(), e()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_e_h:      // 0x5c
		ld_8(h(), e()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_e_l:      // 0x5d
		ld_8(l(), e()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_e_hl:     // 0x5e
		ld_8(mm_.read(hl()), e()); pc_ += 1; cycles_ = 8;
		return;
lab_ld_e_a:      // 0x5f
		ld_8(a(), e()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_h_b:      // 0x60
		ld_8(b(), h()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_h_c:      // 0x61
		ld_8(c(), h()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_h_d:      // 0x62
		ld_8(d(), h()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_h_e:      // 0x63
		ld_8(e(), h()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_h_h:      // 0x64
		ld_8(h(), h()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_h_l:      // 0x65
		ld_8(l(), h()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_h_hl:     // 0x66
		ld_8(mm_.read(hl()), h()); pc_ += 1; cycles_ = 8;
		return;
lab_ld_h_a:      // 0x67
		ld_8(a(), h()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_l_b:      // 0x68
		ld_8(b(), l()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_l_c:      // 0x69
		ld_8(c(), l()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_l_d:      // 0x6a
		ld_8(d(), l()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_l_e:      // 0x6b
		ld_8(e(), l()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_l_h:      // 0x6c
		ld_8(h(), l()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_l_l:      // 0x6d
		ld_8(l(), l()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_l_hl:     // 0x6e
		ld_8(mm_.read(hl()), l()); pc_ += 1; cycles_ = 8;
		return;
lab_ld_l_a:      // 0x6f
		ld_8(a(), l()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_hl_b:     // 0x70
		{
			reg_t i = 0; ld_8(b(), i); mm_.write(hl(), i);}
		pc_ += 1; cycles_ = 8;
		return;
lab_ld_hl_c:     // 0x71
		{
			reg_t i = 0; ld_8(c(), i); mm_.write(hl(), i);
		}
		pc_ += 1; cycles_ = 8;
		return;
lab_ld_hl_d:     // 0x72
		{
			reg_t i = 0; ld_8(d(), i); mm_.write(hl(), i);
		}
		pc_ += 1; cycles_ = 8;
		return;
lab_ld_hl_e:     // 0x73
		{    reg_t i = 0; ld_8(e(), i); mm_.write(hl(), i);
		}pc_ += 1; cycles_ = 8;
		return;
lab_ld_hl_h:     // 0x74
		{
			reg_t i = 0; ld_8(h(), i); mm_.write(hl(), i);}
		pc_ += 1; cycles_ = 8;
		return;
lab_ld_hl_l:     // 0x75
		{
			reg_t i = 0; ld_8(l(), i); mm_.write(hl(), i);
		}
		pc_ += 1; cycles_ = 8;
		return;
lab_halm:        // 0x76
		halted_ = true; pc_ += 1; cycles_ = 4;
			return;
lab_ld_hl_a:     // 0x77
		{reg_t i = 0; ld_8(a(), i); mm_.write(hl(), i);
		}
		pc_ += 1; cycles_ = 8;
		return;
lab_ld_a_b:      // 0x78
		ld_8(b(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_a_c:      // 0x79
		ld_8(c(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_a_d:      // 0x7a
		ld_8(d(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_a_e:      // 0x7b
		ld_8(e(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_a_h:      // 0x7c
		ld_8(h(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_a_l:      // 0x7d
		ld_8(l(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_ld_a_hl:     // 0x7e
		ld_8(mm_.read(hl()), a()); pc_ += 1; cycles_ = 8;
		return;
lab_ld_a_a:      // 0x7f
		ld_8(a(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_add_a_b:     // 0x80
		add_8(b(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_add_a_c:     // 0x81
		add_8(c(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_add_a_d:     // 0x82
		add_8(d(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_add_a_e:     // 0x83
		add_8(e(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_add_a_h:     // 0x84
		add_8(h(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_add_a_l:     // 0x85
		add_8(l(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_add_a_hl:    // 0x86
		add_8(mm_.read(hl()), a()); pc_ += 1; cycles_ = 8;
		return;
lab_add_a_a:     // 0x87
		add_8(a(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_adc_a_b:     // 0x88
		adc_8(b(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_adc_a_c:     // 0x89
		adc_8(c(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_adc_a_d:     // 0x8a
		adc_8(d(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_adc_a_e:     // 0x8b
		adc_8(e(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_adc_a_h:     // 0x8c
		adc_8(h(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_adc_a_l:     // 0x8d
		adc_8(l(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_adc_a_hl:    // 0x8e
		adc_8(mm_.read(hl()), a()); pc_ += 1; cycles_ = 8;
		return;
lab_adc_a_a:     // 0x8f
		adc_8(a(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_sub_b:       // 0x90
		sub_8(b(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_sub_c:       // 0x91
		sub_8(c(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_sub_d:       // 0x92
		sub_8(d(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_sub_e:       // 0x93
		sub_8(e(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_sub_h:       // 0x94
		sub_8(h(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_sub_l:       // 0x95
		sub_8(l(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_sub_hl:      // 0x96
		sub_8(mm_.read(hl()), a()); pc_ += 1; cycles_ = 8;
		return;
lab_sub_a:       // 0x97
		sub_8(a(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_sbc_a_b:     // 0x98
		sbc_8(b(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_sbc_a_c:     // 0x99
		sbc_8(c(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_sbc_a_d:     // 0x9a
		sbc_8(d(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_sbc_a_e:     // 0x9b
		sbc_8(e(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_sbc_a_h:     // 0x9c
		sbc_8(h(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_sbc_a_l:     // 0x9d
		sbc_8(l(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_sbc_a_hl:    // 0x9e
		sbc_8(mm_.read(hl()), a());
		pc_ += 1; cycles_ = 8;
		return;
lab_sbc_a_a:     // 0x9f
		sbc_8(a(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_and_b:       // 0xa0
		and_(b(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_and_c:       // 0xa1
		and_(c(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_and_d:       // 0xa2
		and_(d(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_and_e:       // 0xa3
		and_(e(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_and_h:       // 0xa4
		and_(h(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_and_l:       // 0xa5
		and_(l(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_and_hl:      // 0xa6
		and_(mm_.read(hl()), a()); pc_ += 1; cycles_ = 8;
		return;
lab_and_a:       // 0xa7
		and_(a(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_xor_b:       // 0xa8
		xor_(b(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_xor_c:       // 0xa9
		xor_(c(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_xor_d:       // 0xaa
		xor_(d(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_xor_e:       // 0xab
		xor_(e(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_xor_h:       // 0xac
		xor_(h(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_xor_l:       // 0xad
		xor_(l(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_xor_hl:      // 0xae
		xor_(mm_.read(hl()), a()); pc_ += 1; cycles_ = 8;
		return;
lab_xor_a:       // 0xaf
		xor_(a(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_or_b:        // 0xb0
		or_(b(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_or_c:        // 0xb1
		or_(c(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_or_d:        // 0xb2
		or_(d(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_or_e:        // 0xb3
		or_(e(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_or_h:        // 0xb4
		or_(h(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_or_l:        // 0xb5
		or_(l(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_or_hl:       // 0xb6
		or_(mm_.read(hl()), a()); pc_ += 1; cycles_ = 8;
		return;
lab_or_a:        // 0xb7
		or_(a(), a()); pc_ += 1; cycles_ = 4;
		return;
lab_cp_b:        // 0xb8
		cp_(b()); pc_ += 1; cycles_ = 4;
		return;
lab_cp_c:        // 0xb9
		cp_(c()); pc_ += 1; cycles_ = 4;
		return;
lab_cp_d:        // 0xba
		cp_(d()); pc_ += 1; cycles_ = 4;
		return;
lab_cp_e:        // 0xbb
		cp_(e()); pc_ += 1; cycles_ = 4;
		return;
lab_cp_h:        // 0xbc
		cp_(h()); pc_ += 1; cycles_ = 4;
		return;
lab_cp_l:        // 0xbd
		cp_(l()); pc_ += 1; cycles_ = 4;
		return;
lab_cp_hl:       // 0xbe
		cp_(mm_.read(hl())); pc_ += 1; cycles_ = 8;
		return;
lab_cp_a:        // 0xbf
		cp_(a()); pc_ += 1; cycles_ = 4;
		return;
lab_ret_nz:      // 0xc0
		if (not zero_flag())  pc_ = pop_(); else pc_ += 1; cycles_ = 12;
		return;
lab_pop_bc:      // 0xc1
		bc(pop_()); pc_ += 1; cycles_ = 12;
		return;
lab_jp_nz_a16:   // 0xc2
		pc_ = (not zero_flag()) ? nn() : pc_ + 3; cycles_ = 12;
		return;
lab_jp_a16:      // 0xc3
		pc_ = nn(); cycles_ = 12;
		return;
lab_call_nz_a16: // 0xc4
		if (not zero_flag())  call_(nn()); else pc_ += 3; cycles_ = 12;
		return;
lab_push_bc:     // 0xc5
		push_(bc()); pc_ += 1; cycles_ = 16;
		return;
lab_add_a_d8:    // 0xc6
		add_8(b1(), a()); pc_ += 2; cycles_ = 8;
		return;
lab_rsm_00h:     // 0xc7
		call_(0x0000, 1); cycles_ = 32;
		return;
lab_rem_z:       // 0xc8
		if (zero_flag())      pc_ = pop_(); else pc_ += 1; cycles_ = 12;
		return;
lab_rem:         // 0xc9
		pc_ = pop_(); cycles_ = 8;
		return;
lab_jp_z_a16:    // 0xca
		pc_ = (zero_flag()) ? nn() : pc_ + 3; cycles_ = 12;
		return;
lab_prefix_cb:   // 0xcb
		goto* cbpfx_labels[b1()];
lab_call_z_a16:  // 0xcc
		if (zero_flag())      call_(nn()); else pc_ += 3; cycles_ = 12;
		return;
lab_call_a16:    // 0xcd
		call_(nn()); cycles_ = 12;
		return;
lab_adc_a_d8:    // 0xce
		adc_8(b1(), a()); pc_ += 2; cycles_ = 8;
		return;
lab_rsm_08h:     // 0xcf
		call_(0x0008, 1); cycles_ = 32;
		return;
lab_rem_nc:      // 0xd0
		if (not carry_flag()) pc_ = pop_(); else pc_ += 1; cycles_ = 12;
		return;
lab_pop_de:      // 0xd1
		de(pop_()); pc_ += 1; cycles_ = 12;
		return;
lab_jp_nc_a16:   // 0xd2
		pc_ = (not carry_flag()) ? nn() : pc_ + 3; cycles_ = 12;
		return;
lab_call_nc_a16: // 0xd4
		if (not carry_flag()) call_(nn()); else pc_ += 3; cycles_ = 12;
		return;
lab_push_de:     // 0xd5
		push_(de()); pc_ += 1; cycles_ = 16;
		return;
lab_sub_d8:      // 0xd6
		sub_8(b1(), a()); pc_ += 2; cycles_ = 8;
		return;
lab_rsm_10h:     // 0xd7
		call_(0x0010, 1); cycles_ = 32;
		return;
lab_rem_c:       // 0xd8
		if (carry_flag()) pc_ = pop_(); else pc_ += 1;
		cycles_ = 12;
		return;
lab_remi:        // 0xd9
		pc_ = pop_(); ime_ = true; cycles_ = 8;
		return;
lab_jp_c_a16:    // 0xda
		pc_ = (carry_flag()) ? nn() : pc_ + 3; cycles_ = 12;
		return;
lab_call_c_a16:  // 0xdc
		if (carry_flag())     call_(nn()); else pc_ += 3; cycles_ = 12;
		return;
lab_sbc_a_d8:    // 0xde
		sbc_8(b1(), a()); pc_ += 2; cycles_ = 4;
		return;
lab_rsm_18h:     // 0xdf
		call_(0x0018, 1); cycles_ = 32;
		return;
lab_ldh_a8_a:    // 0xe0
		{
		reg_t i = 0; ld_8(a(), i); mm_.write(0xFF00 + b1(), i);
		}
		pc_ += 2; cycles_ = 12;
		return;
lab_pop_hl:      // 0xe1
		hl(pop_()); pc_ += 1; cycles_ = 12;
		return;
lab_ld_Zc_a:      // 0xe2
		{ reg_t i = 0; ld_8(a(), i); mm_.write(0xFF00 + c(), i);}
		pc_ += 1; cycles_ = 8;
		return;
lab_push_hl:     // 0xe5
		push_(hl()); pc_ += 1; cycles_ = 16;
		return;
lab_and_d8:      // 0xe6
		and_(b1(), a()); pc_ += 2; cycles_ = 8;
		return;
lab_rsm_20h:     // 0xe7
		call_(0x0020, 1); cycles_ = 32;
		return;
lab_add_sp_r8:   // 0xe8
		{
		wide_reg_t reg = sp();
		int8_t   value = b1();
		int result = static_cast<int>(reg + value);
		zero_flag(false);
		substract_flag(false);
		half_carry_flag(((reg ^ value ^ (result & 0xFFFF)) & 0x10) == 0x10);
		carry_flag(((reg ^ value ^ (result & 0xFFFF)) & 0x100) == 0x100);
		sp(static_cast<wide_reg_t>(result));
		}
		pc_    += 2;
		cycles_ = 16;
		return;
lab_jp_hl:       // 0xe9
		pc_ = hl(); cycles_ = 4;
		return;
lab_ld_a16_a:    // 0xea
		{
		reg_t i = 0; ld_8(a(), i); mm_.write(nn(), i);
		}
		pc_ += 3; cycles_ = 8;
		return;
lab_xor_d8:      // 0xee
		xor_(b1(), a()); pc_ += 2; cycles_ = 8;
		return;
lab_rsm_28h:     // 0xef
		call_(0x0028, 1); cycles_ = 32;
		return;
lab_ldh_a_a8:    // 0xf0
		ld_8(mm_.read(0xFF00 + b1()), a());
		pc_ += 2; cycles_ = 12;
		return;
lab_pop_af:      // 0xf1
		af(pop_()); pc_ += 1; cycles_ = 12;
		return;
lab_ld_a_Zc:      // 0xf2
		ld_8(mm_.read(0xFF00 + c()), a());
		pc_ += 1; cycles_ = 8;
		return;
lab_di:          // 0xf3
		ime_ = false; pc_ += 1; cycles_ = 4;
		return;
lab_push_af:     // 0xf5
		push_(af()); pc_ += 1; cycles_ = 16;
		return;
lab_or_d8:       // 0xf6
		or_(b1(), a()); pc_ += 2; cycles_ = 8;
		return;
lab_rsm_30h:     // 0xf7
		call_(0x0030, 1); cycles_ = 32;
		return;
lab_ld_hl_sppr8: // 0xf8
		{
		wide_reg_t reg = sp();
		int8_t   value = b1();
		int result = static_cast<int>(reg + value);
		zero_flag(false);
		substract_flag(false);
		half_carry_flag(((reg ^ value ^ (result & 0xFFFF)) & 0x10) == 0x10);
		carry_flag(((reg ^ value ^ (result & 0xFFFF)) & 0x100) == 0x100);
		hl(static_cast<wide_reg_t>(result));
		}
		pc_ += 2;
		cycles_ = 12;
		return;
lab_ld_sp_hl:    // 0xf9
		sp(hl()); pc_ += 1; cycles_ = 8;
		return;
lab_ld_a_a16:    // 0xfa
		ld_8(mm_.read(nn()), a()); pc_ += 3; cycles_ = 16;
		return;
lab_ei:          // 0xfb
		ime_ = true; pc_ += 1; cycles_ = 4;
		return;
lab_cp_d8:       // 0xfe
		cp_(b1()); pc_ += 2; cycles_ = 8;
		return;
lab_rsm_38h:     // 0xff
		call_(0x0038, 1); cycles_ = 32;
		return;
lab_cbpfx_rlc_b   :  //0x00
		rlc_(b(), true);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_rlc_c   :  //0x01
		rlc_(c(), true);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_rlc_d   :  //0x02
		rlc_(d(), true);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_rlc_e   :  //0x03
		rlc_(e(), true);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_rlc_h   :  //0x04
		rlc_(h(), true);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_rlc_l   :  //0x05
		rlc_(l(), true);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_rlc_hl  :  //0x06
		{
			reg_t i = mm_.read(hl());
			rlc_(i, true);
			mm_.write(hl(), i);
		}
		pc_ += 2;
		cycles_ = 16;
		return;
lab_cbpfx_rlc_a   :  //0x07
		rlc_(a(), true);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_rrc_b   :  //0x08
		rrc_(b(), true);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_rrc_c   :  //0x09
		rrc_(c(), true);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_rrc_d   :  //0x0a
		rrc_(d(), true);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_rrc_e   :  //0x0b
		rrc_(e(), true);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_rrc_h   :  //0x0c
		rrc_(h(), true);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_rrc_l   :  //0x0d
		rrc_(l(), true);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_rrc_hl  :  //0x0e
		{
			reg_t i = mm_.read(hl());
			rrc_(i, true);
			mm_.write(hl(), i);
		}
		pc_ += 2;
		cycles_ = 16;
		return;
lab_cbpfx_rrc_a   :  //0x0f
		rrc_(a(), true);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_rl_b    :  //0x10
		rl_(b(), true);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_rl_c    :  //0x11
		rl_(c(), true);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_rl_d    :  //0x12
		rl_(d(), true);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_rl_e    :  //0x13
		rl_(e(), true);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_rl_h    :  //0x14
		rl_(h(), true);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_rl_l    :  //0x15
		rl_(l(), true);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_rl_hl   :  //0x16
		{
			reg_t i = mm_.read(hl());
			rl_(i, true);
			mm_.write(hl(), i);
		}
		pc_ += 2;
		cycles_ = 16;
		return;
lab_cbpfx_rl_a    :  //0x17
		rl_(a(), true);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_rr_b    :  //0x18
		rr_(b(), true);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_rr_c    :  //0x19
		rr_(c(), true);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_rr_d    :  //0x1a
		rr_(d(), true);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_rr_e    :  //0x1b
		rr_(e(), true);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_rr_h    :  //0x1c
		rr_(h(), true);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_rr_l    :  //0x1d
		rr_(l(), true);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_rr_hl   :  //0x1e
		{
			reg_t i = mm_.read(hl());
			rr_(i, true);
			mm_.write(hl(), i);
		}
		pc_ += 2;
		cycles_ = 16;
		return;
lab_cbpfx_rr_a    :  //0x1f
		rr_(a(), true);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_sla_b   :  //0x20
		sla_(b());
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_sla_c   :  //0x21
		sla_(c());
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_sla_d   :  //0x22
		sla_(d());
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_sla_e   :  //0x23
		sla_(e());
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_sla_h   :  //0x24
		sla_(h());
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_sla_l   :  //0x25
		sla_(l());
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_sla_hl  :  //0x26
		{
			reg_t i = mm_.read(hl());
			sla_(i);
			mm_.write(hl(), i);
		}
		pc_ += 2;
		cycles_ = 16;
		return;
lab_cbpfx_sla_a   :  //0x27
		sla_(a());
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_sra_b   :  //0x28
		sra_(b());
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_sra_c   :  //0x29
		sra_(c());
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_sra_d   :  //0x2a
		sra_(d());
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_sra_e   :  //0x2b
		sra_(e());
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_sra_h   :  //0x2c
		sra_(h());
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_sra_l   :  //0x2d
		sra_(l());
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_sra_hl  :  //0x2e
		{
			reg_t i = mm_.read(hl());
			sra_(i);
			mm_.write(hl(), i);
		}
		pc_ += 2;
		cycles_ = 16;
		return;
lab_cbpfx_sra_a   :  //0x2f
		sra_(a());
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_swap_b  :  //0x30
		swap_(b());
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_swap_c  :  //0x31
		swap_(c());
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_swap_d  :  //0x32
		swap_(d());
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_swap_e  :  //0x33
		swap_(e());
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_swap_h  :  //0x34
		swap_(h());
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_swap_l  :  //0x35
		swap_(l());
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_swaphl_ :  //0x36
		{
			reg_t i = mm_.read(hl());
			swap_(i);
			mm_.write(hl(), i);
		}
		pc_ += 2;
		cycles_ = 16;
		return;
lab_cbpfx_swap_a  :  //0x37
		swap_(a());
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_srl_b   :  //0x38
		srl_(b());
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_srl_c   :  //0x39
		srl_(c());
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_srl_d   :  //0x3a
		srl_(d());
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_srl_e   :  //0x3b
		srl_(e());
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_srl_h   :  //0x3c
		srl_(h());
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_srl_l   :  //0x3d
		srl_(l());
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_srl_hl  :  //0x3e
		{
			reg_t i = mm_.read(hl());
			srl_(i);
			mm_.write(hl(), i);
		}
		pc_ += 2;
		cycles_ = 16;
		return;
lab_cbpfx_srl_a   :  //0x3f
		srl_(a());
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_0_b :  //0x40
		bit_(b(), 0);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_0_c :  //0x41
		bit_(c(), 0);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_0_d :  //0x42
		bit_(d(), 0);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_0_e :  //0x43
		bit_(e(), 0);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_0_h :  //0x44
		bit_(h(), 0);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_0_l :  //0x45
		bit_(l(), 0);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_0_hl:  //0x46
		{
			reg_t i = mm_.read(hl());
			bit_(i, 0);
			mm_.write(hl(), i);
		}
		pc_ += 2;
		cycles_ = 16;
		return;
lab_cbpfx_bit_0_a :  //0x47
		bit_(a(), 0);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_1_b :  //0x48
		bit_(b(), 1);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_1_c :  //0x49
		bit_(c(), 1);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_1_d :  //0x4a
		bit_(d(), 1);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_1_e :  //0x4b
		bit_(e(), 1);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_1_h :  //0x4c
		bit_(h(), 1);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_1_l :  //0x4d
		bit_(l(), 1);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_1_hl:  //0x4e
		{
			reg_t i = mm_.read(hl());
			bit_(i, 1);
			mm_.write(hl(), i);
		}
		pc_ += 2;
		cycles_ = 16;
		return;
lab_cbpfx_bit_1_a :  //0x4f
		bit_(a(), 1);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_2_b :  //0x50
		bit_(b(), 2);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_2_c :  //0x51
		bit_(c(), 2);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_2_d :  //0x52
		bit_(d(), 2);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_2_e :  //0x53
		bit_(e(), 2);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_2_h :  //0x54
		bit_(h(), 2);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_2_l :  //0x55
		bit_(l(), 2);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_2_hl:  //0x56
		{
			reg_t i = mm_.read(hl());
			bit_(i, 2);
			mm_.write(hl(), i);
		}
		pc_ += 2;
		cycles_ = 16;
		return;
lab_cbpfx_bit_2_a :  //0x57
		bit_(a(), 2);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_3_b :  //0x58
		bit_(b(), 3);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_3_c :  //0x59
		bit_(c(), 3);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_3_d :  //0x5a
		bit_(d(), 3);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_3_e :  //0x5b
		bit_(e(), 3);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_3_h :  //0x5c
		bit_(h(), 3);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_3_l :  //0x5d
		bit_(l(), 3);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_3_hl:  //0x5e
		bit_(mm_.read(hl()), 3);
		pc_ += 2;
		cycles_ = 16;
		return;
lab_cbpfx_bit_3_a :  //0x5f
		bit_(a(), 3);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_4_b :  //0x60
		bit_(b(), 4);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_4_c :  //0x61
		bit_(c(), 4);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_4_d :  //0x62
		bit_(d(), 4);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_4_e :  //0x63
		bit_(e(), 4);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_4_h :  //0x64
		bit_(h(), 4);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_4_l :  //0x65
		bit_(l(), 4);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_4_hl:  //0x66
		bit_(mm_.read(hl()), 4);
		pc_ += 2;
		cycles_ = 16;
		return;
lab_cbpfx_bit_4_a :  //0x67
		bit_(a(), 4);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_5_b :  //0x68
		bit_(b(), 5);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_5_c :  //0x69
		bit_(c(), 5);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_5_d :  //0x6a
		bit_(d(), 5);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_5_e :  //0x6b
		bit_(e(), 5);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_5_h :  //0x6c
		bit_(h(), 5);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_5_l :  //0x6d
		bit_(l(), 5);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_5_hl:  //0x6e
		bit_(mm_.read(hl()), 5);
		pc_ += 2;
		cycles_ = 16;
		return;
lab_cbpfx_bit_5_a :  //0x6f
		bit_(a(), 5);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_6_b :  //0x70
		bit_(b(), 6);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_6_c :  //0x71
		bit_(c(), 6);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_6_d :  //0x72
		bit_(d(), 6);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_6_e :  //0x73
		bit_(e(), 6);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_6_h :  //0x74
		bit_(h(), 6);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_6_l :  //0x75
		bit_(l(), 6);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_6_hl:  //0x76
		bit_(mm_.read(hl()), 6);
		pc_ += 2;
		cycles_ = 16;
		return;
lab_cbpfx_bit_6_a :  //0x77
		bit_(a(), 6);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_7_b :  //0x78
		bit_(b(), 7);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_7_c :  //0x79
		bit_(c(), 7);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_7_d :  //0x7a
		bit_(d(), 7);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_7_e :  //0x7b
		bit_(e(), 7);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_7_h :  //0x7c
		bit_(h(), 7);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_7_l :  //0x7d
		bit_(l(), 7);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_bit_7_hl:  //0x7e
		bit_(mm_.read(hl()), 7);
		pc_ += 2;
		cycles_ = 16;
		return;
lab_cbpfx_bit_7_a :  //0x7f
		bit_(a(), 7);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_0_b :  //0x80
		res_(b(), 0);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_0_c :  //0x81
		res_(c(), 0);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_0_d :  //0x82
		res_(d(), 0);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_0_e :  //0x83
		res_(e(), 0);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_0_h :  //0x84
		res_(h(), 0);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_0_l :  //0x85
		res_(l(), 0);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_0_hl:  //0x86
		{
			reg_t i = mm_.read(hl());
			res_(i, 0);
			mm_.write(hl(), i);
		}
		pc_ += 2;
		cycles_ = 16;
		return;
lab_cbpfx_res_0_a :  //0x87
		res_(a(), 0);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_1_b :  //0x88
		res_(b(), 1);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_1_c :  //0x89
		res_(c(), 1);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_1_d :  //0x8a
		res_(d(), 1);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_1_e :  //0x8b
		res_(e(), 1);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_1_h :  //0x8c
		res_(h(), 1);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_1_l :  //0x8d
		res_(l(), 1);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_1_hl:  //0x8e
		{
			reg_t i = mm_.read(hl());
			res_(i, 1);
			mm_.write(hl(), i);
		}
		pc_ += 2;
		cycles_ = 16;
		return;
lab_cbpfx_res_1_a :  //0x8f
		res_(a(), 1);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_2_b :  //0x90
		res_(b(), 2);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_2_c :  //0x91
		res_(c(), 2);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_2_d :  //0x92
		res_(d(), 2);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_2_e :  //0x93
		res_(e(), 2);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_2_h :  //0x94
		res_(h(), 2);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_2_l :  //0x95
		res_(l(), 2);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_2_hl:  //0x96
		{
			reg_t i = mm_.read(hl());
			res_(i, 2);
			mm_.write(hl(), i);
		}
		pc_ += 2;
		cycles_ = 16;
		return;
lab_cbpfx_res_2_a :  //0x97
		res_(a(), 2);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_3_b :  //0x98
		res_(b(), 3);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_3_c :  //0x99
		res_(c(), 3);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_3_d :  //0x9a
		res_(d(), 3);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_3_e :  //0x9b
		res_(e(), 3);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_3_h :  //0x9c
		res_(h(), 3);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_3_l :  //0x9d
		res_(l(), 3);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_3_hl:  //0x9e
		{
			reg_t i = mm_.read(hl());
			res_(i, 3);
			mm_.write(hl(), i);
		}
		pc_ += 2;
		cycles_ = 16;
		return;
lab_cbpfx_res_3_a :  //0x9f
		res_(a(), 3);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_4_b :  //0xa0
		res_(b(), 4);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_4_c :  //0xa1
		res_(c(), 4);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_4_d :  //0xa2
		res_(d(), 4);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_4_e :  //0xa3
		res_(e(), 4);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_4_h :  //0xa4
		res_(h(), 4);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_4_l :  //0xa5
		res_(l(), 4);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_4_hl:  //0xa6
		{
			reg_t i = mm_.read(hl());
			res_(i, 4);
			mm_.write(hl(), i);
		}
		pc_ += 2;
		cycles_ = 16;
		return;
lab_cbpfx_res_4_a :  //0xa7
		res_(a(), 4);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_5_b :  //0xa8
		res_(b(), 5);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_5_c :  //0xa9
		res_(c(), 5);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_5_d :  //0xaa
		res_(d(), 5);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_5_e :  //0xab
		res_(e(), 5);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_5_h :  //0xac
		res_(h(), 5);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_5_l :  //0xad
		res_(l(), 5);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_5_hl:  //0xae
		{
			reg_t i = mm_.read(hl());
			res_(i, 5);
			mm_.write(hl(), i);
		}
		pc_ += 2;
		cycles_ = 16;
		return;
lab_cbpfx_res_5_a :  //0xaf
		res_(a(), 5);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_6_b :  //0xb0
		res_(b(), 6);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_6_c :  //0xb1
		res_(c(), 6);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_6_d :  //0xb2
		res_(d(), 6);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_6_e :  //0xb3
		res_(e(), 6);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_6_h :  //0xb4
		res_(h(), 6);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_6_l :  //0xb5
		res_(l(), 6);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_6_hl:  //0xb6
		{
			reg_t i = mm_.read(hl());
			res_(i, 6);
			mm_.write(hl(), i);
		}
		pc_ += 2;
		cycles_ = 16;
		return;
lab_cbpfx_res_6_a :  //0xb7
		res_(a(), 6);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_7_b :  //0xb8
		res_(b(), 7);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_7_c :  //0xb9
		res_(c(), 7);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_7_d :  //0xba
		res_(d(), 7);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_7_e :  //0xbb
		res_(e(), 7);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_7_h :  //0xbc
		res_(h(), 7);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_7_l :  //0xbd
		res_(l(), 7);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_res_7_hl:  //0xbe
		{
			reg_t i = mm_.read(hl());
			res_(i, 7);
			mm_.write(hl(), i);
		}
		pc_ += 2;
		cycles_ = 16;
		return;
lab_cbpfx_res_7_a :  //0xbf
		res_(a(), 7);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_0_b :  //0xc0
		set_(b(), 0);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_0_c :  //0xc1
		set_(c(), 0);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_0_d :  //0xc2
		set_(d(), 0);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_0_e :  //0xc3
		set_(e(), 0);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_0_h :  //0xc4
		set_(h(), 0);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_0_l :  //0xc5
		set_(l(), 0);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_0_hl:  //0xc6
		{
			reg_t i = mm_.read(hl());
			set_(i, 0);
			mm_.write(hl(), i);
		}
		pc_ += 2;
		cycles_ = 16;
		return;
lab_cbpfx_set_0_a :  //0xc7
		set_(a(), 0);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_1_b :  //0xc8
		set_(b(), 1);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_1_c :  //0xc9
		set_(c(), 1);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_1_d :  //0xca
		set_(d(), 1);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_1_e :  //0xcb
		set_(e(), 1);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_1_h :  //0xcc
		set_(h(), 1);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_1_l :  //0xcd
		set_(l(), 1);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_1_hl:  //0xce
		{
			reg_t i = mm_.read(hl());
			set_(i, 1);
			mm_.write(hl(), i);
		}
		pc_ += 2;
		cycles_ = 16;
		return;
lab_cbpfx_set_1_a :  //0xcf
		set_(a(), 1);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_2_b :  //0xd0
		set_(b(), 2);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_2_c :  //0xd1
		set_(c(), 2);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_2_d :  //0xd2
		set_(d(), 2);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_2_e :  //0xd3
		set_(e(), 2);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_2_h :  //0xd4
		set_(h(), 2);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_2_l :  //0xd5
		set_(l(), 2);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_2_hl:  //0xd6
		{
			reg_t i = mm_.read(hl());
			set_(i, 2);
			mm_.write(hl(), i);
		}
		pc_ += 2;
		cycles_ = 16;
		return;
lab_cbpfx_set_2_a :  //0xd7
		set_(a(), 2);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_3_b :  //0xd8
		set_(b(), 3);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_3_c :  //0xd9
		set_(c(), 3);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_3_d :  //0xda
		set_(d(), 3);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_3_e :  //0xdb
		set_(e(), 3);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_3_h :  //0xdc
		set_(h(), 3);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_3_l :  //0xdd
		set_(l(), 3);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_3_hl:  //0xde
		{
			reg_t i = mm_.read(hl());
			set_(i, 3);
			mm_.write(hl(), i);
		}
		pc_ += 2;
		cycles_ = 16;
		return;
lab_cbpfx_set_3_a :  //0xdf
		set_(a(), 3);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_4_b :  //0xe0
		set_(b(), 4);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_4_c :  //0xe1
		set_(c(), 4);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_4_d :  //0xe2
		set_(d(), 4);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_4_e :  //0xe3
		set_(e(), 4);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_4_h :  //0xe4
		set_(h(), 4);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_4_l :  //0xe5
		set_(l(), 4);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_4_hl:  //0xe6
		{
			reg_t i = mm_.read(hl());
			set_(i, 4);
			mm_.write(hl(), i);
		}
		pc_ += 2;
		cycles_ = 16;
		return;
lab_cbpfx_set_4_a :  //0xe7
		set_(a(), 4);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_5_b :  //0xe8
		set_(b(), 5);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_5_c :  //0xe9
		set_(c(), 5);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_5_d :  //0xea
		set_(d(), 5);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_5_e :  //0xeb
		set_(e(), 5);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_5_h :  //0xec
		set_(h(), 5);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_5_l :  //0xed
		set_(l(), 5);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_5_hl:  //0xee
		{
			reg_t i = mm_.read(hl());
			set_(i, 5);
			mm_.write(hl(), i);
		}
		pc_ += 2;
		cycles_ = 16;
		return;
lab_cbpfx_set_5_a :  //0xef
		set_(a(), 5);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_6_b :  //0xf0
		set_(b(), 6);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_6_c :  //0xf1
		set_(c(), 6);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_6_d :  //0xf2
		set_(d(), 6);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_6_e :  //0xf3
		set_(e(), 6);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_6_h :  //0xf4
		set_(h(), 6);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_6_l :  //0xf5
		set_(l(), 6);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_6_hl:  //0xf6
		{
			reg_t i = mm_.read(hl());
			set_(i, 6);
			mm_.write(hl(), i);
		}
		pc_ += 2;
		cycles_ = 16;
		return;
lab_cbpfx_set_6_a :  //0xf7
		set_(a(), 6);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_7_b :  //0xf8
		set_(b(), 7);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_7_c :  //0xf9
		set_(c(), 7);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_7_d :  //0xfa
		set_(d(), 7);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_7_e :  //0xfb
		set_(e(), 7);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_7_h :  //0xfc
		set_(h(), 7);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_7_l :  //0xfd
		set_(l(), 7);
		pc_ += 2;
		cycles_ = 8;
		return;
lab_cbpfx_set_7_hl:  //0xfe
		{
			reg_t i = mm_.read(hl());
			set_(i, 7);
			mm_.write(hl(), i);
		}
		pc_ += 2;
		cycles_ = 16;
		return;
lab_cbpfx_set_7_a :  //0xff
		set_(a(), 7);
		pc_ += 2;
		cycles_ = 8;
		return;
	}

};

