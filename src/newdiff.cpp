#include "Flattened2.h"
#include "xml-utils.h"
#include "Interval.h"
#include <unordered_map>
#include <cstring>
#include <assert.h>

#include "diff-match-patch/diff_match_patch.h"

using namespace std;
using namespace pugi;

typedef vector<xml_node> xml_nodes;
typedef set<xml_node> xml_node_set;
typedef map<xml_node,xml_node> xml_node_map;
typedef map<string,xml_node> string_to_xml;
typedef map<string,xml_nodes> ngram_map;
typedef map<xml_node, int> match_table;

void count_nodes(AIP::Flattened* flat, const Interval& in, match_table& res)
{
    size_t i = in.first();
    //cerr<<"Interval from: "<<in.first()<<" to: "<<in.last()<<" size:"<<in.size()<<endl;
    //cerr<<"\""<<flat->get_text().substr(in.first(), in.size())<<"\""<<endl;
    while (i<=in.last()) {
        //cerr<<"P1. i = "<<i<<endl;
        AIP::Position pos = flat->getPos(i);
        AIP::DefaultSelector selector;
        AIP::Flattened_node* fn = flat->get_fn(pos.node);
        //cerr<<"length="<<fn->length<<", offset="<<pos.offset<<", last="<<in.last()<<endl;
        size_t num = min(fn->length-pos.offset, in.last()+1-i);
        //cerr<<"P1. i="<<i<<" node:"<<fn->node.name()<<" num="<<num<<endl;
        if (num>0) {
            i+=(pos.offset+num);
            //cerr<<"P2. offset = "<<pos.offset<<", num = "<<num<<endl;
        } else {
            i++;
        }
        while (fn) {
            if (selector(fn->node)) {
                res[fn->node]+=num;
            }
            fn = fn->parent;
        }
    }
}

match_table count_mapped_nodes(AIP::Flattened* flat, PositionMap& pmap, const Interval& from)
{
    //cerr<<"From interval from: "<<from.first()<<" to: "<<from.last()<<" size:"<<from.size()<<endl;
    Intervals to = pmap.projection(from);
    match_table res;
    for (const Interval& in: to) {
        count_nodes(flat, in, res);
    }
    return res;
}

void print_node_header(const xml_node& n)
{
    if (n.empty()) {
        cerr<<"Empty";
        return;
    }
    if (n.type()==node_pcdata) {
        cerr<<"pcdata";
        return;
    }
    xml_attribute id = n.attribute("id");
    if (!id.empty()) {
        cerr<<"id:"<<id.value()<<" ";
    }
    xml_node num = n.child("enum");
    if (num.empty()) {
        num = n.child("num");
    }
    xml_node header = n.child("header");
    if (header.empty()) {
        header = n.child("heading");
    }
    if (!num.empty()) {
        cerr<<num.first_child().value()<<". ";
    }
    if (!header.empty()) {
        cerr<<getTextXML(header);
    }
    cerr<<" node:"<<n.name();
}

void print_mapping(const xml_node_map& nmap)
{
    for (auto& m: nmap) {
        print_node_header(m.first);
        cerr<<" -> ";
        print_node_header(m.second);
        cerr<<endl;
    }
}

xml_node insert_provision(xml_node inserted, xml_node after, xml_node parent)
{
    xml_node ins_node;
    if (after.empty()) {
        ins_node = parent.prepend_child("diff-insert");
        xml_node res = ins_node.append_copy(inserted);
        return ins_node;
    }
    string pname = after.parent().name();
    if (pname=="ins" || pname=="diff-insert") {
        after = after.parent();
    }
    if (after.parent()==parent) {
        ins_node = parent.insert_child_after("diff-insert", after);
    } else {
        ins_node = parent.append_child("diff-insert");
    }
    xml_node res = ins_node.append_copy(inserted);
    return ins_node;
}

typedef diff_match_patch<string> DMPS;
typedef DMPS::Diffs Diffs;

