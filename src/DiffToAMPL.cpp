#include "Flattened2.h"
#include "xml-utils.h"
#include "shared/utils.h"
#include "shared/params.h"
#include <cstring>
#include <sstream>
#include <iomanip>
#include <memory>
#include "expr.h"
#include "Parser.h"
#include "ExprFactory.h"

#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/error/en.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"
#include <rapidjson/ostreamwrapper.h>

#include "boost/boost/uuid/uuid.hpp"
#include "boost/boost/uuid/uuid_io.hpp"
#include "boost/boost/uuid/uuid_generators.hpp"

using namespace std;
using namespace pugi;
using namespace rapidjson;

typedef rapidjson::Value JSONValue;
typedef rapidjson::PrettyWriter<rapidjson::OStreamWrapper> JSONWriter;
typedef set<string> StringSet;

string xml_to_string(const xml_node& node)
{
    if (node.type()==node_pcdata) {
        return node.value();
    }
    stringstream ss;
    node.print(ss);
    return ss.str();
}

class Amendment
{
public:
    Amendment(const xml_node& dn, const AIP::TokenList& dt, const xml_node& in, const AIP::TokenList& it);
    string getId() const;
    string getChangeType() const;
    xml_node getParentNode();
    
public:
    xml_node del_node;
    xml_node ins_node;
    AIP::TokenList del_tokens;
    AIP::TokenList ins_tokens;
    string id;
};

typedef std::shared_ptr<Amendment> AmendmentP;
typedef vector<AmendmentP> Amendments;


Amendment::Amendment(const xml_node& dn, const AIP::TokenList& dt, const xml_node& in, const AIP::TokenList& it)
    : del_node(dn), del_tokens(dt), ins_node(in), ins_tokens(it)
{
}

string Amendment::getId() const
{
    if (!id.empty()) {
        return id;
    }
    string res;
    AIP::Token* first = del_tokens.first();
    if (!first) {
        first = del_tokens.prev();
    }
    if (first && first->page>0) {
        res = "p"+to_string(first->page)+"l"+to_string(first->line);
        return res;
    }
    xml_node grandparent;
    if (del_node.empty()) {
        grandparent = ins_node.parent().parent();
    } else {
        grandparent = del_node.parent().parent();
    }
    res = grandparent.attribute("id").value();
    return res;
}

string getLevel(const xml_node& n)
{
    if (strcmp(n.name(), "ins")!=0 && strcmp(n.name(), "del")!=0) {
        return n.name();
    }
    if (n.first_child().type()==node_element) {
        return n.first_child().name();
    }
    return {};
}

string Amendment::getChangeType() const
{
    if (ins_node.empty() || ins_tokens.empty()) {
        string level = getLevel(del_node);
        if (!level.empty()) {
            return "strike("+level+")";
        }
        return "strike";
    }
    if (del_node.empty() || del_tokens.empty()) {
        string level = getLevel(ins_node);
        if (!level.empty()) {            
            return "insert("+level+")";
        }
        return "insert";
    }
    string pname = del_node.parent().name();
    if (pname=="enum" || pname=="num") {
        return "redesignate("+string(del_node.parent().parent().name())+")";
    }
    if (pname=="header" || pname=="heading") {
        return string("heading(")+del_node.parent().parent().name()+")";
    }
    string level = getLevel(del_node);
    if (!level.empty() && level==getLevel(ins_node)) {
        return "rewrite("+level+")";
    }
    return "strike and insert";
}

xml_node Amendment::getParentNode()
{
    if (del_node.empty()) {
        return ins_node.parent();
    }
    return del_node.parent();
}

class DiffToAMPL
{
public:
    DiffToAMPL(const string& doc_filename, const string& doc_conf_filename);
    
    void setBillnumber(const string& s) {
        billnumber = s;
    }
    
    string getLabel(const xml_node& node);
    
    void traverseXML();
    void traverseXML(const xml_node& node);
    void printAMPL(ostream& os);
    void printAMPL(JSONWriter* json_writer);
    
