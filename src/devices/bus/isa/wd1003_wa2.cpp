// license:BSD-3-Clause
// copyright-holders:Mr. MikroMikko
/***************************************************************************

    Western Digital WD1003-WA2 AT Winchester/floppy controller (hard disk part)

    High-level emulation of the Winchester side, written from the WD1003-WA2
    OEM manual (preliminary, 7 Jan 1986):
      - 1F0-1F7 task file, 3F6 alternate status / fixed disk register
      - commands: Restore (1x), Seek (7x), Read Sector (20/21), Write Sector
        (30/31), Format Track (50), Read Verify (40/41), Diagnose (90),
        Set Parameters (91)
      - interrupts: Read = one per sector as it is ready (none at the end),
        Write = one per sector when the next data is required and one at the
        end, Seek/Restore/Verify/Diagnose/Format = one at completion
    Drive timing is modelled mechanically (see the header); the constants are
    estimates, not measurements.

***************************************************************************/

#include "emu.h"
#include "wd1003_wa2.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#define VERBOSE 0
#include "logmacro.h"

DEFINE_DEVICE_TYPE(ISA16_WD1003_WA2, isa16_wd1003_wa2_device, "wd1003_wa2", "WD1003-WA2 AT Winchester/Floppy Controller")

// ---- timing estimates (TODO: calibrate against the real machine and drive) ----
static constexpr double CMD_OVERHEAD      = 0.0005;  // WD1015 command handling, seconds
static constexpr double INDEX_GAP         = 0.000793; // unused part of a track (before the index), measured on the NEC D5126

// Seek time of the NEC D5126 (20 MB, 615 cylinders, 4 heads, MM3-0), measured with DISKTIME.COM on 2026-10-23:
// time against cylinder distance, average rotational latency removed. Includes the step rate the Phoenix BIOS sets
// (so the step rate bits of Restore/Seek are not modelled separately) and head settling.
// TODO: measure the drive of the MM3-1 and make the drive model selectable.
static const struct { int d; double ms; } SEEK_TAB[] =
{
	{ 0, 0.0 }, { 1, 15.77 }, { 2, 15.77 }, { 4, 23.13 }, { 8, 25.24 }, { 16, 28.39 }, { 32, 32.59 },
	{ 64, 41.01 }, { 128, 65.19 }, { 256, 101.98 }, { 512, 171.38 }, { 613, 199.75 }
};

static double seek_ms(int dist)
{
	constexpr int n = std::size(SEEK_TAB);
	if (dist <= 0)
		return 0.0;
	for (int i = 1; i < n; i++)
		if (dist <= SEEK_TAB[i].d)
			return SEEK_TAB[i - 1].ms + (SEEK_TAB[i].ms - SEEK_TAB[i - 1].ms) * double(dist - SEEK_TAB[i - 1].d) / double(SEEK_TAB[i].d - SEEK_TAB[i - 1].d);
	// beyond the measured range: continue with the slope of the last interval
	const double slope = (SEEK_TAB[n - 1].ms - SEEK_TAB[n - 2].ms) / double(SEEK_TAB[n - 1].d - SEEK_TAB[n - 2].d);
	return SEEK_TAB[n - 1].ms + slope * double(dist - SEEK_TAB[n - 1].d);
}
static constexpr double SELFTEST_TIME     = 0.050;   // WD1015 self test after reset
static constexpr double INDEX_WIDTH       = 0.0001;  // index pulse width

// stepping rates (Table 5-2) in seconds; 0 = buffered seek (35 us pulses, drive limited)
static const double STEP_PERIOD[16] =
{
	0.000035, 0.0005, 0.001, 0.0015, 0.002, 0.0025, 0.003, 0.0035,
	0.004, 0.0045, 0.005, 0.0055, 0.006, 0.0065, 0.0000032, 0.000016
};

