#include "order_server.h"

namespace Exchange {
    OrderServer::OrderServer(ClientRequestLFQueue* client_requests, ClientResponseLFQueue* client_responses, const std::string& iface, int port)
        : iface_(iface), port_(port), outgoing_responses_(client_responses), logger_("exchange_order_server.log"),
        tcp_server_(logger_), fifo_sequencer_(client_requests, &logger_) {

        cid_next_outgoing_seq_num_.fill(1);
        cid_next_exp_seq_num_.fill(1);
        cid_tcp_socket_.fill(nullptr);

        //Set callbacks
        tcp_server_.recv_callback_ = [this](auto socket, auto rx_time) { recvCallback(socket, rx_time); };
        tcp_server_.recv_finished_callback_ = [this]() { recvFinishedCallback(); };
    }

    auto OrderServer::start() -> void {
        run_ = true;
        tcp_server_.listen(iface_, port_);
    }

    auto OrderServer::stop() -> void {
        run_ = false;
    }

    OrderServer::~OrderServer() {
        stop();

        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(1s);
    }

}