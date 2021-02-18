#include "Flattened2.h"
#include "ctype.h"
#include <algorithm>
#include <cstring>
#include "utils.h"
#include "xml-utils.h"

using namespace pugi;
using namespace std;

Token::~Token()
{
}

xml_node Token::node(bool ins_or_del) const
{
    if (!fn) {
        return xml_node();
    }
    xml_node res = fn->node.parent();
    if (!ins_or_del) {
        if (strcmp(res.name(),"ins")==0 || strcmp(res.name(),"del")==0) {
            return res.parent();
        }
    }
    return res;
}

pugi::xml_node Token::node(const NodeSelector* select) const
{
    if (!fn) {
        return xml_node();
    }
    xml_node res = fn->node.parent();
    while (res && !(*select)(res)) {
        res = res.parent();
    }
    return res;
}


void Token::output(std::ostream& os) const
{
    os<<"(\""<<word<<"\", "<<start_offset<<", "<<end_offset<<")";
}

bool Token::empty() const
{
    return word.empty();
}

bool Token::match_with(const Token& other) const
{
    if (case_sensitive) {
        return word==other.word;
    }
    return to_lowercase(word)==to_lowercase(other.word);
}

TokenList::TokenList(Token* first, Token* last)
    : _first(first), _last(last)
{
}

void TokenList::add(Token* token)
{
    if (!_first) {
        _first = token;
    }
    if (_last) {
        (_last)->next = token;
        token->prev = _last;
    }
    _last = token;
}

void TokenList::addFront(Token* token)
{
    token->next = _first;
    if (_first) {
        _first->prev = token;
    }
    if (!_last) {
        _last = token;
    }
    _first = token;
}

void TokenList::append(TokenList& other)
{/*
    if (_last && _last->hyphen && other._first) {
        _last->hyphen = false;
        _last->word+=other._first->word;
        _last->end_offset = other._first->end_offset;
        TokenList other2 = other;
        other2.remove(other2._first, other2._first);
        append(other2);
        return;
    }*/
    
    if (!_first) {
        _first = other._first;
    }
    if (_last) {
        _last->next = other._first;
    }
    if (other._first) {
        other._first->prev = _last;
    }
    if (other._last) {
        _last = other._last;
    }
}

void TokenList::insertAfter(Token* after, TokenList& other)
{
    if (other.empty()) {
        return;
    }
    if (_last.GetPtr()==after) {
        append(other);
        return;
    }
    // If token is null, append to beginning of list
    if (!after) {
        other._last->next = _first;
        _first->prev = other._last;
        _first = other._first;
        return;
    }
    other._last->next = after->next;
    after->next->prev = other._last;
    after->next = other._first;
    other._first->prev = after;
}

Token* TokenList::insertAfter(Token* after, Token* added_token)
{
    if (_last.GetPtr()==after) {
        add(added_token);
        return added_token;
    }
    // If token is null, append to beginning of list
    if (!after) {
        addFront(added_token);
        return added_token;
    }
    added_token->next = after->next;
    after->next->prev = added_token;
    after->next = added_token;
    added_token->prev = after;
    return added_token;
}


TokenList TokenList::cutAt(Token* token)
{
    if (!token->next) {
        return TokenList();
    }
    TokenList res(token->next, _last);
    if (token->next) {
        token->next->prev = NULL;
    }
    token->next = NULL;
    _last= token;
    return res;
}

void TokenList::remove(Token* from_token, Token* to_token)
{
    if (!from_token || ! to_token) {
        return;
    }
    TokenP prev = from_token->prev;
    TokenP after = to_token->next;
    if (prev) {
        prev->next = after;
    } else {
        _first = after;
    }
    if (after) {
        after->prev = prev;
    } else {
        _last = prev;
    }
}

bool TokenList::widenLeft()
{
    if (prev() && (!_first || _first->node()==_first->prev->node())) {
    //if (_first && _first->prev && _first->node()==_first->prev->node()) {
        _first = prev();
        //cerr<<"Widen Left: "<<*_first<<endl;
        return true;
    }
    return false;
}

