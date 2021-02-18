#include "utils.h"
#include <algorithm>
#include <map>
#include <vector>
#include <set>
#include <ctime>
#include <iostream>

using namespace std;

static const vector<string> openQuotes = {
        "\"",
        "``",
        "‘‘",
        //"‘",
        "“"
};
static const vector<string> closeQuotes = {
        "\"",
        "\'\'",
        "’’",
        //"’",
        "”"
};

int isOpenQuotes(const string& s, size_t pos)
{
    for (string q: openQuotes) {
        if (s.substr(pos, q.size())==q) {
            return q.size();
        }
    }
    return 0;
}

int isCloseQuotes(const string& s, size_t pos)
{
    for (string q: closeQuotes) {
        if (s.substr(pos, q.size())==q) {
            return q.size();
        }
    }
    return 0;
}

bool is_quoted(const string& s)
{
    return s=="quoted-block" || s=="quotedContent";
}

string stripQuotes(string s)
{
    for (string q: openQuotes) {
        if (s.size()>= q.size() && s.substr(0,q.size())==q) {
            s = s.substr(q.size());
            break;
        }
    }
    int len = s.size();
    for (string q: closeQuotes) {
        if (s.size()>=q.size() && s.substr(len-q.size())==q) {
            s = s.substr(0,len-q.size());
            break;
        }
    }
    return s;
}


// Deep stripping quotes for string originating from xml
string deepStripQuotes(const string& s)
{
    static string ldq = u8"\u201C";
    static string rdq = u8"\u201D";
    string res;
    int in_quote = 0;
    for (size_t i = 0; i<s.size(); i++) {
        if (s.compare(i, ldq.size(), ldq)==0) {
            in_quote++;
            if (in_quote==1) {
                i+= (ldq.size()-1);
                continue;
            }
        }
        if (s.compare(i, rdq.size(), rdq)==0) {
            in_quote--;
            if (in_quote==0) {
                i+= (rdq.size()-1);
                continue;
            }
        }
        char c = s[i];
        if (c=='\n' && in_quote==1) {
            in_quote==0;
        }
        res+=c;
    }
    return res;
}

int roman_digit(char c)
{
    if (c>='a' && c<='z') {
        c = c -'a' + 'A';
    }
    switch (c) {
        case 'I': return 1;
        case 'V': return 5;
        case 'X': return 10;
        case 'L': return 50;
        case 'C': return 100;
        case 'D': return 500;
        case 'M': return 1000;
        default: return -1;
    }
}

LetterCase getCase(const string& s)
{
    bool haveUpper = false;
    bool haveLower = false;
    for (char c: s) {
        if (islower(c)) {
            haveLower = true;
        } else if (isupper(c)) {
            haveUpper = true;
        }
    }
    if (haveUpper && haveLower) {
        return LetterCase::Mixed;
    } else if (haveUpper) {
        return LetterCase::Upper;
    } else if (haveLower) {
        return LetterCase::Lower;
    }
    return LetterCase::None;
}


int roman2int(const string& roman)
{
    int res = 0;
    int prev_digit = 0;
    int len = roman.length();
    for (int i=0; i<len; i++) {
        int digit = roman_digit(roman[i]);
        if (digit==-1) {
            return -1;
        }
        int next_digit;
        if (i!=len-1 && digit<(next_digit=roman_digit(roman[i+1]))) {
            if (digit*5!=next_digit && digit*10!=next_digit) {
                return -1;
            }
            res-=digit;
        } else {
            res +=digit;
        }
    }
    return res;
}


string int2roman(int n, bool uppercase)
{
    static const string ones[] = {"", "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX"};
    static const string tens[] = {"", "X", "XX", "XXX", "XL", "L", "LX", "LXX", "LXXX", "XC"};
    static const string hundreds[] = {"", "C", "CC", "CCC", "CD", "D", "DC", "DCC", "DCCC", "CM"};

    if (n<1 || n>999) {
        return string();
    }
    string res = ones[n%10];
    n/=10;
    res = tens[n%10]+res;
    n/=10;
    res = hundreds[n]+res;
    if (!uppercase) {
        transform(res.begin(), res.end(), res.begin(), ::tolower);
    }
    return res;
}