    void handleContext(ostream& os, const AmendmentP& amendment);
    void handleInsert(ostream& os, const AmendmentP& amendment);
    
private:
    void _addAmendment(const xml_node& del, const xml_node& ins);
    void _printOutline(xml_node node, JSONWriter* json_writer);
    string getPageAndLine(const AIP::TokenList& tokens);
    bool _isInOutline(const xml_node& node) const;

    string billnumber;
    DocStructure doc_conf;
    xml_document document;
    AIP::FlattenedP from_flat;
    AIP::FlattenedP to_flat;
    bool line_nums = true;
    xml_node skip_to;
    
    vector<AmendmentP> amendments;
    map<xml_node, Amendments> nodeToAmendments;
    map<xml_node, StringSet> actions;
};

DiffToAMPL::DiffToAMPL(const string& doc_filename, const string& doc_conf_filename)
    : doc_conf(doc_conf_filename)
{
    ExprFactory* factory = ExprFactory::getFactory();
    factory->processDocStructure(doc_conf);
    
    xml_parse_result result;

    if (doc_filename.empty()) {
        result = document.load(cin);
    } else {
        result = document.load_file(doc_filename.c_str());
    }
    if (!result)
    {
        cerr << "XML parsed with errors, attr value: [" << document.child("node").attribute("attr").value() << "]\n";
        cerr << "Error description: " << result.description() << "\n";
        cerr << "Error offset: " << result.offset << " (error at [..." << result.offset << "]\n\n";
    }
    from_flat = new AIP::Flattened(document, &doc_conf, {"ins"});
    to_flat = new AIP::Flattened(document, &doc_conf, {"del"});
    cerr<<"Del flat size = "<<from_flat->get_text().size()<<endl;
    cerr<<"Ins flat size = "<<to_flat->get_text().size()<<endl;
}

string DiffToAMPL::getLabel(const xml_node& node)
{
    xml_attribute name_attr = node.attribute("name");
    if (!name_attr.empty()) {
        return name_attr.value();
    }
    xml_node cur = node;
    string ret;
    string quoted_ret;
    while (!cur.parent().empty()) {
        string name = cur.name();
        if (is_quoted(name)) {
            ret = ":quoted:"+quoted_ret;
            //ret = " \""+quoted_ret+"\"";
            cur = cur.parent();
            continue;
        }
        string designation = getNodeDesignation(cur);
        cur = cur.parent();
        if (designation.empty()) {
            continue;
        }
        string short_name = doc_conf[name].identifier;
        string parent_name = cur.name();
        if (is_quoted(parent_name) && short_name.empty()) {
            quoted_ret = designation+quoted_ret;
        } else if (name=="section") {
            ret = name+"(\""+designation+ret+"\")";
            quoted_ret = ret;
        } else if (!short_name.empty()) { //Biglevel
            ret = name+"(\""+designation+"\")"+(ret.empty()?"":":")+ret;
            if (quoted_ret.empty()) {
                quoted_ret = name+"("+designation+")";
            }
        } else if (name=="main") {
            break;
        } else {
            ret = "("+string(designation)+")"+ret;
            quoted_ret = ret;
        }
        //cerr<<"ret = "<<ret<<endl;
    }
    return ret;
}

string nodeToString(const xml_node& node)
{
    ostringstream oss;
    node.print(oss, "\t", format_raw);
    return oss.str();
}

bool DiffToAMPL::_isInOutline(const xml_node& node) const
{
    return doc_conf.isInFilter(node.name(), "outline") && !node.attribute("id").empty();
}