bool TokenList::widenRight()
{
    if (_last && _last->next && _last->node()==_last->next->node()) {
        _last = _last->next;
        //cerr<<"Widen Right: "<<*_last<<endl;
        return true;
    }
    return false;
}

bool TokenList::shiftRight()
{
    if (_first && _last && _last->next) {
        _first = _first->next;
        _last = _last->next;
        return true;
    }
    return false;
}

int TokenList::size() const
{
    if (empty()) {
        return 0;
    }
    return _last->end_offset - _first->start_offset;
}

int TokenList::tokensNum() const
{
    if (empty()) {
        return 0;
    }
    int count = 0;
    Token* cur = _first;
    while (cur) {
        count++;
        cur = cur->next;
    }
    return count;
}

TokenList TokenList::prevTokenList() const
{
    return TokenList(prev(), prev());
}

TokenList TokenList::nextTokenList() const
{
    return TokenList(next(), next());
}


std::ostream& operator<<(std::ostream& os, TokenList& tl)
{
    Token* token = tl.first();
    if (!token && tl.prev()) {
        os<<"Prev="<<*tl.prev();
        return os;
    }
    while (token) {
        os<<*token<<" ";
        if (token==tl.last()) {
            break;
        }
        token = token->next;
    }
    return os;
}

Flattened_node::Flattened_node(xml_node node_, size_t length_, Flattened* flat_, Flattened_node* parent_):
    node(node_), length(length_), flat(flat_), parent(parent_)
{
}

std::ostream& operator<<(std::ostream& os, const Token& t)
{
    t.output(os);
    return os;
}

Flattened_node::~Flattened_node()
{
    //cerr<<"Destructor for Flattened_node for node: "<<node.internal_object()<<endl;
    if (!tokens.empty() && !flat->tokens.empty()) {
        TokenP last_token = tokens.first();
        while (last_token->next && last_token->next->fn==this) {
            last_token = last_token->next;
        }
        flat->tokens.remove(tokens.first(), last_token);
    }
}

size_t Flattened_node::tree_index(size_t pos)
{
    if (!parent) {
        return pos;
    }
    size_t index = pos;
    for (Flattened_nodeP& child: parent->children) {
        if (child.GetPtr()==this) {
            break;
        }
        index+=child->length;
    }
    return parent->tree_index(index);
}

void Flattened_node::changeLength(size_t delta)
{
    length+=delta;
    if (parent) {
        parent->changeLength(delta);
    }
}

Token* Flattened_node::getToken(size_t pos, bool is_start_pos)
{
    //cerr<<"getToken for pos: "<<pos<<endl;
    Token* token = tokens.first();
    Token* last_token = token;
    while (token && token->fn==this) {
        //cerr<<"Check token: "<<*token<<endl;
        if ((is_start_pos && token->start_offset>=pos)
            || (!is_start_pos && token->end_offset>=pos)) {
            return token;
        }
        last_token = token;
        token = token->next;
    }
    return last_token;
}

Flattened::Flattened(xml_node node, DocStructure* doc_conf_, const Strings& skipped_, NodeSelector* exclude_)
    : doc_conf(doc_conf_), skipped(skipped_), exclude(exclude_)
{
    _tree = create(node, 0, tokens, true);
    _starts = 0;
    _ends = _text.size();
    //cerr<<"Created flattened with "<<_toFlattened.size()<<" nodes"<<endl;
    //cerr<<"Created tree. Node length = "<<_tree->length<<". Text length = "<<_text.length()<<endl;
}

Flattened::~Flattened()
{
    tokens.clear();
    //dtor
}

Flattened_node* Flattened::get_tree()
{
    return _tree;
}

xml_node Flattened::get_top_node() const
{
    return _tree->node;
}

const string& Flattened::get_text() const
{
    return _text;
}

string Flattened::get_text(const TokenList& tokens) const
{
    if (tokens.empty()) {
        return {};
    }
    size_t spos = tokens.first()->start_offset;
    size_t epos = tokens.last()->end_offset;
    return _text.substr(spos, epos-spos);
}

string Flattened::get_text(const pugi::xml_node& node) const
{
    const Flattened_node* fn = _getFlattenedNode(node);
    return  get_text(fn->tokens);
}