bool isRepeatedChar(const string& s)
{
    char repeated = s[0];
    if (!isalpha(repeated)) {
        return false;
    }
    for (char c: s) {
        if (c!=repeated) {
            return false;
        }
    }
    return true;
}

// Letter enumeration to int
int letter2int(const string& letter)
{
    if (!isRepeatedChar(letter)) {
        return -1;
    }
    int res;
    char ch = letter[0];
    if (ch>='A' && ch<='Z') {
        res = ch - 'A'+1;
    } else if (ch>='a' && ch<='z') {
        res = ch - 'a'+1;
    } else {
        return -1;
    }
    return res+(letter.length()-1)*26;
}

string int2letter(int n, bool uppercase)
{
    int repeat = (n-1)/26;
    int reminder = n-1-repeat*26;
    char c;
    if (uppercase) {
        c = 'A'+reminder;
    } else {
        c = 'a'+reminder;
    }
    string res(repeat+1,c);
    cerr<<"Numer: "<<n<<", Letter: "<<res<<endl;
    return res;
}

bool is_number(const string& s)
{
    if (s.empty()) {
        return false;
    }
    for (char c: s) {
        if (!isdigit(c)) {
            return false;
        }
    }
    return true;
}

string normalize_string(const string& s, bool is_identifier)
{
    // Add to map as needed
    static const map<string,string> chmap = {
        {u8"\u2013", "-"},
        {u8"\u2014", "-"},
        {u8"\u0060", "\""},        
        {u8"\u2018", "\""},
        {u8"\u2019", "\""},
        {"'","\""},
        {u8"\u201C", "\""},
        {u8"\u201D", "\""},
        {u8"\u00BD", "1/2"},
        {u8"\u2153", "1/3"},
        {u8"\u2154", "2/3"},
        {u8"\u00BC", "1/4"},
        {u8"\u00BE", "3/4"},
        {u8"\u2155", "1/5"},
        {u8"\u2155", "1/5"},
        {u8"\u2156", "2/5"},
        {u8"\u2157", "3/5"},
        {u8"\u2158", "4/5"},
        {u8"\u202F", " "},
        {"__", "_"}
    };
    static const string uslm = "/uslm";
    string res = s;

    if (is_identifier && res.substr(0,uslm.size())==uslm) {
        res = res.substr(uslm.size());
    }

    for (auto& it: chmap) {
        size_t pos=0;
        while ((pos=res.find(it.first, pos))!=string::npos) {
            res.replace(pos, it.first.size(), it.second);
        }
    }

    //if (s!=res)
    //    cerr<<"Before normalize:"<<s<<" after:"<<res<<endl;
    return res;
}

size_t is_wspace(const std::string& s, size_t i)
{
    char c = s[i];
    if (isspace(c)) {
        return 1;
    }
    if ((c&0xc0)==0xc0) {
        if (s.substr(i,3)=="\u202F") {
            return 3;
        }
    }
    return 0;
}

string single_to_double_quotes(const string& s)
{
    string res = s;
    size_t pos = 0;
    while ((pos=res.find("'", pos))!=string::npos) {
        res.replace(pos, 1, "\"");
    }
    return res;
}

string get_identifier_part(const string& name, const string& des)
{
    if (name=="title") {
        return "t"+des;
    } else if (name=="chapter") {
        return "ch"+des;
    } else if (name=="section") {
        return "s"+des;
    }

    return des;
}

bool isNumOrNumeral(const string& s)
{
    if (isRepeatedChar(s)) {
        return true;
    } else if (isdigit(s[0])) {
        return true;
    } else if (roman2int(s)>0) {
        return true;
    }
    return false;
}

size_t find_gen_amendment(const string& s)
{
    size_t amended_to_read = s.find("amended to read");
    if (amended_to_read!=string::npos) {
        return amended_to_read;
    }
    size_t to_read = s.find("to read");
    if (to_read!=string::npos) {
        size_t amending = s.find("amending");
        if (amending!=string::npos && amending<to_read) {
            return amending;
        }
    }
    return string::npos;
}

size_t find_is_amended(const std::string& s, bool get_start)
{
    string_range_t range = find_is_amended(s);
    if (get_start||range.start==string::npos) {
        return range.start;
    }
    return range.end;
}

