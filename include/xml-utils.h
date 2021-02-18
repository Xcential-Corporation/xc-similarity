#ifndef XML_UTILS_H
#define XML_UTILS_H

#include <vector>
#include <set>
#include <map>
#include <iostream>
#include "pugixml.hpp"
#include "DocStructure.h"
#include <iostream>

typedef std::string string;

string getTextXML(const pugi::xml_node& node, bool include_deleted = false);

string getNodeDesignation(const pugi::xml_node& node);

string get_identifier_part(DocStructure& doc_conf, pugi::xml_node& node);

void remove_identifier_attrs(pugi::xml_node& node);

void get_affecting_amendments(pugi::xml_node& node, /* output*/std::set<string>& s);

string get_node_xcitepath(const pugi::xml_node& node);

bool similar_node(const pugi::xml_node& n1, const pugi::xml_node& n2);

#endif // XML_UTILS_H