Position Flattened::getPos(size_t index) const
{
    size_t i = index;
    Flattened_node* fn = _tree;
    while (!fn->children.empty()) {
        for (Flattened_nodeP& child: fn->children) {
            if (i<=child->length && strcmp(child->node.name(),"del")!=0) {
                fn = child;
                break;
            }
            i -= child->length;
        }
    }
    if (fn->length<i) {
        return {};
    }
    
    //cerr<<"P4. Index="<<index<<" offset="<<i<<" length="<<fn->length<<endl;
    return {fn->node, i};
}

Token* Flattened::get_token(size_t index)
{
    Position pos = getPos(index);
    Flattened_node* fn = _getFlattenedNode(pos.node);
    Token* res = fn->getToken(index, false);
    return res;
}

const int INS_FLAG = 1<<1;
const int DEL_FLAG = 1<<2;
const int LINE_FLAG = 1<<3;
const int PAGE_FLAG = 1<<4;

Flattened_node* Flattened::create(xml_node node, Flattened_node* parent, TokenList& the_tokens, bool update_text, int flags)
{
    if (true/*flags&DEL_FLAG*/) {
        if (flags&PAGE_FLAG) {
            string s = getTextXML(node,true);
            _page = stoi(s);
            //cerr<<"Page = "<<_page<<endl;
            return _makeFlattenedNode(node, 0, parent);
        }
        if (flags&LINE_FLAG) {
            string s = getTextXML(node, true);
            //cerr<<"Line = "<<s<<endl;
            size_t pos;
            if ((pos=s.find(':'))!=string::npos) {
                string page_part = s.substr(0,pos);
                string line_part = s.substr(pos+1);
                if (!page_part.empty()) {
                    _page = stoi(s.substr(0, pos));
                }
                if (!line_part.empty()) {
                    _line = stoi(s.substr(pos+1));
                }
            } else {
                _line = stoi(s);
            }
            return _makeFlattenedNode(node, 0, parent);
        }
    }
    
    if (node.type()==node_pcdata || (node.first_child().empty() && node.first_child()==node.last_child())) {
        string node_val = node.value();
        //cerr<<"\""<<node_val<<"\" ";
        string pname;
        if (parent) {
            pname = parent->node.name();
        }
        /*
        if (pname == "header" || pname=="heading") {
            node_val+="\n";
        } else if (pname == "num" || pname=="enum") {
            node_val+=" ";
        }*/
        Flattened_node* res = _makeFlattenedNode(node, node_val.size(), parent);
        bool case_sensitive = true;
        if (doc_conf && parent) {
            // Case insensitive if either parent or grandparent is case insensitive node (e.g. headers)
            case_sensitive = (doc_conf->matchXML(parent->node)).case_sensitive_search;
            if (case_sensitive && parent->parent) {
                case_sensitive = (doc_conf->matchXML(parent->parent->node)).case_sensitive_search;
            }
        }
        //cerr<<"Tokens for \""<<node_val<<"\" with parent: "<<parent->node.name()<<(case_sensitive?" case sensitive":" not case sensitive")<<endl;
        TokenList new_tokens = _addTokens(node_val, res, case_sensitive, flags);
        if (update_text) {
            _text+=node_val;
        }        
        res->tokens = new_tokens;
        the_tokens.append(new_tokens);
        return res;
    }
    Token* first_token = 0;
    Token* last_token = 0;
    Flattened_node* new_fn = _makeFlattenedNode(node, 0, parent);
    string node_name = node.name();
    if (node_name=="ins") {
        flags|= INS_FLAG;
    } else if (node_name=="del") {
        flags |= DEL_FLAG;
    } else if (node_name=="line") {
        flags |= LINE_FLAG;
    } else if (node_name=="page") {
        flags |= PAGE_FLAG;
    }
    if (skipped.find(node_name)!=skipped.end() || (doc_conf && doc_conf->matchXML(node).skip_text)) {
        new_fn->tokens = TokenList(0,the_tokens.last());
        //cerr<<"Flattened skipping node: "<<name<<endl;
        return new_fn;
    }
    
    list<Flattened_nodeP> children;
    size_t length = 0;
    for (xml_node& child: node.children()) {
        if (exclude && (*exclude)(child)) {
            continue;
        }
        // Ignore deleted nodes
        string name = child.name();
        if (name=="hyphen") {
            the_tokens.last()->hyphen = true;
        }
        Token* prev_last = the_tokens.last();
        Flattened_node* fn = create(child, new_fn, the_tokens, update_text, flags);
        length+=fn->length;
        children.push_back(fn);
        if (name == "term" || name == "quote") {
            if (!_relaxed_search) {
                Token* new_first_token = the_tokens.insertAfter(prev_last, new Token("\"", 0, 0, fn));
                the_tokens.add(new Token("\"", 0, 0, fn));
                fn->tokens = TokenList(new_first_token, the_tokens.last());
            }
        }
        if (!first_token) {
            first_token = fn->tokens.first();
        }
        last_token = the_tokens.last();
        
    }
    new_fn->length = length;
    new_fn->children = children;
    new_fn->tokens = TokenList(first_token, last_token);
    return new_fn;
}