void align_to_tokens(Diffs& diffs, AIP::Flattened& from_flat, AIP::Flattened& to_flat)
{
    size_t from = 0;
    size_t to = 0;
    string inserted;
    string deleted;
    
    auto it = diffs.begin();
    while (it!=diffs.end()) {
        auto& diff = *it;
        switch (diff.operation) {
            case DMPS::Operation::DELETE: {
                from+= diff.text.size();
                AIP::Token* t = from_flat.get_token(from-1);
                if (t && t->end_offset>from && t->start_offset<from) {
                    deleted+=diff.text;
                    auto prev = it++;
                    diffs.erase(prev);
                    continue;
                }
                
                if (!deleted.empty()) {
                    diff.text = deleted+diff.text;
                    deleted.clear();
                }
                break;
            }
            case DMPS::Operation::INSERT: {
                to+= diff.text.size();
                AIP::Token* t = to_flat.get_token(to-1);
                if (t && t->end_offset>to && t->start_offset<to) {
                    inserted+=diff.text;
                    auto prev = it++;
                    diffs.erase(prev);
                    continue;
                }
                if (!deleted.empty()) { /* ensure that deleted is before inserted */
                    diffs.insert(it, DMPS::Diff(DMPS::Operation::DELETE, deleted));
                    deleted.clear();
                }
                if (!inserted.empty()) {
                    diff.text = inserted+diff.text;
                    inserted.clear();
                }
                break;
            }
            case DMPS::Operation::EQUAL: {
                size_t size = diff.text.size();
                AIP::Token* t = from_flat.get_token(from);
                if (t && t->start_offset<from) {
                    int r = t->end_offset-from;
                    if (r>size) {
                        r = size;
                    }
                    //cerr<<"text="<<diff.text<<" token="<<*t<<" size="<<size<<" r ="<<r<<endl;
                    string added = diff.text.substr(0, r);
                    diff.text = diff.text.substr(r);
                    deleted+=added;
                    inserted+=added;
                }
                if (!deleted.empty()) {
                    diffs.insert(it, DMPS::Diff(DMPS::Operation::DELETE, deleted));
                    deleted.clear();
                }
                if (!inserted.empty()) {
                    diffs.insert(it, DMPS::Diff(DMPS::Operation::INSERT, inserted));
                    inserted.clear();
                }
                int r = 0;
                from+=size;
                to+=size;
                size = diff.text.size();
                AIP::Token* ft = from_flat.get_token(from-1);
                if (ft && ft->end_offset>from && ft->start_offset<from) {
                    r = from - ft->start_offset;
                }
                AIP::Token* tt = to_flat.get_token(to-1);
                if (tt && tt->end_offset>to && tt->start_offset<to) {
                    r = to - tt->start_offset;
                }
                if (r>size) {
                    r = size;
                }
                if (r>0) {
                    //cerr<<"size="<<size<<" r="<<r<<endl;
                    inserted = diff.text.substr(size-r);
                    deleted = inserted;
                    diff.text = diff.text.substr(0, size-r);
                }
                break;
            }
        };
        ++it;
    }
    if (!deleted.empty()) {
        diffs.push_back(DMPS::Diff(DMPS::Operation::DELETE, inserted));
        deleted.clear();
    }
    if (!inserted.empty()) {
        diffs.push_back(DMPS::Diff(DMPS::Operation::INSERT, inserted));
        inserted.clear();
    }
    
}


void compare_nodes(const xml_node& from_node, const xml_node& to_node)
{
    AIP::DefaultSelector selector;
    AIP::Flattened from_flat(from_node, NULL, {}, &selector);
    AIP::Flattened to_flat(to_node, NULL, {}, &selector);
    const string& from_text = from_flat.get_text();
    const string& to_text = to_flat.get_text();   
    DMPS dmp;
    auto diffs = dmp.diff_main(from_text, to_text, true);
    if (diffs.size() > 2) {
      dmp.diff_cleanupSemantic(diffs);
      dmp.diff_cleanupEfficiency(diffs);
    }
    align_to_tokens(diffs, from_flat, to_flat);
    size_t from = 0;
    size_t to = 0;
    
    for (auto& diff: diffs) {
        switch (diff.operation) {
            case DMPS::Operation::DELETE:
                from_flat.strike(from, from+diff.text.size());
                from+= diff.text.size();
                //cerr<<"<del>"<<diff.text<<"</del>";
                break;
            case DMPS::Operation::INSERT:
                from_flat.insert(from, diff.text);
                to+= diff.text.size();
                //cerr<<"<ins>"<<diff.text<<"</ins>";
                break;
            case DMPS::Operation::EQUAL: {
                //cerr<<diff.text;
                size_t size = diff.text.size();
                from+=size;
                to+=size;
                break;
            }
        };
        
    }
}

