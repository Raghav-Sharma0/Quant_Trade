// =============================================================================
// exchange-sim/market_data/ws_publisher.hpp
//
// Zero-dependency single-header WebSocket server (RFC 6455) that broadcasts
// every MarketData snapshot and Trade as a BINARY message to all connected
// clients.
// =============================================================================
#pragma once

#include "../../core-cpp/include/common/types.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>

// POSIX sockets
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

// SHA-1 for WebSocket handshake
namespace ws_detail {

struct SHA1 {
    uint32_t h[5] = {0x67452301,0xEFCDAB89,0x98BADCFE,0x10325476,0xC3D2E1F0};
    uint8_t  buf[64]{};
    uint64_t total = 0;

    static uint32_t rol(uint32_t v, int n){ return (v<<n)|(v>>(32-n)); }

    void process_block(const uint8_t* b) {
        uint32_t w[80];
        for(int i=0;i<16;++i)
            w[i]=(b[i*4]<<24)|(b[i*4+1]<<16)|(b[i*4+2]<<8)|b[i*4+3];
        for(int i=16;i<80;++i) w[i]=rol(w[i-3]^w[i-8]^w[i-14]^w[i-16],1);
        uint32_t a=h[0],bb=h[1],c=h[2],d=h[3],e=h[4];
        auto step=[&](int i,uint32_t f,uint32_t k){
            uint32_t t=rol(a,5)+f+e+k+w[i];
            e=d; d=c; c=rol(bb,30); bb=a; a=t;
        };
        for(int i= 0;i<20;++i) step(i,(bb&c)|((~bb)&d),0x5A827999);
        for(int i=20;i<40;++i) step(i, bb^c^d,           0x6ED9EBA1);
        for(int i=40;i<60;++i) step(i,(bb&c)|(bb&d)|(c&d),0x8F1BBCDC);
        for(int i=60;i<80;++i) step(i, bb^c^d,           0xCA62C1D6);
        h[0]+=a; h[1]+=bb; h[2]+=c; h[3]+=d; h[4]+=e;
    }
    void update(const uint8_t* data, size_t len) {
        size_t off = total & 63;
        total += len;
        while(len--) {
            buf[off++] = *data++;
            if(off==64){ process_block(buf); off=0; }
        }
    }
    void digest(uint8_t out[20]) {
        size_t off = total & 63;
        buf[off++] = 0x80;
        if(off>56){ while(off<64) buf[off++]=0; process_block(buf); off=0; }
        while(off<56) buf[off++]=0;
        uint64_t bits = total*8;
        for(int i=7;i>=0;--i){ buf[56+7-i]=(bits>>(i*8))&0xFF; }
        process_block(buf);
        for(int i=0;i<5;++i){
            out[i*4]=(h[i]>>24)&0xFF; out[i*4+1]=(h[i]>>16)&0xFF;
            out[i*4+2]=(h[i]>>8)&0xFF; out[i*4+3]=h[i]&0xFF;
        }
    }
};

static std::string base64(const uint8_t* data, size_t len) {
    static const char T[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out; out.reserve(((len+2)/3)*4);
    for(size_t i=0;i<len;i+=3){
        uint32_t v=(data[i]<<16)|((i+1<len?data[i+1]:0)<<8)|(i+2<len?data[i+2]:0);
        out+=T[(v>>18)&63]; out+=T[(v>>12)&63];
        out+=(i+1<len?T[(v>>6)&63]:'=');
        out+=(i+2<len?T[v&63]:'=');
    }
    return out;
}

static std::vector<uint8_t> ws_frame_binary(const uint8_t* payload, size_t n) {
    std::vector<uint8_t> frame;
    frame.push_back(0x82); // FIN + binary opcode
    if(n < 126) {
        frame.push_back(static_cast<uint8_t>(n));
    } else if(n < 65536) {
        frame.push_back(126);
        frame.push_back((n>>8)&0xFF);
        frame.push_back(n&0xFF);
    } else {
        frame.push_back(127);
        for(int i=7;i>=0;--i) frame.push_back((n>>(i*8))&0xFF);
    }
    frame.insert(frame.end(), payload, payload + n);
    return frame;
}

} // namespace ws_detail

namespace hft {

#pragma pack(push, 1)
struct WireTick {
    uint8_t  msg_type; // 1 for tick
    uint16_t symbol_id;
    int64_t  timestamp_ns;
    uint32_t bid;
    uint32_t ask;
    uint32_t bid_sz;
    uint32_t ask_sz;
    uint32_t last_price;
    uint32_t volume;
    int64_t  sequence;
};

struct WireTrade {
    uint8_t  msg_type; // 2 for trade
    uint16_t symbol_id;
    int64_t  timestamp_ns;
    uint32_t price;
    uint32_t quantity;
    uint64_t trade_id;
    uint64_t bid_order_id;
    uint64_t ask_order_id;
    int64_t  sequence;
};
#pragma pack(pop)

class WsPublisher {
public:
    explicit WsPublisher(int port = 8080) : port_(port), running_(false) {}

