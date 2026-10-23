// license:BSD-3-Clause
// copyright-holders:Mr. MikroMikko
/***************************************************************************

    Western Digital WD1003-WA2 AT Winchester/floppy controller (hard disk part)

    Board chip set (parts list 61-000107-00): WD11C00A-22 (AT host interface
    gate array), WD2010A-05 / WD2010B-10 (Winchester controller), WD1015-37
    (8049 buffer manager, masked ROM, not dumped), WD10C20A (data separator),
    uPD765A + WD16C92 (floppy, not emulated here yet), 2 KB sector buffer.

    This device is a high-level emulation (HLE) of the Winchester side. The
    register map, bit definitions, command set and command sequence follow the
    WD1003-WA2 OEM manual (preliminary, 7 Jan 1986); see
    Tietokoneet/MikroMikko 3/Lisalaitteet/WD1003-WA2 .../REKISTERIT_JA_TOIMINTA.md

    The drive is modelled mechanically so that command times resemble a real
    ST-506 drive: rotation (3600 rpm), logical to physical sector interleave
    (WD1003-WA2 disks were formatted 1:3), seek time from cylinder distance and
    step rate, and a fixed controller overhead. The timing constants are
    estimates (marked TODO) and need calibration against the real machine.

***************************************************************************/

#ifndef MAME_BUS_ISA_WD1003_WA2_H
#define MAME_BUS_ISA_WD1003_WA2_H

#pragma once

#include "isa.h"
#include "imagedev/harddriv.h"

class isa16_wd1003_wa2_device : public device_t, public device_isa16_card_interface
{
public:
	isa16_wd1003_wa2_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	// drive model parameters (TODO: calibrate against the real drive)
	void set_interleave(int interleave) { m_interleave = interleave; }

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;

private:
	enum : u8
	{
		ST_ERR = 0x01, ST_IDX = 0x02, ST_CRD = 0x04, ST_DRQ = 0x08,
		ST_SC = 0x10, ST_WF = 0x20, ST_RDY = 0x40, ST_BSY = 0x80
	};

	enum : u8
	{
		ER_DM = 0x01, ER_TK = 0x02, ER_AC = 0x04, ER_ID = 0x10, ER_ECC = 0x40, ER_BB = 0x80
	};

	enum state_t : u8 { S_IDLE, S_BUSY, S_READ_DRQ, S_WRITE_DRQ, S_FORMAT_DRQ };

	void map(address_map &map) ATTR_COLD;
	void alt_map(address_map &map) ATTR_COLD;
	u16 io_r(offs_t offset, u16 mem_mask);
	void io_w(offs_t offset, u16 data, u16 mem_mask);

	u8 alt_r();
	void alt_w(u8 data);
	u8 reg_r(int reg);
	void reg_w(int reg, u8 data);
	u16 data_r();
	void data_w(u16 data);

	void start_command(u8 cmd);
	void finish(u8 error);
	void set_irq(bool state);
	void update_status_idle();
	void controller_reset();

	// drive model
	harddisk_image_device *drive() const;
	bool geometry(u32 &cyls, u32 &heads, u32 &spt) const;
	attotime seek_time(int from_cyl, int to_cyl) const;
	attotime rotation() const { return attotime::from_usec(16820); } // 3567 rpm, measured on the NEC D5126
	int index_pulse() const;
	int slot_of_sector(int sector, int spt) const;
	attotime wait_for_sector(attotime from, int sector, int spt, attotime &sector_time) const;
	bool lba(u32 &lba_out);

	// command steps
	void begin_read_sector();
	void begin_write_sector();
	void begin_read_verify();
	void begin_format();
	TIMER_CALLBACK_MEMBER(cmd_step);

	required_device_array<harddisk_image_device, 2> m_hdd;

	emu_timer *m_timer;
	state_t m_state;
	u8 m_cmd;
	u8 m_tf[8];       // 0 = data (unused), 1 = error/precomp, 2 = count, 3 = sector, 4 = cyl low, 5 = cyl high, 6 = SDH, 7 = status/command
	u8 m_status;
	u8 m_error;
	u8 m_fixed_disk;  // 3F6 write: HS3EN, RST, IEN-
	bool m_irq;
	u8 m_buffer[528];
	int m_buf_pos;
	int m_buf_len;
	int m_sectors_left;
	int m_step_code;   // R3..R0 of the last restore/seek
	int m_cyl[2];      // present cylinder of each drive
	int m_interleave;
	int m_stage;       // command stage (0 = seek done, 1 = sector transferred)
	bool m_long;
	bool m_retry_off;
	u32 m_cur_cyl, m_cur_head, m_cur_sector;
	attotime m_cmd_start;
	bool m_reset_line;
};

DECLARE_DEVICE_TYPE(ISA16_WD1003_WA2, isa16_wd1003_wa2_device)

#endif // MAME_BUS_ISA_WD1003_WA2_H