void DiffToAMPL::_addAmendment(const xml_node& del, const xml_node& ins)
{
    //cerr<<"Add Amendment. Level="<<del.name()<<", parent="<<del.parent().name()<<" \n";
    AIP::TokenList dt;
    if (!del.empty()) {
        dt = from_flat->get_tokens(del);
    }
    
    AIP::TokenList it;
    if (!ins.empty()) {
        it = to_flat->get_tokens(ins);
    }
    if (dt.empty() && it.empty()) {
        return;
    }
    if (from_flat->get_text(dt)==to_flat->get_text(it)) {
        return;
    }
    cerr<<"Striking: "<<dt<<" and inserting: "<<it<<endl;
    /*
    while (from_flat->isAlnumBefore(dt) && to_flat->isAlnumBefore(it) && dt.widenLeft()) {
        it.widenLeft();
    }
    while (from_flat->isAlnumAfter(dt) && to_flat->isAlnumAfter(it) && dt.widenRight()) {
        it.widenRight();
    }
*/
    AmendmentP amendment = AmendmentP(new Amendment(del, dt, ins, it));
    amendments.push_back(amendment);
    xml_node parent = amendment->getParentNode();
    while(!_isInOutline(parent) && !parent.parent().empty()) {
        parent = parent.parent();
    }
    if (!parent.empty()) {
        nodeToAmendments[parent].push_back(amendment);
    }
}

string DiffToAMPL::getPageAndLine(const AIP::TokenList& tokens)
{
    string res;
    AIP::Token* first = tokens.first();
    if (!first) {
        first = tokens.prev();
    }
    if (first) {
        AIP::Token* last = tokens.last();
        if (last && first->page!=last->page) {
            res="page("+to_string(first->page)+"):line("+to_string(first->line)+") through page("+to_string(last->page)+"):line("+to_string(last->line)+")";
        } else if (last && first->line!=last->line) {
            res="page("+to_string(first->page)+"):(line("+to_string(first->line)+") through line("+to_string(last->line)+"))";
        } else {
            res="page("+to_string(first->page)+"):line("+to_string(first->line)+")";
        }
    }

    return res;
}


void DiffToAMPL::handleContext(ostream& os, const AmendmentP& amendment)
{
    xml_node grandparent;
    if (amendment->del_node.empty()) {
        grandparent = amendment->ins_node.parent().parent();
    } else {
        grandparent = amendment->del_node.parent().parent();
    }
    if (line_nums) {
        string pandl = getPageAndLine(amendment->del_tokens);
        if (pandl.empty()) {
            xml_node prev = amendment->ins_node.parent().previous_sibling();
            pandl = getPageAndLine(from_flat->get_tokens(prev));
        }
        if (!pandl.empty()) {
            os<<"select "<<pandl<<"; ";
            return;
        }
    }
    
    if (!grandparent.empty()) {
        os<<"select "<<getLabel(grandparent)<<"; ";
    }
}

void DiffToAMPL::handleInsert(ostream& os, const AmendmentP& amendment)
{
    os<<"insert ";
    AIP::TokenList prev = amendment->ins_tokens.prevTokenList();
    AIP::TokenList next = amendment->ins_tokens.nextTokenList();
    if (!next.empty() && next.size()>prev.size()) {
        os<<"before(search(\""<<to_flat->get_text(next)<<"\")) ";        
    } else if (!prev.empty()) {
        os<<"after(search(\""<<to_flat->get_text(prev)<<"\")) ";
    }
    os<<" \""<<to_flat->get_text(amendment->ins_tokens)<<"\"; ";
}


