#pragma once

#include "types.h"
#include "error.hpp"
#include "mbc.hpp"

#include <memory>

class Cartridge
{
public:
  enum class MbcType {
    RomOnly,
    Mbc1,
    Mbc2,
    Mbc3,
    Mbc5,
    Mbc6,
    Mbc7,
    RomRam,
    Mmm,
    Unsupported,
  };

  Error load(std::vector<reg_t> const& data)
  {
    rom_ = data;
    mbc_ = gen_mbc_();

    if (mbc_.get() == nullptr)
      return Error(Error::Code::RomNotSupported);

    return Error::NoError();
  }

  Error load_ram(std::vector<reg_t> const& data)
  {
    ram_ = data;

    return Error::NoError();
  }

  mem_t ram() const
  {
    return ram_;
  }

  void power_on()
  {
    printf(
      "CART: mbc type:%d rom banks:%d ram banks:%d\n",
      read(0x0147),
      count_rom_banks(),
      count_ram_banks());

    ram_.resize(0x2000 * (count_ram_banks()+1));
  }

  reg_t read(wide_reg_t addr) const
  {
    return mbc_->read(addr);
  }

  void write(wide_reg_t addr, reg_t value)
  {
    mbc_->write(addr, value);
  }

  MbcType mbc_type() const {
    switch (rom_[0x0147]) {
    case 0x00: return MbcType::RomOnly;
    case 0x01:
    case 0x02:
    case 0x03: return MbcType::Mbc1;
    case 0x05:
    case 0x06: return MbcType::Mbc2;
    case 0x08:
    case 0x09: return MbcType::RomRam;
    case 0x19:
    case 0x1A:
    case 0x1B:
    case 0x1C:
    case 0x1D:
    case 0x1E: return MbcType::Mbc5;
    default:
      return MbcType::Unsupported;
    }
  }

  wide_reg_t count_rom_banks() const
  {
    switch (rom_[0x0148]) {
    case 0x01: return 4;
    case 0x02: return 8;
    case 0x03: return 16;
    case 0x04: return 32;
    case 0x05: return 64;
    case 0x06: return 128;
    case 0x07: return 256;
    case 0x08: return 512;
    case 0x52: return 72;
    case 0x53: return 80;
    case 0x54: return 96;
    default:
      return 0;
    }
  }

  wide_reg_t count_ram_banks() const
  {
    switch (rom_[0x0149]) {
    case 0x01: return 1;
    case 0x02: return 1;
    case 0x03: return 4;
    case 0x04: return 16;
    case 0x05: return 8;
    default:
      return 0;
    }
  }

private:
  std::unique_ptr<MBC> gen_mbc_()
  {
    switch (mbc_type()) {
    case MbcType::RomOnly:
      return std::make_unique<MBCRomOnly>(rom_);
    case MbcType::Mbc1:
      return std::make_unique<MBC1>(rom_, ram_);
    case MbcType::Mbc2:
      return std::make_unique<MBC2>(rom_, ram_);
    case MbcType::Mbc5:
      return std::make_unique<MBC5>(rom_, ram_);
    default:
      return std::unique_ptr<MBC>();
    }
  }

private:
  std::unique_ptr<MBC> mbc_;
  mem_t                rom_ = mem_t();
  mem_t                ram_ = mem_t();
};
