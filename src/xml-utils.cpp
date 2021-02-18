#include "utils.h"
#include "xml-utils.h"
#include <sstream>
#include <cstring>

using namespace std;
using namespace pugi;

string getTextXML(const xml_node& node, bool include_children)
{
    if (node.type()==node_pcdata) {
        return node.value();
    }
    string res;
    for (xml_node child : node.children()) {
        string name = child.name();
        if (name=="quote" || name=="quotedContent") {
            res += "\""+getTextXML(child,true)+"\"";
        } else if (name=="line" || name=="page") {
            continue;
        } else if (include_children || name.empty() || name=="ins" || name=="legis-comment")
            res += getTextXML(child, include_children);
    }
    return res;
}

// Note: this is adapted from XMLextract.
string getNodeDesignation(const xml_node& node)
{
    string designation;
    xml_node num_node = node.child("num");
    if (!num_node.empty()) { // USLM
        xml_attribute val_attr = num_node.attribute("value");
        if (!val_attr.empty()) {
            designation = val_attr.value();
            designation = normalize_white_spaces(strip_nonalnum(designation), '_');
        }
            
    } else { // DTD
        num_node = node.child("enum");
        if (!num_node.empty()) {
            string text = getTextXML(num_node);
            designation = clean_designation(text);
        } else if (string(node.name())=="toc-entry") {
            designation = getTextXML(node);
        }
    }
    return designation;
}

string get_identifier_part(DocStructure& doc_conf, xml_node& node)
{
    string name = node.name();
    string designation = getNodeDesignation(node);
    string short_name = doc_conf[name].identifier;
    if (!designation.empty() || !doc_conf[name].has_designation && !short_name.empty()) {
        return short_name+designation;
    }
    return string();

}

void remove_identifier_attrs(pugi::xml_node& node)
{
    xml_attribute attr = node.attribute("identifier");
    if (!attr.empty()) {
        attr.set_name("orig_identifier");
    }
    node.remove_attribute("db:identifier");
    node.remove_attribute("db:xcitepath");
    for (xml_node& child: node.children()) {
        remove_identifier_attrs(child);
    }
}

void get_affecting_amendments(pugi::xml_node& node, set<string>& s)
{
    string name = node.name();
    if (name=="ins" || name=="del") {
        xml_attribute attr = node.attribute("amendment");
        if (!attr.empty()) {
            s.insert(attr.value());
        }
    }
    for (xml_node child: node.children()) {
        get_affecting_amendments(child, s);
    }
}

string get_node_xcitepath(const pugi::xml_node& node)
{
    xml_attribute attr = node.attribute("db:xcitepath");
    if (!attr.empty()) {
        return attr.value();
    }
    return {};
}

bool similar_node(const pugi::xml_node& n1, const pugi::xml_node& n2)
{
    string name1 = n1.name();
    if (name1.empty()) {
        return false;
    }
    string name2 = n2.name();
    if (name1!=name2) {
        return false;
    }
    string des1 = getNodeDesignation(n1);
    string des2 = getNodeDesignation(n2);
    return des1==des2;
}
