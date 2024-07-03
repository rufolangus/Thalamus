#pragma once

#include <set>
#include <map>
#include <util.h>
#include <string>
#include <vector>
#include <memory>
#include <variant>
#include <optional>
#include <functional>

#include <boost/json.hpp>
#include <boost/signals2.hpp>

#ifdef __clang__
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wdeprecated-builtins"
    #include <absl/strings/str_split.h>
  #pragma clang diagnostic pop
#else
  #include <absl/strings/str_split.h>
#endif

#include <absl/strings/str_format.h>
#include <absl/strings/str_join.h>
#include <absl/strings/numbers.h>

namespace thalamus {
  using namespace std::placeholders;
  class ObservableCollection;
  class ObservableDict;
  class ObservableList;
  using ObservableDictPtr = std::shared_ptr<ObservableDict>;
  using ObservableListPtr = std::shared_ptr<ObservableList>;
  using StateDict = ObservableDict; 
  using StateList = ObservableList;
  using StateColl = ObservableCollection;

  class ObservableCollection {
  public:

    using Key = std::variant<std::monostate, long long int, bool, std::string>;
    using Value = std::variant<std::monostate, long long int, double, bool, std::string, std::shared_ptr<ObservableDict>, std::shared_ptr<ObservableList>>;
    using Map = thalamus::map<Key, Value>;
    using Vector = thalamus::vector<Value>;
    enum class Action {
      Set,
      Delete
    };
    using Observer = std::function<void(Action, const Key&, const Value&)>;
    ObservableCollection* parent;
    std::function<bool(Action, const std::string&, ObservableCollection::Value, std::function<void()>)> remote_storage;
  public:
    using Changed = boost::signals2::signal<void(Action, const Key&, const Value&)>;
    Changed changed;
    class ValueWrapper {
      Key key;
      std::function<Value&()> get_value;
      Changed* changed;
      ObservableCollection* collection;
    public:
      ValueWrapper(const Key& key, std::function<Value& ()> get_value, Changed* changed, ObservableCollection* collection);

      void assign(const Value& new_value, std::function<void()> callback = nullptr, bool from_remote = false);

      operator ObservableDictPtr();
      operator ObservableListPtr();
      operator long long int();
      operator unsigned long long int();
      operator unsigned long();
      operator double();
      operator bool();
      operator std::string();
      operator Value();
      Value get();
      bool operator==(const Value& other);
    };

    class VectorIteratorWrapper {
      size_t key;
      Vector::iterator iterator;
      Changed* changed;
      ObservableCollection* collection;
      friend ObservableList;
      friend ObservableDict;
    public:
      VectorIteratorWrapper();
      VectorIteratorWrapper(size_t key, Vector::iterator iterator, Changed* changed, ObservableCollection* collection);
      ValueWrapper operator*();
      VectorIteratorWrapper& operator+(size_t count);
      VectorIteratorWrapper& operator+=(size_t count);
      VectorIteratorWrapper& operator++();
      VectorIteratorWrapper operator++(int);
      VectorIteratorWrapper& operator-(size_t count);
      VectorIteratorWrapper& operator-=(size_t count);
      VectorIteratorWrapper& operator--();
      VectorIteratorWrapper operator--(int);
      bool operator!=(const VectorIteratorWrapper& other) const;
    };

    class MapIteratorWrapper {
    protected:
      Map::iterator iterator;
      Changed* changed;
      ObservableCollection* collection;
      std::optional<std::pair<Key, ValueWrapper>> pair;
      friend ObservableList;
      friend ObservableDict;
    public:
      MapIteratorWrapper();
      MapIteratorWrapper(Map::iterator iterator, Changed* changed, ObservableCollection* collection);
      ValueWrapper operator*();
      std::pair<Key, ValueWrapper>* operator->();
      MapIteratorWrapper& operator++();
      MapIteratorWrapper operator++(int);
      MapIteratorWrapper& operator--();
      MapIteratorWrapper operator--(int);
      bool operator!=(const MapIteratorWrapper& other) const;
    };
    ObservableCollection(ObservableCollection* parent = nullptr);
    virtual ~ObservableCollection() {}

