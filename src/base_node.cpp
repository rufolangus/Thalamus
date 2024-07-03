#include <base_node.h>
#include <chrono>
#include <cmath>
#include <map>
#include <util.h>
#include <tracing/tracing.h>
#include <random>
//#include <plot.h>

namespace thalamus {

  std::string AnalogNode::EMPTY = "";

  struct StarterNode::Impl {
    ObservableDictPtr state;
    ObservableList* nodes;
    boost::signals2::scoped_connection state_connection;
    boost::signals2::scoped_connection nodes_state_connection;
    NodeGraph* graph;
    std::vector<std::weak_ptr<ObservableDict>> targets;
    std::weak_ptr<Node> trigger;
    boost::signals2::scoped_connection trigger_connection;
    double threshold;
    bool _on;
    double last_sample;
    size_t channel = 0;

    Impl(ObservableDictPtr state, NodeGraph* graph)
      : state(state)
      , graph(graph)
      , _on(false)
      , last_sample(0) {
      nodes = static_cast<ObservableList*>(state->parent);
      using namespace std::placeholders;
      state_connection = state->changed.connect(std::bind(&Impl::on_change, this, _1, _2, _3));
      state->recap(std::bind(&Impl::on_change, this, _1, _2, _3));
      nodes_state_connection = nodes->changed.connect(std::bind(&Impl::on_nodes_change, this, _1, _2, _3));
    }

    void on_nodes_change(ObservableCollection::Action, const ObservableCollection::Key&, const ObservableCollection::Value&) {
      if (state->contains("Targets")) {
        std::string value_str = state->at("Targets");
        auto tokens = absl::StrSplit(value_str, ',');
        targets = get_nodes(nodes, tokens);
      }
    }

    void on_data(Node* raw_node, AnalogNode* node) {
      TRACE_EVENT0("thalamus", "StarterNode::Impl::on_data");
      const auto& buffer = node->data(channel);

      std::for_each(buffer.begin(), buffer.end(), [&](double d) {
        if (last_sample < threshold && d >= threshold) {
          _on = !_on;
          for (auto target : targets) {
            auto locked = target.lock();
            (*locked)["Running"].assign(_on, [&] {});
          }
        }
        last_sample = d;
        });
    }

    void on_change(ObservableCollection::Action, const ObservableCollection::Key& k, const ObservableCollection::Value& v) {
      auto key_str = std::get<std::string>(k);
      if (key_str == "Source") {
        trigger_connection.disconnect();

        auto value_str = std::get<std::string>(v);
        graph->get_node(value_str, [&](auto weak_node) {
          auto node = weak_node.lock();
          auto analog_node = dynamic_cast<AnalogNode*>(node.get());
          if (node && analog_node) {
            trigger_connection = node->ready.connect(std::bind(&Impl::on_data, this, _1, analog_node));
            trigger = weak_node;
          }
          });
      }
      else if (key_str == "Targets") {
        auto value_str = std::get<std::string>(v);
        auto tokens = absl::StrSplit(value_str, ',');
        targets = get_nodes(nodes, tokens);
      }
      else if (key_str == "Threshold") {
        threshold = std::get<double>(v);
      }
      else if (key_str == "Channel") {
        channel = state->at("Channel");
      }
    }
  };

  StarterNode::StarterNode(ObservableDictPtr state, boost::asio::io_context&, NodeGraph* graph)
      : impl(new Impl(state, graph)) {}

  StarterNode::~StarterNode() {}

  std::string StarterNode::type_name() {
    return "STARTER";
  }

