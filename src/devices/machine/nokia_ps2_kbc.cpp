// license:BSD-3-Clause
// copyright-holders:Mr. MikroMikko
/***************************************************************************

    PS/2-type keyboard controller, high level emulation (see the header)

    Not emulated: the password commands (A5 is accepted and ignored), the controller RAM above the command byte
    has no effect, and the parity error resend that the firmware does on its own.

***************************************************************************/

#include "emu.h"
#include "nokia_ps2_kbc.h"

#define LOG_CMD (1U << 1)

//#define VERBOSE (LOG_CMD)
#include "logmacro.h"


DEFINE_DEVICE_TYPE(NOKIA_PS2_KBC, nokia_ps2_kbc_device, "nokia_ps2_kbc", "PS/2-type keyboard controller (HLE)")

namespace {

// scan code set 2 -> set 1, the translation of the 8042 (command byte bit 6)
const u8 xlat_table[0x80] = {
	0xff, 0x43, 0x41, 0x3f, 0x3d, 0x3b, 0x3c, 0x58, 0x64, 0x44, 0x42, 0x40, 0x3e, 0x0f, 0x29, 0x59,
	0x65, 0x38, 0x2a, 0x70, 0x1d, 0x10, 0x02, 0x5a, 0x66, 0x71, 0x2c, 0x1f, 0x1e, 0x11, 0x03, 0x5b,
	0x67, 0x2e, 0x2d, 0x20, 0x12, 0x05, 0x04, 0x5c, 0x68, 0x39, 0x2f, 0x21, 0x14, 0x13, 0x06, 0x5d,
	0x69, 0x31, 0x30, 0x23, 0x22, 0x15, 0x07, 0x5e, 0x6a, 0x72, 0x32, 0x24, 0x16, 0x08, 0x09, 0x5f,
	0x6b, 0x33, 0x25, 0x17, 0x18, 0x0b, 0x0a, 0x60, 0x6c, 0x34, 0x35, 0x26, 0x27, 0x19, 0x0c, 0x61,
	0x6d, 0x73, 0x28, 0x74, 0x1a, 0x0d, 0x62, 0x6e, 0x3a, 0x36, 0x1c, 0x1b, 0x75, 0x2b, 0x63, 0x76,
	0x55, 0x56, 0x77, 0x78, 0x79, 0x7a, 0x0e, 0x7b, 0x7c, 0x4f, 0x7d, 0x4b, 0x47, 0x7e, 0x7f, 0x6f,
	0x52, 0x53, 0x50, 0x4c, 0x4d, 0x48, 0x01, 0x45, 0x57, 0x4e, 0x51, 0x4a, 0x37, 0x49, 0x46, 0x54 };

// the host holds the clock low at least this long before it sends a byte to the keyboard
constexpr attotime tx_inhibit = attotime::from_usec(110);

// no clock from the keyboard within this time: transmit timeout
constexpr attotime kbd_timeout = attotime::from_msec(15);

// a command for the auxiliary port fails after this time (no device connected)
constexpr attotime aux_timeout = attotime::from_msec(2);

unsigned ones(unsigned data)
{
	unsigned n = 0;
	for ( ; data; data >>= 1)
		n += data & 1;
	return n;
}

} // anonymous namespace


nokia_ps2_kbc_device::nokia_ps2_kbc_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, NOKIA_PS2_KBC, tag, owner, clock)
	, m_hot_res_cb(*this)
	, m_gate_a20_cb(*this)
	, m_kbd_irq_cb(*this)
	, m_aux_irq_cb(*this)
	, m_kbd_clk_cb(*this)
	, m_kbd_data_cb(*this)
	, m_inport(0xbc)
{
}

void nokia_ps2_kbc_device::device_start()
{
	m_tx_timer = timer_alloc(FUNC(nokia_ps2_kbc_device::tx_start), this);
	m_timeout_timer = timer_alloc(FUNC(nokia_ps2_kbc_device::timeout), this);
	m_reset_timer = timer_alloc(FUNC(nokia_ps2_kbc_device::reset_pulse_end), this);

	m_clk_in = m_data_in = 1;
	m_clk_out = m_data_out = 1;

	save_item(NAME(m_inport));
	save_item(NAME(m_outport));
	save_item(NAME(m_ram));
	save_item(NAME(m_status));
	save_item(NAME(m_output));
	save_item(NAME(m_pending));
	save_item(NAME(m_xlat_break));
	save_item(NAME(m_clk_in));
	save_item(NAME(m_data_in));
	save_item(NAME(m_clk_out));
	save_item(NAME(m_data_out));
	save_item(NAME(m_rx_bit));
	save_item(NAME(m_rx_shift));
	save_item(NAME(m_tx_active));
	save_item(NAME(m_tx_byte));
	save_item(NAME(m_tx_bit));
	save_item(NAME(m_response_window));
}