void merge_trees(xml_node from_node, xml_node to_node, const string& from_str, const string& to_str, bool biglevel, AIP::NodeSelector* stop = NULL)
{
    //cerr<<"Merging from: "; print_node_header(from_node); cerr<<" To: "; print_node_header(to_node);cerr<<endl;
    static const string diff_delete_str = "diff-delete";
    static const string diff_insert_str = "diff-insert";
    static const string ins_str = "ins";
    static const string del_str = "del";
    const string& insert_str = biglevel?diff_insert_str:ins_str;
    const string& delete_str = biglevel?diff_delete_str:del_str;
    xml_node cur_from = from_node.first_child();
    xml_node cur_to = to_node.first_child();
    xml_node prev;
    while (!cur_from.empty() || !cur_to.empty()) {
        xml_attribute from_from_attr = cur_from.attribute(from_str.c_str());
        xml_attribute from_to_attr = cur_from.attribute(to_str.c_str());
        xml_attribute to_to_attr = cur_to.attribute(to_str.c_str());
        xml_attribute to_from_attr = cur_to.attribute(from_str.c_str());
        //cerr<<"cur_from: "; print_node_header(cur_from); cerr<<" cur_to: "; print_node_header(cur_to);cerr<<endl;
        //cerr<<"from_from: "<<from_from_attr.value()<<" from_to: "<<from_to_attr.value()<<" to_from: "<<to_from_attr.value()<<" to_to:"<<to_to_attr.value()<<endl;
        
        if (!cur_from.empty() && from_from_attr.empty() && !to_to_attr.empty()) { //From node not included
            //cerr<<"P1. Skip from.\n";
            //merge_trees(cur_from, xml_node(), from_str, to_str, biglevel, stop);
            prev = cur_from;
            cur_from = cur_from.next_sibling();            
        } else if (!cur_to.empty() && to_to_attr.empty() && !from_from_attr.empty()) { //To node not included
            //cerr<<"P2. Skip to.\n";
            //merge_trees(xml_node(), cur_to, from_str, to_str, biglevel, stop);
            cur_to = cur_to.next_sibling();
        } else if (!from_from_attr.empty() && from_to_attr.empty()) { // From node deleted
            //cerr<<"P3. Delete from.\n";
            if (strcmp(cur_from.name(),cur_to.name())==0 && !to_to_attr.empty() && to_from_attr.empty()) {
                // Special handling for cur_from and cur_to with the same name, one is deleted and one is inserted. Compare instead ...
                compare_nodes(cur_from, cur_to);
                cur_from = cur_from.next_sibling();
                cur_to = cur_to.next_sibling();
            } else {
                xml_node temp = cur_from.next_sibling();
                xml_node del_node = cur_from.parent().insert_child_after(delete_str.c_str(), cur_from);
                del_node.append_move(cur_from);
                cur_from = temp;
                prev = del_node;
            }
        } else if (!to_to_attr.empty() && to_from_attr.empty()) { // To node inserted
            //cerr<<"P4. Insert to.\n";
            if (cur_to.attribute(from_str.c_str()).empty() && !cur_to.attribute(to_str.c_str()).empty()) {
                xml_node ins_node;
                if (prev.empty()) {
                    ins_node = from_node.prepend_child(insert_str.c_str());
                } else {
                    ins_node = from_node.insert_child_after(insert_str.c_str(), prev);
                }
                ins_node.append_copy(cur_to);
                prev = ins_node;
            }
            cur_to = cur_to.next_sibling();
        } else /*if (!from_from_attr.empty() && !from_to_attr.empty())*/ { //Nodes mapped to each other or both not included
            //cerr<<"P5. merge both. \n";
            
            if (!stop || !(*stop)(cur_from) || !(*stop)(cur_to)) { // Should it be "||" or "&&"? For a match, we assume it doesn't matter
                merge_trees(cur_from, cur_to, from_str, to_str, biglevel, stop);
            }
            prev = cur_from;
            cur_from = cur_from.next_sibling();
            cur_to = cur_to.next_sibling();
            continue;
        }
    }
}


