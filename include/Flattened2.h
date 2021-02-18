#ifndef FLATTENED_H
#define FLATTENED_H

#include "smartptr.h"
#include "pugixml.hpp"
#include <vector>
#include <map>
#include <list>
#include <set>
#include "DocStructure.h"
#include <cstring>


// Forward declarations

class Token;
class Flattened_node;
class Flattened;
class Context;

typedef SmartPtr<Flattened_node> Flattened_nodeP;
typedef SmartPtr<Token> TokenP;
typedef std::set<std::string> Strings;

class NodeSelector {
public:
    virtual bool operator() (const pugi::xml_node& node) const = 0;
};

class NodeHaveId: public NodeSelector {
public:
    bool operator() (const pugi::xml_node& node) const {
        return !node.attribute("id").empty()||!node.attribute("idref").empty();
    }
};

class NodeIsMatched: public NodeSelector {
public:
    bool operator() (const pugi::xml_node& node) const {
        return !node.attribute("xcdiff:from").empty()||!node.attribute("xcdiff:to").empty() || node.name()==diff_insert || node.name()==diff_delete;
    }
const string diff_insert = "diff-insert";
const string diff_delete = "diff-delete";
    
};

class HaveIdOrHeading: public NodeSelector {
public:
    bool operator() (const pugi::xml_node& node) const {
        
        string name = node.name();
        if (name=="line" || name=="page") {
            return false;
        }
        return !node.attribute("id").empty()||!node.attribute("idref").empty()
            || name=="header" || name=="heading";
            
        //return node.type()==pugi::xml_node_type::node_element;
    }
};

class NodeNotInline: public NodeSelector {
public:
    bool operator() (const pugi::xml_node& node) const {
        return node.type()==pugi::node_element && 
        (node.previous_sibling().empty() || node.previous_sibling().type() == pugi::node_element) && 
        (node.next_sibling().empty() || node.next_sibling().type()==pugi::node_element) && 
        strcmp(node.name(),"line")!=0 && strcmp(node.name(),"page")!=0;
    }
    
};

class NodeIsName: public NodeSelector {
public:
    NodeIsName(const string& str): name(str) {}
    bool operator() (const pugi::xml_node& node) const {
        return node.name()==name;
    }
    const string name;
    
};


typedef NodeNotInline DefaultSelector;

class Token: public IRefCount
{
public:
    Token()
        : start_offset(0), end_offset(0), fn(NULL) {}
    Token(const std::string& val, size_t starts, size_t ends, Flattened_node* fn_ = NULL)
        : word(val), start_offset(starts), end_offset(ends), fn(fn_) {}
    virtual ~Token();
    bool empty() const;
    bool match_with(const Token& other) const;
    pugi::xml_node node(bool ins_or_del = false) const;
    pugi::xml_node node(const NodeSelector* select) const;    
    
    void output(std::ostream& os) const;
    std::string word;
    size_t start_offset;
    size_t end_offset;
    int page = 0;
    int line = 0;
    bool hyphen = false;
    bool deleted = false;
    bool inserted = false;
    Flattened_node* fn = NULL;
    TokenP next;
    Token* prev = NULL;
    bool case_sensitive = true;
};

std::ostream& operator<<(std::ostream& os, const Token& t);

class TokenList
{
public:
    TokenList() {}
    TokenList(Token* first, Token* last);
    void add(Token* t);
    // Perform "in-place" append. The list in "other" becomes connected to the end of this list
    void append(TokenList& other);
    void insertAfter(Token* after, TokenList& other);
    Token* insertAfter(Token* after, Token* added_token);    
    void addFront(Token* token);
    bool widenLeft();
    bool widenRight();
    bool shiftRight();
    
    // Cut the list after "token". Return the tail list
    TokenList cutAt(Token* token);
    void remove(Token* from_token, Token* to_token);
    Token* first() const {
        return _first;
    }
    Token* last() const {
        return _first?_last:NULL;
    }
    bool empty() const {
        return !_first;
    }
    Token* prev() const {
        return (_first)?_first->prev:&*_last;
    }
    Token* next() const {
        return (_last)?_last->next:NULL;
    }
    
    int size() const;
    int tokensNum() const;