    static ObservableCollection::Value from_json(const boost::json::value&);
    static boost::json::value to_json(const ObservableCollection::Value&);
    static std::string to_string(const ObservableCollection::Value&);
    static std::string to_string(const ObservableCollection::Key&);
    virtual std::optional<ObservableCollection::Key> key_of(const ObservableCollection& v) const = 0;
    virtual void set_remote_storage(std::function<bool(Action, const std::string&, ObservableCollection::Value, std::function<void()>)>) = 0;

    std::string address() const;
  };

  class ObservableList : public ObservableCollection {
    Vector content;
  public:
    ObservableList(ObservableCollection* parent = nullptr);
    ValueWrapper operator[](size_t i);
    const Value& operator[](size_t i) const;
    ValueWrapper at(size_t i);
    const Value& at(size_t i) const;
    VectorIteratorWrapper begin();
    Vector::const_iterator begin() const;
    VectorIteratorWrapper end();
    Vector::const_iterator end() const;
    VectorIteratorWrapper erase(VectorIteratorWrapper i);
    VectorIteratorWrapper erase(Vector::const_iterator i, std::function<void(VectorIteratorWrapper)> callback = nullptr, bool from_remote = false);
    VectorIteratorWrapper erase(size_t i, std::function<void(VectorIteratorWrapper)> callback = nullptr, bool from_remote = false);
    void push_back(const Value& value, std::function<void()> callback = nullptr, bool from_remote = false);
    void pop_back(std::function<void()> callback = nullptr, bool from_remote = false);
    void clear();
    void recap();
    void recap(Observer target);
    size_t size() const;
    bool empty() const;
    ObservableList& operator=(const ObservableList& that) = delete;
    ObservableList& assign(const ObservableList& that, bool from_remote = false);
    ObservableList& operator=(const boost::json::array& that);
    ObservableList(const boost::json::array& that);
    operator boost::json::array() const;
    std::optional<ObservableCollection::Key> key_of(const ObservableCollection& v) const override;
    void set_remote_storage(std::function<bool(Action, const std::string&, ObservableCollection::Value, std::function<void()>)> remote_storage) override;
  };

  class ObservableDict : public ObservableCollection {
    Map content;
  public:
    ObservableDict(ObservableCollection* parent = nullptr);
    ValueWrapper operator[](const Key& i);
    ValueWrapper at(const Key& i);
    const Value& at(const Key& i) const;
    bool contains(const Key& i) const;
    MapIteratorWrapper begin();
    Map::const_iterator begin() const;
    MapIteratorWrapper end();
    Map::const_iterator end() const;
    MapIteratorWrapper erase(MapIteratorWrapper i);
    MapIteratorWrapper erase(Map::const_iterator i, std::function<void(MapIteratorWrapper)> callback = nullptr, bool from_remote = false);
    MapIteratorWrapper erase(const ObservableCollection::Key& i, std::function<void(MapIteratorWrapper)> callback = nullptr, bool from_remote = false);
    MapIteratorWrapper find(const Key& i);
    Map::const_iterator find(const Key& i) const;
    void clear();
    void recap();
    void recap(Observer target);
    size_t size() const;
    bool empty() const;
    ObservableDict& operator=(const ObservableDict&) = delete;
    ObservableDict& assign(const ObservableDict& that, bool from_remote = false);
    ObservableDict& operator=(const boost::json::object& that);
    ObservableDict(const boost::json::object& that);
    operator boost::json::object() const;
    std::optional<ObservableCollection::Key> key_of(const ObservableCollection& v) const override;
    void set_remote_storage(std::function<bool(Action, const std::string&, ObservableCollection::Value, std::function<void()>)> remote_storage) override;
    ObservableCollection::Value get_jsonpath(ObservableCollection::Value store, const std::list<std::string>& tokens);
    ObservableCollection::Value get_jsonpath(ObservableCollection::Value store, const std::string& query);
  };
  void set_jsonpath(ObservableCollection::Value store, const std::string& query, ObservableCollection::Value value, bool from_remote = false);
  void delete_jsonpath(ObservableCollection::Value store, const std::string& query, bool from_remote = false);
}