string_range_t find_is_amended(const string& s)
{

    static vector<string> amended_expressions = {
        "is amended",
        "is further amended",
        "are amended",
        "are each amended",
        "is repealed",
        "is hereby repealed",
        "are repealed"
    };
    
    static vector<string> following_expressions = {
        "as follows:",
        "by"
    };

    size_t pos = string::npos;
    string_range_t res(string::npos, 0);
    size_t xml_start = s.find("__xml__");
    do {
        for (const string& amended_str: amended_expressions) {
            pos = s.find(amended_str, res.end);
            if (xml_start!=string::npos && pos>xml_start) {
                pos = string::npos;
            }
            if (pos!=string::npos) {
                res.start = pos;
                res.end = res.start+amended_str.size();
                break;
            }
        }
    } while (pos!=string::npos);
    
    if (res.end!=s.size()) {
        for (const string& following_str: following_expressions) {
            if (s.compare(res.end+1, following_str.size(), following_str)==0) {
                res.end+=(1+following_str.size());
                break;
            }
        }
    }
    
    return res;
}

string get_blanket_context_level(const string& s)
{
    static const string whenever_in_this = "whenever in this";
    size_t pos = s.find(whenever_in_this);
    if (pos==string::npos) {
        return string();
    }
    pos+= whenever_in_this.size()+1;
    size_t next_space = s.find(" ",pos);
    string res = s.substr(pos, next_space-pos);
    transform(res.begin(), res.end(), res.begin(), ::tolower);
    return res;
}

string get_blanket_context_predicate(const string& s)
{
    static const string other_provision_of = "section or other provision of";
    static const string the_reference_shall = "the reference shall";
    size_t spos = s.find(other_provision_of);
    if (spos==string::npos) {
        return string();
    }
    spos+=other_provision_of.size()+1;
    size_t epos = s.find(the_reference_shall,spos);
    if (epos==string::npos) {
        return string();
    }
    return s.substr(spos, epos-spos-1);
}

size_t find_blanket_context(const string& s)
{
    static const string the_reference_shall = "the reference shall be considered to be made to";
    static const string the_amendment_or_repeal = "the amendment or repeal shall be considered to be made to";   
	static const string are_amendments_to_such = "are amendments to such section or other provision of";
    static const string provision_of = "provision of";
	size_t pos = s.find(are_amendments_to_such);
	if (pos!=string::npos) {
		return pos+are_amendments_to_such.size();
	}
    pos = s.find(the_reference_shall);
    if (pos==string::npos) {
        pos = s.find(the_amendment_or_repeal);
    }
    if (pos==string::npos) {
        return pos;
    }
    size_t pos2 = s.find(provision_of, pos);
    if (pos2 != string::npos) {
        return pos2+provision_of.size();
    }
    return pos+the_reference_shall.size();
}

size_t find_action(const string& s)
{
    static const set<string> action_words = {
        "amending",
        "inserting",
        "insert",
        "striking",
        "strike",
        "adding",
        "redesignating",
        "redesignate",
        "designating",
        "moving",
        "move",
        "transferring",
        "deleting",
        "delete"};


    size_t by = 0;
    while ((by=s.find("by",by))!=string::npos) {
        size_t next_space = s.find(" ",by+3); // TODO: look for next word instead of space
        if (next_space==string::npos) {
            break;
        }
        string next_word = s.substr(by+3,next_space-by-3);
        if (action_words.find(next_word)!=action_words.end()) {
            return by;
        }
        by++;
    }
    return string::npos;
}

string normalize_white_spaces(const string& s, char sep)
{
    string res;
    bool whitespace = false;
    for (char c: s) {
        if (c==' ' || c=='\n' || c=='\t') {
            whitespace = true;
        } else {
            if (whitespace) {
                res+=sep;
                whitespace = false;
            }
            res+=c;
        }

    }
    if (whitespace) {
        res+=sep;
    }
    return res;
}

string clean_designation(const string& s)
{
    string res;
    bool seen_space = false;
    for (char c: s) {
        if (isalnum(c)) {
            if (seen_space) {
                res.clear();
                seen_space = false;
            }
            res+=c;
        } else if (c==' ' || c=='\n') {
            seen_space=true;
        }
    }
    return res;
}

