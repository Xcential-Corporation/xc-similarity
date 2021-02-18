#ifndef INTERVAL_H
#define INTERVAL_H

#include <vector>
#include <set>
#include <map>
#include <algorithm>
#include <iostream>

typedef std::string string;

class DiffException
{
public:
    DiffException(const string& msg): _msg(msg) {}
    const string& what() const {
        return _msg;
    }
private:
    string _msg;
};

class Interval {
public:
    Interval(size_t x, size_t y): _first(x), _last(y) {}
    bool operator< (const Interval& o) const {
        return _first<o._first;
    }
    size_t first() const {
        return _first;
    }
    size_t last() const {
        return _last;
    }
    size_t size() const {
        return (_last-_first)+1;
    }
    bool contains(size_t x) const {
        return x>=_first && x<=_last;
    }
    bool overlaps(const Interval& o) const {
        return _first<=o._last && o._first<=_last;
    }
    Interval intersect(const Interval& o) const {
        return {std::max(_first, o._first), std::min(_last, o._last)};
    }
    
private:
    size_t _first;
    size_t _last;
};

typedef std::vector<Interval> Intervals;

class PositionMap {
public:
    typedef std::map<Interval,size_t> PMap;
    
    void add(const Interval& from, const Interval& to);
    Intervals projection(const Interval& from) const;
    void output(std::ostream&) const;
    void output(std::ostream& os, const string& text1, const string& text2);
    
private:
    PMap _map;
};


#endif //INTERVAL_H
