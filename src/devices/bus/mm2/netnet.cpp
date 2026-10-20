// license:BSD-3-Clause
// copyright-holders:Mr. MikroMikko
/**********************************************************************

    Nokia MikroMikko 2 NETNET local area network (experimental)

    See netnet.h.

**********************************************************************/

#include "emu.h"
#include "netnet.h"

DEFINE_DEVICE_TYPE(MM2_NETNET, mm2_netnet_device, "mm2_netnet", "Nokia MikroMikko 2 NETNET")

// 500 kbit/s line rate, the timer runs at twice the bit rate
static constexpr int NETNET_BITRATE = 500000;
static constexpr unsigned POLL_TICKS = 2000;

mm2_netnet_device::mm2_netnet_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, MM2_NETNET, tag, owner, clock),
	m_bitb(*this, "bitb"),
	m_txc_cb(*this),
	m_rxc_cb(*this),
	m_rxd_cb(*this),
	m_timer(nullptr),
		m_poll_timer(nullptr),
	m_enabled(false),
	m_clk(0),
	m_txd(1),
	m_rxd(1),
	m_poll_count(0),
	m_ones(0),
	m_in_frame(false)
{
}

void mm2_netnet_device::device_add_mconfig(machine_config &config)
{
	BITBANGER(config, m_bitb, 0);
}

void mm2_netnet_device::device_start()
{
	m_timer = timer_alloc(FUNC(mm2_netnet_device::clock_tick), this);
	m_poll_timer = timer_alloc(FUNC(mm2_netnet_device::poll_tick), this);
	// the adapter listens to the line whether or not the station has selected the external clock
	m_poll_timer->adjust(attotime::from_msec(2), 0, attotime::from_msec(2));

	save_item(NAME(m_enabled));
	save_item(NAME(m_clk));
	save_item(NAME(m_txd));
	save_item(NAME(m_rxd));
}

void mm2_netnet_device::device_reset()
{
	enable(false);
	m_txd = 1;
	m_rxd = 1;
	m_ones = 0;
	m_in_frame = false;
	m_tx_bits.clear();
	m_rx_bits.clear();
	m_rx_frames.clear();
	m_sock_buf.clear();
}

void mm2_netnet_device::enable(bool state)
{
	if (state == m_enabled)
		return;

	m_enabled = state;
	if (state)
	{
		m_clk = 0;
		m_ones = 0;
		m_in_frame = false;
		m_tx_bits.clear();
		attotime const half = attotime::from_hz(NETNET_BITRATE * 2);
		m_timer->adjust(half, 0, half);
	}
	else
	{
		m_timer->adjust(attotime::never);
		if (m_clk)
		{
			m_clk = 0;
			m_txc_cb(0);
			m_rxc_cb(0);
		}
	}
}

TIMER_CALLBACK_MEMBER(mm2_netnet_device::poll_tick)
{
	rx_poll_socket();
}

TIMER_CALLBACK_MEMBER(mm2_netnet_device::clock_tick)
{
	m_clk ^= 1;
	m_txc_cb(m_clk);
	m_rxc_cb(m_clk);

	if (m_clk)
	{
		// the MPSC changes TxD on the falling edge, sample it on the rising edge
		tx_bit(m_txd);
	}

	else
	{
		// the MPSC samples RxD on the rising edge, change it on the falling edge
		int bit = 1;
		if (m_rx_bits.empty() && !m_rx_frames.empty())
		{
			rx_make_bits(m_rx_frames.front());
			m_rx_frames.pop_front();
		}
		if (!m_rx_bits.empty())
		{
			bit = m_rx_bits.front();
			m_rx_bits.pop_front();
		}
		if (bit != m_rxd)
		{
			m_rxd = bit;
			m_rxd_cb(bit);
		}
	}
}

//-------------------------------------------------
//  SDLC bit stream -> frames
//-------------------------------------------------

