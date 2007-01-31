#ifndef NEWSBEUTER_CONTROLLER__H
#define NEWSBEUTER_CONTROLLER__H

#include <urlreader.h>
#include <rss.h>
#include <cache.h>
#include <nxml.h>
#include <configcontainer.h>

namespace newsbeuter {

	class view;

	class controller {
		public:
			controller();
			~controller();
			void set_view(view * vv);
			void run(int argc = 0, char * argv[] = NULL);
			bool open_feed(unsigned int pos, bool auto_open, std::string tag = "");
			bool open_item(const rss_feed& feed, rss_item& item);
			void reload(unsigned int pos, unsigned int max = 0);
			void reload_all();
			void start_reload_all_thread();
			rss_feed& get_feed(unsigned int pos);
			inline unsigned int get_feedcount() { return feeds.size(); }
			inline void unlock_reload_mutex() { reload_mutex->unlock(); }
			void update_feedlist(std::string tag = "");
			void mark_all_read(unsigned int pos);
			void catchup_all();
			inline bool get_refresh_on_start() { return refresh_on_start; }
		private:
			void usage(char * argv0);
			void import_opml(const char * filename);
			void export_opml();
			void rec_find_rss_outlines(nxml_data_t * node);

			bool try_fs_lock(pid_t & pid);
			void remove_fs_lock();

			view * v;
			urlreader urlcfg;
			cache * rsscache;
			std::vector<rss_feed> feeds;
			std::string config_dir;
			std::string url_file;
			std::string cache_file;
			std::string config_file;
			bool refresh_on_start;
			configcontainer * cfg;

			mutex * reload_mutex;
	};

}


#endif