Flattened_node* Flattened::_makeFlattenedNode(pugi::xml_node node, size_t length, Flattened_node* parent)
{
    //cerr<<"Make flattened node: "<<node.name()<<endl;
    Flattened_node* res = new Flattened_node(node, length, this, parent);
    _toFlattened[node] = res;
    return res;
}

Token* Flattened::_newToken(const std::string& word, size_t starts, size_t ends, Flattened_node* fn, bool case_sensitive, int flags)
{
    // Assumes that _text is updated after calling _newToken
    Token* new_token = new Token(normalize_string(word, false), _text.size()+starts, _text.size()+ends, fn);
    new_token->case_sensitive = case_sensitive;
    new_token->page = _page;
    new_token->line = _line;
    new_token->inserted = flags&INS_FLAG;
    new_token->deleted = flags&DEL_FLAG;
    //cerr<<"Added token: "<<*new_token<<endl;
    return new_token; 
}


TokenList Flattened::_addTokens(const std::string& s, Flattened_node* fn, bool case_sensitive, int flags)
{
    TokenList res;
    size_t starts = 0;
    string word;
    bool prev_digit = true;
    for (size_t i = 0; i<s.size(); i++) {
        char c = s[i];
        if (size_t ln = is_wspace(s, i)) {
            prev_digit = true;
            if (!word.empty()) {
                res.add(_newToken(word, starts, i, fn, case_sensitive, flags));
                word.clear();
            }
            starts = i+ln;
            i+=(ln-1);
        } else if (isalnum(c)) {
            if (_relaxed_search) {
                word+=tolower(c);
            } else {
                word+=c;
            }
            prev_digit=isdigit(c);
        } else if (!_relaxed_search) { // Not alnum and not whitespace
            // Note: if we have a punctation characted followed by a digit, eg .5 or ,9, continue current word
            // if next character is not a digit, store current word and start a new one
            if ((c=='.'||c==','||c=='-' || c=='/') && i+1<s.size() && isdigit(s[i+1])&&prev_digit) {
                word+=c;
            } else {
                if (!word.empty()) {
                    res.add(_newToken(word, starts, i, fn, case_sensitive, flags));
                    
                    word.clear();
                    starts = i;
                }
            // Handle utf8
                if ((c & 0xc0) == 0xc0) {
                    while (i<s.size() && (s[i]&0x80)!=0) {
                        word+=s[i++];
                    }
                    i--;
                } else {
                    word += c;
                }
                res.add(_newToken(word, starts, i+1, fn, case_sensitive, flags));
                word.clear();
                starts = i+1;
            }
        } else { // _relaxed_search
            if (!word.empty()) {
                res.add(_newToken(word, starts, i, fn, case_sensitive, flags));
                word.clear();
            }
            starts = i+1;
        }
    }
    if (!word.empty()) {
        res.add(_newToken(word, starts, s.size(), fn, case_sensitive, flags));
    }
    return res;
}