void nokia_ps2_kbc_device::device_reset()
{
	std::fill(std::begin(m_ram), std::end(m_ram), 0);
	m_status = 0;
	m_output = 0;
	m_pending = 0;
	m_xlat_break = 0;
	m_rx_bit = 0;
	m_rx_shift = 0;
	m_tx_active = false;
	m_tx_byte = 0;
	m_tx_bit = 0;
	m_response_window = 0;

	m_tx_timer->enable(false);
	m_timeout_timer->enable(false);
	m_reset_timer->enable(false);

	m_hot_res_cb(CLEAR_LINE);
	set_outport(0xff); // port pins come out of reset high: A20 open, CPU reset released
	update_irqs();
	update_lines();
}


//**************************************************************************
//  HOST INTERFACE
//**************************************************************************

uint8_t nokia_ps2_kbc_device::data_r()
{
	u8 const data = m_output;
	if (!machine().side_effects_disabled() && (m_status & ST_OBF))
	{
		m_status &= ~(ST_OBF | ST_AUXB | ST_TIME | ST_PERR);
		update_irqs();
		update_lines();
	}
	return data;
}

uint8_t nokia_ps2_kbc_device::status_r()
{
	return m_status | (command_byte() & CB_SYS) | ST_KEYL;
}

void nokia_ps2_kbc_device::command_w(uint8_t data)
{
	LOGMASKED(LOG_CMD, "command %02x\n", data);
	m_status |= ST_CMD;
	m_pending = 0;

	if (data >= 0x20 && data < 0x40)
	{
		put_output(m_ram[data & 0x1f], false);
		return;
	}
	if (data >= 0x60 && data < 0x80)
	{
		m_pending = data;
		return;
	}
	if (data >= 0xf0)
	{
		// pulse output port bits 3-0 low; only bit 0 (CPU reset) is connected
		if (!BIT(data, 0))
		{
			m_hot_res_cb(ASSERT_LINE);
			m_reset_timer->adjust(attotime::from_usec(6));
		}
		return;
	}

	switch (data)
	{
	case 0xa4: // password installed?
		put_output(0xf1, false);
		break;

	case 0xa5: // load password, terminated by 00
	case 0xd1: // write output port
	case 0xd2: // write keyboard output buffer
	case 0xd3: // write auxiliary output buffer
	case 0xd4: // write to auxiliary device
		m_pending = data;
		break;

	case 0xa6: // enable security
		break;

	case 0xa7: // disable auxiliary interface
		m_ram[0] |= CB_AUX_DIS;
		break;

	case 0xa8: // enable auxiliary interface
		m_ram[0] &= ~CB_AUX_DIS;
		break;

	case 0xa9: // auxiliary interface test: lines fine
	case 0xab: // keyboard interface test
		put_output(0x00, false);
		break;

	case 0xaa: // self test
		put_output(0x55, false);
		break;

	case 0xad: // disable keyboard interface
		m_ram[0] |= CB_KBD_DIS;
		update_lines();
		break;

	case 0xae: // enable keyboard interface
		m_ram[0] &= ~CB_KBD_DIS;
		update_lines();
		break;

	case 0xc0: // read input port
		put_output((m_inport & 0xfc) | 0x02 | m_data_in, false);
		break;

	case 0xc1: // poll input port into the status register
	case 0xc2:
		break;

	case 0xd0: // read output port
		put_output((m_outport & 0x0f) | ((m_status & ST_OBF) && !(m_status & ST_AUXB) ? 0x10 : 0x00) |
				((m_status & ST_OBF) && (m_status & ST_AUXB) ? 0x20 : 0x00) | (m_clk_out << 6) | (m_data_out << 7), false);
		break;

	case 0xe0: // read test inputs: T0 = keyboard clock, T1 = auxiliary clock
		put_output(m_clk_in | 0x02, false);
		break;

	default:
		logerror("unknown command %02x\n", data);
		break;
	}
}

void nokia_ps2_kbc_device::data_w(uint8_t data)
{
	m_status &= ~ST_CMD;
	u8 const pending = m_pending;
	m_pending = 0;

	if (pending >= 0x60 && pending < 0x80)
	{
		LOGMASKED(LOG_CMD, "RAM %02x = %02x\n", pending & 0x1f, data);
		m_ram[pending & 0x1f] = data;
		if (pending == 0x60)
		{
			update_irqs();
			update_lines();
		}
		return;
	}

	switch (pending)
	{
	case 0xa5:
		if (data)
			m_pending = pending;
		break;

	case 0xd1:
		set_outport(data);
		break;

	case 0xd2:
		put_output(data, false);
		break;

	case 0xd3:
		put_output(data, true);
		break;

	case 0xd4: // nothing is connected to the auxiliary port: the transfer times out
		m_ram[0] &= ~CB_AUX_DIS;
		m_timeout_timer->adjust(aux_timeout, 1);
		break;

	default:
		send_to_keyboard(data);
		break;
	}
}

void nokia_ps2_kbc_device::put_output(u8 data, bool aux, u8 extra_status)
{
	m_output = data;
	m_status = (m_status & ~(ST_AUXB | ST_TIME | ST_PERR)) | ST_OBF | (aux ? ST_AUXB : 0) | extra_status;
	update_irqs();
	update_lines();
}

