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

#include<signal.h>
#include<string.h>
#include<router.h>
#include<path_handlers.h>
#include<debug.h>
#include<task_manager.h>
#include<context.h>
#include<pschannel.h>
#include<periodic_task_intfs.h>
#include<pstreamer.h>

#include<mongoose.h>


static uint8_t running = 1;

void sig_exit(int signo)
{
	running = 0;
}

void show_help()
{
	fprintf(stdout, "This is PeerStreamer-ng, a P2P live streaming platform\n");
	fprintf(stdout, "Options:\n");
	fprintf(stdout, "\t-p <http_port>:\t\tport number of the ReST HTTP server\n");
	fprintf(stdout, "\t-s <csv_key_values>:\tcomma separated key=value string for advanced configuration\n");
	fprintf(stdout, "\t-c <csv_channel_file>:\tfile with available remote channels\n");
	fprintf(stdout, "\t-f <http_folder>:\tfolder for HTTP accessible files\n");
	fprintf(stdout, "\t-v:\t\t\tverbose output\n");
	fprintf(stdout, "\t-q:\t\t\tquiet output\n");
	fprintf(stdout, "\t-h:\t\t\tshows this help\n");
	fprintf(stdout, "\n");

}

void parse_args(struct context *c, int argc, char *const* argv)
{
	int o;

	while ((o = getopt (argc, argv, "hvqp:c:s:f:")) != -1)
		switch (o) {
			case 'f':
				free((char *)c->http_opts.document_root);
				c->http_opts.document_root = strdup(optarg);
				break;
			case 'p':
				strncpy(c->http_port, optarg, 16);
				break;
			case 'c':
				c->csvfile = strdup(optarg);
				break;
			case 's':
				c->config_string = strdup(optarg);
				break;
			case 'q':
				set_debug(0);
				break;
			case 'v':
				set_debug(2);
				break;
			case 'h':
				show_help();
				running = 0;
				break;
		}
}

void mg_request_decode(char * buff, int buff_len, const struct http_message *hm)
{
	size_t i = 0;

	if (buff_len - i -2 > hm->method.len)
	{
		memmove(buff + i, hm->method.p, hm->method.len);
		i += hm->method.len;
	}
	buff[i++] = ' ';
	if (buff_len - i -2 > hm->uri.len)
	{
		memmove(buff + i, hm->uri.p, hm->uri.len);
		i += hm->uri.len;
	}
	buff[i++] = ' ';
	if (buff_len - i -1 > hm->query_string.len)
	{
		memmove(buff + i, hm->query_string.p, hm->query_string.len);
		i += hm->query_string.len;
	}
	buff[i] = '\0';
}

void ev_handler(struct mg_connection *nc, int ev, void *ev_data)
{
	struct context *c = nc->user_data;
	struct http_message *hm;
	char buff[80];

	switch (ev) {
		case MG_EV_HTTP_REQUEST:
			hm  = (struct http_message *) ev_data;
			info("Received a request:\n");
			mg_request_decode(buff, 80, hm);
			info("\t%s\n", buff);
			// Try to call a path handler. If it fails serve
			// public contents
			if(router_handle(c->router, nc, hm))
				mg_serve_http(nc, hm, c->http_opts);
			break;
		case MG_EV_CLOSE:
			nc->flags |= CONNECTION_CLOSED;
			break;
		default:
			break;
	}
}

void init(struct context *c, int argc, char **argv)
{
	strncpy(c->http_port, "3000", 16);
	signal(SIGINT, sig_exit);

	c->http_opts.enable_directory_listing = "no";
	c->http_opts.document_root = strdup("Public/");
	c->http_opts.index_files = "index.html,player.html";
	c->csvfile = NULL;
	c->config_string = NULL;
	set_debug(1);
	parse_args(c, argc, argv);

	if (running)
	{
		c->router = router_create(10);
		load_path_handlers(c->router);
		c->tm = task_manager_new();

		c->mongoose_srv = (struct mg_mgr*) malloc(sizeof(struct mg_mgr));
		mg_mgr_init(c->mongoose_srv, c);

		c->janus = janus_instance_create(c->mongoose_srv, c->tm, c->config_string);
		janus_instance_launch(c->janus);
		c->psm = pstreamer_manager_new(6001, c->janus);

		pstreamer_manager_set_streamer_options(c->psm, c->config_string);
		c->pb = pschannel_bucket_new(c->csvfile, c->psm);
		// pschannel_bucket_insert(c->pb, "local_channel", "127.0.0.1", "6000", "300kbps", "127.0.0.1:3000/lchannel.sdp");
	}
}

struct mg_mgr * launch_http_task(struct context *c)
{
	struct mg_connection *nc;

	nc = mg_bind(c->mongoose_srv, c->http_port, ev_handler);
	nc->user_data = c;
	if (nc == NULL) {
		fprintf(stderr, "Error starting server on port %s\n", c->http_port);
		exit(1);
	}
	mg_set_protocol_http_websocket(nc);
	task_manager_new_task(c->tm, mongoose_task_reinit, mongoose_task_callback, 1000, (void *)c->mongoose_srv->ifaces[MG_MAIN_IFACE]);

	return c->mongoose_srv;
}

void context_deinit(struct context *c)
{
	if (c->csvfile)
		free(c->csvfile);
	if (c->config_string)
		free(c->config_string);
	router_destroy(&(c->router));
	pstreamer_manager_destroy(&(c->psm));  // this must be destroyed before task managers!
	sleep(1); //  let janus get the http notications for interrupting streaming sessions
	janus_instance_destroy(&(c->janus));  // this has to be destroyed after pstreamer_manager and before task_manager
	task_manager_destroy(&(c->tm));
	pschannel_bucket_destroy(&(c->pb));
	mg_mgr_free(c->mongoose_srv);
	if(c->mongoose_srv)
		free(c->mongoose_srv);
	free((char*)c->http_opts.document_root);
}

int main(int argc, char** argv)
{
	static struct context c;

	init(&c, argc, argv);

	if (running)
	{
		info("Starting server on port %s\n", c.http_port);
		launch_http_task(&c);
		task_manager_new_task(c.tm, NULL, pschannel_populate_task_callback, 1000, (void *) c.pb);
		task_manager_new_task(c.tm, NULL, pstreamer_purge_task_callback, 5000, (void *) c.psm);
		while (running)
			task_manager_poll(c.tm, 1000);
	}

	info("\nExiting..\n");
	context_deinit(&c);
	return 0;
}