isa16_wd1003_wa2_device::isa16_wd1003_wa2_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, ISA16_WD1003_WA2, tag, owner, clock)
	, device_isa16_card_interface(mconfig, *this)
	, m_hdd(*this, "hdd%u", 0U)
	, m_timer(nullptr)
	, m_state(S_IDLE)
	, m_cmd(0)
	, m_status(ST_RDY | ST_SC)
	, m_error(0x01)
	, m_fixed_disk(0)
	, m_irq(false)
	, m_buf_pos(0)
	, m_buf_len(0)
	, m_sectors_left(0)
	, m_step_code(0xd)
	, m_interleave(3)
	, m_stage(0)
	, m_long(false)
	, m_retry_off(false)
	, m_cur_cyl(0)
	, m_cur_head(0)
	, m_cur_sector(0)
	, m_reset_line(false)
{
	std::fill(std::begin(m_tf), std::end(m_tf), 0);
	std::fill(std::begin(m_buffer), std::end(m_buffer), 0);
	m_cyl[0] = m_cyl[1] = 0;
}

void isa16_wd1003_wa2_device::device_add_mconfig(machine_config &config)
{
	HARDDISK(config, m_hdd[0], "st_hdd");
	HARDDISK(config, m_hdd[1], "st_hdd");
}

void isa16_wd1003_wa2_device::device_start()
{
	set_isa_device();
	m_timer = timer_alloc(FUNC(isa16_wd1003_wa2_device::cmd_step), this);

	save_item(NAME(m_tf));
	save_item(NAME(m_status));
	save_item(NAME(m_error));
	save_item(NAME(m_fixed_disk));
	save_item(NAME(m_irq));
	save_item(NAME(m_buffer));
	save_item(NAME(m_buf_pos));
	save_item(NAME(m_buf_len));
	save_item(NAME(m_sectors_left));
	save_item(NAME(m_step_code));
	save_item(NAME(m_cyl));
	save_item(NAME(m_stage));
}

void isa16_wd1003_wa2_device::device_reset()
{
	// primary addresses (jumpers E2-E3, E5-E6)
	m_isa->install_device(0x01f0, 0x01f7, *this, &isa16_wd1003_wa2_device::map);
	m_isa->install_device(0x03f0, 0x03f7, *this, &isa16_wd1003_wa2_device::alt_map);
	controller_reset();
}

void isa16_wd1003_wa2_device::map(address_map &map)
{
	map(0x0, 0x7).rw(FUNC(isa16_wd1003_wa2_device::io_r), FUNC(isa16_wd1003_wa2_device::io_w));
}

void isa16_wd1003_wa2_device::alt_map(address_map &map)
{
	// 3F6 only: the other ports belong to the floppy side (not emulated here yet)
	map(0x6, 0x6).rw(FUNC(isa16_wd1003_wa2_device::alt_r), FUNC(isa16_wd1003_wa2_device::alt_w));
}

/***************************************************************************
    host side
***************************************************************************/

u16 isa16_wd1003_wa2_device::io_r(offs_t offset, u16 mem_mask)
{
	// 16-bit I/O space: word 0 of the 1F0 range is the data register
	const int reg = offset * 2 + ((mem_mask & 0x00ff) ? 0 : 1);
	if (reg == 0 && mem_mask == 0xffff)
		return data_r();
	if (reg == 0)
	{
		// byte access to the data register (ECC bytes of Read Long)
		u8 data = 0;
		if (m_state == S_READ_DRQ && m_buf_pos < m_buf_len)
			data = m_buffer[m_buf_pos++];
		if (m_state == S_READ_DRQ && m_buf_pos >= m_buf_len)
			cmd_step(0);
		return data | (data << 8);
	}
	const u8 data = reg_r(reg);
	return (mem_mask & 0x00ff) ? data : (data << 8);
}