void DiffToAMPL::printAMPL(ostream& os)
{
    int i = 0;
    for (AmendmentP& amendment: amendments) {
        os<<"["<<std::setfill('0') << std::setw(4)<<++i<<"] ";
        string parent_name = amendment->del_node.parent().name();
        if (amendment->ins_node.empty()) {
            handleContext(os, amendment);
            xml_node node = amendment->del_node;
            if (line_nums) {
                AIP::TokenList tokens = from_flat->get_tokens(node);
                os<<"strike "<<getPageAndLine(tokens)<<"; ";
            } else {
                string name = string(node.name())+"("+getNodeDesignation(node)+")";
                os<<"strike "<<name<<"; ";
            }
        } else if (amendment->del_node.empty()) {
            handleContext(os, amendment);
            xml_node node = amendment->ins_node;
            if (strcmp(node.parent().name(), "diff-insert")==0) {
                node = node.parent();
            }
            xml_node prev_node = node.previous_sibling();
            while (!prev_node.empty() && strcmp(prev_node.name(), "diff-delete")==0) {
                prev_node = prev_node.previous_sibling();
            }
            string prev_name = string(prev_node.name())+"(\""+getNodeDesignation(prev_node)+"\")";
            if (prev_node.empty()) {
                os<<"insert begin ";
            } else {
                os<<"insert after("<<prev_name<<") ";
            }
            os<<"<__xml__>";
            amendment->ins_node.print(os);
            os<<"</__xml__>; ";
            //os<<"\"; ";
            
        } else if (!amendment->del_tokens.empty()) {
            if (parent_name=="enum") {
                handleContext(os, amendment);
                xml_node ppnode = amendment->del_node.parent().parent();
                string ppname = ppnode.name();
                string from = ppname+"(\""+clean_designation(from_flat->get_text(amendment->del_tokens))+"\")";
                string to = ppname+"(\""+clean_designation(to_flat->get_text(amendment->ins_tokens))+"\")";
                os<<"rename "+from+" as "+to+"; ";
            } else {
                handleContext(os, amendment);
                os<<"strike search(\""<<from_flat->get_text(amendment->del_tokens)<<"\"); ";
                if (!amendment->ins_tokens.empty()) {
                    os<<"insert \""<<to_flat->get_text(amendment->ins_tokens)<<"\"; ";
                }
            }
        } else if (!amendment->ins_tokens.empty()) {
            handleContext(os, amendment);
            handleInsert(os, amendment);
        }
        os<<endl;
    }
}

void printEnglish(ExprVec& stmts, JSONWriter* writer)
{
    //stringstream ampl_ss;
    stringstream ess;
    string prev_name;
    for (auto& expr: stmts) {
        if (!prev_name.empty() && prev_name!="select") {
            ess<<" and ";
        }
        try {
            expr->toEnglish(ess);
        } catch (std::exception& e) {
            cerr<<e.what()<<endl;
        }
        prev_name = expr->name();
    }
    //ctx.reset();
    writer->Key("xml");
    string xml = "<amendment-instruction><text>"+ess.str()+"</text></amendment-instruction>";
    writer->String(xml.c_str());
    // Load xml into doc so we can strip it from the xml tags.
    xml_document doc;
    xml_parse_result result = doc.load_string(xml.c_str());
    if (!result) {
        string msg = string("Error reading XML object. attr value: [") + doc.child("node").attribute("attr").value()
        + "Error description: " + result.description();
        throw AMPLException(ErrorKind::Runtime, ErrorType::StringNotFound, Severity::Error, UserMessageType::LikelySoftwareIssue, msg);
    }
    writer->Key("text");
    writer->String(getTextXML(doc, true).c_str());
}

void addAmendmentId(xml_node& node, const string& id)
{
    if (node.empty()) {
        return;
    }
    node.append_attribute("amendment").set_value(id.c_str());
}

void DiffToAMPL::_printOutline(xml_node node, JSONWriter* writer)
{
    string name = string(node.name());
    name[0]= name[0]+'A'-'a';
    string des = getNodeDesignation(node);
    if (!des.empty()) {
        name+= " "+des;
    }
    writer->Key("name");
    writer->String(name.c_str());
    xml_attribute id = node.attribute("id");
    if (!id.empty()) {
        writer->Key("id");
        writer->String(id.value());
    }
    xml_attribute identifier = node.attribute("identifier");
    if (!identifier.empty()) {
        writer->Key("identifier");
        writer->String(identifier.value());
    }
    xml_node num = node.child("num");
    if (num.empty()) {
        num = node.child("enum");
    }
    if (!num.empty()) {
        writer->Key("numXml");
        writer->String(nodeToString(num).c_str());
    }
    xml_node header = node.child("header");
    if (header.empty()) {
        header = node.child("heading");
    }
    if (!header.empty()) {
        writer->Key("headerXml");
        writer->String(nodeToString(header).c_str());
    }
    
    auto it = nodeToAmendments.find(node);
    if (it!=nodeToAmendments.end()) {
        writer->Key("amended_by");
        writer->StartArray();
        set<string> actions;
        for (const AmendmentP& a: it->second) {
            string amendment_id = a->getId();
            actions.insert(a->getChangeType());
            writer->String(amendment_id.c_str());
        }
        writer->EndArray();
        writer->Key("changeTypes");
        writer->StartArray();
        for (const string& s: actions) {
            writer->String(s.c_str());
        }
        writer->EndArray();
    }
    // Traverse children first, to check if any is included in outline
    vector<xml_node> provisions;
    for (const xml_node& child: node.children()) {
        if (_isInOutline(child)) {
            provisions.push_back(child);
        }
    }
    if (!provisions.empty()) {
        writer->Key("provisions");
        writer->StartArray();
        for (const xml_node& provision: provisions) {
            writer->StartObject();
            _printOutline(provision, writer);
            writer->EndObject();
        }
        writer->EndArray();
    }
}