void nokia_ps2_kbc_device::update_irqs()
{
	bool const full = m_status & ST_OBF;
	bool const aux = m_status & ST_AUXB;
	m_kbd_irq_cb((full && !aux && (command_byte() & CB_KBD_INT)) ? ASSERT_LINE : CLEAR_LINE);
	m_aux_irq_cb((full && aux && (command_byte() & CB_AUX_INT)) ? ASSERT_LINE : CLEAR_LINE);
}

void nokia_ps2_kbc_device::set_outport(u8 data)
{
	m_outport = data;
	m_gate_a20_cb(BIT(data, 1) ? ASSERT_LINE : CLEAR_LINE);
	m_hot_res_cb(BIT(data, 0) ? CLEAR_LINE : ASSERT_LINE);
}

TIMER_CALLBACK_MEMBER(nokia_ps2_kbc_device::reset_pulse_end)
{
	m_hot_res_cb(BIT(m_outport, 0) ? CLEAR_LINE : ASSERT_LINE);
}

TIMER_CALLBACK_MEMBER(nokia_ps2_kbc_device::timeout)
{
	if (param)
	{
		put_output(0xfe, true, ST_TIME);
		return;
	}

	// the keyboard did not clock the byte in
	logerror("keyboard transmit timeout (%02x)\n", m_tx_byte);
	m_tx_active = false;
	m_response_window = 0;
	m_data_out = 1;
	m_kbd_data_cb(1);
	put_output(0xfe, false, ST_TIME);
}


//**************************************************************************
//  KEYBOARD SERIAL INTERFACE
//**************************************************************************

void nokia_ps2_kbc_device::update_lines()
{
	if (m_tx_active)
		return;

	// inhibit the keyboard while the interface is disabled or the output buffer is full
	bool const disabled = (command_byte() & CB_KBD_DIS) && !m_response_window;
	u8 const clk = (disabled || (m_status & ST_OBF)) ? 0 : 1;
	if (!clk)
		m_rx_bit = 0;
	if (clk != m_clk_out)
	{
		m_clk_out = clk;
		m_kbd_clk_cb(clk);
	}
}

void nokia_ps2_kbc_device::send_to_keyboard(u8 data)
{
	LOGMASKED(LOG_CMD, "to keyboard %02x\n", data);
	m_tx_active = true;
	m_tx_byte = data;
	m_tx_bit = 0;
	m_rx_bit = 0;
	m_response_window = (data == 0xff) ? 2 : 1; // reset answers FA and then AA

	// request to send: hold the clock low, then pull data low (start bit) and release the clock
	m_clk_out = 0;
	m_kbd_clk_cb(0);
	m_tx_timer->adjust(tx_inhibit);
}

TIMER_CALLBACK_MEMBER(nokia_ps2_kbc_device::tx_start)
{
	m_timeout_timer->adjust(kbd_timeout, 0);
	m_data_out = 0;
	m_kbd_data_cb(0);
	m_clk_out = 1;
	m_kbd_clk_cb(1);
}

void nokia_ps2_kbc_device::kbd_data_w(int state)
{
	m_data_in = state ? 1 : 0;
}

void nokia_ps2_kbc_device::kbd_clk_w(int state)
{
	u8 const old = m_clk_in;
	m_clk_in = state ? 1 : 0;

	// only falling edges made by the keyboard count
	if (!old || m_clk_in || !m_clk_out)
		return;

	if (m_tx_active)
	{
		if (m_tx_bit < 8)
			m_data_out = BIT(m_tx_byte, m_tx_bit);
		else if (m_tx_bit == 8)
			m_data_out = (ones(m_tx_byte) & 1) ? 0 : 1; // odd parity
		else
			m_data_out = 1; // stop bit; the keyboard acknowledges on the next clock

		if (m_tx_bit == 10)
		{
			m_tx_active = false;
			m_timeout_timer->enable(false);
			m_rx_bit = 0;
			update_lines();
		}
		else
		{
			m_kbd_data_cb(m_data_out);
			m_tx_bit++;
		}
		return;
	}

	// receive: start bit, 8 data bits (LSB first), odd parity, stop bit
	if (m_rx_bit == 0)
	{
		if (m_data_in)
			return; // no start bit
		m_rx_shift = 0;
	}
	else if (m_rx_bit <= 9)
	{
		m_rx_shift |= m_data_in << (m_rx_bit - 1);
	}
	m_rx_bit++;

	if (m_rx_bit == 11)
	{
		m_rx_bit = 0;
		u8 const data = m_rx_shift & 0xff;
		bool const parity_ok = (ones(m_rx_shift & 0x1ff) & 1) != 0;
		if (!m_data_in || !parity_ok)
			put_output(0xff, false, ST_PERR);
		else
			received_from_keyboard(data);
	}
}

void nokia_ps2_kbc_device::received_from_keyboard(u8 data)
{
	if (m_response_window)
		m_response_window--;

	if (command_byte() & CB_XLAT)
	{
		if (data == 0xf0)
		{
			m_xlat_break = 0x80;
			update_lines();
			return;
		}
		if (data < 0x80)
			data = xlat_table[data] | m_xlat_break;
		else if (data == 0x83)
			data = 0x41 | m_xlat_break;
		m_xlat_break = 0;
	}

	put_output(data, false);
}
