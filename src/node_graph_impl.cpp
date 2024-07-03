#include <node_graph_impl.h>
#include <grpcpp/create_channel.h>
#include <run_node.h>
#include <ophanim_node.h>
#include <image_node.h>
#include <task_controller_node.h>
#include <oculomatic_node.h>
#include <distortion_node.h>
#include <genicam_node.h>
#include <channel_picker_node.h>
#include <algebra_node.h>
#include <normalize_node.h>
#include <thread_pool.h>
#include <remote_node.h>
#include <lua_node.h>

namespace thalamus {
  using namespace std::chrono_literals;

  struct INodeFactory {
    virtual Node* create(ObservableDictPtr state, boost::asio::io_context& io_context, NodeGraph* graph) = 0;
    virtual bool prepare() = 0;
    virtual std::string type_name() = 0;
  };

  template <typename T>
  struct NodeFactory : public INodeFactory {
    T* create(ObservableDictPtr state, boost::asio::io_context& io_context, NodeGraph* graph) override {
      return new T(state, io_context, graph);
    }
    bool prepare() override {
      constexpr bool has_prepare = requires { T::prepare(); };
      if constexpr (has_prepare) {
        return T::prepare();
      }
      else {
        return true;
      }
    }
    std::string type_name() override {
      return T::type_name();
    }
  };

  static std::map<std::string, INodeFactory*> node_factories = {
    {"NONE", new NodeFactory<NoneNode>()},
    {"NIDAQ", new NodeFactory<NidaqNode>()},
    {"NIDAQ_OUT", new NodeFactory<NidaqOutputNode>()},
    {"ALPHA_OMEGA", new NodeFactory<AlphaOmegaNode>()},
    {"TOGGLE", new NodeFactory<ToggleNode>()},
    {"XSENS", new NodeFactory<XsensNode>()},
    {"HAND_ENGINE", new NodeFactory<HandEngineNode>()},
    {"WAVE", new NodeFactory<WaveGeneratorNode>()},
    {"STORAGE", new NodeFactory<StorageNode>()},
    {"STARTER", new NodeFactory<StarterNode>()},
    {"RUNNER", new NodeFactory<RunNode>()},
    {"OPHANIM", new NodeFactory<OphanimNode>()},
    {"TASK_CONTROLLER", new NodeFactory<TaskControllerNode>()},
    {"ANALOG", new NodeFactory<AnalogNodeImpl>()},
    {"FFMPEG", new NodeFactory<FfmpegNode>()},
    {"OCULOMATIC", new NodeFactory<OculomaticNode>()},
    {"DISTORTION", new NodeFactory<DistortionNode>()},
    {"GENICAM", new NodeFactory<GenicamNode>()},
    {"THREAD_POOL", new NodeFactory<ThreadPoolNode>()},
    {"CHANNEL_PICKER", new NodeFactory<ChannelPickerNode>()},
    {"NORMALIZE", new NodeFactory<NormalizeNode>()},
    {"ALGEBRA", new NodeFactory<AlgebraNode>()},
    {"LUA", new NodeFactory<LuaNode>()},
    {"REMOTE_NODE", new NodeFactory<RemoteNode>()}
  };

  struct NodeGraphImpl::Impl {
    ObservableListPtr nodes;
    std::vector<std::shared_ptr<Node>> node_impls;
    std::vector<std::string> node_types;
    size_t num_nodes;
    boost::asio::io_context& io_context;
    std::optional<Service*> service;
    std::vector<std::pair<thalamus_grpc::NodeSelector, std::function<void(std::weak_ptr<Node>)>>> callbacks;
    std::vector<std::pair<thalamus_grpc::NodeSelector, boost::signals2::signal<void(std::weak_ptr<Node>)>>> signals;
    NodeGraphImpl* outer;
    thalamus::map<std::string, std::weak_ptr<grpc::Channel>> channels;
    std::chrono::system_clock::time_point system_time;
    std::chrono::steady_clock::time_point steady_time;
    ThreadPool thread_pool;
  public:
    Impl(ObservableListPtr nodes, boost::asio::io_context& io_context, NodeGraphImpl* outer, std::chrono::system_clock::time_point system_time, std::chrono::steady_clock::time_point steady_time)
      : nodes(nodes)
      , num_nodes(nodes->size())
      , io_context(io_context)
      , outer(outer)
      , system_time(system_time)
      , steady_time(steady_time)
      , thread_pool("ThreadPool") {
      using namespace std::placeholders;
      auto i = node_factories.begin();
      while (i != node_factories.end()) {
        if (!i->second->prepare()) {
          i = node_factories.erase(i);
        }
        else {
          ++i;
        }
      }
      nodes->changed.connect(std::bind(&Impl::on_nodes, this, _1, _2, _3));
    }

    void clean_signals() {
      for(auto i = signals.begin();i != signals.end();) {
        if(i->second.empty()) {
          i = signals.erase(i);
        } else {
          ++i;
        }
      }
    }

    void on_nodes(ObservableCollection::Action a, const ObservableCollection::Key& k, const ObservableCollection::Value& v) {
      using namespace std::placeholders;
      if (a == ObservableCollection::Action::Set) {
        size_t index = std::get<long long>(k);
        ObservableDictPtr node = std::get<ObservableDictPtr>(v);
        node->changed.connect(std::bind(&Impl::on_node, this, node.get(), _1, _2, _3));

        std::string type_str = node->at("type");
        auto factory = node_factories.at(type_str);
        auto node_impl = std::shared_ptr<Node>(factory->create(node, io_context, outer));
        node_impls.insert(node_impls.begin() + index, node_impl);
        node_types.insert(node_types.begin() + index, type_str);
        node->recap();
      }
    }

