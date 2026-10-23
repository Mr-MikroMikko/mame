// license:BSD-3-Clause
// copyright-holders:Mr. MikroMikko
/***************************************************************************

    Paradise Autoswitch EGA2 (PEGA 2A) - the Nokia MikroMikko 3 EGA card

    Board: "(C) 1986 PARADISE AUTOSWITCH EGA2", ASIC L1A2861 PARADISE PEGA 2A, 8 x NEC D41464C-12 (256 KB), oscillators
    16.257, 25.000 and 27.256 MHz, BIOS EPROM 27128 "22030-C1201 C/S C400" (version string 22030-C12), six configuration
    levers (CTS 194-6).

    Emulated from the card BIOS and port reads of the real card:
    - the configuration levers, read through Input Status Register 0,
    - the scratch RAM window at A0000-A03FF with the shadow of the CGA/MDA register writes,
    - the AutoSwitch NMI trap (IOCHCK),
    - the dot clock selection with the four clocks (BIOS modes 50h-53h 640x480, 54h/55h 132 columns),
    - the input status bits and the undriven (open bus) ports as read on the real card,
    - a border shown only up to the start of blanking.

    Not emulated: the 2A extension and locking registers (the BIOS reads 3D8 twice and then writes CRTC index 00h/1Ah
    and 3DB; PEGA.EXE also writes 3DE) and the hardware CGA/MDA/Hercules/Plantronics emulation. The Nokia setting of
    the card does not use them; the BIOS emulates MDA/CGA writes in software through the NMI trap when it is enabled.

***************************************************************************/

#include "emu.h"
#include "pega2a.h"

#define LOG_PORTS (1U << 1)
#define LOG_TRAP  (1U << 2)

//#define VERBOSE (LOG_PORTS | LOG_TRAP)
#include "logmacro.h"

#define LOGPORTS(...) LOGMASKED(LOG_PORTS, __VA_ARGS__)
#define LOGTRAP(...)  LOGMASKED(LOG_TRAP, __VA_ARGS__)


DEFINE_DEVICE_TYPE(ISA8_PEGA2A, isa8_pega2a_device, "pega2a", "Paradise Autoswitch EGA2 (PEGA 2A, Nokia MikroMikko 3)")

ROM_START( pega2a )
	ROM_REGION(0x4000, "bios", 0)
	ROM_LOAD("22030-c1201.bin", 0x0000, 0x4000, CRC(711ad079) SHA1(da1cfed06da5f9baac01332eb10704b8c7657f67)) // Paradise PEGA BIOS 1986, 27128 (label 22030-C1201 C/S C400), same on two Nokia cards
ROM_END

// Configuration levers SW1 (CTS 194-6, ON = down). Levers 1-4 are read like the IBM EGA switches through bit 4 of Input
// Status Register 0 (ON senses 0), Misc Output bits 3-2 selecting the lever; the BIOS reads them with bits 3-2 = 3, 2, 1, 0
// and builds its setting nibble from the first read upwards, so sense 3 is lever 1 and sense 0 is lever 4.
// Monitor table of the card (lever 1 2 3 4): Monochrome OFF OFF ON OFF; Standard RGB 40 col ON OFF OFF ON; Standard RGB
// 80 col OFF OFF OFF ON; Enhanced RGB 200 line ON ON ON OFF; Enhanced RGB 350 line OFF ON ON OFF.
// Levers 5 and 6 are read in bits 6 and 5 of the same register; with bit 6 = 0 the BIOS replaces the setting of levers
// 1-4 by a sensed one. Which of levers 5/6 is bit 6 and which is bit 5 is a guess (both are OFF on the real card).
// Default: the setting of the Nokia card, levers 2 and 3 ON, the others OFF (Enhanced RGB, 350 line).
static INPUT_PORTS_START( pega2a )
	PORT_START("SW")
	PORT_DIPNAME( 0x01, 0x01, "Lever 1" ) PORT_DIPLOCATION("SW1:1")
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPNAME( 0x02, 0x00, "Lever 2" ) PORT_DIPLOCATION("SW1:2")
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPNAME( 0x04, 0x00, "Lever 3" ) PORT_DIPLOCATION("SW1:3")
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPNAME( 0x08, 0x08, "Lever 4" ) PORT_DIPLOCATION("SW1:4")
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPNAME( 0x10, 0x10, "Lever 5" ) PORT_DIPLOCATION("SW1:5")
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPNAME( 0x20, 0x20, "Lever 6" ) PORT_DIPLOCATION("SW1:6")
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
INPUT_PORTS_END


isa8_pega2a_device::isa8_pega2a_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: isa8_ega_device(mconfig, ISA8_PEGA2A, tag, owner, clock)
	, m_sw(*this, "SW")
	, m_scratch_en(false)
	, m_trap_armed(false)
	, m_crtc_index(0)
	, m_mono_monitor(false)
{
	std::fill(std::begin(m_scratch), std::end(m_scratch), 0);
}