void DiffToAMPL::printAMPL(JSONWriter* writer)
{
    int i = 0;
    writer->StartObject();
    if (!billnumber.empty()) {
        writer->Key("billnumber");
        writer->String(billnumber.c_str());
    }
    boost::uuids::uuid tag = boost::uuids::random_generator()();
    string uid = boost::uuids::to_string(tag);
    writer->Key("id");
    writer->String(uid.c_str());
    writer->Key("amendments");
    writer->StartArray();
    
    for (AmendmentP& amendment: amendments) {
        writer->StartObject();
        writer->Key("amendmentOrdinal");
        writer->Int(++i);
        writer->Key("id");
        string id = to_string(i);
        writer->String(id.c_str());
        amendment->id = id;
        writer->Key("locationId");
        string locationId = amendment->getId();
        writer->String(locationId.c_str());
        writer->Key("changeType");
        writer->String(amendment->getChangeType().c_str());
        addAmendmentId(amendment->del_node, id);
        addAmendmentId(amendment->ins_node, id);
        ostringstream os;
        string parent_name = amendment->del_node.parent().name();
        if (amendment->ins_node.empty()) {
            handleContext(os, amendment);
            xml_node node = amendment->del_node;
            if (line_nums) {
                AIP::TokenList tokens = from_flat->get_tokens(node);
                os<<"strike "<<getPageAndLine(tokens)<<"; ";
            } else {
                string name = string(node.name())+"("+getNodeDesignation(node)+")";
                os<<"strike "<<name<<"; ";
            }
        } else if (amendment->del_node.empty()) {
            handleContext(os, amendment);
            xml_node node = amendment->ins_node;
            if (strcmp(node.parent().name(), "diff-insert")==0 || strcmp(node.parent().name(),"ins")==0) {
                node = node.parent();
            }
            xml_node prev_node = node.previous_sibling();
            while (!prev_node.empty() && (strcmp(prev_node.name(), "diff-delete")==0 || strcmp(prev_node.name(), "line")==0 || strcmp(prev_node.name(), "page")==0) ) {
                prev_node = prev_node.previous_sibling();
            }
            if (strcmp(prev_node.name(), "ins")==0) {
                prev_node = prev_node.first_child();
            }
            auto& el = doc_conf.getTagElement(prev_node.name());
            string prev_name = el.name;        
            if (el.has_designation) {
                prev_name += "(\""+getNodeDesignation(prev_node)+"\")";
            }
            if (prev_node.empty()) {
                os<<"insert begin ";
            } else {
                cerr<<"Inserting after "<<prev_node.name()<<endl;
                os<<"insert after("<<prev_name<<") ";
            }
            os<<"<__xml__>";
            amendment->ins_node.print(os);
            os<<"</__xml__>; ";
            //os<<"\"; ";
            
        } else if (!amendment->del_tokens.empty()) {
            if (parent_name=="enum") {
                handleContext(os, amendment);
                xml_node ppnode = amendment->del_node.parent().parent();
                string ppname = ppnode.name();
                string from = ppname+"(\""+clean_designation(from_flat->get_text(amendment->del_tokens))+"\")";
                string to = ppname+"(\""+clean_designation(to_flat->get_text(amendment->ins_tokens))+"\")";
                os<<"rename "+from+" as "+to+"; ";
            } else {
                handleContext(os, amendment);
                os<<"strike search(\""<<from_flat->get_text(amendment->del_tokens)<<"\"); ";
                if (!amendment->ins_tokens.empty()) {
                    os<<"insert \""<<to_flat->get_text(amendment->ins_tokens)<<"\"; ";
                }
            }
        } else if (!amendment->ins_tokens.empty()) {
            handleContext(os, amendment);
            handleInsert(os, amendment);
        }
        writer->Key("ampl");
        string ampl = os.str();
        writer->String(ampl.c_str());
        AMPLParser::Parser parser(ampl);
        ExprVec stmts = parser.statements();
        printEnglish(stmts, writer);
        writer->EndObject();
    }
    writer->EndArray();
    writer->Key("results");
    writer->StartObject();
    writer->Key("outline");
    writer->StartArray();
    writer->StartObject();
    _printOutline(document.first_child(), writer);
    writer->EndObject();
    writer->EndArray();
    
    writer->Key("xmls");
    writer->StartArray();
    {
        ostringstream oss;
        document.print(oss);
        writer->String(oss.str().c_str());
    }
    writer->EndArray();
    writer->EndObject(); // "results" object
    
    writer->EndObject();
}


