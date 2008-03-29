#include <itemview_formaction.h>
#include <view.h>
#include <config.h>
#include <logger.h>
#include <exceptions.h>
#include <utils.h>
#include <formatstring.h>
#include <listformatter.h>

#include <sstream>

namespace newsbeuter {

itemview_formaction::itemview_formaction(view * vv, std::string formstr)
	: formaction(vv,formstr), feed(0), show_source(false), quit(false) { 
}

itemview_formaction::~itemview_formaction() { }


void itemview_formaction::init() {
	f->set("msg","");
	do_redraw = true;
	quit = false;
	links.erase(links.begin(), links.end());
	set_keymap_hints();
}

void itemview_formaction::prepare() {
	static bool render_hack;

	/*
	 * whenever necessary, the item view is regenerated. This is done
	 * by putting together the feed name, title, link, author, optional
	 * flags and podcast download URL (enclosures) and then render the
	 * HTML. The links extracted by the renderer are then appended, too.
	 */
	if (do_redraw) {
		rss_item& item = feed->get_item_by_guid(guid);
		listformatter listfmt;

		rss_feed * feedptr = item.get_feedptr();

		std::string title, feedtitle;
		if (feedptr->title().length() > 0) {
			title = feedptr->title();
		} else if (feedptr->link().length() > 0) {
			title = feedptr->link();
		} else if (feedptr->rssurl().length() > 0) {
			title = feedptr->rssurl();
		}
		feedtitle = utils::strprintf("%s%s", _("Feed: "), title.c_str());
		listfmt.add_line(feedtitle);

		if (item.title().length() > 0) {
			title = utils::strprintf("%s%s", _("Title: "), item.title().c_str());
			listfmt.add_line(title);
		}

		if (item.author().length() > 0) {
			std::string author = utils::strprintf("%s%s", _("Author: "), item.author().c_str());
			listfmt.add_line(author);
		}

		if (item.link().length() > 0) {
			std::string link = utils::strprintf("%s%s", _("Link: "), item.link().c_str());
			listfmt.add_line(link);
		}

		std::string date = utils::strprintf("%s%s", _("Date: "), item.pubDate().c_str());
		listfmt.add_line(date);

		if (item.flags().length() > 0) {
			std::string flags = utils::strprintf("%s%s", _("Flags: "), item.flags().c_str());
			listfmt.add_line(flags);
		}

		if (item.enclosure_url().length() > 0) {
			std::string enc_url = utils::strprintf("%s%s (%s%s)", _("Podcast Download URL: "), item.enclosure_url().c_str(), _("type: "), item.enclosure_type().c_str());
			listfmt.add_line(enc_url);
		}

		listfmt.add_line("");

		set_head(item.title());

		if (!render_hack) {
			f->run(-3); // XXX HACK: render once so that we get a proper widget width
			render_hack = true;
		}

		std::vector<std::string> lines;
		std::string widthstr = f->get("article:w");
		unsigned int render_width = 80;
		if (widthstr.length() > 0) {
			std::istringstream is(widthstr);
			is >> render_width;
			if (render_width - 5 > 0)
				render_width -= 5; 	
		}

		unsigned int textwidth = v->get_cfg()->get_configvalue_as_int("text-width");
		if (textwidth > 0) {
			render_width = textwidth;
		}

		if (show_source) {
			render_source(lines, item.description(), render_width);
		} else {
			lines = render_html(item.description(), links, item.feedurl(), render_width);
		}

		listfmt.add_lines(lines);

		f->modify("article","replace_inner",listfmt.format_list());
		f->set("articleoffset","0");

		do_redraw = false;
	}

}

void itemview_formaction::process_operation(operation op, bool automatic, std::vector<std::string> * args) {
	rss_item& item = feed->get_item_by_guid(guid);

	/*
	 * whenever we process an operation, we mark the item
	 * as read. Don't worry: when an item is already marked as
	 * read, and then marked as read again, no database update
	 * is done, since only _changes_ to the unread flag are
	 * recorded in the database.
	 */
	try {
		item.set_unread(false);
	} catch (const dbexception& e) {
		v->show_error(utils::strprintf(_("Error while marking article as read: %s"), e.what()));
	}

	switch (op) {
		case OP_OPEN:
			// nothing
			break;
		case OP_TOGGLESOURCEVIEW:
			GetLogger().log(LOG_INFO, "view::run_itemview: toggling source view");
			show_source = !show_source;
			do_redraw = true;
			break;
		case OP_ENQUEUE: {
				if (item.enclosure_url().length() > 0) {
					v->get_ctrl()->enqueue_url(item.enclosure_url());
					v->set_status(utils::strprintf(_("Added %s to download queue."), item.enclosure_url().c_str()));
				}
			}
			break;
		case OP_SAVE:
			{
				GetLogger().log(LOG_INFO, "view::run_itemview: saving article");
				std::string filename;
				if (automatic) {
					if (args->size() > 0)
						filename = (*args)[0];
				} else {
					filename = v->run_filebrowser(FBT_SAVE,v->get_filename_suggestion(item.title()));
				}
				if (filename == "") {
					v->show_error(_("Aborted saving."));
				} else {
					try {
						v->write_item(item, filename);
						v->show_error(utils::strprintf(_("Saved article to %s."), filename.c_str()));
					} catch (...) {
						v->show_error(utils::strprintf(_("Error: couldn't write article to file %s"), filename.c_str()));
					}
				}
			}
			break;
		case OP_OPENINBROWSER:
			GetLogger().log(LOG_INFO, "view::run_itemview: starting browser");
			v->set_status(_("Starting browser..."));
			v->open_in_browser(item.link());
			v->set_status("");
			break;
		case OP_BOOKMARK:
			if (automatic) {
				qna_responses.erase(qna_responses.begin(), qna_responses.end());
				qna_responses.push_back(item.title());
				qna_responses.push_back(item.link());
				qna_responses.push_back(args->size() > 0 ? (*args)[0] : "");
			} else {
				this->start_bookmark_qna(item.title(), item.link(), "");
			}
			break;
		case OP_EDITFLAGS: 
			if (automatic) {
				qna_responses.erase(qna_responses.begin(), qna_responses.end());
				if (args->size() > 0) {
					qna_responses.push_back((*args)[0]);
					this->finished_qna(OP_INT_EDITFLAGS_END);
				}
			} else {
				std::vector<qna_pair> qna;
				qna.push_back(qna_pair(_("Flags: "), item.flags()));
				this->start_qna(qna, OP_INT_EDITFLAGS_END);
			}
			break;
		case OP_SHOWURLS:
			GetLogger().log(LOG_DEBUG, "view::run_itemview: showing URLs");
			if (links.size() > 0) {
				v->push_urlview(links);
			} else {
				v->show_error(_("URL list empty."));
			}
			break;
		case OP_NEXTUNREAD:
			GetLogger().log(LOG_INFO, "view::run_itemview: jumping to next unread article");
			if (v->get_next_unread()) {
				do_redraw = true;
			} else {
				v->pop_current_formaction();
				v->show_error(_("No unread items."));
			}
			break;
		case OP_PREVUNREAD:
			GetLogger().log(LOG_INFO, "view::run_itemview: jumping to previous unread article");
			if (v->get_previous_unread()) {
				do_redraw = true;
			} else {
				v->pop_current_formaction();
				v->show_error(_("No unread items."));
			}
			break;
		case OP_QUIT:
			GetLogger().log(LOG_INFO, "view::run_itemview: quitting");
			quit = true;
			break;
		case OP_HELP:
			v->push_help();
			break;
		default:
			break;
	}

	if (quit) {
		v->pop_current_formaction();
	}

}

keymap_hint_entry * itemview_formaction::get_keymap_hint() {
	static keymap_hint_entry hints[] = {
		{ OP_QUIT, _("Quit") },
		{ OP_OPEN, _("Open") },
		{ OP_SAVE, _("Save") },
		{ OP_NEXTUNREAD, _("Next Unread") },
		{ OP_OPENINBROWSER, _("Open in Browser") },
		{ OP_ENQUEUE, _("Enqueue") },
		{ OP_HELP, _("Help") },
		{ OP_NIL, NULL }
	};
	return hints;
}

void itemview_formaction::set_head(const std::string& s) {
	fmtstr_formatter fmt;
	fmt.register_fmt('N', PROGRAM_NAME);
	fmt.register_fmt('V', PROGRAM_VERSION);
	fmt.register_fmt('T', s);

	std::string listwidth = f->get("article:w");
	std::istringstream is(listwidth);
	unsigned int width;
	is >> width;

	f->set("head",fmt.do_format(v->get_cfg()->get_configvalue("itemview-title-format"), width));
}

void itemview_formaction::render_source(std::vector<std::string>& lines, std::string desc, unsigned int width) {
	/*
	 * this function is called instead of htmlrenderer::render() when the
	 * user requests to have the source displayed instead of seeing the
	 * rendered HTML.
	 */
	std::string line;
	do {
		std::string::size_type pos = desc.find_first_of("\r\n");
		line = desc.substr(0,pos);
		if (pos == std::string::npos)
			desc.erase();
		else
			desc.erase(0,pos+1);
		while (line.length() > width) {
			int i = width;
			while (i > 0 && line[i] != ' ' && line[i] != '<')
				--i;
			if (0 == i) {
				i = width;
			}
			std::string subline = line.substr(0, i);
			line.erase(0, i);
			pos = subline.find_first_not_of(" ");
			subline.erase(0,pos);
			lines.push_back(subline);
		}
		pos = line.find_first_not_of(" ");
		line.erase(0,pos);
		lines.push_back(line);
	} while (desc.length() > 0);
}

void itemview_formaction::handle_cmdline(const std::string& cmd) {
	std::vector<std::string> tokens = utils::tokenize_quoted(cmd);
	if (tokens.size() > 0) {
		if (tokens[0] == "save" && tokens.size() >= 2) {
			std::string filename = utils::resolve_tilde(tokens[1]);
			rss_item& item = feed->get_item_by_guid(guid);

			if (filename == "") {
				v->show_error(_("Aborted saving."));
			} else {
				try {
					v->write_item(item, filename);
					v->show_error(utils::strprintf(_("Saved article to %s"), filename.c_str()));
				} catch (...) {
					v->show_error(utils::strprintf(_("Error: couldn't save article to %s"), filename.c_str()));
				}
			}

		} else {
			formaction::handle_cmdline(cmd);
		}
	}
}

void itemview_formaction::finished_qna(operation op) {
	formaction::finished_qna(op); // important!

	rss_item& item = feed->get_item_by_guid(guid);

	switch (op) {
		case OP_INT_EDITFLAGS_END:
			item.set_flags(qna_responses[0]);
			item.update_flags();
			v->set_status(_("Flags updated."));
			do_redraw = true;
			break;
		default:
			break;
	}
}

std::vector<std::string> itemview_formaction::render_html(const std::string& source, std::vector<linkpair>& links, const std::string& feedurl, unsigned int render_width) {
	std::vector<std::string> lines;
	std::string renderer = v->get_cfg()->get_configvalue("html-renderer");
	if (renderer == "internal") {
		htmlrenderer rnd(render_width);
		rnd.render(source, lines, links, feedurl);
	} else {
		std::string output = utils::run_filter(renderer, source);
		std::istringstream is(output);
		std::string line;
		getline(is, line);
		while (!is.eof()) {
			lines.push_back(line);
			getline(is, line);
		}
	}
	return lines;
}

}