const tiny_rom_entry *isa8_pega2a_device::device_rom_region() const
{
	return ROM_NAME( pega2a );
}

ioport_constructor isa8_pega2a_device::device_input_ports() const
{
	return INPUT_PORTS_NAME( pega2a );
}

void isa8_pega2a_device::device_add_mconfig(machine_config &config)
{
	isa8_ega_device::device_add_mconfig(config);

	// The screen shows the border around the active display, so the card composes the frame itself (direct colour) and
	// the CRTC only reports its timing through the reconfigure callback.
	m_screen->set_screen_update(FUNC(isa8_pega2a_device::screen_update));
	m_screen->set_no_palette();
	// in the 350 line mode of the Nokia setting the frame is 656 x 350 around the 640 x 350 image, which a 4:3 monitor
	// shows as 4:3; the aspect ratio of a screen cannot change at run time
	m_screen->set_physical_aspect(4, 3);

	m_crtc_ega->set_screen(nullptr);
	m_crtc_ega->set_reconfigure_callback(FUNC(isa8_pega2a_device::crtc_reconfigure));
}

// isa8_ega_device::device_start() is not called: it loads a generic IBM EGA BIOS with swapped address lines and installs
// the plain EGA port handlers.
void isa8_pega2a_device::device_start()
{
	if (m_palette != nullptr && !m_palette->started())
		throw device_missing_dependencies();

	set_isa_device();

	set_monitor_palette(false);

	m_vram = make_unique_clear<uint8_t[]>(256 * 1024);
	m_videoram = m_vram.get();
	m_plane[0] = m_videoram + 0x00000;
	m_plane[1] = m_videoram + 0x10000;
	m_plane[2] = m_videoram + 0x20000;
	m_plane[3] = m_videoram + 0x30000;

	ega_save_state();
	save_item(NAME(m_scratch));
	save_item(NAME(m_scratch_en));
	save_item(NAME(m_trap_armed));
	save_item(NAME(m_crtc_index));
	save_item(NAME(m_mono_monitor));

	m_screen->register_screen_bitmap(m_ega_bitmap);

	m_isa->install_rom(this, 0xc0000, 0xc3fff, "bios");
	m_isa->install_device(0x3b0, 0x3bf, read8sm_delegate(*this, FUNC(isa8_pega2a_device::port_3b0_r)), write8sm_delegate(*this, FUNC(isa8_pega2a_device::port_3b0_w)));
	m_isa->install_device(0x3c0, 0x3cf, read8sm_delegate(*this, FUNC(isa8_pega2a_device::port_3c0_r)), write8sm_delegate(*this, FUNC(isa8_pega2a_device::port_3c0_w)));
	m_isa->install_device(0x3d0, 0x3df, read8sm_delegate(*this, FUNC(isa8_pega2a_device::port_3d0_r)), write8sm_delegate(*this, FUNC(isa8_pega2a_device::port_3d0_w)));
}

void isa8_pega2a_device::device_reset()
{
	m_scratch_en = false;
	m_trap_armed = false;
	m_crtc_index = 0;
	std::fill(std::begin(m_scratch), std::end(m_scratch), 0);

	isa8_ega_device::device_reset();

	// a monochrome monitor (monitor table: levers OFF OFF ON OFF) shows the EGA outputs as two grey levels
	set_monitor_palette((m_sw->read() & 0x0f) == 0x0b);
}

void isa8_pega2a_device::device_post_load()
{
	set_monitor_palette(m_mono_monitor);
	change_mode();
	install_banks();
}


//-------------------------------------------------
//  palette - colour monitor (64 colours) or
//  monochrome monitor (video and intensity)
//-------------------------------------------------

void isa8_pega2a_device::set_monitor_palette(bool mono)
{
	for (int i = 0; i < 64; i++)
	{
		uint8_t r, g, b;

		if (mono)
		{
			r = g = b = BIT(i, 3) ? (BIT(i, 4) ? 0xff : 0xaa) : 0x00;
		}
		else
		{
			r = ((i & 0x04) ? 0xaa : 0x00) + ((i & 0x20) ? 0x55 : 0x00);
			g = ((i & 0x02) ? 0xaa : 0x00) + ((i & 0x10) ? 0x55 : 0x00);
			b = ((i & 0x01) ? 0xaa : 0x00) + ((i & 0x08) ? 0x55 : 0x00);
		}

		m_palette->set_pen_color(i, r, g, b);
	}

	m_mono_monitor = mono;
}


//-------------------------------------------------
//  memory: scratch RAM window and dot clock
//-------------------------------------------------

