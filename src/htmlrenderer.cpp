#include <htmlrenderer.h>
#include <xmlpullparser.h>
#include <sstream>

using namespace noos;

htmlrenderer::htmlrenderer(unsigned int width) : w(width) { }

std::vector<std::string> htmlrenderer::render(const std::string& source) {
	std::vector<std::string> lines;
	std::vector<std::string> links;
	unsigned int link_count = 0;
	std::string curline;
	int indent_level = 0;
	bool inside_list = false, inside_li = false, is_ol = false;
	unsigned int ol_count = 1;
	
	std::istringstream input(source);
	xmlpullparser xpp;
	xpp.setInput(input);
	
	for (xmlpullparser::event e = xpp.next(); e != xmlpullparser::END_DOCUMENT; e = xpp.next()) {	
		switch (e) {
			case xmlpullparser::START_TAG:
				if (xpp.getText() == "a") {
					std::string link = xpp.getAttributeValue("href");
					if (link.length() > 0) {
						links.push_back(link);
						std::ostringstream ref;
						ref << "[" << link_count << "]";
						link_count++;
						curline.append(ref.str());
					}
				} else if (xpp.getText() == "br") {
					if (curline.length() > 0)
						lines.push_back(curline);
					prepare_newline(curline, indent_level);	
				} else if (xpp.getText() == "img") {
					std::string imgurl = xpp.getAttributeValue("src");
					if (imgurl.length() > 0) {
						links.push_back(imgurl);
						std::ostringstream ref;
						ref << "[" << link_count << "]";
						link_count++;
						curline.append(ref.str());
					}					
				} else if (xpp.getText() == "blockquote") {
					++indent_level;
					if (curline.length() > 0)
						lines.push_back(curline);
					lines.push_back(std::string(""));
					prepare_newline(curline, indent_level);	
				} else if (xpp.getText() == "p") {
					if (curline.length() > 0)
						lines.push_back(curline);
					if (lines[lines.size()-1].length() > static_cast<unsigned int>(indent_level*2))
						lines.push_back(std::string(""));
					prepare_newline(curline, indent_level);	
				} else if (xpp.getText() == "ol") {
					inside_list = true;
					is_ol = true;
					ol_count = 1;
					if (curline.length() > 0)
						lines.push_back(curline);	
					lines.push_back(std::string(""));
					prepare_newline(curline, indent_level);	
				} else if (xpp.getText() == "ul") {
					inside_list = true;
					is_ol = false;
					if (curline.length() > 0)
						lines.push_back(curline);
					lines.push_back(std::string(""));
					prepare_newline(curline, indent_level);						
				} else if (xpp.getText() == "li") {
					if (inside_li) {
						indent_level-=2;
						if (curline.length() > 0)
							lines.push_back(curline);
						prepare_newline(curline, indent_level);	
					}
					inside_li = true;
					if (curline.length() > 0)
						lines.push_back(curline);
					prepare_newline(curline, indent_level);
					indent_level+=2;
					if (is_ol) {
						std::ostringstream num;
						num << ol_count;
						if (ol_count < 10)
							curline.append(" ");
						curline.append(num.str());
						curline.append(". ");
						++ol_count;
					} else {
						curline.append("  * ");
					}
				}
				break;
			case xmlpullparser::END_TAG:
				if (xpp.getText() == "blockquote") {
					--indent_level;
					if (indent_level < 0)
						indent_level = 0;
					if (curline.length() > 0)
						lines.push_back(curline);
					lines.push_back(std::string(""));
					prepare_newline(curline, indent_level);
				} else if (xpp.getText() == "ol" || xpp.getText() == "ul") {
					inside_list = false;
					if (inside_li) {
						indent_level-=2;
						if (curline.length() > 0)
							lines.push_back(curline);
						prepare_newline(curline, indent_level);
					}
					if (curline.length() > 0)
						lines.push_back(curline);
					lines.push_back(std::string(""));
					prepare_newline(curline, indent_level);	
				} else if (xpp.getText() == "li") {
					indent_level-=2;
					inside_li = false;
					if (curline.length() > 0)
						lines.push_back(curline);
					prepare_newline(curline, indent_level);
				}
				break;
			case xmlpullparser::TEXT:
				{
					std::vector<std::string> words = xmlpullparser::tokenize(xpp.getText());
					unsigned int i=0;
					for (std::vector<std::string>::iterator it=words.begin();it!=words.end();++it,++i) {
						if ((curline.length() + it->length()) >= w) {
							lines.push_back(curline);
							prepare_newline(curline, indent_level);
						}
						curline.append(*it);
						if (i < words.size()-1)
							curline.append(" ");
					}
				}
				break;
			default:
				/* do nothing */
				break;
		}
	}
	lines.push_back(curline);
	
	if (links.size() > 0) {
		lines.push_back(std::string(""));
		lines.push_back(std::string("Links: "));
		for (unsigned int i=0;i<links.size();++i) {
			std::ostringstream line;
			line << "[" << i << "]: " << links[i];
			lines.push_back(line.str());
		}
	}

	return lines;
}

void htmlrenderer::prepare_newline(std::string& line, int indent_level) {
	line = "";
	for (int i=0;i<indent_level;++i) {
		line.append("  ");	
	}
}
