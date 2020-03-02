#include "selectformaction.h"

#include <cassert>
#include <langinfo.h>
#include <sstream>

#include "config.h"
#include "fmtstrformatter.h"
#include "listformatter.h"
#include "rssfeed.h"
#include "strprintf.h"
#include "utils.h"
#include "view.h"

namespace newsboat {

/*
 * The SelectFormAction is used both for the "select tag" dialog
 * and the "select filter", as they do practically the same. That's
 * why there is the decision between SELECTTAG and SELECTFILTER on
 * a few places.
 */

SelectFormAction::SelectFormAction(View* vv,
	Cache* cc,
	std::string formstr,
	FilterContainer* f,
	ConfigContainer* cfg)
	: FormAction(vv, formstr, cfg)
	, quit(false)
	, apply_filter(false)
	, search_dummy_feed(new RssFeed(cc))
	, filterpos(0)
	, set_filterpos(false)
	, type(SelectionType::TAG)
	, filters2(f)
{
	search_dummy_feed->set_search_feed(true);
}

SelectFormAction::~SelectFormAction() {}

void SelectFormAction::handle_cmdline(const std::string& cmd)
{
	unsigned int idx = 0;
	if (1 == sscanf(cmd.c_str(), "%u", &idx)) {
		if (idx > 0 &&
			idx <= ((type == SelectionType::TAG)
				? tags.size()
				: filters.size())) {
			f->set("tagpos", std::to_string(idx - 1));
		}
	} else {
		FormAction::handle_cmdline(cmd);
	}
}

void SelectFormAction::finished_qna(Operation op)	// Duplicate of "feedlistformaction.cpp".
{
	FormAction::finished_qna(op); // important!

	switch (op) {
	case OP_INT_END_SETFILTER:
		op_end_setfilter();
		break;
	case OP_INT_START_SEARCH:
		op_start_search();
		break;
	default:
		break;
	}
}

void SelectFormAction::op_end_setfilter()
{
	std::string filtertext = qna_responses[0];
	filterhistory.add_line(filtertext);
	if (filtertext.length() > 0) {
		if (!m.parse(filtertext)) {
			v->show_error(
				_("Error: couldn't parse filter command!"));
		} else {
			save_filterpos();
			apply_filter = true;
			do_redraw = true;
		}
	}
}

void SelectFormAction::op_start_search()
{
	std::string searchphrase = qna_responses[0];
	LOG(Level::DEBUG,
		"FeedListFormAction::op_start_search: starting search for "
		"`%s'",
		searchphrase);
	if (searchphrase.length() > 0) {
		v->set_status(_("Searching..."));
		searchhistory.add_line(searchphrase);
		std::vector<std::shared_ptr<RssItem>> items;
		try {
			std::string utf8searchphrase = utils::convert_text(
					searchphrase, "utf-8", nl_langinfo(CODESET));
			items = v->get_ctrl()->search_for_items(
					utf8searchphrase, nullptr);
		} catch (const DbException& e) {
			v->show_error(strprintf::fmt(
					_("Error while searching for `%s': %s"),
					searchphrase,
					e.what()));
			return;
		}
		if (!items.empty()) {
			search_dummy_feed->item_mutex.lock();
			search_dummy_feed->clear_items();
			search_dummy_feed->add_items(items);
			search_dummy_feed->item_mutex.unlock();
			v->push_searchresult(search_dummy_feed, searchphrase);
		} else {
			v->show_error(_("No results."));
		}
	}
}

void SelectFormAction::process_operation(Operation op,
	bool automatic,
	std::vector<std::string>* args)
{
	bool hardquit = false;
	switch (op) {
	case OP_QUIT:
		value = "";
		quit = true;
		break;
	case OP_HARDQUIT:
		value = "";
		hardquit = true;
		break;
	case OP_OPEN: {
		std::string tagposname = f->get("tagposname");
		unsigned int pos = utils::to_u(tagposname);
		if (tagposname.length() > 0) {
			switch (type) {
			case SelectionType::TAG: {
				if (pos < tags.size()) {
					value = tags[pos];
					quit = true;
				}
			}
			break;
			case SelectionType::FILTER: {
				if (pos < filters.size()) {
					value = filters[pos].second;
					quit = true;
				}
			}
			break;
			default:
				assert(0); // should never happen
			}
		}
	}
	break;	// I assume this still applies to OP_OPEN, but not fixing indentation because I'm not sure. What's with those braces?
	case OP_SEARCH:	// Duplicated from "feedlistformaction.cpp". Probably not optimal.
		if (automatic && args->size() > 0) {
			qna_responses.clear();
			qna_responses.push_back((*args)[0]);
			finished_qna(OP_INT_START_SEARCH);
		} else {
			std::vector<QnaPair> qna;
			qna.push_back(QnaPair(_("Search for: "), ""));
			this->start_qna(
				qna, OP_INT_START_SEARCH, &searchhistory);
		}
		break;
	case OP_SELECTFILTER:
		if (filters2->size() > 0) {
			std::string newfilter;
			if (automatic && args->size() > 0) {
				newfilter = (*args)[0];
			} else {
				newfilter = v->select_filter(
						//filters2->get_filters());
						filters2);
			}
			if (newfilter != "") {
				filterhistory.add_line(newfilter);
				if (newfilter.length() > 0) {
					if (!m.parse(newfilter)) {
						v->show_error(strprintf::fmt(
								_("Error: couldn't "
									"parse filter "
									"command `%s': %s"),
								newfilter,
								m.get_parse_error()));
					} else {
						save_filterpos();
						apply_filter = true;
						do_redraw = true;
					}
				}
			}
		} else {
			v->show_error(_("No filters defined."));
		}
		break;
	case OP_CLEARFILTER:
		apply_filter = false;
		do_redraw = true;
		save_filterpos();
		break;
	case OP_SETFILTER:
		if (automatic && args->size() > 0) {
			qna_responses.clear();
			qna_responses.push_back((*args)[0]);
			finished_qna(OP_INT_END_SETFILTER);
		} else {
			std::vector<QnaPair> qna;
			qna.push_back(QnaPair(_("Filter: "), ""));
			this->start_qna(
				qna, OP_INT_END_SETFILTER, &filterhistory);
		}
		break;
	case OP_HELP:
		v->push_help();
		break;
	default:
		break;
	}

	if (hardquit) {
		while (v->formaction_stack_size() > 0) {
			v->pop_current_formaction();
		}
	} else if (quit) {	// Is this behaviour supposed to differ from feedlistformaction.cpp?
		v->pop_current_formaction();
	}
}

bool SelectFormAction::tag_is_relevant(
	//const std::vector<std::string> tag
	const std::string tag
) {
	bool show_read = cfg->get_configvalue_as_bool("show-read-feeds");
	for (const auto& feed : tag) {
		if (
			(show_read || feed->unread_item_count() > 0) &&
			(!apply_filter || m.matches(feed.get())) &&
			!feed->hidden()
		) return true;
	}
	return false;
}

void SelectFormAction::prepare()
{
	if (do_redraw) {
		ListFormatter listfmt;
		unsigned int i = 0;
		const auto selecttag_format = cfg->get_configvalue("selecttag-format");
		const auto width = utils::to_u(f->get("tags:w"));

		switch (type) {
		case SelectionType::TAG:
			for (const auto& tag : tags) {
				if (!tag_is_relevant(tag)) continue;
				listfmt.add_line(format_line(selecttag_format,
						tag,
						i + 1,
						width),
					i);
				i++;
			}
			break;
		case SelectionType::FILTER:
			for (const auto& filter : filters) {
				std::string tagstr = strprintf::fmt(
						"%4u  %s", i + 1, filter.first);
				listfmt.add_line(tagstr, i);
				i++;
			}
			break;
		default:
			assert(0);
		}
		f->modify("taglist", "replace_inner", listfmt.format_list());

		do_redraw = false;
	}
}

void SelectFormAction::init()
{
	std::string title;
	do_redraw = true;
	quit = false;
	value = "";

	std::string viewwidth = f->get("taglist:w");
	unsigned int width = utils::to_u(viewwidth, 80);

	set_keymap_hints();

	FmtStrFormatter fmt;
	fmt.register_fmt('N', PROGRAM_NAME);
	fmt.register_fmt('V', utils::program_version());

	switch (type) {
	case SelectionType::TAG:
		title = fmt.do_format(
				cfg->get_configvalue("selecttag-title-format"), width);
		break;
	case SelectionType::FILTER:
		title = fmt.do_format(
				cfg->get_configvalue("selectfilter-title-format"),
				width);
		break;
	default:
		assert(0); // should never happen
	}
	f->set("head", title);
}

std::string SelectFormAction::format_line(const std::string& selecttag_format,
	const std::string& tag,
	unsigned int pos,
	unsigned int width)
{
	FmtStrFormatter fmt;

	const auto feedcontainer = v->get_ctrl()->get_feedcontainer();

	const auto total_feeds = feedcontainer->get_feed_count_per_tag(tag);
	const auto unread_feeds =
		feedcontainer->get_unread_feed_count_per_tag(tag);
	const auto unread_articles =
		feedcontainer->get_unread_item_count_per_tag(tag);

	fmt.register_fmt('i', strprintf::fmt("%u", pos));
	fmt.register_fmt('T', tag);
	fmt.register_fmt('f', std::to_string(unread_feeds));
	fmt.register_fmt('n', std::to_string(unread_articles));
	fmt.register_fmt('u', std::to_string(total_feeds));

	auto formattedLine = fmt.do_format(selecttag_format, width);

	return formattedLine;
}

KeyMapHintEntry* SelectFormAction::get_keymap_hint()
{
	static KeyMapHintEntry hints_tag[] = {{OP_QUIT, _("Cancel")},
		{OP_OPEN, _("Select Tag")},
		{OP_HELP, _("Help")},
		{OP_NIL, nullptr}
	};
	static KeyMapHintEntry hints_filter[] = {{OP_QUIT, _("Cancel")},
		{OP_OPEN, _("Select Filter")},
		{OP_HELP, _("Help")},
		{OP_NIL, nullptr}
	};
	switch (type) {
	case SelectionType::TAG:
		return hints_tag;
	case SelectionType::FILTER:
		return hints_filter;
	}
	return nullptr;
}

void SelectFormAction::save_filterpos()	// Duplicated from "feedlistformaction.cpp"...
{
	unsigned int i = utils::to_u(f->get("feedpos"));
	if (i < visible_feeds.size()) {
		filterpos = visible_feeds[i].second;
		set_filterpos = true;
	}
}

std::string SelectFormAction::title()
{
	switch (type) {
	case SelectionType::TAG:
		return _("Select Tag");
	case SelectionType::FILTER:
		return _("Select Filter");
	default:
		return "";
	}
}

} // namespace newsboat