void isa16_wd1003_wa2_device::io_w(offs_t offset, u16 data, u16 mem_mask)
{
	const int reg = offset * 2 + ((mem_mask & 0x00ff) ? 0 : 1);
	if (reg == 0 && mem_mask == 0xffff)
	{
		data_w(data);
		return;
	}
	if (reg == 0)
	{
		if (m_state == S_WRITE_DRQ && m_buf_pos < m_buf_len)
			m_buffer[m_buf_pos++] = u8((mem_mask & 0x00ff) ? data : (data >> 8));
		if (m_state == S_WRITE_DRQ && m_buf_pos >= m_buf_len)
			cmd_step(0);
		return;
	}
	reg_w(reg, u8((mem_mask & 0x00ff) ? data : (data >> 8)));
}

u8 isa16_wd1003_wa2_device::alt_r()
{
	// alternate status: same bits as the status register, does not clear the interrupt
	u8 data = m_status & ~ST_IDX;
	if (index_pulse())
		data |= ST_IDX;
	return data;
}

void isa16_wd1003_wa2_device::alt_w(u8 data)
{
	// fixed disk register: HS3EN (bit 3), RST (bit 2), IEN- (bit 1)
	const bool was_reset = BIT(m_fixed_disk, 2);
	m_fixed_disk = data;
	if (BIT(data, 2))
	{
		m_timer->reset();
		m_state = S_IDLE;
		m_status = ST_BSY | ST_RDY;
		m_irq = false;
		m_isa->irq14_w(0);
	}
	else if (was_reset)
	{
		// reset released: the WD1015 runs its self test, then the controller is ready with error code 01
		controller_reset();
		m_state = S_BUSY;
		m_status = ST_BSY | ST_RDY;
		m_stage = 30;
		m_timer->adjust(attotime::from_double(SELFTEST_TIME), 0);
	}
	m_isa->irq14_w(m_irq && !BIT(m_fixed_disk, 1));
}

u8 isa16_wd1003_wa2_device::reg_r(int reg)
{
	// 1F0-1F7: reg 0-7; 3F6 arrives here as reg 0 of the alt map -> mapped by the caller to 6 via offset
	switch (reg)
	{
	case 1:
		return m_error;
	case 2: case 3: case 4: case 5: case 6:
		return m_tf[reg];
	case 7:
	{
		// reading the status register clears the interrupt request
		u8 data = m_status & ~ST_IDX;
		if (index_pulse())
			data |= ST_IDX;
		m_irq = false;
		m_isa->irq14_w(0);
		return data;
	}
	default:
		return 0xff;
	}
}

void isa16_wd1003_wa2_device::reg_w(int reg, u8 data)
{
	switch (reg)
	{
	case 1: case 2: case 3: case 4: case 5: case 6:
		m_tf[reg] = data;
		break;
	case 7:
		start_command(data);
		break;
	default:
		break;
	}
}

u16 isa16_wd1003_wa2_device::data_r()
{
	u16 data = 0;
	if (m_state == S_READ_DRQ && m_buf_pos < m_buf_len)
	{
		data = m_buffer[m_buf_pos] | (m_buffer[m_buf_pos + 1] << 8);
		m_buf_pos += 2;
		// the four ECC bytes of Read Long follow the data and are read as bytes (see io_r)
		if (!m_long && m_buf_pos >= m_buf_len)
			cmd_step(0);
	}
	return data;
}

void isa16_wd1003_wa2_device::data_w(u16 data)
{
	if ((m_state == S_WRITE_DRQ || m_state == S_FORMAT_DRQ) && m_buf_pos < m_buf_len)
	{
		m_buffer[m_buf_pos] = u8(data);
		m_buffer[m_buf_pos + 1] = u8(data >> 8);
		m_buf_pos += 2;
		if (m_buf_pos >= m_buf_len)
			cmd_step(0);
	}
}

/***************************************************************************
    drive model
***************************************************************************/

harddisk_image_device *isa16_wd1003_wa2_device::drive() const
{
	const int d = BIT(m_tf[6], 4);
	return m_hdd[d]->exists() ? m_hdd[d].target() : nullptr;
}

