#include "pugixml.hpp"
#include "Flattened2.h"
#include <cstring>
#include <iostream>
#include <string>
#include <set>
#include <unordered_set>
#include <algorithm>

using namespace std;
using namespace pugi;

//typedef set<string> ngrams_set;
typedef set<size_t> ngrams_set;
typedef vector<xml_node> xml_nodes;
typedef map<string,xml_nodes> ngram_map;
typedef map<xml_node, int> match_table;


void loadXML(xml_document& doc, const string& filename)
{
    xml_parse_result result;

    result = doc.load_file(filename.c_str());
    if (!result)
    {
        cerr << "XML parsed with errors, attr value: [" << doc.child("node").attribute("attr").value() << "]\n";
        cerr << "Error description: " << result.description() << "\n";
        cerr << "Error offset: " << result.offset << " (error at [..." << result.offset << "]\n\n";
    }
}


ngrams_set get_ngrams(const xml_node& node, int n)
{
    ngrams_set res;
    Flattened flat(node, NULL, {});
    Token* first = flat.tokens.first();
    if (!first) {
        return res;
    }
    Token* last = first;
    for (int i=1; i<n && last->next; i++) {
        last = last->next;
    }
    TokenList ngram(first, last);
    do {
        string text = flat.get_text(ngram);
        //res.insert(text);
        res.insert(hash<string>{}(text));
    } while (ngram.shiftRight());
    return res;
}


void add_ngrams(const xml_node& node, int n, ngram_map& ngrams)
{
    Flattened flat(node, NULL, {});
    Token* first = flat.tokens.first();
    if (!first) {
        return;
    }
    Token* last = first;
    for (int i=1; i<n && last->next; i++) {
        last = last->next;
    }
    TokenList ngram(first, last);
    do {
        string text = flat.get_text(ngram);
        ngrams[text].push_back(node);
    } while (ngram.shiftRight());
}

match_table match_node_by_ngrams(const xml_node& node, int n, const ngram_map& ngrams)
{
    Flattened flat(node, NULL, {});
    Token* first = flat.tokens.first();
    if (!first) {
        return {};
    }
    Token* last = first;
    for (int i=1; i<n && last->next; i++) {
        last = last->next;
    }
    TokenList ngram(first, last);
    match_table res;
    do {
        string text = flat.get_text(ngram);
        auto it = ngrams.find(text);
        if (it==ngrams.end()) {
            continue;
        }
        for (const xml_node& match: it->second) {
            res[match]++;
        }
    } while (ngram.shiftRight());
    return res;
}

int main(int argc, char* argv[])
{
    int i = 1;
    int n = 3;
    if (argv[1]==string("-n")) {
        n = stoi(argv[2]);
        i+=2;
    }
    
    if ((argc-i)!=2) {
        cerr<<"Syntax: XMLSimilar [-n number] <from> <to>\n\n";
        exit(1);
    }
    string from_file = argv[i];
    string to_file = argv[i+1];
    xml_document from_doc;
    loadXML(from_doc, from_file);
    xml_document to_doc;
    loadXML(to_doc, to_file);
    ngrams_set from_ngrams = get_ngrams(from_doc, n);
    ngrams_set to_ngrams = get_ngrams(to_doc, n);
    ngrams_set intersection;
    set_intersection(from_ngrams.begin(), from_ngrams.end(), to_ngrams.begin(), to_ngrams.end(), inserter(intersection, intersection.begin()));
    int c1 = intersection.size()*100/to_ngrams.size();
    int c2 = intersection.size()*100/from_ngrams.size();
    cout<<"{\n";
    cout<<"    \"from_ngram_num\": "<<from_ngrams.size()<<",\n";
    cout<<"    \"to_ngram_num\": "<<to_ngrams.size()<<",\n";
    cout<<"    \"intersection_num\": "<<intersection.size()<<",\n";
    cout<<"    \"from_match_percent\": "<<c2<<",\n";
    cout<<"    \"to_match_percent\": "<<c1<<endl;
    cout<<"}\n";

  return 0;
}