void assign_attrs_biglevel(const xml_node_map& nmap, const xml_nodes& from_vec, const xml_nodes& to_vec)
{
    string from_str = "xcdiff:from";
    string to_str = "xcdiff:to";
    xml_node_set all_deleted;
    for (int i=0; i<from_vec.size(); i++) {
        xml_node n = from_vec[i];
        n.append_attribute(from_str.c_str()).set_value(i);
        auto it = nmap.find(n);
        if (it!=nmap.end()) {
            xml_node other = it->second;
            other.append_attribute(from_str.c_str()).set_value(i);
        } else {
            all_deleted.insert(n);
            xml_node del_node = n.parent().insert_child_after("diff-delete", n);
            del_node.append_move(n);
        }
    }

    xml_node_set all_inserted;
    xml_node prev;
    for (int i=0; i<to_vec.size(); i++) {
        xml_node n = to_vec[i];
        cerr<<"P0. Inserting: ";print_node_header(n);cerr<<endl;
        n.append_attribute(to_str.c_str()).set_value(i);
        xml_attribute from_attr = n.attribute(from_str.c_str());
        if (!from_attr.empty()) {
            int j = from_attr.as_int();
            xml_node other = from_vec[j];
            other.append_attribute(to_str.c_str()).set_value(i);
            prev = other;
            // Add insert node after all deleted nodes
            while (all_deleted.find(from_vec[j+1])!=all_deleted.end()) {
                j++;
                prev = from_vec[j];
            }
        } else {
            cerr<<"Inserting: ";print_node_header(n);cerr<<endl;
            all_inserted.insert(n);
            xml_node parent = n.parent();
            bool parent_inserted = false;
            while (!parent.empty()) {
                if (all_inserted.find(parent)!=all_inserted.end()) {
                    parent_inserted = true;
                    break;
                }
                parent = parent.parent();
            }
            xml_node from_parent = from_vec[0].parent();
            if (!n.parent().empty()) {
                xml_attribute from_parent_attr = n.parent().attribute(from_str.c_str());
                if (!from_parent_attr.empty()) {
                    xml_node new_from_parent = from_vec[from_parent_attr.as_int()];
                    if (new_from_parent.parent()==from_parent) {
                        prev = xml_node();
                    }
                    from_parent = new_from_parent;
                } else {
                    from_parent = prev.parent();
                }
            }
            if (!parent_inserted) {
                prev = insert_provision(n, prev, from_parent);
            }
        }
    }
}


void assign_attrs(const xml_node_map& nmap, const xml_nodes& from_vec, const xml_nodes& to_vec, bool biglevel)
{
    static const string from_str_p = "xcdiff:from-provision";
    static const string to_str_p = "xcdiff:to-provision";
    static const string from_str_b = "xcdiff:from";
    static const string to_str_b = "xcdiff:to";
    const string& from_str = biglevel?from_str_b:from_str_p;
    const string& to_str = biglevel?to_str_b:to_str_p;
    
    for (int i=0; i<from_vec.size(); i++) {
        xml_node n = from_vec[i];
        n.append_attribute(from_str.c_str()).set_value(i);
        auto it = nmap.find(n);
        if (it!=nmap.end()) {
            xml_node other = it->second;
            other.append_attribute(from_str.c_str()).set_value(i);
        }
    }

    for (int i=0; i<to_vec.size(); i++) {
        xml_node n = to_vec[i];
        n.append_attribute(to_str.c_str()).set_value(i);
        xml_attribute from_attr = n.attribute(from_str.c_str());
        if (!from_attr.empty()) {
            int j = from_attr.as_int();
            xml_node other = from_vec[j];
            other.append_attribute(to_str.c_str()).set_value(i);
        }
    }
}

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