bool isa16_wd1003_wa2_device::geometry(u32 &cyls, u32 &heads, u32 &spt) const
{
	harddisk_image_device *hd = drive();
	if (!hd)
		return false;
	const hard_disk_file::info &info = hd->get_info();
	cyls = info.cylinders;
	heads = info.heads;
	spt = info.sectors;
	return info.sectorbytes == 512;
}

attotime isa16_wd1003_wa2_device::seek_time(int from_cyl, int to_cyl) const
{
	const int dist = std::abs(from_cyl - to_cyl);
	if (dist == 0)
		return attotime::zero;
	return attotime::from_double(seek_ms(dist) / 1000.0);
}

int isa16_wd1003_wa2_device::index_pulse() const
{
	return fmod(machine().time().as_double(), rotation().as_double()) < INDEX_WIDTH;
}

int isa16_wd1003_wa2_device::slot_of_sector(int sector, int spt) const
{
	// default layout: logical sectors placed every m_interleave slots, skipping used slots
	int map[64];
	for (int i = 0; i < spt; i++)
		map[i] = 0;
	int slot = 0;
	for (int s = 1; s <= spt; s++)
	{
		while (map[slot])
			slot = (slot + 1) % spt;
		map[slot] = s;
		slot = (slot + m_interleave) % spt;
	}
	for (int i = 0; i < spt; i++)
		if (map[i] == sector)
			return i;
	return 0;
}

attotime isa16_wd1003_wa2_device::wait_for_sector(attotime from, int sector, int spt, attotime &sector_time) const
{
	const double rev = rotation().as_double();
	const double slot_t = (rev - INDEX_GAP) / spt; // sector pitch: 0.943 ms with 17 sectors
	const double start = slot_of_sector(sector, spt) * slot_t;
	double wait = fmod(start - fmod(from.as_double(), rev) + rev, rev);
	sector_time = attotime::from_double(slot_t);
	return attotime::from_double(wait + slot_t);
}

bool isa16_wd1003_wa2_device::lba(u32 &lba_out)
{
	u32 cyls, heads, spt;
	if (!geometry(cyls, heads, spt))
		return false;
	if (m_cur_cyl >= cyls || m_cur_head >= heads || m_cur_sector < 1 || m_cur_sector > spt)
		return false;
	lba_out = (m_cur_cyl * heads + m_cur_head) * spt + (m_cur_sector - 1);
	return true;
}

/***************************************************************************
    controller
***************************************************************************/

void isa16_wd1003_wa2_device::set_irq(bool state)
{
	m_irq = state;
	m_isa->irq14_w(state && !BIT(m_fixed_disk, 1));
}

void isa16_wd1003_wa2_device::controller_reset()
{
	m_timer->reset();
	m_state = S_IDLE;
	m_status = ST_RDY | ST_SC;
	m_error = 0x01;
	m_step_code = 0xd; // 6.5 ms after reset
	m_tf[2] = 1;
	m_tf[3] = 1;
	m_tf[4] = m_tf[5] = m_tf[6] = 0;
	m_cyl[0] = m_cyl[1] = 0;
	m_irq = false;
	m_isa->irq14_w(0);
}

void isa16_wd1003_wa2_device::finish(u8 error)
{
	m_state = S_IDLE;
	m_status = ST_RDY | ST_SC;
	m_error = error;
	if (error)
		m_status |= ST_ERR;
}