    ~WsPublisher() {
        running_ = false;
        if(listen_fd_ >= 0) {
            ::shutdown(listen_fd_, SHUT_RDWR);
            ::close(listen_fd_);
        }
        if(accept_thread_.joinable()) accept_thread_.join();
        
        std::lock_guard<std::mutex> lk(clients_mu_);
        // 1000 Normal Closure payload
        uint8_t close_frame[4] = {0x88, 0x02, 0x03, 0xE8};
        for(int fd : clients_) {
            ::send(fd, close_frame, sizeof(close_frame), MSG_NOSIGNAL);
            ::close(fd);
        }
        clients_.clear();
    }

    void start() {
        listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        int yes = 1;
        ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);

        sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_port        = htons(static_cast<uint16_t>(port_));
        addr.sin_addr.s_addr = INADDR_ANY;
        ::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof addr);
        ::listen(listen_fd_, 8);

        running_ = true;
        accept_thread_ = std::thread([this]{ accept_loop(); });
        std::printf("[WsPublisher] Listening on ws://0.0.0.0:%d/\n", port_);
    }

    template<typename SymMap>
    void publish_tick(const MarketData& md, const SymMap& sym_map, uint64_t now_ns) {
        WireTick msg;
        msg.msg_type = 1;
        msg.symbol_id = static_cast<uint16_t>(md.symbol_id);
        msg.timestamp_ns = static_cast<int64_t>(now_ns);
        msg.bid = md.best_bid_price;
        msg.ask = md.best_ask_price;
        msg.bid_sz = md.best_bid_qty;
        msg.ask_sz = md.best_ask_qty;
        msg.last_price = md.last_trade_price;
        msg.volume = md.last_trade_qty;
        msg.sequence = static_cast<int64_t>(md.sequence);
        
        broadcast(reinterpret_cast<const uint8_t*>(&msg), sizeof(msg));
    }

    void publish_trade(const Trade& t) {
        WireTrade msg;
        msg.msg_type = 2;
        msg.symbol_id = static_cast<uint16_t>(t.symbol_id);
        msg.timestamp_ns = static_cast<int64_t>(t.timestamp);
        msg.price = t.price;
        msg.quantity = t.quantity;
        msg.trade_id = static_cast<uint64_t>(t.trade_id);
        msg.bid_order_id = static_cast<uint64_t>(t.bid_order_id);
        msg.ask_order_id = static_cast<uint64_t>(t.ask_order_id);
        msg.sequence = static_cast<int64_t>(t.sequence);

        broadcast(reinterpret_cast<const uint8_t*>(&msg), sizeof(msg));
    }

    size_t client_count() const {
        std::lock_guard<std::mutex> lk(clients_mu_);
        return clients_.size();
    }

private:
    int  port_;
    int  listen_fd_ = -1;
    std::atomic<bool> running_;
    std::thread  accept_thread_;

    mutable std::mutex clients_mu_;
    std::vector<int>   clients_;

    void accept_loop() {
        while(running_) {
            pollfd pfd{listen_fd_, POLLIN, 0};
            if(::poll(&pfd, 1, 200) <= 0) continue;

            sockaddr_in peer{};
            socklen_t   plen = sizeof peer;
            int fd = ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&peer), &plen);
            if(fd < 0) continue;

            std::thread([this, fd]{
                if(do_handshake(fd)) {
                    std::lock_guard<std::mutex> lk(clients_mu_);
                    clients_.push_back(fd);
                    std::printf("[WsPublisher] Client connected, total=%zu\n", clients_.size());
                } else {
                    ::close(fd);
                }
            }).detach();
        }
    }

    bool do_handshake(int fd) {
        char req[2048]{};
        ssize_t n = ::recv(fd, req, sizeof req - 1, 0);
        if(n <= 0) return false;

        const char* key_hdr = ::strstr(req, "Sec-WebSocket-Key:");
        if(!key_hdr) return false;
        key_hdr += 18;
        while(*key_hdr == ' ') ++key_hdr;
        char key[64]{};
        size_t ki = 0;
        while(*key_hdr && *key_hdr != '\r' && *key_hdr != '\n' && ki < 63)
            key[ki++] = *key_hdr++;

        const char magic[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        ws_detail::SHA1 sha;
        sha.update(reinterpret_cast<const uint8_t*>(key), ::strlen(key));
        sha.update(reinterpret_cast<const uint8_t*>(magic), ::strlen(magic));
        uint8_t digest[20];
        sha.digest(digest);
        std::string accept = ws_detail::base64(digest, 20);

        std::string resp =
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: " + accept + "\r\n\r\n";
        return ::send(fd, resp.c_str(), resp.size(), MSG_NOSIGNAL) > 0;
    }

    void broadcast(const uint8_t* payload, size_t n) {
        auto frame = ws_detail::ws_frame_binary(payload, n);
        std::lock_guard<std::mutex> lk(clients_mu_);
        std::vector<int> alive;
        for(int fd : clients_) {
            // Remove MSG_DONTWAIT: blocking send prevents partial writes that corrupt the
            // WebSocket stream and prevents immediately dropping clients under backpressure.
            ssize_t sent = ::send(fd, frame.data(), frame.size(), MSG_NOSIGNAL);
            if(sent == static_cast<ssize_t>(frame.size())) {
                alive.push_back(fd);
            } else {
                ::close(fd);
                std::printf("[WsPublisher] Client dropped, total=%zu\n", alive.size());
            }
        }
        clients_ = std::move(alive);
    }
};

} // namespace hft
