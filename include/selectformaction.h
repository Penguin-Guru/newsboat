#ifndef NEWSBOAT_SELECTFORMACTION_H_
#define NEWSBOAT_SELECTFORMACTION_H_

#include "filtercontainer.h"
#include "formaction.h"
#include "matcher.h"
#include "dbexception.h"
#include "cache.h"
#include "controller.h"

namespace newsboat {

typedef std::pair<std::shared_ptr<RssFeed>, unsigned int> FeedPtrPosPair;	// Really shouldn't need to typdef this twice.

class SelectFormAction : public FormAction {
public:
	enum class SelectionType { TAG, FILTER };

	SelectFormAction(
		View*,
		Cache* cc,
		std::string formstr,
		FilterContainer* f,
		ConfigContainer* cfg);
	~SelectFormAction() override;
	void prepare() override;
	void init() override;
	KeyMapHintEntry* get_keymap_hint() override;
	std::string get_selected_value()
	{
		return value;
	}
	void set_tags(const std::vector<std::string>& t)
	{
		tags = t;
	}
	void set_filters(const std::vector<FilterNameExprPair>& ff)
	{
		filters = ff;
	}
	void set_type(SelectionType t)
	{
		type = t;
	}
	void handle_cmdline(const std::string& cmd) override;
	void finished_qna(Operation op) override;
	std::string id() const override
	{
		return (type == SelectionType::TAG) ? "tagselection"
			: "filterselection";
	}
	std::string title() override;

private:

	void save_filterpos();
	void process_operation(Operation op,
		bool automatic = false,
		std::vector<std::string>* args = nullptr) override;
	bool quit;

	void op_end_setfilter();
	void op_start_search();

	std::vector<FeedPtrPosPair> visible_feeds;

	Matcher m;
	bool apply_filter;

	History filterhistory;

	std::shared_ptr<RssFeed> search_dummy_feed;

	unsigned int filterpos;
	bool set_filterpos;

	SelectionType type;
	std::string value;
	std::vector<std::string> tags;
	std::vector<FilterNameExprPair> filters;

	std::string format_line(const std::string& selecttag_format,
		const std::string& tag,
		unsigned int pos,
		unsigned int width);

	FilterContainer* filters2;	// Naming should be sorted out.
};

} // namespace newsboat

#endif /* NEWSBOAT_SELECTFORMACTION_H_ */
