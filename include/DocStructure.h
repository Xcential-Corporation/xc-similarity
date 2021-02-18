#ifndef DOCSTRUCTURE_H
#define DOCSTRUCTURE_H
#include <string>
#include <map>
#include <vector>
#include <memory>
#include "pugixml.hpp"

#define DOC_CONF_MAJOR_VERSION 2
#define DOC_CONF_MINOR_VERSION 6

typedef std::string string;

enum class DStyle {
    None = 0,
    Number,
    Alpha,
    ALPHA,
    Roman,
    ROMAN
};

class DocElement
{
public:
    typedef std::vector<std::string> Words;
    typedef std::map<std::string,std::string> Attributes;
    DocElement(string name_ = string()):
        name(name_) {}
    string name;
    bool is_level = false;
    bool has_designation = false;
    bool is_main_content = true;
    bool skip_text = false;
    bool case_sensitive_search = true;
    DStyle dstyle;
    Words singular;
    Words plural;
    Words xml_tag;
    Words filters;
    Attributes xml_attributes;
    string identifier;
    string lrc_name;
};

typedef std::shared_ptr<DocElement> DocElementP;

class DocStructure
{
public:
    DocStructure(string filename);
    DocStructure() {}
    virtual ~DocStructure();

    const DocElement& operator[](const string& s) const;
    const DocElement& getTagElement(const string& xml_tag) const;
    const DocElement& matchXML(const pugi::xml_node& n) const;
    bool isInFilter(const string& xml_tag, const string& filter_name) const;
    bool empty() const {
        return elements.empty();
    }

    typedef std::map<string, DocElementP> DocElements;
    typedef std::vector<DocElementP> DocElementsList;
    DocElements elements;
    std::map<string, DocElementsList> tagToElements;
};

DStyle str_to_dstyle(const string& s);
string dstyle_to_str(DStyle s);

#endif // DOCSTRUCTURE_H