void mm2_netnet_device::tx_bit(int bit)
{
	if (bit)
	{
		m_ones++;
		if (m_ones >= 7)
		{
			// abort or idle
			m_tx_bits.clear();
			m_in_frame = false;
			m_ones = 7;
		}
		else
			m_tx_bits.push_back(1);
		return;
	}

	if (m_ones == 5)
	{
		// stuffed zero, drop it
	}
	else if (m_ones == 6)
	{
		// flag: remove the six ones and the leading zero
		if (m_tx_bits.size() >= 6)
			m_tx_bits.resize(m_tx_bits.size() - 6);
		if (!m_tx_bits.empty())
			m_tx_bits.pop_back();
		tx_frame_done();
		m_tx_bits.clear();
		m_in_frame = true;
	}
	else
		m_tx_bits.push_back(0);

	m_ones = 0;
}

void mm2_netnet_device::tx_frame_done()
{
	if (!m_in_frame || m_tx_bits.empty())
		return;

	if ((m_tx_bits.size() % 8) != 0 || m_tx_bits.size() < 24)
	{
		logerror("NETNET: bad frame length %u bits dropped\n", unsigned(m_tx_bits.size()));
		return;
	}

	std::vector<uint8_t> frame(m_tx_bits.size() / 8);
	for (size_t i = 0; i < m_tx_bits.size(); i++)
	{
		if (m_tx_bits[i])
			frame[i / 8] |= 1 << (i % 8);
	}

	log_frame("TX", frame);
	send_frame(frame);
}

void mm2_netnet_device::send_frame(const std::vector<uint8_t> &frame)
{
	if (!m_bitb->exists())
		return;

	std::vector<uint8_t> buf;
	buf.reserve(frame.size() + 2);
	buf.push_back(frame.size() & 0xff);
	buf.push_back((frame.size() >> 8) & 0xff);
	buf.insert(buf.end(), frame.begin(), frame.end());
	uint32_t const written = m_bitb->fwrite(buf.data(), buf.size());
	if (written != buf.size())
		logerror("NETNET socket write failed (%u of %u)\n", written, unsigned(buf.size()));
}

//-------------------------------------------------
//  frames -> SDLC bit stream
//-------------------------------------------------

void mm2_netnet_device::rx_poll_socket()
{
	if (!m_bitb->exists())
		return;

	uint8_t buf[512];
	uint32_t const n = m_bitb->input(buf, sizeof(buf));
	if (n)
		m_sock_buf.insert(m_sock_buf.end(), buf, buf + n);

	while (m_sock_buf.size() >= 2)
	{
		size_t const len = m_sock_buf[0] | (m_sock_buf[1] << 8);
		if (m_sock_buf.size() < 2 + len)
			break;

		std::vector<uint8_t> frame(m_sock_buf.begin() + 2, m_sock_buf.begin() + 2 + len);
		m_sock_buf.erase(m_sock_buf.begin(), m_sock_buf.begin() + 2 + len);
		log_frame("RX", frame);
		m_rx_frames.push_back(std::move(frame));
	}
}

void mm2_netnet_device::rx_make_bits(const std::vector<uint8_t> &frame)
{
	auto flag = [this]()
	{
		static const uint8_t f[8] = { 0, 1, 1, 1, 1, 1, 1, 0 };
		for (uint8_t b : f)
			m_rx_bits.push_back(b);
	};

	flag();
	flag();

	int ones = 0;
	for (uint8_t byte : frame)
	{
		for (int i = 0; i < 8; i++)
		{
			int const b = BIT(byte, i);
			m_rx_bits.push_back(b);
			if (b)
			{
				if (++ones == 5)
				{
					m_rx_bits.push_back(0);
					ones = 0;
				}
			}
			else
				ones = 0;
		}
	}

	flag();
	for (int i = 0; i < 16; i++)
		m_rx_bits.push_back(1);
}

void mm2_netnet_device::log_frame(const char *dir, const std::vector<uint8_t> &frame)
{
	std::string s;
	for (size_t i = 0; i < frame.size() && i < 48; i++)
		s += util::string_format("%02x ", frame[i]);
	logerror("NETNET %s len=%u: %s%s\n", dir, unsigned(frame.size()), s, frame.size() > 48 ? "..." : "");
}