void isa16_wd1003_wa2_device::start_command(u8 cmd)
{
	if (m_state != S_IDLE)
		return;

	m_cmd = cmd;
	m_long = BIT(cmd, 1) && ((cmd & 0xf0) == 0x20 || (cmd & 0xf0) == 0x30);
	m_retry_off = BIT(cmd, 0);
	if ((cmd & 0xf0) == 0x20 || (cmd & 0xf0) == 0x30)
	{
		m_long = BIT(cmd, 1);
		m_retry_off = BIT(cmd, 0);
	}
	m_irq = false;
	m_isa->irq14_w(0);
	m_error = 0;
	m_status = ST_BSY | ST_RDY;
	m_state = S_BUSY;
	m_stage = 0;

	if (!drive() && !((cmd & 0xf0) == 0x90))
	{
		// drive not ready: command terminates with an aborted command error
		m_timer->adjust(attotime::from_double(CMD_OVERHEAD), 1);
		return;
	}

	m_cur_cyl = ((m_tf[5] & 3) << 8) | m_tf[4];
	m_cur_head = m_tf[6] & 0x0f;
	m_cur_sector = m_tf[3];
	m_sectors_left = m_tf[2] ? m_tf[2] : 256;

	switch (cmd & 0xf0)
	{
	case 0x10: // Restore
	{
		m_step_code = cmd & 15;
		const int d = BIT(m_tf[6], 4);
		attotime t = seek_time(m_cyl[d], 0);
		if (t == attotime::zero)
			t = attotime::from_double(seek_ms(1) / 1000.0);
		m_stage = 10;
		m_timer->adjust(attotime::from_double(CMD_OVERHEAD) + t, 0);
		break;
	}
	case 0x70: // Seek
	{
		m_step_code = cmd & 15;
		const int d = BIT(m_tf[6], 4);
		m_stage = 11;
		m_timer->adjust(attotime::from_double(CMD_OVERHEAD) + seek_time(m_cyl[d], m_cur_cyl), 0);
		break;
	}
	case 0x20: begin_read_sector(); break;
	case 0x30: begin_write_sector(); break;
	case 0x40: begin_read_verify(); break;
	case 0x50: begin_format(); break;
	case 0x90:
		if (cmd == 0x90) // Diagnose
		{
			m_stage = 12;
			m_timer->adjust(attotime::from_double(0.002), 0);
		}
		else if (cmd == 0x91) // Set Parameters: heads (SDH) and sectors per track (sector count); the image defines the real geometry
		{
			m_stage = 13;
			m_timer->adjust(attotime::from_double(CMD_OVERHEAD), 0);
		}
		else
			m_timer->adjust(attotime::from_double(CMD_OVERHEAD), 1);
		break;
	default:
		m_timer->adjust(attotime::from_double(CMD_OVERHEAD), 1); // invalid command: aborted
		break;
	}
}

// seek to the cylinder of the command first, then position on the sector
void isa16_wd1003_wa2_device::begin_read_sector()
{
	const int d = BIT(m_tf[6], 4);
	m_stage = 1;
	m_timer->adjust(attotime::from_double(CMD_OVERHEAD) + seek_time(m_cyl[d], m_cur_cyl), 0);
}

void isa16_wd1003_wa2_device::begin_write_sector()
{
	// the first sector may be written to the buffer immediately: DRQ without interrupt
	m_state = S_WRITE_DRQ;
	m_buf_pos = 0;
	m_buf_len = m_long ? 516 : 512;
	m_status = ST_RDY | ST_SC | ST_DRQ;
	m_stage = 2;
}

void isa16_wd1003_wa2_device::begin_read_verify()
{
	const int d = BIT(m_tf[6], 4);
	m_stage = 3;
	m_timer->adjust(attotime::from_double(CMD_OVERHEAD) + seek_time(m_cyl[d], m_cur_cyl), 0);
}

void isa16_wd1003_wa2_device::begin_format()
{
	m_state = S_FORMAT_DRQ;
	m_buf_pos = 0;
	m_buf_len = 512;
	m_status = ST_RDY | ST_SC | ST_DRQ;
	m_stage = 4;
}