void DiffToAMPL::traverseXML()
{
    skip_to = {};
    traverseXML(document);
}

xml_node get_next_node(const xml_node& node, const AIP::FlattenedP& flat, bool inserted)
{
    const AIP::TokenList& tokens = flat->get_tokens(node);
    AIP::Token* next_token = tokens.next();
    if (!next_token) {
        return {};
    }
    if (inserted?!next_token->inserted:!next_token->deleted) {
        return {};
    }
    xml_node res = next_token->node(true);
    while (!res.empty() && strcmp(res.name(),"ins")!=0 && strcmp(res.name(), "del")!=0) {
        cerr<<"Node = "<<res.name()<<endl;
        res = res.parent();
    }
    return res;
}

bool containsProvision(const xml_node& node)
{
    xml_node child = node.first_child();
    return child.type()==node_element && child.next_sibling().empty();
}

void DiffToAMPL::traverseXML(const xml_node& node)
{
    for (xml_node& child: node.children()) {
        if (!skip_to.empty()) {
            cerr<<"Skipping: ";child.print(cerr);
            if (skip_to==child) {
                skip_to = xml_node();
            } else {
                traverseXML(child);
            }
            continue;
        }
        xml_attribute xcdiff_from = child.attribute("xcdiff:from");
        xml_attribute xcdiff_to = child.attribute("xcdiff:to");
        
        if (!xcdiff_from.empty() && xcdiff_to.empty()) {
            _addAmendment(child, xml_node());
        } else if (xcdiff_from.empty() && !xcdiff_to.empty()) {
            _addAmendment(xml_node(), child);
        } else if (strcmp(child.name(), "del")==0) {
            xml_node next = get_next_node(child, to_flat, true);
            if (!next.empty() && strcmp(next.name(), "ins")==0 && !containsProvision(next)) {
                _addAmendment(child, next);
                skip_to = next;
            } else {
                _addAmendment(child, child);            
            }
        } else if (strcmp(child.name(), "ins")==0) {
            xml_node next = get_next_node(child, from_flat, false);
            if (!next.empty() && strcmp(next.name(), "del")==0) {
                _addAmendment(next, child);
                skip_to = next;
                cerr<<"Skipping next\n";
            } else {
                if (containsProvision(child)) {
                    _addAmendment(xml_node(), child.first_child());
                } else {
                    _addAmendment(child, child);
                }
            }
        } else {
            traverseXML(child);
        }
    }
}

int main(int argc, char* argv[])
{
  xcite::Params args(argc, argv);
  DiffToAMPL diff2ampl({}, "document.conf");
  while (!args.isEnd() && args.isOpt()) {
      string arg = args.nextArg();
      if (arg=="-billnumber") {
          diff2ampl.setBillnumber(args.nextArg());
      }
  }
 
  diff2ampl.traverseXML();
  OStreamWrapper osw(cout);
  JSONWriter *json_writer = new PrettyWriter<OStreamWrapper>(osw);
  
  diff2ampl.printAMPL(json_writer);
  delete json_writer;
  return 0;
}
