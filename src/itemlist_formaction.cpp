#include <itemlist_formaction.h>
#include <view.h>
#include <config.h>
#include <logger.h>
#include <exceptions.h>
#include <utils.h>
#include <cassert>

#include <sstream>

namespace newsbeuter {

itemlist_formaction::itemlist_formaction(view * vv, std::string formstr)
	: formaction(vv,formstr), feed(0), apply_filter(false), update_visible_items(true), search_dummy_feed(v->get_ctrl()->get_cache()) { 
}

itemlist_formaction::~itemlist_formaction() { }

void itemlist_formaction::process_operation(operation op) {
	bool quit = false;
	switch (op) {
		case OP_OPEN: {
				std::string itemposname = f->get("itempos");
				GetLogger().log(LOG_INFO, "itemlist_formaction: opening item at pos `%s'", itemposname.c_str());
				if (itemposname.length() > 0) {
					std::istringstream posname(itemposname);
					unsigned int itempos = 0;
					posname >> itempos;
					v->push_itemview(feed, visible_items[itempos].first->guid());
					do_redraw = true;
				} else {
					v->show_error(_("No item selected!")); // should not happen
				}
			}
			break;
		case OP_OPENINBROWSER: {
				std::string itemposname = f->get("itempos");
				GetLogger().log(LOG_INFO, "itemlist_formaction: opening item at pos `%s'", itemposname.c_str());
				if (itemposname.length() > 0) {
					std::istringstream posname(itemposname);
					unsigned int itempos = 0;
					posname >> itempos;
					if (itempos < visible_items.size()) {
						v->open_in_browser(visible_items[itempos].first->link());
						do_redraw = true;
					}
				} else {
					v->show_error(_("No item selected!")); // should not happen
				}
			}
			break;
		case OP_BOOKMARK: {
				std::string itemposname = f->get("itempos");
				GetLogger().log(LOG_INFO, "itemlist_formaction: bookmarking item at pos `%s'", itemposname.c_str());
				if (itemposname.length() > 0) {
					std::istringstream posname(itemposname);
					unsigned int itempos = 0;
					posname >> itempos;
					if (itempos < visible_items.size()) {
						this->start_bookmark_qna(visible_items[itempos].first->title(), visible_items[itempos].first->link(), "");
					}
				} else {
					v->show_error(_("No item selected!")); // should not happen
				}
			}
			break;
		case OP_EDITFLAGS: {
				std::string itemposname = f->get("itempos");
				if (itemposname.length() > 0) {
					std::istringstream posname(itemposname);
					unsigned int itempos = 0;
					posname >> itempos;
					if (itempos < visible_items.size()) {
						std::vector<std::pair<std::string, std::string> > qna;
						qna.push_back(std::pair<std::string,std::string>(_("Flags: "), visible_items[itempos].first->flags()));
						this->start_qna(qna, OP_INT_EDITFLAGS_END);
					}
				} else {
					v->show_error(_("No item selected!")); // should not happen
				}
			}
			break;
		case OP_SAVE: 
			{
				char buf[1024];
				std::string itemposname = f->get("itempos");
				GetLogger().log(LOG_INFO, "itemlist_formaction: saving item at pos `%s'", itemposname.c_str());
				if (itemposname.length() > 0) {
					std::istringstream posname(itemposname);
					unsigned int itempos = 0;
					posname >> itempos;
					
					std::string filename = v->run_filebrowser(FBT_SAVE,v->get_filename_suggestion(visible_items[itempos].first->title()));
					if (filename == "") {
						v->show_error(_("Aborted saving."));
					} else {
						try {
							v->write_item(*visible_items[itempos].first, filename);
							snprintf(buf, sizeof(buf), _("Saved article to %s"), filename.c_str());
							v->show_error(buf);
						
						} catch (...) {
							snprintf(buf, sizeof(buf), _("Error: couldn't save article to %s"), filename.c_str());
							v->show_error(buf);
						}
					}
				} else {
					v->show_error(_("Error: no item selected!"));
				}
			}
			break;
		case OP_HELP:
			v->push_help();
			break;
		case OP_RELOAD:
			if (!show_searchresult) {
				GetLogger().log(LOG_INFO, "itemlist_formaction: reloading current feed");
				v->get_ctrl()->reload(pos);
				// feed = v->get_ctrl()->get_feed(pos);
				update_visible_items = true;
				do_redraw = true;
			} else {
				v->show_error(_("Error: you can't reload search results."));
			}
			break;
		case OP_QUIT:
			GetLogger().log(LOG_INFO, "itemlist_formaction: quitting");
			quit = true;
			break;
		case OP_NEXTUNREAD:
			GetLogger().log(LOG_INFO, "itemlist_formaction: jumping to next unread item");
			if (!jump_to_next_unread_item(false)) {
				v->show_error(_("No unread items."));
			}
			break;
		case OP_PREVUNREAD:
			GetLogger().log(LOG_INFO, "itemlist_formaction: jumping to previous unread item");
			if (!jump_to_previous_unread_item(false)) {
				v->show_error(_("No unread items."));
			}
			break;
		case OP_MARKFEEDREAD:
			GetLogger().log(LOG_INFO, "itemlist_formaction: marking feed read");
			v->set_status(_("Marking feed read..."));
			try {
				v->get_ctrl()->mark_all_read(pos);
				do_redraw = true;
				v->set_status("");
			} catch (const dbexception& e) {
				char buf[1024];
				snprintf(buf, sizeof(buf), _("Error: couldn't mark feed read: %s"), e.what());
				v->show_error(buf);
			}
			break;
		case OP_SEARCH: {
				std::vector<std::pair<std::string, std::string> > qna;
				qna.push_back(std::pair<std::string, std::string>(_("Search for: "), ""));
				this->start_qna(qna, OP_INT_START_SEARCH, &searchhistory);
			}
			break;
		case OP_TOGGLEITEMREAD: {
				std::string itemposname = f->get("itempos");
				GetLogger().log(LOG_INFO, "itemlist_formaction: toggling item read at pos `%s'", itemposname.c_str());
				if (itemposname.length() > 0) {
					std::istringstream posname(itemposname);
					unsigned int itempos = 0;
					posname >> itempos;
					v->set_status(_("Toggling read flag for article..."));
					try {
						visible_items[itempos].first->set_unread(!visible_items[itempos].first->unread());
						v->set_status("");
					} catch (const dbexception& e) {
						char buf[1024];
						snprintf(buf, sizeof(buf), _("Error while toggling read flag: %s"), e.what());
						v->set_status(buf);
					}
					do_redraw = true;
				}
			}
			break;
		case OP_SELECTFILTER:
			if (v->get_ctrl()->get_filters().size() > 0) {
				std::string newfilter = v->select_filter(v->get_ctrl()->get_filters().get_filters());
				if (newfilter != "") {
					filterhistory.add_line(newfilter);
					if (newfilter.length() > 0) {
						if (!m.parse(newfilter)) {
							v->show_error(_("Error: couldn't parse filter command!"));
						} else {
							apply_filter = true;
							update_visible_items = true;
							do_redraw = true;
						}
					}
				}
			} else {
				v->show_error(_("No filters defined."));
			}

			break;
		case OP_SETFILTER: {
				std::vector<std::pair<std::string, std::string> > qna;
				qna.push_back(std::pair<std::string,std::string>(_("Filter: "), ""));
				this->start_qna(qna, OP_INT_END_SETFILTER, &filterhistory);
			}
			break;
		case OP_CLEARFILTER:
			apply_filter = false;
			update_visible_items = true;
			do_redraw = true;
			break;
		default:
			break;
	}
	if (quit) {
		v->pop_current_formaction();
	}
}

void itemlist_formaction::finished_qna(operation op) {
	formaction::finished_qna(op); // important!

	switch (op) {
		case OP_INT_END_SETFILTER: {
				std::string filtertext = qna_responses[0];
				filterhistory.add_line(filtertext);
				if (filtertext.length() > 0) {
					if (!m.parse(filtertext)) {
						v->show_error(_("Error: couldn't parse filter command!"));
					} else {
						apply_filter = true;
						update_visible_items = true;
						do_redraw = true;
					}
				}
			}
			break;
		case OP_INT_EDITFLAGS_END: {
				std::string itemposname = f->get("itempos");
				if (itemposname.length() > 0) {
					std::istringstream posname(itemposname);
					unsigned int itempos = 0;
					posname >> itempos;
					if (itempos < visible_items.size()) {
						visible_items[itempos].first->set_flags(qna_responses[0]);
						visible_items[itempos].first->update_flags();
						v->set_status(_("Flags updated."));
						GetLogger().log(LOG_DEBUG, "itemlist_formaction::finished_qna: updated flags");
						do_redraw = true;
					}
				} else {
					v->show_error(_("No item selected!")); // should not happen
				}
			}
			break;
		case OP_INT_START_SEARCH: {
				v->set_status(_("Searching..."));
				std::string searchphrase = qna_responses[0];
				if (searchphrase.length() > 0) {
					searchhistory.add_line(searchphrase);
					std::vector<rss_item> items;
					try {
						if (show_searchresult) {
							items = v->get_ctrl()->search_for_items(searchphrase, "");
						} else {
							items = v->get_ctrl()->search_for_items(searchphrase, feed->rssurl());
						}
					} catch (const dbexception& e) {
						char buf[1024];
						snprintf(buf, sizeof(buf), _("Error while searching for `%s': %s"), searchphrase.c_str(), e.what());
						v->show_error(buf);
						return;
					}
					if (items.size() > 0) {
						search_dummy_feed.items() = items;
						if (show_searchresult) {
							v->pop_current_formaction();
						}
						v->push_searchresult(&search_dummy_feed);
					} else {
						v->show_error(_("No results."));
					}
				}
			}
			break;
		default:
			break;
	}
}

void itemlist_formaction::do_update_visible_items() {
	std::vector<rss_item>& items = feed->items();

	if (visible_items.size() > 0)
		visible_items.erase(visible_items.begin(), visible_items.end());

	unsigned int i=0;
	for (std::vector<rss_item>::iterator it = items.begin(); it != items.end(); ++it, ++i) {
		if (!apply_filter || m.matches(&(*it))) {
			visible_items.push_back(std::pair<rss_item *, unsigned int>(&(*it), i));
		}
	}
}

void itemlist_formaction::prepare() {
	if (update_visible_items) {
		do_update_visible_items();
		update_visible_items = false;
	}

	if (do_redraw) {
		GetLogger().log(LOG_DEBUG, "itemlist_formaction::prepare: redrawing");
		std::string code = "{list";

		std::string datetimeformat = v->get_cfg()->get_configvalue("datetime-format");
		if (datetimeformat.length() == 0)
			datetimeformat = "%b %d";

		for (std::vector<std::pair<rss_item *, unsigned int> >::iterator it = visible_items.begin(); it != visible_items.end(); ++it) {
			std::string line = "{listitem[";
			std::ostringstream x;
			x << it->second;
			line.append(x.str());
			line.append("] text:");
			std::string title;
			char buf[20];
			snprintf(buf,sizeof(buf),"%4u ",it->second + 1);
			title.append(buf);
			if (it->first->unread()) {
				title.append("N");
			} else {
				title.append(" ");
			}
			if (it->first->flags().length() > 0) {
				title.append("! ");
			} else {
				title.append("  ");
			}
			char datebuf[64];
			time_t t = it->first->pubDate_timestamp();
			struct tm * stm = localtime(&t);
			strftime(datebuf,sizeof(datebuf), datetimeformat.c_str(), stm);
			title.append(datebuf);
			title.append("   ");
			if (feed->rssurl() != it->first->feedurl()) {
				char buf[20];
				std::string feedtitle = it->first->get_feedptr()->title();
				snprintf(buf,sizeof(buf),"|%-17s|",feedtitle.substr(0,17).c_str());
				title.append(buf);
				title.append("  ");
			}
			title.append(it->first->title());
			std::string quoted_title = stfl::quote(title);
			GetLogger().log(LOG_DEBUG, "itemlist_formaction: XXXTITLE it->first->title = `%s' title = `%s' quoted title = `%s'", 
				it->first->title().c_str(), title.c_str(), quoted_title.c_str());
			// assert(quoted_title.substr(1,1) != "??");
			line.append(quoted_title);
			line.append("}");
			GetLogger().log(LOG_INFO, "prepare: title  = %s", title.c_str());
			GetLogger().log(LOG_INFO, "prepare: qtitle = %s", quoted_title.c_str());
			GetLogger().log(LOG_INFO, "prepare: line = %s", line.c_str());
			code.append(line);
		}

		code.append("}");

		GetLogger().log(LOG_INFO, "prepare: code = `%s'", code.c_str());

		f->modify("items","replace_inner",code);
		
		set_head(feed->title(),feed->unread_item_count(),feed->items().size(), feed->rssurl());

		do_redraw = false;
	}
}

void itemlist_formaction::init() {
	f->set("itempos","0");
	f->set("msg","");
	do_redraw = true;
	set_keymap_hints();
}

void itemlist_formaction::set_head(const std::string& s, unsigned int unread, unsigned int total, const std::string &url) {
	char buf[1024];
	if (!show_searchresult) {
		snprintf(buf, sizeof(buf), _("%s %s - Articles in feed '%s' (%u unread, %u total) - %s"), PROGRAM_NAME, PROGRAM_VERSION, s.c_str(), unread, total, url.c_str());
	} else {
		snprintf(buf, sizeof(buf), _("%s %s - Search results (%u unread, %u total)"), PROGRAM_NAME, PROGRAM_VERSION, unread, total);
	}
	f->set("head", buf);
}

bool itemlist_formaction::jump_to_previous_unread_item(bool start_with_last) {
	unsigned int itempos;
	std::istringstream is(f->get("itempos"));
	is >> itempos;
	for (int i=(start_with_last?itempos:(itempos-1));i>=0;--i) {
		GetLogger().log(LOG_DEBUG, "itemlist_formaction::jump_to_previous_unread_item: visible_items[%u] unread = %s", i, visible_items[i].first->unread() ? "true" : "false");
		if (visible_items[i].first->unread()) {
			std::ostringstream os;
			os << i;
			f->set("itempos", os.str());
			return true;
		}
	}
	for (unsigned int i=visible_items.size()-1;i>=itempos;--i) {
		if (visible_items[i].first->unread()) {
			std::ostringstream os;
			os << i;
			f->set("itempos", os.str());
			return true;
		}
	}
	return false;

}

bool itemlist_formaction::jump_to_next_unread_item(bool start_with_first) {
	unsigned int itempos;
	std::istringstream is(f->get("itempos"));
	is >> itempos;
	for (unsigned int i=(start_with_first?itempos:(itempos+1));i<visible_items.size();++i) {
		GetLogger().log(LOG_DEBUG, "itemlist_formaction::jump_to_next_unread_item: visible_items[%u] unread = %s", i, visible_items[i].first->unread() ? "true" : "false");
		if (visible_items[i].first->unread()) {
			std::ostringstream os;
			os << i;
			f->set("itempos", os.str());
			return true;
		}
	}
	for (unsigned int i=0;i<=itempos;++i) {
		if (visible_items[i].first->unread()) {
			std::ostringstream os;
			os << i;
			f->set("itempos", os.str());
			return true;
		}
	}
	return false;
}

std::string itemlist_formaction::get_guid() {
	unsigned int itempos;
	std::istringstream is(f->get("itempos"));
	is >> itempos;
	return visible_items[itempos].first->guid();
}

keymap_hint_entry * itemlist_formaction::get_keymap_hint() {
	static keymap_hint_entry hints[] = {
		{ OP_QUIT, _("Quit") },
		{ OP_OPEN, _("Open") },
		{ OP_SAVE, _("Save") },
		{ OP_RELOAD, _("Reload") },
		{ OP_NEXTUNREAD, _("Next Unread") },
		{ OP_MARKFEEDREAD, _("Mark All Read") },
		{ OP_SEARCH, _("Search") },
		{ OP_HELP, _("Help") },
		{ OP_NIL, NULL }
	};
	return hints;
}

void itemlist_formaction::handle_cmdline(const std::string& cmd) {
	unsigned int idx = 0;
	if (1==sscanf(cmd.c_str(),"%u",&idx)) {
		if (idx > 0 && idx <= visible_items[visible_items.size()-1].second + 1) {
			int i = get_pos(idx - 1);
			if (i == -1) {
				v->show_error(_("Position not visible!"));
			} else {
				std::ostringstream idxstr;
				idxstr << i;
				f->set("itempos", idxstr.str());
			}
		} else {
			v->show_error(_("Invalid position!"));
		}
	} else {
		std::vector<std::string> tokens = utils::tokenize_quoted(cmd);
		if (tokens.size() > 0) {
			if (tokens[0] == "save" && tokens.size() >= 2) {
				std::string filename = utils::resolve_tilde(tokens[1]);
				char buf[1024];

				std::string itemposname = f->get("itempos");
				GetLogger().log(LOG_INFO, "itemlist_formaction::handle_cmdline: saving item at pos `%s' to `%s'", itemposname.c_str(), filename.c_str());
				if (itemposname.length() > 0) {
					std::istringstream posname(itemposname);
					unsigned int itempos = 0;
					posname >> itempos;

					if (filename == "") {
						v->show_error(_("Aborted saving."));
					} else {
						try {
							v->write_item(*visible_items[itempos].first, filename);
							snprintf(buf, sizeof(buf), _("Saved article to %s"), filename.c_str());
							v->show_error(buf);
						} catch (...) {
							snprintf(buf, sizeof(buf), _("Error: couldn't save article to %s"), filename.c_str());
							v->show_error(buf);
						}
					}
				} else {
					v->show_error(_("Error: no item selected!"));
				}
			} else {
				formaction::handle_cmdline(cmd);
			}
		}
	}
}

int itemlist_formaction::get_pos(unsigned int realidx) {
	for (unsigned int i=0;i<visible_items.size();++i) {
		if (visible_items[i].second == realidx)
			return i;
	}
	return -1;
}


}