  struct WaveGeneratorNode::Impl {
    ObservableDictPtr state;
    ObservableList* nodes;
    boost::signals2::scoped_connection state_connection;
    NodeGraph* graph;
    std::weak_ptr<Node> source;
    boost::asio::io_context& io_context;
    boost::asio::high_resolution_timer timer;
    std::vector<double> buffer;
    std::vector<std::string> recommended_names;
    size_t _num_channels;
    size_t buffer_size;
    size_t source_observer_id;
    std::map<size_t, std::function<void(Node*)>> observers;
    size_t counter = 0;
    double current = 0;
    std::random_device random_device;
    std::mt19937 random_range;
    std::uniform_int_distribution<std::mt19937::result_type> random_distribution;
    double frequency;
    double amplitude;
    double phase;
    size_t poll_interval;
    bool is_running;
    std::chrono::nanoseconds _sample_interval;
    std::chrono::nanoseconds _time;
    std::chrono::steady_clock::time_point last_time;
    std::chrono::nanoseconds last_switch;
    std::chrono::steady_clock::time_point _start_time;
    AnalogNodeImpl analog_impl;
    WaveGeneratorNode* outer;
    std::string shape;
    double offset;
    double duty_cycle;
    std::function<double(std::chrono::nanoseconds)> wave;
    Impl(ObservableDictPtr state, boost::asio::io_context& io_context, NodeGraph* graph, WaveGeneratorNode* outer)
      : state(state)
      , nodes(static_cast<ObservableList*>(state->parent))
      , graph(graph)
      , io_context(io_context)
      , timer(io_context)
      , outer(outer)
      , random_range(random_device())
      , random_distribution(0, 1)
      , recommended_names(1, "0") {
      analog_impl.inject({ std::span<double const>() }, { 0ns }, {""});
      state_connection = state->changed.connect(std::bind(&Impl::on_change, this, _1, _2, _3));


      analog_impl.ready.connect([outer](Node*) {
        outer->ready(outer);
      });

      this->state->recap(std::bind(&Impl::on_change, this, _1, _2, _3));
    }

    void on_timer(const boost::system::error_code& error) {
      TRACE_EVENT0("thalamus", "WaveGeneratorNode::Impl::on_timer");
      if (error.value() == boost::asio::error::operation_aborted) {
        return;
      }
      BOOST_ASSERT(!error);
      if (!_sample_interval.count()) {
        timer.expires_after(std::chrono::milliseconds(poll_interval));
        timer.async_wait(std::bind(&Impl::on_timer, this, _1));
        return;
      }
      auto now = std::chrono::steady_clock::now();
      auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - _start_time);
      //auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - last_time).count();
      //std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time).count() << " ";
      last_time = now;
      buffer.clear();

      auto new_time = _time;
      while (new_time <= elapsed) {
        buffer.push_back(wave(new_time));
        new_time += _sample_interval;
      }
      analog_impl.inject({ {buffer.begin(), buffer.end()} }, { _sample_interval }, {""}, now.time_since_epoch());
      _time = new_time;
      //auto after = std::chrono::steady_clock::now();
      //std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(after - now).count() << std::endl;
      if (!is_running) {
        return;
      }
      timer.expires_after(std::chrono::milliseconds(poll_interval));
      timer.async_wait(std::bind(&Impl::on_timer, this, _1));
    }