// the scratch RAM overlays the video RAM aperture, so every remap of the aperture has to put it back on top
void isa8_pega2a_device::install_banks()
{
	isa8_ega_device::install_banks();

	if (m_scratch_en)
		m_isa->install_memory(0xa0000, 0xa03ff, read8sm_delegate(*this, FUNC(isa8_pega2a_device::scratch_r)), write8sm_delegate(*this, FUNC(isa8_pega2a_device::scratch_w)));
}

void isa8_pega2a_device::scratch_map(bool enable)
{
	if (enable == m_scratch_en)
		return;

	m_scratch_en = enable;
	install_banks();
}

// Misc Output bits 3-2 select 14.318 MHz (from the bus), 16.257 MHz (EGA/MDA 350 line), 25.000 MHz and 27.256 MHz (the
// BIOS tables use the last two for the 480 line and 132 column modes)
uint32_t isa8_pega2a_device::dot_clock() const
{
	static const uint32_t clocks[4] = { 14318181, 16257000, 25000000, 27256000 };
	return clocks[(m_misc_output >> 2) & 3];
}


//-------------------------------------------------
//  I/O ports
//-------------------------------------------------

// 3B0-3BF: read as the generic EGA (the mono range reads FF while the card uses the colour range, as on the real card)
uint8_t isa8_pega2a_device::port_3b0_r(offs_t offset)
{
	return pc_ega8_3b0_r(offset);
}

// 3C0-3CF. Measured on the real card: 3C2 and 3C3 decode alike, the card drives bits 7-4 only and bits 3-0 show the low
// address bits; 3CC-3CF are not driven (the open bus shows the low address bits as well).
uint8_t isa8_pega2a_device::port_3c0_r(offs_t offset)
{
	if (offset == 2 || offset == 3)
	{
		const uint8_t sw = m_sw->read();
		uint8_t data = offset;
		data |= m_irq ? 0x00 : 0x80;
		data |= BIT(sw, 5) << 5; // lever 6
		data |= BIT(sw, 4) << 6; // lever 5
		data |= BIT(sw, 3 - ((m_misc_output >> 2) & 0x03)) << 4;
		return data;
	}

	if (offset >= 0x0c)
		return offset;

	return pc_ega8_3c0_r(offset);
}

// 3D0-3DF: 3D8 and 3DC-3DF are not driven by the real card (open bus, low address bits)
uint8_t isa8_pega2a_device::port_3d0_r(offs_t offset)
{
	if (offset == 0x08)
		return 0x18;
	if (offset >= 0x0c)
		return 0x10 | offset;

	return pc_ega8_3d0_r(offset);
}

void isa8_pega2a_device::port_3b0_w(offs_t offset, uint8_t data)
{
	LOGPORTS("%s: 3B%X <- %02x\n", machine().describe_context(), offset, data);
	pc_ega8_3b0_w(offset, data);
	crtc_and_trap_write(0x3b0 + offset, data);
}

void isa8_pega2a_device::port_3c0_w(offs_t offset, uint8_t data)
{
	LOGPORTS("%s: 3C%X <- %02x\n", machine().describe_context(), offset, data);
	pc_ega8_3c0_w(offset, data);
}

void isa8_pega2a_device::port_3d0_w(offs_t offset, uint8_t data)
{
	LOGPORTS("%s: 3D%X <- %02x\n", machine().describe_context(), offset, data);
	pc_ega8_3d0_w(offset, data);

	if (offset == 0x0f)
	{
		// 3DF: bit 6 opens the scratch RAM window, bit 7 arms the trap
		scratch_map(BIT(data, 6));
		if (BIT(data, 7))
			m_trap_armed = true;
	}

	crtc_and_trap_write(0x3d0 + offset, data);
}

// common part of the 3B0-3BF and 3D0-3DF writes: shadow, CRTC index tracking for the blanking registers, trap
void isa8_pega2a_device::crtc_and_trap_write(offs_t port, uint8_t data)
{
	m_scratch[port & 0x3ff] = data;

	// the CRTC answers in the range selected by Misc Output bit 0, at the even (index) and odd (data) ports 0-7
	const bool crtc_range = (port >= 0x3d0) == BIT(m_misc_output, 0);
	const uint8_t reg = port & 0x0f;
	if (crtc_range && reg < 8)
	{
		if (!(reg & 1))
		{
			m_crtc_index = data & 0x1f;
		}
		else
		{
			// the blanking registers do not make the CRTC reconfigure, but they decide how much border the monitor shows
			switch (m_crtc_index)
			{
			case 0x02: case 0x03: case 0x07: case 0x09: case 0x15: case 0x16:
				apply_blanking(false);
				break;
			}
		}
	}

	trap_write(port);
}

