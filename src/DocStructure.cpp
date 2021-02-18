#include "DocStructure.h"
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/error/en.h"
#include <iostream>

using namespace std;
using namespace rapidjson;

DStyle str_to_dstyle(const string& s) {
    if (s=="number") {
        return DStyle::Number;
    } else if (s=="alpha") {
        return DStyle::Alpha;
    } else if (s=="ALPHA") {
        return DStyle::ALPHA;
    } else if (s=="roman") {
        return DStyle::Roman;
    } else if (s=="ROMAN") {
        return DStyle::ROMAN;
    }
    return DStyle::None;
}

string dstyle_to_str(DStyle s) {
    if (s==DStyle::Number) {
        return "number";
    } else if (s==DStyle::Alpha) {
        return "alpha";
    } else if (s==DStyle::ALPHA) {
        return "ALPHA";
    } else if (s==DStyle::Roman) {
        return "roman";
    } else if (s==DStyle::ROMAN) {
        return "ROMAN";
    }
    return "none";
}

DocElementP parseJsonElement(string name, rapidjson::Value value)
{
    auto json_obj = value.GetObject();
    DocElementP ret = make_shared<DocElement>(name);
    Value::MemberIterator it = json_obj.FindMember("is_level");
    if (it!=json_obj.MemberEnd()) {
       ret->is_level = it->value.GetBool();
    }
    if ((it=json_obj.FindMember("designation"))!=json_obj.MemberEnd()) {
        ret->has_designation = it->value.GetBool();
    }
    if ((it=json_obj.FindMember("is_main_content"))!=json_obj.MemberEnd()) {
        ret->is_main_content = it->value.GetBool();
    }
    if ((it=json_obj.FindMember("skip_text"))!=json_obj.MemberEnd()) {
        ret->skip_text = it->value.GetBool();
    }
    if ((it=json_obj.FindMember("case_sensitive_search"))!=json_obj.MemberEnd()) {
        ret->case_sensitive_search = it->value.GetBool();
    }
    
    if ((it=json_obj.FindMember("dstyle"))!=json_obj.MemberEnd()) {
        ret->dstyle = str_to_dstyle(it->value.GetString());
    }
    if ((it=json_obj.FindMember("xml_tag"))!=json_obj.MemberEnd()) {
        DocElement::Words& xml_tag = ret->xml_tag;
        for (auto& val: it->value.GetArray()) {
            xml_tag.push_back(val.GetString());
        }
    }
    if ((it=json_obj.FindMember("filters"))!=json_obj.MemberEnd()) {
        DocElement::Words& filters = ret->filters;
        for (auto& val: it->value.GetArray()) {
            filters.push_back(val.GetString());
        }
    }
    if ((it=json_obj.FindMember("short"))!=json_obj.MemberEnd()) {
        ret->identifier = it->value.GetString();
    }
    if ((it=json_obj.FindMember("LRCname"))!=json_obj.MemberEnd()) {
        ret->lrc_name = it->value.GetString();
    }
    if ((it=json_obj.FindMember("singular"))!=json_obj.MemberEnd()) {
        DocElement::Words& singular = ret->singular;
        for (auto& val: it->value.GetArray()) {
            singular.push_back(val.GetString());
        }
    }
    if ((it=json_obj.FindMember("plural"))!=json_obj.MemberEnd()) {
        DocElement::Words& plural = ret->plural;
        for (auto& val: it->value.GetArray()) {
            plural.push_back(val.GetString());
        }
    }
    if ((it=json_obj.FindMember("xml_attributes"))!=json_obj.MemberEnd()) {
        DocElement::Attributes& attrs = ret->xml_attributes;
        for (auto& val: it->value.GetArray()) {
            string s = val.GetString();
            size_t pos = s.find("=");
            if (pos==string::npos) {
                attrs[s]="";
            } else {
                attrs[s.substr(0,pos)] = s.substr(pos+1);
            }
        }
    }


    return ret;
}

DocStructure::DocStructure(string filename)
{
    Document json;
    FILE* fp;
    fp = fopen(filename.c_str(), "r");
    if (!fp) {
        cerr<<"Error: cannot open "<<filename<<" for input\n";
        exit(1);
    }

    char readBuffer[65536];
    FileReadStream jsonfile(fp, readBuffer, sizeof(readBuffer));
    json.ParseStream<kParseCommentsFlag>(jsonfile);
    if (json.HasParseError()) {
        cerr<<"Schema file '"<<filename<<"' is not a valid JSON\n";
        cerr<<"Error(offset "<<static_cast<unsigned>(json.GetErrorOffset())<<"): "
            <<GetParseError_En(json.GetParseError())<<endl;
        fclose(fp);
        exit(EXIT_FAILURE);
    }
    
    string doc_ver = to_string(json["doc_conf_version"].GetDouble());
    size_t dot_pos = doc_ver.find(".");
    int major_version = 0;
    int minor_version = 0;
    if (dot_pos==string::npos) {
        major_version = stoi(doc_ver);
    } else {
        major_version = stoi(doc_ver.substr(0,dot_pos));
        minor_version = stoi(doc_ver.substr(dot_pos+1));
    }
    if (major_version!=DOC_CONF_MAJOR_VERSION || minor_version<DOC_CONF_MINOR_VERSION) {
        cerr<<"Error: expected doc_conf_version of "<<filename<<" should be "<<DOC_CONF_MAJOR_VERSION<<"."<<DOC_CONF_MINOR_VERSION<<endl;
        exit(1);
    }
    Value::MemberIterator elements_it = json.FindMember("elements");
    if (elements_it==json.MemberEnd()) {
        return;
    }
    auto els = elements_it->value.GetObject();

    for (auto& it: els) {
        string name = it.name.GetString();
        DocElementP element = parseJsonElement(name, it.value.GetObject());
        elements[name] = element;
        for (const string& tag: element->xml_tag) {
            tagToElements[tag].push_back(element);
        }
    }

}

const DocElement& DocStructure::operator[](const string& s) const
{
    static DocElement empty_element;
    auto it = elements.find(s);
    if (it==elements.end())
        return empty_element;
    return *it->second;
}

const DocElement& DocStructure::getTagElement(const string& xml_tag) const
{
    static DocElement empty_element;
    auto it = tagToElements.find(xml_tag);
    if (it==tagToElements.end() || (it->second).empty()) {
        return empty_element;
    }
    return *(it->second)[0];
    
}

bool DocStructure::isInFilter(const string& xml_tag, const string& filter_name) const
{
    const DocElement& e = getTagElement(xml_tag);
    if (e.name.empty()) {
        return false;
    }
    for (const string& filter: e.filters) {
        if (filter==filter_name) {
            return true;
        }
    }
    return false;
}

bool matchAttribute(const DocElementP& e, const pugi::xml_node& n)
{
    for (auto& it: e->xml_attributes) {
        pugi::xml_attribute attr = n.attribute(it.first.c_str());
        if (attr.empty()) {
            return false;
        }
        if (it.second!=attr.value()) {
            return false;
        }
    }
    return true;
}

const DocElement& DocStructure::matchXML(const pugi::xml_node& n) const
{
    static DocElement empty_element;    
    string tag = n.name();
    auto it = tagToElements.find(tag);
    if (it==tagToElements.end()) {
        return empty_element;
    }
    
    
    for (const DocElementP& e: it->second) {
        if (matchAttribute(e, n)) {
            return *e;
        }
    }
    return empty_element;
}


DocStructure::~DocStructure()
{
    //dtor
}

