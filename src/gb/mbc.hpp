#pragma once

class MBC
{
public:
  virtual ~MBC() = default;

  virtual reg_t read(wide_reg_t addr) const = 0;
  virtual void  write(wide_reg_t addr, reg_t value) = 0;
  virtual std::string name() const = 0;
};

class MBCRomOnly : public MBC
{
public:
  MBCRomOnly(mem_t const& rom)
    : rom_(rom)
  {
  }

  reg_t read(wide_reg_t addr) const override
  {
    return rom_[addr];
  }

  void write(wide_reg_t /*addr*/, reg_t /*value*/) override
  {
  }

  std::string name() const override
  {
    return "Rom";
  }

private:
  mem_t const& rom_;
};

class MBC1 : public MBC
{
  enum class Mode
  {
    Ram,
    Rom,
  };

public:
  MBC1(mem_t const& rom, mem_t& ram)
    : mode_(Mode::Rom)
    , low_(1)
    , high_(0)
    , rom_(rom)
    , ram_(ram)
  {
  }

  reg_t read(wide_reg_t addr) const override
  {
    // FIXME: handle oom access

    if (addr < 0x8000)
      return rom_[map_rom_addr_(addr)];

    return ram_[map_ram_addr_(addr)];
  }

  void write(wide_reg_t addr, reg_t value) override
  {
    if (addr > 0x8000) {
      ram_[map_ram_addr_(addr)] = value;
      return;
    }

    if (addr >= 0x2000 and addr <= 0x3FFF) {
      low_ = value == 0 ? 1 : value;
      return;
    }

    if (addr >= 0x4000 and addr <= 0x5FFF) {
      high_ = value & 0x03;
      return;
    }

    if (addr >= 0x6000 and addr <= 0x7FFF) {
      mode_ = value == 0 ? Mode::Rom : Mode::Ram;
      return;
    }
  }

  std::string name() const override
  {
    return "MBC1";
  }

private:
  int rom_bank_nr_() const
  {
    switch (mode_) {
    case Mode::Ram:
      return low_;
    case Mode::Rom:
      return low_ | ((high_ & 0x03) << 5);
    }
  }

  int ram_bank_nr_() const
  {
    switch (mode_) {
    case Mode::Ram:
      return high_ & 0x03;
    case Mode::Rom:
      return 0x00;
    }
  }

  size_t map_rom_addr_(wide_reg_t addr) const
  {
    if (addr < 0x4000 or addr > 0x7FFF)
      return addr;

    return (addr - 0x4000) + 0x4000 * rom_bank_nr_();
  }

  size_t map_ram_addr_(wide_reg_t addr) const
  {
    return (addr - 0xA000) + 0x2000 * ram_bank_nr_();
  }

private:
  Mode         mode_;
  int          low_;
  int          high_;

  mem_t const& rom_;
  mem_t&       ram_;
};

class MBC2 : public MBC
{
public:
  MBC2(mem_t const& rom, mem_t& ram)
    : rom_bank_nr_(1)
    , rom_(rom)
    , ram_(ram)
  {
  }

  reg_t read(wide_reg_t addr) const override
  {
    // FIXME: handle oom access

    if (addr < 0x8000)
      return rom_[map_rom_addr_(addr)];

    return ram_[map_ram_addr_(addr)];
  }

  void write(wide_reg_t addr, reg_t value) override
  {
    if (addr > 0x8000) {
      ram_[map_ram_addr_(addr)] = value;
      return;
    }

    if (addr >= 0x2000 and addr <= 0x3FFF and addr & 0x0100) {
      rom_bank_nr_ = value == 0 ? 1 : value;
      return;
    }
  }

  std::string name() const override
  {
    return "MBC2";
  }

private:
  size_t map_rom_addr_(wide_reg_t addr) const
  {
    if (addr < 0x4000 or addr > 0x7FFF)
      return addr;

    return (addr - 0x4000) + 0x4000 * rom_bank_nr_;
  }

  size_t map_ram_addr_(wide_reg_t addr) const
  {
    return (addr - 0xA000) + 0x2000;
  }

private:
  int          rom_bank_nr_;

  mem_t const& rom_;
  mem_t&       ram_;
};

class MBC5 : public MBC
{
public:
  MBC5(mem_t const& rom, mem_t& ram)
    : ram_bank_nr_(1)
    , rom_bank_nr_(0)
    , rom_(rom)
    , ram_(ram)
  {
  }

  reg_t read(wide_reg_t addr) const override
  {
    // FIXME: handle oom access

    if (addr < 0x8000)
      return rom_[map_rom_addr_(addr)];

    return ram_[map_ram_addr_(addr)];
  }

  void write(wide_reg_t addr, reg_t value) override
  {
    if (addr > 0x8000) {
      ram_[map_ram_addr_(addr)] = value;
      return;
    }

    if (addr >= 0x2000 and addr <= 0x3FFF) {
      rom_bank_nr_ = value;
      return;
    }

    if (addr >= 0x4000 and addr <= 0x5FFF) {
      ram_bank_nr_ = value & 0x0F;
      return;
    }
  }

  std::string name() const override
  {
    return "MBC5";
  }

private:
  size_t map_rom_addr_(wide_reg_t addr) const
  {
    if (addr < 0x4000 or addr > 0x7FFF)
      return addr;

    return (addr - 0x4000) + 0x4000 * rom_bank_nr_;
  }

  size_t map_ram_addr_(wide_reg_t addr) const
  {
    return (addr - 0xA000) + 0x2000 * ram_bank_nr_;
  }

private:
  int          rom_bank_nr_;
  int          ram_bank_nr_;

  mem_t const& rom_;
  mem_t&       ram_;
};
