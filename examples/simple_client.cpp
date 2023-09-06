/*

Copyright (c) 2003-2017, Arvid Norberg
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the distribution.
    * Neither the name of the author nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

*/


#include <cstdlib>
#include <iostream>

#include "print.hpp"
#include "libtorrent/entry.hpp"
#include "libtorrent/bencode.hpp"
#include "libtorrent/session.hpp"
#include "libtorrent/torrent_info.hpp"
#include "libtorrent/alert.hpp"
#include "libtorrent/settings_pack.hpp"

using namespace libtorrent;

FILE* g_log_file = nullptr;

char const* timestamp()
{
	time_t t = std::time(nullptr);
	tm* timeinfo = std::localtime(&t);
	static char str[200];
	std::strftime(str, 200, "%b %d %X", timeinfo);
	return str;
}

void pop_alerts(lt::session& ses)
{
	std::vector<lt::alert*> alerts;
	ses.pop_alerts(&alerts);
	for (auto a : alerts)
	{
		if (g_log_file)
			std::fprintf(g_log_file, "%s %s\n", timestamp(),  a->message().c_str());
		else
			std::cout << timestamp() << " " << a->message() << std::endl;
	}
}

int main(int argc, char* argv[]) try
{
	if (argc != 8) {
		std::cerr << "usage: ./simple_client <save path> <torrent seed file> <log file path> <whether enable compression> <whether auto exit> <white upload list> <group members>\n";
		return 1;
	}

	lt::session_params params;
	auto& settings = params.settings;

	settings = lt::high_performance_seed();
	settings.set_int(settings_pack::cache_size, -1);
	settings.set_int(settings_pack::choking_algorithm, settings_pack::rate_based_choker);
	settings.set_bool(settings_pack::enable_incoming_utp, false);
	settings.set_bool(settings_pack::enable_outgoing_utp, false);
	settings.set_bool(settings_pack::enable_incoming_tcp, true);
	settings.set_bool(settings_pack::enable_outgoing_tcp, true);
	settings.set_bool(settings_pack::enable_dht, false);
	settings.set_int(settings_pack::max_allowed_in_request_queue, 20000);
    settings.set_int(settings_pack::max_out_request_queue, 20000);
	settings.set_bool(settings_pack::smooth_connects, false);
	settings.set_bool(settings_pack::auto_sequential, false);
	settings.set_int(settings_pack::aio_threads, 30);

	std::string log_file_path(argv[3]);
	if (log_file_path != "console") {
		g_log_file = std::fopen(log_file_path.c_str(), "a+");
	}

	bool auto_exit;
	std::string s_auto_exit(argv[5]);
	if (s_auto_exit == "true" || s_auto_exit == "True" || s_auto_exit == "True") {
		auto_exit = true;
	} else {
		auto_exit = false;
	}

	settings.set_int(settings_pack::alert_mask
		, alert_category::error
		| alert_category::peer
		| alert_category::port_mapping
		| alert_category::storage
		| alert_category::tracker
		| alert_category::connect
		| alert_category::status
		| alert_category::ip_block
		| alert_category::performance_warning
		| alert_category::dht
		| alert_category::stats
		| alert_category::session_log
		| alert_category::torrent_log
		| alert_category::peer_log
		| alert_category::incoming_request
		| alert_category::dht_operation
		| alert_category::port_mapping_log
		| alert_category::file_progress);


	lt::session ses(std::move(params));
	lt::add_torrent_params p;
	p.save_path = argv[1];
//	p.flags |= lt::torrent_flags::share_mode;
//    p.flags |= lt::torrent_flags::seed_mode;
	p.ti = std::make_shared<lt::torrent_info>(argv[2]);
	p.group_members = argv[7];
	std::string enable_compression(argv[4]);
	if (enable_compression == "true" || enable_compression == "True" || enable_compression == "TRUE") {
		p.enable_compression = true;
	} else {
		p.enable_compression = false;
	}
	p.upload_white_list = argv[6];
	auto handle = ses.add_torrent(p);

	while (!(handle.is_finished() && auto_exit))
	{
		libtorrent::alert const* a = ses.wait_for_alert(std::chrono::seconds(2));
		if (a == nullptr) continue;
		pop_alerts(ses);
	}
}
catch (std::exception const& e) {
	std::cerr << "ERROR: " << e.what() << "\n";
}
