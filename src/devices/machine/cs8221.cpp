// license:BSD-3-Clause
// copyright-holders:Miodrag Milanovic
/***************************************************************************

    Chips & Technologies CS8221 chipset

    a.k.a. NEW ENHANCED AT (NEAT)

    Consists of four individual chips:

    * 82C211 - CPU/Bus controller
    * 82C212 - Page/Interleave and EMS Memory controller
    * 82C215 - Data/Address buffer
    * 82C206 - Integrated Peripherals Controller(IPC)

***************************************************************************/

#include "emu.h"
#include "cs8221.h"

#include "machine/ram.h"

#define LOG_REGISTER    (1U << 1)
#define LOG_MEMORY      (1U << 2)

#define VERBOSE (LOG_REGISTER | LOG_MEMORY)
#include "logmacro.h"

#define LOGREGISTER(...)    LOGMASKED(LOG_REGISTER, __VA_ARGS__)
#define LOGMEMORY(...)      LOGMASKED(LOG_MEMORY,   __VA_ARGS__)


DEFINE_DEVICE_TYPE(CS8221, cs8221_device, "cs8221", "CS8221 NEAT")


static const char *const register_names[] =
{
	/* 00 */ "PROCCLK",
	/* 01 */ "COMMAND DELAY",
	/* 02 */ "WAIT STATES",
	/* 03 */ "RESERVED",
	/* 04 */ "VERSION",
	/* 05 */ "ROM CONFIGURATION",
	/* 06 */ "MEMORY ENABLE-1",
	/* 07 */ "MEMORY ENABLE-2",
	/* 08 */ "MEMORY ENABLE-3",
	/* 09 */ "MEMORY ENABLE-4",
	/* 0a */ "BANK 0/1 ENABLE",
	/* 0b */ "DRAM CONFIGURATION",
	/* 0c */ "BANK 2/3 ENABLE",
	/* 0d */ "EMS BASE ADDRESS",
	/* 0e */ "EMS ADDRESS EXTENSION",
	/* 0f */ "MISCELLANEOUS"
};

//**************************************************************************
//  LIVE DEVICE
//**************************************************************************

//-------------------------------------------------
//  cs8221_device - constructor
//-------------------------------------------------

cs8221_device::cs8221_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, CS8221, tag, owner, clock),
		m_address(0),
		m_address_valid(false),
		m_cputag(nullptr),
		m_cpu(nullptr)
{
}

//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void cs8221_device::device_start()
{
	m_cpu = m_cputag ? machine().root_device().subdevice<cpu_device>(m_cputag) : nullptr;
	save_item(NAME(m_registers));
}

//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void cs8221_device::device_reset()
{
	// setup default values
	memset(&m_registers, 0x00, sizeof(m_registers));
}


//**************************************************************************
//  READ/WRITE HANDLERS
//**************************************************************************
// installed at 0x22-0x23 by the board, so the addresses here are relative to that range (absolute 0x22/0x23 put both
// entries outside the range and the writes went to the interrupt controller mirror instead)
void cs8221_device::map(address_map &map)
{
	map(0x00, 0x00).w(FUNC(cs8221_device::address_w));
	map(0x01, 0x01).rw(FUNC(cs8221_device::data_r), FUNC(cs8221_device::data_w));
}

void cs8221_device::address_w(uint8_t data)
{
	m_address = data;
	m_address_valid = ((m_address & 0x60)== 0x60) ? true : false;
}

uint8_t cs8221_device::data_r()
{
	uint8_t result = 0xff;

	if (m_address_valid)
	{
		LOGREGISTER("cs8221_device: read %s = %02x\n", register_names[m_address & 0x0f], m_registers[m_address & 0x0f]);

		result = m_registers[m_address & 0x0f];
	}

	// after a read the selected address needs to be reset
	m_address_valid = false;

	return result;
}

void cs8221_device::data_w(uint8_t data)
{
	if (m_address_valid)
	{
		LOGREGISTER("cs8221_device: write %s = %02x\n", register_names[m_address & 0x0f], data);

		const uint8_t old = m_registers[m_address & 0x0f];

		// update register with new data
		m_registers[m_address & 0x0f] = data;

		// execute command
		switch (m_address)
		{
			case 0x60:
				// PROCCLK (82C211): bit 4 selects the processor clock. Setting bit 5 resets the CPU without touching memory;
				// a 286 BIOS uses it to leave protected mode (the Nokia ROM BIOS 7.02 of the MikroMikko 3TT m215 stores
				// its resume address at 40:67 and shutdown code 0Ah in the CMOS, then toggles this bit and waits)
				if (!BIT(old, 5) && BIT(data, 5) && m_cpu)
				{
					LOGREGISTER("cs8221_device: CPU reset\n");
					m_cpu->pulse_input_line(INPUT_LINE_RESET, attotime::zero);
				}
				break;
			case 0x61: break;
			case 0x62: break;
			case 0x63: break;
			case 0x64: break;
			case 0x65: break;
			case 0x66: break;
			case 0x67: break;
			case 0x68: break;
			case 0x69: break;
			case 0x6a: break;
			case 0x6b: break;
			case 0x6c: break;
			case 0x6d: break;
			case 0x6e: break;
			case 0x6f: break;
		}
	}

	// after a write the selected address needs to be reset
	m_address_valid = false;
}