void find_level(const xml_node& node, const string& level, xml_nodes& res)
{
    if (level==node.name()) {
        res.push_back(node);
        return;
    }
    for (const xml_node& child: node.children()) {
        find_level(child, level, res);
    }
}

xml_nodes find_level(const xml_node& node, const string& level)
{
    xml_nodes res;
    find_level(node, level, res);
    return res;
}

// Find all nodes that match "select". Parent nodes are listed before child nodes
bool find_provisions(const xml_node& node, xml_nodes& res, AIP::NodeSelector* select, AIP::NodeSelector* stop)
{
    bool lowest = true;
    if ((*select)(node)) {
        res.push_back(node);
    }
    for (const xml_node& child: node.children()) {
        if ((*stop)(child)) {
            continue;
        }
        string name = child.name();
        bool not_included = find_provisions(child, res, select, stop);
        lowest = lowest && not_included;
    }
    
    //if (/*lowest &&*/ (*select)(node)) {
    //    res.push_back(node);
    //    return false;
    //}
    return lowest;
}

xml_node_set node_vector_to_set(const xml_nodes& nodes)
{
    xml_node_set res;
    for (const xml_node& n: nodes) {
        res.insert(n);
    }
    return res;
}

void remove_mapped(const xml_node_map& nmap, xml_node_set& from, xml_node_set& to)
{
    for (auto& m: nmap) {
        from.erase(m.first);
        to.erase(m.second);
    }
}

void match_by_header(xml_node_set& from, xml_node_set& to, xml_node_map& nmap)
{
    string_to_xml headers;
    for (const xml_node& n: from) {
        xml_node header = n.child("header");
        if (header.empty()) {
            header = n.child("heading");
            if (header.empty()) {
                continue;
            }
        }
        headers[getTextXML(header)] = n;
    }
    for (const xml_node& n: to) {
        xml_node header = n.child("header");
        if (header.empty()) {
            header = n.child("heading");
            if (header.empty()) {
                continue;
            }
        }
        auto it = headers.find(getTextXML(header));
        if (it==headers.end()) {
            continue;
        }
        nmap[it->second] = n;
    }
    remove_mapped(nmap, from, to);
}

void add_ngrams(const xml_node& node, int n, ngram_map& ngrams)
{
    AIP::Flattened flat(node, NULL, {});
    AIP::Token* first = flat.tokens.first();
    if (!first) {
        return;
    }
    AIP::Token* last = first;
    for (int i=1; i<n && last->next; i++) {
        last = last->next;
    }
    AIP::TokenList ngram(first, last);
    do {
        string text = flat.get_text(ngram);
        ngrams[text].push_back(node);
    } while (ngram.shiftRight());
}