    void on_change(ObservableCollection::Action, const ObservableCollection::Key& k, const ObservableCollection::Value& v) {
      auto key_str = std::get<std::string>(k);
      if (key_str == "Frequency") {
        frequency = std::get<double>(v);
      }
      else if (key_str == "Amplitude") {
        amplitude = std::get<double>(v);
      }
      else if (key_str == "Shape") {
        shape = std::get<std::string>(v);
        if (shape == "Sine") {
          wave = [&](std::chrono::nanoseconds time) {
            return amplitude * std::sin(2 * M_PI * (frequency * time.count() / 1e9 + phase)) + offset;
          };
        }
        else if (shape == "Square") {
          wave = [&](std::chrono::nanoseconds time) {
            size_t interval = 1e9 / frequency;
            auto modulo = (time.count() - size_t(1e9*phase)) % interval;
            auto duty_interval = interval * duty_cycle;
            return (modulo < duty_interval ? amplitude : -amplitude) + offset;
          };
        }
        else if (shape == "Triangle") {
          wave = [&](std::chrono::nanoseconds time) {
            size_t interval = 1e9 / frequency;
            size_t quarter_interval = interval / 4;
            size_t three_quarter_interval = 3 * quarter_interval;
            auto modulo = (time.count() - size_t(1e9 * phase)) % interval;
            if (modulo < quarter_interval) {
              return double(modulo) / quarter_interval * amplitude + offset;
            } else if (modulo < three_quarter_interval) {
              return (1 - (double(modulo) - quarter_interval) / quarter_interval) * amplitude + offset;
            } else {
              return (double(modulo) - three_quarter_interval) / quarter_interval * amplitude - amplitude + offset;
            }
          };
        }
        else if (shape == "Random") {
          last_switch = std::chrono::steady_clock::now().time_since_epoch();
          wave = [&](std::chrono::nanoseconds time) {
            size_t interval = 1e9 / frequency;
            if((time - last_switch).count() > interval) {
              current = amplitude*random_distribution(random_range);
              last_switch = time;
            }
            return current;
          };
        }
        else {
          wave = [&](std::chrono::nanoseconds time) {
            return amplitude * std::sin(2 * M_PI * (frequency * time.count() / 1e9 + phase)) + offset;
          };
        }
      }
      else if (key_str == "Offset") {
        offset = std::get<double>(v);
      }
      else if (key_str == "Duty Cycle") {
        duty_cycle = std::get<double>(v);
      }
      else if (key_str == "Phase") {
        phase = std::get<double>(v);
      }
      else if (key_str == "Poll Interval") {
        poll_interval = std::get<long long int>(v);
      }
      else if (key_str == "Sample Rate") {
        auto sample_rate = std::get<double>(v);
        _sample_interval = std::chrono::nanoseconds(static_cast<std::chrono::nanoseconds::rep>(1e9 / sample_rate));
        outer->channels_changed(outer);
      }
      else if (key_str == "Running") {
        is_running = std::get<bool>(v);
        if (is_running) {
          last_time = std::chrono::steady_clock::now();
          _start_time = last_time;
          _time = 0ns;
          on_timer(boost::system::error_code());
        }
      }
    }
  };

  WaveGeneratorNode::WaveGeneratorNode(ObservableDictPtr state, boost::asio::io_context& io_context, NodeGraph* graph)
    : impl(new Impl(state, io_context, graph, this)) {}

  WaveGeneratorNode::~WaveGeneratorNode() {
    (*impl->state)["Running"].assign(false, [] {});
  }

  std::string WaveGeneratorNode::type_name() {
    return "WAVE";
  }

  std::string_view WaveGeneratorNode::name(int channel) const {
    return impl->recommended_names.at(channel);
  }
  std::span<const std::string> WaveGeneratorNode::get_recommended_channels() const {
    return std::span<const std::string>(impl->recommended_names.begin(), impl->recommended_names.end());
  }

  std::span<const double> WaveGeneratorNode::data(int index) const {
    return impl->analog_impl.data(index);
  }

  int WaveGeneratorNode::num_channels() const {
    return impl->analog_impl.num_channels();
  }

  void WaveGeneratorNode::inject(const thalamus::vector<std::span<double const>>& data, const thalamus::vector<std::chrono::nanoseconds>& sample_intervals, const thalamus::vector<std::string_view>& names) {
    impl->analog_impl.inject(data, sample_intervals, names);
  }

  std::chrono::nanoseconds WaveGeneratorNode::sample_interval(int channel) const {
    return impl->analog_impl.sample_interval(channel);
  }
  std::chrono::nanoseconds WaveGeneratorNode::time() const {
    return impl->analog_impl.time();
  }

  struct ToggleNode::Impl {
    ObservableList* nodes;
    ObservableDictPtr state;
    std::weak_ptr<Node> source;
    NodeGraph* graph;
    boost::signals2::scoped_connection state_connection;
    boost::signals2::scoped_connection source_connection;
    std::map<size_t, std::function<void(Node*)>> observers;
    boost::asio::io_context& io_context;
    thalamus::vector<double> buffer;
    std::list<thalamus::vector<double>> previous_buffers;
    double threshold = 1.6;
    bool high = false;
    double last_sample = 0;
    std::chrono::nanoseconds _time;
    size_t channel = 0;
    ToggleNode* outer;
    std::chrono::nanoseconds last_toggle;
    std::chrono::nanoseconds current_time;
    AnalogNodeImpl analog_impl;
    thalamus::vector<std::string> recommended_names;
    Impl(ObservableDictPtr state, boost::asio::io_context& io_context, NodeGraph* graph, ToggleNode* outer)
      : nodes(static_cast<ObservableList*>(state->parent))
      , state(state)
      , graph(graph)
      , io_context(io_context)
      , outer(outer)
      , recommended_names(1, "0") {
      using namespace std::placeholders;
      analog_impl.inject({ {std::span<double const>()} }, { 0ns }, {""});

      analog_impl.ready.connect([outer](Node*) {
        outer->ready(outer);
      });

      state_connection = state->changed.connect(std::bind(&Impl::on_change, this, _1, _2, _3));
      state->recap(std::bind(&Impl::on_change, this, _1, _2, _3));
    }

    std::optional<double> previous_value(int current, int lag) {
      auto index = current - lag;
      if (index >= 0) {
        return buffer.at(index);
      }
      for (auto i = previous_buffers.rbegin(); i != previous_buffers.rend(); ++i) {
        long long prev_index = i->size() + index;
        if (prev_index >= 0) {
          return i->at(prev_index);
        }
        index += i->size();
      }
      return std::nullopt;
    }

    void on_data(Node* raw_node, AnalogNode* node) {
      TRACE_EVENT0("thalamus", "ToggleNode::Impl::on_data");
      auto source = node->data(channel);

      buffer.assign(source.begin(), source.end());
      auto sample_interval = node->sample_interval(channel);
      _time = node->time();

      auto lag_time = 100ms;
      auto lag = lag_time / sample_interval;
      auto total = 0;
      for (auto i = previous_buffers.rbegin(); i != previous_buffers.rend(); ++i) {
        if (total > lag+1) {
          previous_buffers.erase(previous_buffers.begin(), i.base());
          break;
        }
        total += i->size();
      }

      auto i = 0;
      std::transform(buffer.begin(), buffer.end(), buffer.begin(), [&](double d) {
        current_time += sample_interval;
        auto lagged = previous_value(i, lag);
        if (lagged && current_time - last_toggle > 2*lag_time && lagged < threshold && d >= threshold) {
          high = !high;
          last_toggle = current_time;
        }
        last_sample = d;
        ++i;
        return high ? 3.3 : 0;
      });
      previous_buffers.emplace_back(source.begin(), source.end());

      analog_impl.inject({ {std::span<double const>(buffer.begin(), buffer.end())} }, { sample_interval }, {""});
    }

    void on_change(ObservableCollection::Action, const ObservableCollection::Key& k, const ObservableCollection::Value& v) {
      using namespace std::placeholders;

      auto key_str = std::get<std::string>(k);
      if (key_str == "Source") {
        source_connection.disconnect();
        auto value_str = std::get<std::string>(v);
        source = graph->get_node(value_str);
        auto locked_source = source.lock();
        auto analog_node = dynamic_cast<AnalogNode*>(locked_source.get());
        if (!locked_source || analog_node == nullptr) {
          source.reset();
          return;
        }

        source_connection = locked_source->ready.connect(std::bind(&Impl::on_data, this, _1, analog_node));
      }
      else if (key_str == "Threshold") {
        if (std::holds_alternative<std::string>(v)) {
          threshold = atof(std::get<std::string>(v).c_str());
        }
        else {
          threshold = std::get<double>(v);
        }
      }
      else if (key_str == "Channel") {
        channel = state->at("Channel");
      }
    }
  };

  ToggleNode::ToggleNode(ObservableDictPtr state, boost::asio::io_context& io_context, NodeGraph* graph)
  : impl(new Impl(state, io_context, graph, this)){}

  ToggleNode::~ToggleNode() {}

  std::string ToggleNode::type_name() {
    return "TOGGLE";
  }

  std::span<const double> ToggleNode::data(int i) const {
    return impl->analog_impl.data(i);
  }
  int ToggleNode::num_channels() const {
    return impl->analog_impl.num_channels();
  }
  std::string_view ToggleNode::name(int channel) const {
    return impl->recommended_names.at(channel);
  }
  std::span<const std::string> ToggleNode::get_recommended_channels() const {
    return std::span<const std::string>(impl->recommended_names.begin(), impl->recommended_names.end());
  }

  void ToggleNode::inject(const thalamus::vector<std::span<double const>>& data, const thalamus::vector<std::chrono::nanoseconds>& sample_intervals, const thalamus::vector<std::string_view>& names) {
    impl->analog_impl.inject(data, sample_intervals, names);
  }

  std::chrono::nanoseconds ToggleNode::sample_interval(int i) const {
    return impl->analog_impl.sample_interval(i);
  }

  std::chrono::nanoseconds ToggleNode::time() const {
    return impl->_time;
  }

  struct AnalogNodeImpl::Impl {
    thalamus::vector<std::string_view> names;
    thalamus::vector<std::span<double const>> spans;
    thalamus::vector<std::chrono::nanoseconds> sample_intervals;
    std::chrono::nanoseconds time;
  };

  AnalogNodeImpl::AnalogNodeImpl(ObservableDictPtr state, boost::asio::io_context&, NodeGraph* graph) : impl(new Impl()) {}
  AnalogNodeImpl::AnalogNodeImpl() : impl(new Impl()) {}
  AnalogNodeImpl::~AnalogNodeImpl() {}
  std::span<const double> AnalogNodeImpl::data(int channel) const {
    return impl->spans.at(channel);
  }
  int AnalogNodeImpl::num_channels() const {
    return impl->spans.size();
  }
  std::chrono::nanoseconds AnalogNodeImpl::sample_interval(int channel) const {
    return impl->sample_intervals.at(channel);
  }
  std::chrono::nanoseconds AnalogNodeImpl::time() const {
    return impl->time;
  }

  std::string_view AnalogNodeImpl::name(int channel) const {
    return impl->names.at(channel);
  }
  std::span<const std::string> AnalogNodeImpl::get_recommended_channels() const {
    return std::span<const std::string>();
  }

  void AnalogNodeImpl::inject(const thalamus::vector<std::span<double const>>& spans, const thalamus::vector<std::chrono::nanoseconds>& sample_intervals, const thalamus::vector<std::string_view>& names) {
    inject(spans, sample_intervals, names, std::chrono::steady_clock::now().time_since_epoch());
  }
  void AnalogNodeImpl::inject(const thalamus::vector<std::span<double const>>& spans, const thalamus::vector<std::chrono::nanoseconds>& sample_intervals, const thalamus::vector<std::string_view>& names, std::chrono::nanoseconds now) {
    impl->spans.assign(spans.begin(), spans.end());
    impl->sample_intervals.assign(sample_intervals.begin(), sample_intervals.end());
    impl->names = names;
    impl->time = now;
    ready(this);
  }
  std::string AnalogNodeImpl::type_name() {
    return "ANALOG";
  }

  std::vector<std::weak_ptr<ObservableDict>> get_nodes(ObservableList* nodes, const std::vector<std::string>& names) {
    std::vector<std::weak_ptr<ObservableDict>> targets;
    for (auto raw_token : names) {
      auto token = absl::StripAsciiWhitespace(raw_token);
      auto i = std::find_if(nodes->begin(), nodes->end(), [&](auto node) {
        ObservableDictPtr dict = node;
        std::string name = dict->at("name");
        auto stripped_name = absl::StripAsciiWhitespace(name);
        return stripped_name == token;
        });

      if (i != nodes->end()) {
        ObservableDictPtr temp = *i;
        targets.push_back(std::weak_ptr<ObservableDict>(temp));
      }
    }

    return targets;
  }
}