// The NMI is raised after the write has been done and the shadow byte stored. Writes to the control registers 3BB, 3DB
// and 3DF themselves do not trap.
void isa8_pega2a_device::trap_write(offs_t port)
{
	if (!m_trap_armed || m_scratch_en)
		return;

	if (port == 0x3bb || port == 0x3db || port == 0x3df)
		return;

	const bool color = BIT(m_misc_output, 0);
	if ((color && port >= 0x3d0) || (!color && port < 0x3c0))
	{
		LOGTRAP("%s: trap NMI, write to %03X\n", machine().describe_context(), port);
		m_trap_armed = false;
		m_isa->nmi();
	}
}


//-------------------------------------------------
//  screen: active display plus border
//-------------------------------------------------

// A raster line runs active - front porch - sync - back porch; the frame here starts after the sync pulse, so the active
// display sits one back porch in from the left edge.
CRTC_EGA_RECONFIGURE( isa8_pega2a_device::crtc_reconfigure )
{
	screen_timing &t = m_timing;

	t.width = width;
	t.height = height;
	t.frame_period = frame_period;
	t.active = visarea;
	t.hsync_off = hsync_off;
	t.vsync_off = vsync_off;
	t.x_off = width - hsync_off;
	t.y_off = height - vsync_off;

	const int visible_width = hsync_on + t.x_off;
	const int visible_height = vsync_on + t.y_off;

	// a CRTC programmed without any porch leaves nothing to draw a border in
	if (visible_width <= visarea.width() || visible_height <= visarea.height() || t.x_off < 0 || t.y_off < 0)
	{
		t.x_off = 0;
		t.y_off = 0;
		t.full = visarea;
	}
	else
	{
		t.full.set(0, visible_width - 1, 0, visible_height - 1);
	}

	apply_blanking(true);
}

// A monitor shows the border (overscan colour) only between the active display and the start of blanking, and between
// the end of blanking and the active display; during blanking the beam is off.
void isa8_pega2a_device::apply_blanking(bool force)
{
	screen_timing &t = m_timing;
	if (t.width == 0)
		return;

	const rectangle previous = t.visarea;
	t.visarea = t.full;

	if (t.x_off != 0 || t.y_off != 0)
	{
		const int hpix = m_crtc_ega->hpixels_per_column();

		// the EGA compares only the five low bits of the counters with the end of blanking
		const int hb_start = m_crtc_ega->horiz_blank_start();
		int hb_end = (hb_start & ~0x1f) | (m_crtc_ega->horiz_blank_end() & 0x1f);
		if (hb_end <= hb_start)
			hb_end += 0x20;
		int x_left = hb_end * hpix - t.hsync_off;
		int x_right = t.x_off + hb_start * hpix;

		const int vb_start = m_crtc_ega->vert_blank_start();
		int vb_end = (vb_start & ~0x1f) | (m_crtc_ega->vert_blank_end() & 0x1f);
		if (vb_end <= vb_start)
			vb_end += 0x20;
		int y_top = vb_end - t.vsync_off;
		int y_bottom = t.y_off + vb_start;

		x_left = std::max(0, std::min(x_left, t.x_off));
		y_top = std::max(0, std::min(y_top, t.y_off));
		x_right = std::min(std::max(x_right, t.x_off + t.active.width()), t.full.right() + 1);
		y_bottom = std::min(std::max(y_bottom, t.y_off + t.active.height()), t.full.bottom() + 1);

		// registers that are not programmed yet (reset, or half way through a mode set) must never give an empty area:
		// the screen divides by its size
		if (x_right > x_left && y_bottom > y_top)
			t.visarea.set(x_left, x_right - 1, y_top, y_bottom - 1);
	}

	// configure() restarts the screen timing, so a blanking register write that does not move the visible area must not
	// call it; a CRTC reconfigure (new total size or frame period) always does
	if (force || t.visarea != previous)
		m_screen->configure(t.width, t.height, t.visarea, t.frame_period);
}

uint32_t isa8_pega2a_device::screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
	const pen_t *const pens = m_palette->pens();

	bitmap.fill(pens[m_attribute.data[0x11] & 0x3f], cliprect);

	if (m_timing.width == 0)
		return 0;

	// crtc_ega draws the active display at the bitmap origin, so it renders into its own bitmap that is copied to where the
	// border leaves room for it
	const rectangle &active = m_timing.active;
	m_crtc_ega->screen_update(screen, m_ega_bitmap, active);

	for (int y = active.top(); y <= active.bottom(); y++)
	{
		const int dst_y = y + m_timing.y_off;
		if (!cliprect.containsy(dst_y))
			continue;

		uint16_t const *src = &m_ega_bitmap.pix(y, active.left());
		uint32_t *const dst = &bitmap.pix(dst_y);

		for (int x = active.left(); x <= active.right(); x++, src++)
		{
			const int dst_x = x + m_timing.x_off;
			if (cliprect.containsx(dst_x))
				dst[dst_x] = pens[*src & 0x3f];
		}
	}

	return 0;
}