match_table match_node_by_ngrams(const xml_node& node, int n, const ngram_map& ngrams)
{
    AIP::Flattened flat(node, NULL, {});
    AIP::Token* first = flat.tokens.first();
    if (!first) {
        return {};
    }
    AIP::Token* last = first;
    for (int i=1; i<n && last->next; i++) {
        last = last->next;
    }
    AIP::TokenList ngram(first, last);
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

void match_by_ngrams(xml_node_set& from, xml_node_set& to, xml_node_map& nmap)
{
    ngram_map ngrams;
    for (const xml_node& node: from) {
        add_ngrams(node, 3, ngrams);
    }
    
    for (const xml_node& node: to) {
        match_table matches = match_node_by_ngrams(node, 3, ngrams);
        int max = 0;
        xml_node best_match;
        for (auto& m: matches) {
            if (m.second>max) {
                max = m.second;
                best_match = m.first;
            }
        }
        print_node_header(node);
        cerr<<", "<<max;
        AIP::Flattened flat(node, NULL, {});
        int size = flat.tokens.tokensNum()-2;
        int ratio = max*100/size;
        cerr<<" ("<<ratio<<"%)\n";
        if (!best_match.empty() && ratio>50) {
            nmap[best_match] = node;
        }
        
    }
    remove_mapped(nmap, from, to);
}

void match_parent_levels(xml_nodes& from, xml_nodes& to, xml_node_map& nmap)
{
    xml_node_map new_map = nmap;
    xml_node_set added;
    while (!new_map.empty()) {
        map<xml_node, match_table> parent_matches;
        for (auto& m: new_map) {
            xml_node from_parent = m.first.parent();
            xml_node to_parent = m.second.parent();
            if (!from_parent.empty() && !to_parent.empty()) {
                parent_matches[from_parent][to_parent]++;
            }
        }
        new_map.clear();
        for (auto& m: parent_matches) {
            if (added.find(m.first)!=added.end()) {
                continue;
            }
            xml_node max_node;
            int max_val = 0;
            for (auto& m2: m.second) {
                if (m2.second>max_val) {
                    max_val = m2.second;
                    max_node = m2.first;
                }
            }
            if (max_val>0) {
                cerr<<"Matching parent: ";print_node_header(m.first); cerr<<" with: "; print_node_header(max_node); cerr<<endl;
                nmap[m.first] = max_node;
                new_map[m.first] = max_node;
                added.insert(m.first);
                from.push_back(m.first);
                to.push_back(max_node);
            }
        }
    }
}

PositionMap diff_to_map(const string& text1, const string& text2)
{
    PositionMap res;
    DMPS dmp;
    auto diffs = dmp.diff_main(text1, text2, true);
    if (diffs.size() > 2) {
      dmp.diff_cleanupSemantic(diffs);
      dmp.diff_cleanupEfficiency(diffs);
    }
    size_t from = 0;
    size_t to = 0;
    for (auto& diff: diffs) {
        switch (diff.operation) {
            case DMPS::Operation::DELETE:
                from+= diff.text.size();
                break;
            case DMPS::Operation::INSERT:
                to+= diff.text.size();
                break;
            case DMPS::Operation::EQUAL: {
                size_t size = diff.text.size();
                res.add({from,from+size-1},{to,to+size-1});
                from+=size;
                to+=size;
                break;
            }
        };
    }
    return res;
    
}


void remove_with_children(const xml_node& node, xml_node_set& nodes)
{
    nodes.erase(node);
    for (const xml_node& child: node.children()) {
        remove_with_children(child, nodes);
    }
}

int xml_node_depth(const xml_node& n, const xml_node& top)
{
    int res = 0;
    xml_node cur = n;
    while (!cur.empty() && cur!=top) {
        cur = cur.parent();
        res++;
    }
    return res;
}

void compare_sections(const xml_node& from_node, const xml_node& to_node)
{
    cerr<<"Compare ";print_node_header(from_node);cerr<<" to: ";print_node_header(to_node);cerr<<endl;
    AIP::NodeIsMatched stop;
    
    AIP::Flattened from_flat(from_node, NULL, {}, &stop);
    AIP::Flattened to_flat(to_node, NULL, {}, &stop);
    /*
    diff_match_patch<string> dmp;
    const string& from_str = from_flat.get_text();
    const string& to_str = to_flat.get_text();    
    string strPatch = dmp.patch_toText(dmp.patch_make(from_str, to_str));
    cerr<<"Patch: "<<strPatch<<endl;
    */
    const string& from_text = from_flat.get_text();
    const string& to_text = to_flat.get_text();
    cerr<<"From size = "<<from_text.size()<<", to size = "<<to_text.size()<<endl;
    if (from_text.empty() || to_text.empty()) {
        return;
    }
    PositionMap pmap = diff_to_map(from_text, to_text);
    //pmap.output(cerr, from_text, to_text);
    xml_nodes provisions;
    AIP::DefaultSelector select;
    find_provisions(from_node, provisions, &select, &stop);
    xml_nodes to_provisions;
    find_provisions(to_node, to_provisions, &select, &stop);   
    xml_node_set remaining_to_nodes = node_vector_to_set(to_provisions);
    xml_node_map nmap;
    for (const xml_node& provision: provisions) {
        const AIP::TokenList& tokens = from_flat.get_tokens(provision);
        if (tokens.empty()) {
            continue;
        }
        Interval in(tokens.first()->start_offset, tokens.last()->end_offset-1);
        match_table tab = count_mapped_nodes(&to_flat, pmap, in);
        cerr<<"  "; print_node_header(provision);cerr<<" ("<<in.size()<<") ";
        if (tab.empty()) {
            cerr<<" deleted.\n";
        }
        xml_node to_provision;
        double max = 0;
        for (auto& m: tab) {
            size_t size2 = to_flat.get_tokens(m.first).size();
            size_t size = in.size();
            size_t larger = (size>size2)?size:size2;
            double score = m.second/(double)larger;
//            if (strcmp(provision.name(), m.first.name())!=0) {
//                continue;
//            }
            
//            cerr<<"compare-provision ";print_node_header(provision);cerr<<"(depth:"<<xml_node_depth(provision, from_node)<<")";
//            cerr<<" and ";print_node_header(m.first);cerr<<"(depth:"<<xml_node_depth(m.first, to_node)<<")\n";
            
//            if (xml_node_depth(provision, from_node)!=xml_node_depth(m.first, to_node)) {
//                continue;
//            }
            
            if (provision!=from_node && nmap[provision.parent()]!=m.first.parent() && 
                (select(provision.parent())||select(m.first.parent())||nmap[provision.parent().parent()]!=m.first.parent().parent())) {
                continue;
            }
            // Do not map to node if already used
            if (remaining_to_nodes.find(m.first)==remaining_to_nodes.end()) {
                continue;
            }
            if (m.second<larger) {
                cerr<<"\n             => "; print_node_header(m.first); cerr<<": "<<m.second<<" score:"<<score<<" ";
            } else {
                nmap[provision] = m.first;
                cerr<<"\n    size:"<<size2<<" score: "<<score<<" "; print_node_header(m.first); cerr<<": "<<m.second;
                cerr<<"perfect match.";
            }
            if (score>max) {
                max = score;
                to_provision = m.first;
            }
        }
        if (max>0.1) {
            nmap[provision] = to_provision;
            compare_nodes(provision, to_provision);
            //cout<<endl;
        }
        cerr<<endl;
        if (!to_provision.empty()) {
            cerr<<"Remove: ";print_node_header(to_provision);cerr<<endl;
            remaining_to_nodes.erase(to_provision);
        }
        
    }
    for (const xml_node& n: remaining_to_nodes) {
        cerr<<"Inserted: "; print_node_header(n);cerr<<endl;
    }
    assign_attrs(nmap, provisions, to_provisions, false);
    merge_trees(from_node, to_node, "xcdiff:from-provision", "xcdiff:to-provision", false, NULL);
}

int main(int argc, char* argv[])
{
    if (argc!=3) {
        cerr<<"Syntax: newdiff <from> <to>\n\n";
        exit(1);
    }
    string from_file = argv[1];
    string to_file = argv[2];
    xml_document from_doc;
    loadXML(from_doc, from_file);
    xml_document to_doc;
    loadXML(to_doc, to_file);
    
    xml_nodes from_sections = find_level(from_doc, "section");
    xml_node_set from_set = node_vector_to_set(from_sections);
    xml_nodes to_sections = find_level(to_doc, "section");
    xml_node_set to_set = node_vector_to_set(to_sections);
    
    xml_node_map nmap;
    match_by_header(from_set, to_set, nmap);
    match_by_ngrams(from_set, to_set, nmap);
    match_parent_levels(from_sections, to_sections, nmap);
    print_mapping(nmap);
    //assign_attrs_biglevel(nmap, from_sections, to_sections);
    assign_attrs(nmap, from_sections, to_sections, true);
    AIP::NodeIsName sectionNodes("section");
    merge_trees(from_doc, to_doc, "xcdiff:from", "xcdiff:to", true, &sectionNodes);
    
    
    for (auto& m: nmap) {
        compare_sections(m.first, m.second);
    }
    from_doc.print(cout, "\t", format_raw);

    
}
