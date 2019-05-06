#ifndef LINKED_LIST_H_
#define LINKED_LIST_H_
#include <unordered_map>
#include <list>
template <class Key,class Value,class Hash=std::hash<Key>>
class LinkedHashMap{
public:
    typedef std::list<std::pair<Key,Value>> ListType;
    // the difference ListType::iterator and typename ListType::iterator
    //https://blog.csdn.net/zhliu1991/article/details/39317819
    typedef std::unordered_map<Key,typename ListType::iterator,Hash> MapType;
    typedef typename ListType::iterator iterator;
    typedef typename ListType::size_type size_type;
    typedef typename ListType::value_type value_type;
    LinkedHashMap()=default;
    LinkedHashMap(LinkedHashMap &&o)=default;
    LinkedHashMap &operator=(LinkedHashMap &&o)=default;
    iterator begin(){
        return list_.begin();
    }
    iterator end(){
        return list_.end();
    }
    Value &operator[](const Key &key){
        return ((this->insert(std::make_pair(key,Value()))).first)->second;
    }
    std::pair<iterator,bool> insert(const std::pair<Key,Value> &pair){
        typename MapType::iterator found=map_.find(pair.first);
        if(found!=map_.end()){
            return std::make_pair(found->second,false);
        }
        list_.push_back(pair);
        iterator last;
        last=list_.end();
        last--;
        map_.insert(std::make_pair(pair.first,last));
        return std::make_pair(last,true);
    }
    size_type size(){
        return list_.size();
    }
    void clear(){
        list_.clear();
        map_.clear();
    }
    value_type &front(){
        return list_.front();
    }
    value_type &back(){
        return list_.back();
    }
    bool empty(){
        return list_.empty();
    }
    void pop_front(){

    }
    size_type erase(const Key &key){
        typename MapType::iterator found=map_.find(key);
        if(found==map_.end()){
            return 0;
        }
        list_.erase(found->second);
        map_.erase(found);
        return 1;
    }
    iterator erase(iterator pos){
        typename MapType::iterator found=map_.find(pos->first);
        map_.erase(found);
        return list_.erase(pos);
    }
private:
    ListType list_;
    MapType map_;
};
#endif