    void notify(std::function<bool(const thalamus_grpc::NodeSelector&)> selector, std::weak_ptr<Node> node_impl) {
      for(auto i = callbacks.begin();i != callbacks.end();) {
        if(selector(i->first)) {
          i->second(std::weak_ptr<Node>(node_impl));
          i = callbacks.erase(i);
        } else {
          ++i;
        }
      }
      for(auto i = signals.begin();i != signals.end();) {
        if(selector(i->first)) {
          i->second(std::weak_ptr<Node>(node_impl));
          i = signals.erase(i);
        } else if (i->second.empty()) {
          i = signals.erase(i);
        } else {
          ++i;
        }
      }
    }

    void on_node(ObservableDict* node, ObservableCollection::Action a, const ObservableCollection::Key& k, const ObservableCollection::Value& v) {
      if (a == ObservableCollection::Action::Set) {
        auto node_index = 0;
        ObservableDictPtr shared_node;
        for (auto i = 0u; i < nodes->size(); ++i) {
          shared_node = nodes->at(i);
          if (node == shared_node.get()) {
            node_index = i;
            break;
          }
        }

        auto key_str = std::get<std::string>(k);
        if (key_str == "type") {
          auto value_str = std::get<std::string>(v);
          if (value_str != node_types.at(node_index)) {
            auto factory = node_factories.at(value_str);

            node_impls.at(node_index).reset(factory->create(shared_node, io_context, outer));
            node_types.at(node_index) = value_str;
          }

          auto node_impl = node_impls.at(node_index);
          notify([&value_str] (auto& selector) { return selector.type() == value_str; }, node_impl);
        } else if (key_str == "name") {
          auto node_impl = node_impls.at(node_index);
          auto value_str = std::get<std::string>(v);
          notify([&value_str] (auto& selector) { return selector.name() == value_str; }, node_impl);
        }
      }
    }
  };

  NodeGraphImpl::NodeGraphImpl(ObservableListPtr nodes, boost::asio::io_context& io_context, std::chrono::system_clock::time_point system_time, std::chrono::steady_clock::time_point steady_time) : impl(new Impl(nodes, io_context, this, system_time, steady_time)) {
    impl->nodes->recap();
    impl->thread_pool.start();
  }

  NodeGraphImpl::~NodeGraphImpl() {}

  std::optional<std::string> NodeGraphImpl::get_type_name(const std::string& type) {
    auto i = node_factories.find(type);
    if (i != node_factories.end()) {
      return i->second->type_name();
    }
    else {
      return std::nullopt;
    }
  }

  void NodeGraphImpl::set_service(Service* service) {
    impl->service = service;
  }

  Service& NodeGraphImpl::get_service() {
    return **impl->service;
  }

  std::weak_ptr<Node> NodeGraphImpl::get_node(const std::string& query_name) {
    thalamus_grpc::NodeSelector selector;
    selector.set_name(query_name);
    return get_node(selector);
  }

  std::weak_ptr<Node> NodeGraphImpl::get_node(const thalamus_grpc::NodeSelector& query_name) {
    std::string key;
    std::string query;
    if(!query_name.name().empty()) {
      key = "name";
      query = query_name.name();
    } else {
      key = "type";
      query = query_name.type();
    }
    for (auto i = 0u; i < impl->nodes->size(); ++i) {
      ObservableDictPtr node = impl->nodes->at(i);
      std::string value = node->at(key);
      if (query == value) {
        return std::weak_ptr<Node>(impl->node_impls.at(i));
      }
    }
    return std::weak_ptr<Node>();
  }

  void NodeGraphImpl::get_node(const std::string& query_name, std::function<void(std::weak_ptr<Node>)> callback) {
    thalamus_grpc::NodeSelector selector;
    selector.set_name(query_name);
    return get_node(selector, callback);
  }

  void NodeGraphImpl::get_node(const thalamus_grpc::NodeSelector& query_name, std::function<void(std::weak_ptr<Node>)> callback) {
    auto value = get_node(query_name);
    if (!value.lock()) {
      impl->callbacks.emplace_back(query_name, callback);
    }
    else {
      callback(value);
    }
  }


  NodeGraph::NodeConnection NodeGraphImpl::get_node_scoped(const thalamus_grpc::NodeSelector& selector, std::function<void(std::weak_ptr<Node>)> callback) {
    auto value = get_node(selector);
    if (!value.lock()) {
      impl->signals.emplace_back(selector, boost::signals2::signal<void(std::weak_ptr<Node>)>());
      auto connection = new boost::signals2::scoped_connection(impl->signals.back().second.connect(callback));
      return NodeConnection(connection, [this] (boost::signals2::scoped_connection* c) {
        delete c;
        impl->notify([] (auto&) { return false; },
                     std::weak_ptr<Node>());
      });
    }
    else {
      callback(value);
      return std::make_shared<NodeConnection::element_type>();
    }
  }

  std::shared_ptr<grpc::Channel> NodeGraphImpl::get_channel(const std::string& url) {
    if (!impl->channels.contains(url) || !impl->channels[url].lock()) {
      auto channel = grpc::CreateChannel(url, grpc::InsecureChannelCredentials());
      impl->channels[url] = channel;
      return channel;
    }
    return impl->channels[url].lock();
  }

  std::chrono::system_clock::time_point NodeGraphImpl::get_system_clock_at_start() {
    return impl->system_time;
  }

  std::chrono::steady_clock::time_point NodeGraphImpl::get_steady_clock_at_start() {
    return impl->steady_time;
  }

  ThreadPool& NodeGraphImpl::get_thread_pool() {
    return impl->thread_pool;
  }
}