TIMER_CALLBACK_MEMBER(isa16_wd1003_wa2_device::cmd_step)
{
	const int d = BIT(m_tf[6], 4);
	u32 cyls = 0, heads = 0, spt = 0;
	geometry(cyls, heads, spt);

	// parameter 1 = abort (drive not ready / invalid command)
	if (param == 1)
	{
		finish(ER_AC);
		set_irq(true);
		return;
	}

	// called without a timer (data register emptied/filled): continue with the pending stage
	switch (m_stage)
	{
	case 10: // restore done
		m_cyl[d] = 0;
		m_tf[4] = m_tf[5] = 0;
		finish(0);
		set_irq(true);
		return;

	case 11: // seek done
		m_cyl[d] = m_cur_cyl;
		finish(0);
		set_irq(true);
		return;

	case 12: // diagnose
		m_tf[1] = 32; // write precompensation starts at cylinder 128
		m_tf[2] = 1;
		m_tf[4] = m_tf[5] = m_tf[6] = 0;
		finish(0x01); // the error register carries the result code 01 = no errors
		m_status = ST_RDY | ST_SC;
		set_irq(true);
		return;

	case 13: // set parameters
		finish(0);
		set_irq(true);
		return;

	case 1: // read: seek finished, position on the sector
	case 5: // read: next sector of a multi sector read
	{
		m_cyl[d] = m_cur_cyl;
		u32 l;
		if (!lba(l))
		{
			// ID not found after the retries
			m_timer->adjust(attotime::from_double((m_retry_off ? 2 : 10) * rotation().as_double()), 2);
			m_stage = 20;
			return;
		}
		attotime sector_time;
		const attotime wait = wait_for_sector(machine().time(), m_cur_sector, spt, sector_time);
		m_stage = 6;
		m_timer->adjust(wait, 0);
		return;
	}

	case 6: // read: sector has passed the head, move it into the buffer
	{
		u32 l;
		lba(l);
		if (!drive()->read(l, m_buffer))
		{
			m_stage = 21;
			m_timer->adjust(attotime::zero, 0);
			return;
		}
		if (m_long)
			std::fill_n(m_buffer + 512, 4, 0); // ECC bytes of Read Long (not modelled)
		m_buf_pos = 0;
		m_buf_len = m_long ? 516 : 512;
		m_state = S_READ_DRQ;
		m_status = ST_RDY | ST_SC | ST_DRQ;
		m_stage = 7;
		set_irq(true); // interrupt as the sector is ready
		return;
	}

	case 7: // read: the host emptied the buffer
	{
		m_state = S_BUSY;
		m_sectors_left--;
		// advance the sector pointer (multi sector transfers cross head and cylinder boundaries)
		if (++m_cur_sector > spt)
		{
			m_cur_sector = 1;
			if (++m_cur_head >= heads)
			{
				m_cur_head = 0;
				m_cur_cyl++;
			}
		}
		m_tf[3] = m_cur_sector;
		m_tf[4] = m_cur_cyl & 0xff;
		m_tf[5] = (m_cur_cyl >> 8) & 3;
		m_tf[6] = (m_tf[6] & 0xf0) | m_cur_head;
		m_tf[2] = m_sectors_left & 0xff;
		if (m_sectors_left <= 0)
		{
			finish(0); // no interrupt at the end of a read command
			return;
		}
		m_status = ST_BSY | ST_RDY;
		const int dd = d;
		const attotime seek = seek_time(m_cyl[dd], m_cur_cyl);
		m_stage = 5;
		m_timer->adjust(seek, 0);
		return;
	}

	case 2: // write: the host filled the buffer
	{
		m_state = S_BUSY;
		m_status = ST_BSY | ST_RDY;
		m_stage = 8;
		m_timer->adjust(seek_time(m_cyl[d], m_cur_cyl), 0);
		return;
	}

	case 8: // write: position on the sector, then write it
	{
		m_cyl[d] = m_cur_cyl;
		u32 l;
		if (!lba(l))
		{
			m_timer->adjust(attotime::from_double((m_retry_off ? 2 : 10) * rotation().as_double()), 2);
			m_stage = 20;
			return;
		}
		attotime sector_time;
		const attotime wait = wait_for_sector(machine().time(), m_cur_sector, spt, sector_time);
		m_stage = 9;
		m_timer->adjust(wait, 0);
		return;
	}

	case 9: // write: sector written
	{
		u32 l;
		lba(l);
		drive()->write(l, m_buffer);
		m_sectors_left--;
		if (++m_cur_sector > spt)
		{
			m_cur_sector = 1;
			if (++m_cur_head >= heads)
			{
				m_cur_head = 0;
				m_cur_cyl++;
			}
		}
		m_tf[3] = m_cur_sector;
		m_tf[4] = m_cur_cyl & 0xff;
		m_tf[5] = (m_cur_cyl >> 8) & 3;
		m_tf[6] = (m_tf[6] & 0xf0) | m_cur_head;
		m_tf[2] = m_sectors_left & 0xff;
		if (m_sectors_left <= 0)
		{
			finish(0);
			set_irq(true); // end of the command
			return;
		}
		// more data required: request the next sector with an interrupt
		m_state = S_WRITE_DRQ;
		m_buf_pos = 0;
		m_buf_len = m_long ? 516 : 512;
		m_status = ST_RDY | ST_SC | ST_DRQ;
		m_stage = 2;
		set_irq(true);
		return;
	}

	case 3: // read verify: positions on each sector without transfer
	{
		m_cyl[d] = m_cur_cyl;
		u32 l;
		if (!lba(l))
		{
			m_timer->adjust(attotime::from_double((m_retry_off ? 2 : 10) * rotation().as_double()), 2);
			m_stage = 20;
			return;
		}
		attotime sector_time;
		const attotime wait = wait_for_sector(machine().time(), m_cur_sector, spt, sector_time);
		m_stage = 14;
		m_timer->adjust(wait, 0);
		return;
	}

	case 14:
	{
		m_sectors_left--;
		if (++m_cur_sector > spt)
		{
			m_cur_sector = 1;
			if (++m_cur_head >= heads)
			{
				m_cur_head = 0;
				m_cur_cyl++;
			}
		}
		m_tf[2] = m_sectors_left & 0xff;
		if (m_sectors_left <= 0)
		{
			m_tf[3] = m_cur_sector;
			m_tf[4] = m_cur_cyl & 0xff;
			m_tf[5] = (m_cur_cyl >> 8) & 3;
			m_tf[6] = (m_tf[6] & 0xf0) | m_cur_head;
			finish(0);
			set_irq(true); // single interrupt at the end
			return;
		}
		m_stage = 3;
		m_timer->adjust(seek_time(m_cyl[d], m_cur_cyl), 0);
		return;
	}

	case 4: // format: the host filled the 512 byte interleave table
	{
		m_state = S_BUSY;
		m_status = ST_BSY | ST_RDY;
		m_stage = 15;
		m_timer->adjust(seek_time(m_cyl[d], m_cur_cyl) + rotation(), 0); // one revolution to write the track
		return;
	}

	case 15: // format done: all data fields are initialised to zero
	{
		m_cyl[d] = m_cur_cyl;
		u32 l = 0;
		const u32 count = m_tf[2];
		m_cur_sector = 1;
		u8 zeros[512];
		std::memset(zeros, 0, sizeof(zeros));
		if (lba(l) && drive())
			for (u32 i = 0; i < std::min(count, spt); i++)
				drive()->write(l + i, zeros);
		finish(0);
		set_irq(true);
		return;
	}

	case 30: // self test finished after a reset
		m_state = S_IDLE;
		m_status = ST_RDY | ST_SC;
		m_error = 0x01;
		return;

	case 20: // ID not found after retries
		finish(ER_ID);
		set_irq(true);
		return;

	case 21: // media read error
		finish(ER_BB | ER_ID);
		set_irq(true);
		return;

	default:
		finish(ER_AC);
		set_irq(true);
		return;
	}
}