string strip_nonalnum(const string& s)
{
    string res;
    bool last_alnum = true;
    for (char c: s) {
        if (isalnum(c)) {
            if (!last_alnum && !res.empty()) {
                res+=' ';
            }
            res+=c;
            last_alnum = true;
        } else {
            last_alnum = false;
        }
    }
    return res;
}

string get_leading_num(const string& s)
{
    string res;
    for (char c: s) {
        if (isdigit(c)) {
            res+=c;
        } else if (!res.empty()||!isspace(c)) {
            return res;
        }
    }
    return res;
}

string to_lowercase(const string& s)
{
    string res = s;
	int i;
	char c;
	for (i=0;i<s.length();i++) {
		c = s[i];
		if (c>='A' && c<='Z')
			res[i]=c+'a'-'A';
	}
	return res;
}

bool contains_alnum(const string& s)
{
    for (char c: s) {
        if (isalnum(c)) {
            return true;
        }
    }
    return false;
}

size_t find_nth_occurance(const string& s, char c, int n)
{
    size_t pos = -1;
    for (int i=0; i<n; i++) {
        pos = s.find(c, pos+1);
        if (pos==string::npos) {
            return pos;
        }
    }
    return pos;
}

int count_occurances(const string& s, const string& subs)
{
    int res = 0;
    size_t pos = 0;
    while ((pos = s.find(subs, pos )) != string::npos) {
        res ++;
        pos += subs.size();
   }
   return res;
}


string getMainIdPart(const string& id)
{
    int left_slashes = 4;
    static const string pl = "/us/pl";
    if (id.substr(0,pl.size())==pl) {
        left_slashes = 5;
    }
    size_t name_end = find_nth_occurance(id, '/', left_slashes);
    if (name_end==string::npos) {
        return id;
    }
    return id.substr(0, name_end);
}

string getCurrentDate()
{
    time_t now = time(0);
    tm *ltm = localtime(&now);
    string year = to_string(ltm->tm_year+1900);
    string month = to_string(ltm->tm_mon+1);
    string day = to_string(ltm->tm_mday);
    return month+"/"+day+"/"+year;
}

std::string filenameToBillnumber(const std::string& filename)
{
    size_t pos = filename.find_last_of("/\\");
    pos = (pos==string::npos)?0:pos+1;
    pos = filename.find("-", pos);
    size_t sp = (pos==string::npos)?0:pos+1;
    pos = filename.find(".", sp);
    size_t ep = pos;
    return filename.substr(sp, ep-sp);
}

std::string yyyy_mm_ddToStandard(const std::string& date)
{
    if (date.empty()) {
        return {};
    }
    string year = date.substr(0,4);
    string month = date.substr(5,2);
    string day = date.substr(8,2);
    return month+"/"+day+"/"+year;
}

string getExtension(const string& filename)
{
        string::size_type pos = filename.rfind('.');
        if (pos!=string::npos) {
            return to_lowercase(filename.substr(pos+1));
        }
        return string();
}

string convertVerboseDate(const string& date)
{
    struct tm tm;
    char buffer[30];
    char* res = strptime(date.c_str(), "%B %d, %Y", &tm);
    if (!res) {
        res = strptime(date.c_str(), "%B. %d, %Y", &tm);
    }
    if (!res) { // Allow for date with skipped day
        res= strptime(date.c_str(), "%B, %Y", &tm);
        tm.tm_mday = 1;
    }
    if (!res) {
        return {};
    }
    strftime(buffer, 12, "%m/%d/%Y", &tm);
    return buffer;
}

string timestampToDate(time_t time)
{
    struct tm * dt;
    char buffer [30];
    dt = gmtime(&time);
    strftime(buffer, sizeof(buffer), "%m/%d/%Y", dt);
    return std::string(buffer);    
}

string addDays(const string& date, int days)
{
    struct tm tm = {0};
    char* success = strptime(date.c_str(), "%m/%d/%Y", &tm);
    if (!success) {
        return {};
    }
    tm.tm_mday+=days;
    mktime(&tm);
    char buffer[30];
    strftime(buffer, 12, "%m/%d/%Y", &tm);
    return buffer;
}
