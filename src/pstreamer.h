/*******************************************************************
* PeerStreamer-ng is a P2P video streaming application exposing a ReST
* interface.
* Copyright (C) 2017 Luca Baldesi <luca.baldesi@unitn.it>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU Affero General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Affero General Public License for more details.
*
* You should have received a copy of the GNU Affero General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*******************************************************************/

#ifndef __PSTREAMER_H__
#define __PSTREAMER_H__ 

#include<ord_set.h>
#include<stdint.h>
#include<name_lengths.h>
#include<task_manager.h>
#include<pstreamer_event.h>
#include<mongoose.h>

struct pstreamer;
struct pstreamer_manager;

struct pstreamer_manager * pstreamer_manager_new(uint16_t starting_port);

void pstreamer_manager_destroy(struct pstreamer_manager ** psm);

const struct pstreamer * pstreamer_manager_create_streamer(struct pstreamer_manager * psm, const char * source_ip, const char * source_port, const char * id, const char * rtp_dst_ip);

char * pstreamer_to_json(const struct pstreamer * ps);

uint8_t pstreamer_manager_destroy_streamer(struct pstreamer_manager *psm, const struct pstreamer *ps);

const char * pstreamer_id(const struct pstreamer * ps);

uint16_t pstreamer_base_port(const struct pstreamer * ps);

int8_t pstreamer_schedule_tasks(struct pstreamer *ps, struct task_manager * tm);

const struct pstreamer * pstreamer_manager_get_streamer(const struct pstreamer_manager *psm, const char * id);

void pstreamer_manager_remove_orphans(struct pstreamer_manager * psm, time_t interval);

void pstreamer_touch(struct pstreamer *ps);

int8_t pstreamer_manager_set_streamer_options(struct pstreamer_manager *psm, const char * opts);

void pstreamer_check_closed_connection(struct pstreamer_manager *psm,
				       struct mg_connection *nc);

void pstreamer_set_ffmuxer_http_connection(const struct pstreamer *ps,
					   struct mg_connection *nc);

int pstreamer_register_ffmuxer_rtp_fds(const struct pstreamer * ps,
			       fd_register_f func, void *handler);
timeout pstreamer_get_ffmuxer_http_timeout(const struct pstreamer *ps);
int pstreamer_handle_ffmuxer(struct pstreamer *ps);

void pstreamer_set_ffmuxer_http_root(const struct pstreamer *ps,
				     const char *root);

#endif