Flattened_node* Flattened::get_fn(const pugi::xml_node& node, NodeSelector* selector)
{
    Flattened_node* fn = _getFlattenedNode(node);
    while (fn && selector && !(*selector)(fn->node)) {
        fn = fn->parent;
    }
    return fn;
}

Flattened_node* Flattened::_getFlattenedNode(xml_node node)
{
    //cerr<<"_toflattened size="<<_toFlattened.size()<<endl;
    auto it = _toFlattened.find(node);
    if (it==_toFlattened.end()) {
        string msg = "Operations on nodes of type "+string(node.name())+" not supported yet";
        return NULL;
        //throw AMPLException(ErrorKind::Runtime, ErrorType::Unimplemented, Severity::Error, UserMessageType::LikelySoftwareIssue, msg);
    }
    return it->second;    
}

const Flattened_node* Flattened::_getFlattenedNode(xml_node node) const
{
    auto it = _toFlattened.find(node);
    if (it==_toFlattened.end()) {
        return NULL;
    }
    return it->second;    
}


size_t Flattened::find_word(const string& s, size_t p, size_t range_start, size_t range_end)
{
    /*
    if (!isalnum(s.front()) && !isalnum(s.back())) {
        return _text.find(s,p);
    }*/

    size_t pos = p;
    while (pos!=string::npos) {
        pos = _text.find(s,pos);
        size_t ends = pos+s.size();
        if (pos==string::npos) {
            break;
        }
        // Word spacing rules:
        // Before word: need space (or in general !alnum), or the beginning of the string, unless word starts with !alnum
        // After word: need space (or in general !alnum), or the end of the string. Even if it ends with !alnum
        if ((pos==range_start || !isalnum(_text[pos-1]) || !isalnum(s[0])) &&
            (ends>=range_end-1 || !isalnum(_text[ends]) /*|| !isalnum(s.back())*/)) {
            //if (ends<_text.size()-1) { cerr<<"P2: '"<<_text[ends]<<"'"<<endl;}
            return pos;
        }
        pos++;
    }
    return string::npos;
}

/*
Ranges* Flattened::search(const string& s, Ranges* ranges)
{
    RangeP r = new Range(get_start(),get_end());
    return search_in(s, r, ranges);
}
*/
/*
Ranges* Flattened::search_in(const string& s, Range* searched, Ranges* ranges)
{
    cerr<<"Search "<<s<<" in: "<<*searched<<endl;
    
    Ranges* res = (ranges)?ranges:new Ranges;
    if (s.empty()) {
        return res;
    }
    Token* token = _getToken(searched->getStart());
    if (!token) {
        throw AMPLException(ErrorKind::Runtime, ErrorType::RangeNotFound, Severity::Error, UserMessageType::LikelySoftwareIssue, "Start token not found for range");
    }
    Token* last = _getToken(searched->getEnd(), false);
    if (!last) {
        throw AMPLException(ErrorKind::Runtime, ErrorType::RangeNotFound, Severity::Error, UserMessageType::LikelySoftwareIssue, "Last token not found for range");
    }
    TokenList search_tokens = _addTokens(s, NULL);
    if (search_tokens.empty()) {
        return res;
    }
    //cerr<<"Searched tokens: "<<search_tokens<<endl;
    Token* curr = search_tokens.first();
    Token* first_matched = NULL;
    do {
       if (token->empty()) {
          token = token->next;
          continue;
       }
       cerr<<token->word<<" ";
       if (token->match_with(*curr)) {
           cerr<<" (match with: "<<curr->word<<") ";
           if (!first_matched) {
               first_matched = token;
           }
           curr = curr->next;
           if (!curr) {
               PositionP sp = first_matched->getStartPos();
               PositionP ep = token->getEndPos();
               RangeP r = new Range(sp, ep);
               cerr<<"Found string at: "; r->output(cerr); cerr<<endl;
               res->add(r);
               first_matched = NULL;
               curr = search_tokens.first();
           }
       } else {
           cerr<<"\""<<token->word<<"\"!=\""<<curr->word<<"\""<<endl;
           curr = search_tokens.first();
           first_matched = NULL;
       }
       if (token==last) {
          break;
       }
       token = token->next;
    } while (token!=NULL);
    cerr<<endl;

   cerr<<"Found "<<res->size()<<" strings."<<endl;

    return res;
}
*/
/*
int Flattened::search_word_in(const std::string& s, Values::Range* searched)
{
    Token* token = _getToken(searched->getStart());
    if (!token) {
        throw AMPLException(ErrorKind::Runtime, ErrorType::RangeNotFound, Severity::Error, UserMessageType::LikelySoftwareIssue, "Start token not found for range");
    }
    Token* last = _getToken(searched->getEnd(), false);
    if (!last) {
        throw AMPLException(ErrorKind::Runtime, ErrorType::RangeNotFound, Severity::Error, UserMessageType::LikelySoftwareIssue, "Last token not found for range");
    }
    int pos = 1;
    while (token!=NULL && token->prev!=last) {
        const string& word = token->word;
        if (!isalnum(word[0])) {
            token = token->next;
            continue;
        }
        if (word==s) {
            return pos;
        }
        token = token->next;
        pos++;
    }
    return 0;
}
*/

