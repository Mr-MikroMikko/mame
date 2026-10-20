// license:BSD-3-Clause
// copyright-holders:Mr. MikroMikko
/**********************************************************************

    Nokia MikroMikko 2 NETNET local area network (experimental)

    NETNET is the SDLC based LAN of the Nokia PC-Net network operating
    system. The station talks to the network through channel A of the
    MPSC (8274) using an external clock supplied by the NETNET adapter
    (CLS1=1, CLS0=0).

    This emulation does not model the cable and the line drivers. It
    generates the line clock, decodes the SDLC bit stream transmitted
    by the MPSC into frames (flag detection, zero bit unstuffing) and
    exchanges the frames with another emulated station over a
    bitbanger (use -bitb socket.host:port / socket.:port). A received
    frame is turned back into a bit stream (flags, zero bit stuffing)
    and shifted into the MPSC receiver. The CRC is calculated by the
    MPSC and is carried unchanged inside the frames.

**********************************************************************/

#ifndef MAME_BUS_MM2_NETNET_H
#define MAME_BUS_MM2_NETNET_H

#pragma once

#include "imagedev/bitbngr.h"

#include <deque>
#include <vector>

class mm2_netnet_device : public device_t
{
public:
	mm2_netnet_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	auto txc_handler() { return m_txc_cb.bind(); }
	auto rxc_handler() { return m_rxc_cb.bind(); }
	auto rxd_handler() { return m_rxd_cb.bind(); }

	void txd_w(int state) { m_txd = state; }
	void enable(bool state);

protected:
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

private:
	TIMER_CALLBACK_MEMBER(clock_tick);
	void tx_bit(int bit);
	void tx_frame_done();
	void rx_poll_socket();
	void rx_make_bits(const std::vector<uint8_t> &frame);
	void send_frame(const std::vector<uint8_t> &frame);
	void log_frame(const char *dir, const std::vector<uint8_t> &frame);

	required_device<bitbanger_device> m_bitb;
	devcb_write_line m_txc_cb;
	devcb_write_line m_rxc_cb;
	devcb_write_line m_rxd_cb;
	emu_timer *m_timer;

	bool m_enabled;
	int m_clk;
	int m_txd;
	int m_rxd;
	unsigned m_poll_count;

	// transmit decoder
	int m_ones;
	bool m_in_frame;
	std::vector<uint8_t> m_tx_bits;

	// receive generator
	std::deque<uint8_t> m_rx_bits;
	std::deque<std::vector<uint8_t>> m_rx_frames;
	std::vector<uint8_t> m_sock_buf;
};

DECLARE_DEVICE_TYPE(MM2_NETNET, mm2_netnet_device)

#endif // MAME_BUS_MM2_NETNET_H
