#ifndef UTILS_H_INCLUDED
#define UTILS_H_INCLUDED
#include <string>

int roman2int(const std::string& roman);
std::string int2roman(int n, bool uppercase);
int letter2int(const std::string& letter);
std::string int2letter(int n, bool uppercase);
bool is_number(const std::string& s);

enum class LetterCase {
    Upper,
    Lower,
    Mixed,
    None
};

struct string_range_t {
    string_range_t() {}
    string_range_t(size_t s, size_t e):
        start(s), end(e) {}
    size_t start = std::string::npos;
    size_t end = std::string::npos;
};

LetterCase getCase(const std::string& s);

int isOpenQuotes(const std::string& s, size_t pos);
int isCloseQuotes(const std::string& s, size_t pos);
bool is_quoted(const std::string& s);
std::string stripQuotes(std::string s);
std::string deepStripQuotes(const std::string& s);

std::string normalize_string(const std::string& s, bool is_identifier = true);

std::string get_identifier_part(const std::string& name, const std::string& des);

// Note: returns true in case its a number, a number and letters combinations (e.g. 10A), a roman numeral, or a letter numeral
bool isNumOrNumeral(const std::string& s);

size_t find_gen_amendment(const std::string& s);

string_range_t find_is_amended(const std::string& s);
size_t find_is_amended(const std::string& s, bool get_start);

size_t find_action(const std::string& s);

size_t find_blanket_context(const std::string& s);

std::string get_blanket_context_level(const std::string& s);

std::string get_blanket_context_predicate(const std::string& s);

std::string single_to_double_quotes(const std::string& s);

std::string normalize_white_spaces(const std::string& s, char sep = ' ');

size_t is_wspace(const std::string& s, size_t i);

std::string clean_designation(const std::string& s);

std::string strip_nonalnum(const std::string& s);

std::string get_leading_num(const std::string& s);

std::string to_lowercase(const std::string& s);

bool contains_alnum(const std::string& s);

size_t find_nth_occurance(const std::string& s, char c, int n);

int count_occurances(const std::string& s, const std::string& subs);

std::string getMainIdPart(const std::string& id);

std::string getCurrentDate();

std::string filenameToBillnumber(const std::string& filename);

std::string yyyy_mm_ddToStandard(const std::string& date);

std::string getExtension(const std::string& filename);

std::string convertVerboseDate(const std::string& date);

std::string timestampToDate(time_t time);

std::string addDays(const std::string& date, int days);

#endif // UTILS_H_INCLUDED