    TokenList prevTokenList() const;
    TokenList nextTokenList() const;
        
    void clear() {
        _first = NULL;
        _last = NULL;
    }
    
private:
    TokenP _first;
    // Stores last token, or previous token when _first is null
    TokenP _last;
};

std::ostream& operator<<(std::ostream& os, TokenList& tl);

class Position
{
public:
    Position(): offset(0) {}
    Position(const pugi::xml_node& n, size_t pos)
        : node(n), offset(pos) {}
    
    pugi::xml_node node;
    size_t offset;
};

class Flattened_node: public IRefCount
{
public:
    Flattened_node(pugi::xml_node node, size_t length_, Flattened* flat, Flattened_node* parent_ = 0);
    ~Flattened_node();
    size_t length;

    size_t tree_index(size_t pos);
    void changeLength(size_t delta);
    Token* getToken(size_t pos, bool is_start_pos);
    
    pugi::xml_node node;
    std::list<Flattened_nodeP> children;
    TokenList tokens;
    //TokenP first_token;
    //Not a smart-pointer to avoid circular reference
    Flattened_node* parent = NULL;
    Flattened* flat = NULL;
};

// Class representing XML as flattened text
class Flattened: public IRefCount
{
public:
    Flattened(pugi::xml_node node, DocStructure* doc_conf, const Strings& skipped, NodeSelector* exclude = NULL);
    virtual ~Flattened();

    Flattened_node* create(pugi::xml_node node, Flattened_node* parent, TokenList& tokens, bool update_text, int flags = 0);

    size_t find_word(const std::string& s, size_t p, size_t range_start, size_t range_end);

    // Get position based on string index
    // Note: sometimes, there are several possible positions (in different nodes) corresponding to the same index.
    // If in_fn is specified, returns a position in the specified node.
    Flattened_node* get_tree();
    Flattened_node* get_fn(const pugi::xml_node& node, NodeSelector* selector = NULL);
    const std::string& get_text() const;
    std::string get_text(const TokenList& tokens) const;
    std::string get_text(const pugi::xml_node& node) const;
    pugi::xml_node get_top_node() const;
    Position getPos(size_t index) const;

//    Values::Ranges* search(const std::string& s, Values::Ranges* ranges = 0);
//    Values::Ranges* search_in(const std::string& s, Values::Range* searched, Values::Ranges* ranges = 0);
    // Search word (alphanumeric token) within range. Returns word location or 0 if not found.
//    int search_word_in(const std::string& s, Values::Range* searched);
    // If doc_conf is provided, use it to find start and end of main text (ignoring parts that are not "main text")
    // Node is contained in flattened, ignoring start and end points
    bool contains_node(pugi::xml_node& node);
    const TokenList& get_tokens(const pugi::xml_node& node) const;
    Token* get_token(size_t index);
    bool isAlnumBefore(const TokenList& tokens);
    bool isAlnumAfter(const TokenList& tokens);
    
    void insert(size_t index, const string& str, bool add_spaces = false);
    void strike(size_t sindex, size_t eindex);  

    TokenList tokens;

private:
    Flattened_node* _makeFlattenedNode(pugi::xml_node node, size_t length, Flattened_node* parent = 0);
    Flattened_node* _getFlattenedNode(pugi::xml_node node);
    const Flattened_node* _getFlattenedNode(pugi::xml_node node) const;
    Token* _newToken(const std::string& word, size_t starts, size_t ends, Flattened_node* fn = NULL, bool case_sensitive = true, int flags = 0);
    TokenList _addTokens(const std::string& s, Flattened_node* fn = NULL, bool case_sensitive = true, int flags = 0);
    pugi::xml_node _find_main_text_child(pugi::xml_node& n, bool from_start);

    NodeSelector* exclude = NULL;
    Strings skipped;
    std::string _text;
    Flattened_nodeP _tree;
    size_t _starts;
    size_t _ends;
    bool _relaxed_search = false;
    
    int _page;
    int _line;
    std::map<pugi::xml_node, Flattened_nodeP> _toFlattened;
    DocStructure* doc_conf;
};

typedef SmartPtr<Flattened> FlattenedP;

#endif // FLATTENED_H
