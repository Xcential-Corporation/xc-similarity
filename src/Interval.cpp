#include "Interval.h"

using namespace std;

void PositionMap::add(const Interval& from, const Interval& to)
{
    if (from.size()!=to.size()) {
        throw DiffException("Mapping from interval of size "+to_string(from.size())+" to interval of size "+to_string(to.size()));
    }
    
    auto it = _map.lower_bound(from);
    size_t new_first = from.first();
    size_t new_last = from.last();
    size_t new_to = to.first();
    if (it!=_map.begin()) {
        auto prev = std::prev(it);
        if (prev->first.overlaps(from)) {
            new_to = new_to+prev->first.first()-from.first();
            new_first = prev->first.first();
            _map.erase(prev);
        }
    }
    while (it!=_map.end() && it->first.overlaps(from)) {
        if (it->first.last()>new_last) {
            new_last = it->first.last();
        }
        new_last = it->first.last();
        auto prev = it++;
        _map.erase(prev);
    }
    _map[{new_first, new_last}] = new_to;
}

Intervals PositionMap::projection(const Interval& from) const
{
    auto it = _map.lower_bound(from);
    Intervals res;
    if (it!=_map.begin() && from.overlaps(prev(it)->first)) {
        it--;
    }
    while (it!=_map.end() && it->first.overlaps(from)) {
        Interval x = it->first.intersect(from);
        size_t to = it->second + x.first()- it->first.first();
        res.push_back({to, to+x.size()-1});
        it++;
    }
    return res;
}

void PositionMap::output(std::ostream& os) const
{
    for (auto& m: _map) {
        const Interval& i = m.first;
        os<<"["<<i.first()<<","<<i.last()<<"] -> ["<<m.second<<","<<m.second+i.size()-1<<"]\n";
    }
}

void PositionMap::output(std::ostream& os, const string& text1, const string& text2)
{
    for (auto& m: _map) {
        const Interval& i = m.first;
        os<<"["<<i.first()<<","<<i.last()<<"] -> ["<<m.second<<","<<m.second+i.size()-1<<"]\n";
        os<<"\""<<text1.substr(i.first(), i.size())<<"\" -> \""<<text2.substr(m.second, i.size())<<"\"\n";
    }    
}