xml_node Flattened::_find_main_text_child(xml_node& n, bool from_start)
{
    xml_node child;
    if (from_start) {
        child = n.first_child();
    } else {
        child = n.last_child();
    }
    while (!child.empty()) {
        if (strcmp(child.name(),"del")!=0 && (doc_conf->matchXML(child)).is_main_content) {
            return _find_main_text_child(child, from_start);
        }
        if (from_start) {
            child = child.next_sibling();
        } else {
            child = child.previous_sibling();
        }
        
    }
    return n;    
}


bool Flattened::contains_node(xml_node& node)
{
    auto it = _toFlattened.find(node);
    return it!=_toFlattened.end();
}

const TokenList& Flattened::get_tokens(const pugi::xml_node& node) const
{
    static TokenList empty_tokens;
    const Flattened_node* fn = _getFlattenedNode(node);
    if (!fn) {
        return empty_tokens;
    }
    return fn->tokens;
}


bool Flattened::isAlnumBefore(const TokenList& tokens)
{
    /*
    if (!tokens.prev()) {
        return false;
    }
    size_t pos = tokens.prev()->end_offset;
    if (pos<0) {
        return false;
    }
    return isalnum(_text[pos]);
    */
    
    if (!tokens.first()) {
        return true;
    }
    size_t pos = tokens.first()->start_offset;
    if (pos<1) {
        return false;
    }
    return isalnum(_text[pos-1]);
    
}

bool Flattened::isAlnumAfter(const TokenList& tokens)
{
    if (!tokens.last()) {
        return false;
    }
    size_t pos = tokens.last()->end_offset;
    if (pos>=_text.size()) {
        return false;
    }
    return isalnum(_text[pos]);
}

void insert_node_in_text(Flattened_nodeP& fn, size_t pos, const string& str, const string& tag)
{
    if (str.empty()) {
        return;
    }
    xml_node node = fn->node;
    string node_str = node.value();
    //cerr<<"Pos="<<pos<<" node len="<<node_str.size()<<endl;
    string left_part = node_str.substr(0,pos);
    string right_part = node_str.substr(pos);
    node.set_value(left_part.c_str());
    xml_node insert_after = node;

    // This one makes sure that we insert after all deleted nodes
    if (right_part.empty()) {
        while (insert_after.next_sibling() && strcmp(insert_after.next_sibling().name(), "del")==0) {
            insert_after = insert_after.next_sibling();
        }
    }
    xml_node parent = node.parent();
    xml_node ins_node = parent.insert_child_after(tag.c_str(), insert_after);
    ins_node.append_child(node_pcdata).set_value(str.c_str());
    xml_node new_node = parent.insert_child_after(node_pcdata, ins_node);
    new_node.set_value(right_part.c_str());
    
    fn->length = left_part.length();
    Flattened_node* parent_fn = fn->parent;
    if (!parent_fn) {
        parent_fn = fn;
    }
    auto it = std::find(parent_fn->children.begin(), parent_fn->children.end(), fn);
    ++it;
    TokenP first_token = fn->getToken(pos, true);
    //cerr<<"first_token is "; if (first_token) cerr<<*first_token<<endl; else cerr<<"NULL\n";
    TokenP before_token = first_token?first_token->prev:fn->flat->tokens.last();
    //cerr<<"before_token is "; if (before_token) cerr<<*before_token<<endl; else cerr<<"NULL\n";
    TokenP last_token = first_token;
    /*
    // Delete tokens till the end of the node ...
    while (last_token && last_token->next && last_token->next->fn == fn) {
        last_token = last_token->next;
    }*/
    TokenList new_tokens;
    if (tag!="ins") {
        Flattened_nodeP inserted_fn = fn->flat->create(ins_node, parent_fn, new_tokens, false);
        parent_fn->children.insert(it, inserted_fn);
    }
    Flattened_nodeP new_fn = fn->flat->create(new_node, parent_fn, new_tokens, false);
    parent_fn->children.insert(it, new_fn);
}

