#pragma once

#include <string>
#include <thalamus.pb.h>
#include <thalamus_asio.h>
#include <base_node.h>
#include <state.h>
#include <image_node.h>

namespace thalamus {
  class ChessBoardNode : public Node, public ImageNode {
    struct Impl;
    std::unique_ptr<Impl> impl;
  public:
    ChessBoardNode(ObservableDictPtr state, boost::asio::io_context& io_context, NodeGraph*);
    ~ChessBoardNode();
    static std::string type_name();
    static bool prepare();
    Plane plane(int) const override;
    size_t num_planes() const override;
    Format format() const override;
    size_t width() const override;
    size_t height() const override;
    std::chrono::nanoseconds frame_interval() const override;
    std::chrono::nanoseconds time() const override;
    void inject(const thalamus_grpc::Image&) override;
    boost::json::value process(const boost::json::value&) override;
    bool has_image_data() const override;
  };
}