void Flattened::insert(size_t index, const string& str, bool add_spaces)
{
    cerr<<_text<<endl;
    cerr<<"Inserting \""<<str<<"\" in:"<<index<<endl;
    string s = str;
    Position p = getPos(index);
    cerr<<"Pos="<<p.offset<<endl;
    xml_node node = p.node;
    Flattened_nodeP the_tree = _getFlattenedNode(node);
    if (add_spaces) {
        if (index>0 && isalnum(_text[index-1])) {
            s = " "+s;
        }
        if (index<_text.length() && isalnum(_text[index])) {
            s += " ";
        }
    }
    //_text.insert(index, s);
    size_t pos = p.offset;
    insert_node_in_text(the_tree, pos, s, "ins");
}

Flattened_node* strike_flattened(Flattened_node* fn, int sindex, int eindex, bool delete_node)
{
    size_t fnlen = fn->length;
    xml_node node = fn->node;
    
    int sp = (sindex>=0)?sindex:0;
    int ep = (eindex<=fnlen)?eindex:fnlen;
    size_t length = ep-sp;
    if (sp!=sindex || ep!=eindex) {
        delete_node = true;
    }

    if (delete_node && fn->length==0) {
        // Range is wholly contained in child. Delete it.

        xml_node new_node = node.parent().insert_child_after("del", node);            
        new_node.append_move(node);
        Flattened_node* parent = fn->parent;
        return fn;
    }

    if (node.type()==node_pcdata) {
        string s = node.value();
        string text = s.substr(sp, ep-sp);
        if (sp<s.size()) {
            // Note: we may add some characters to the flattened text after the node's text (e.g. spaces)
            if (ep>s.length()) {
                s.erase(s.begin()+sp, s.end());
            } else {
                s.erase(s.begin()+sp, s.begin()+ep);
            }
            int shift = ep-sp;
            node.set_value(s.c_str());
            Flattened_nodeP fnp = fn;
            insert_node_in_text(fnp, sp, text, "del");
        }
        return fn;
    }

    Flattened_node* first = NULL;
    for (auto it = fn->children.begin(); it != fn->children.end(); /*advance it later*/) {
        Flattened_nodeP child = *it;
        auto next= std::next(it);
        if (ep<=0) {
            break;
        }
        int length = child->length;
        if (sp<length && ep>0) {
            // Child contains part of range. Recursively call strike_flattened
            Flattened_node* ret = strike_flattened(child, sp, ep, delete_node);
            if (!first) {
                first = ret;
            }
        }
        sp-=length;
        ep-=length;
        it = next;
    }
    return first;
}

void Flattened::strike(size_t sindex, size_t eindex)
{
    cerr<<"Striking from:"<<sindex<<" to: "<<eindex<<endl;
    /*
    if (sindex>0 && eindex<_tree->length && _text[sindex-1]==' ' && _text[eindex]==' ') {
        sindex--;
    }*/
    //cerr<<"Striking the text: \""<<_text.substr(sindex, eindex-sindex)<<"\"\n";
    Flattened_node* ret_fn = strike_flattened(_tree, sindex, eindex, false);
}
