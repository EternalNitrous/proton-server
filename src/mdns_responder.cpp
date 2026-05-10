#include "mdns_responder.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <iphlpapi.h>
    using NativeSocket = SOCKET;
    using SocketLength = int;
#else
    #include <arpa/inet.h>
    #include <cerrno>
    #include <fcntl.h>
    #include <ifaddrs.h>
    #include <net/if.h>
    #include <netinet/in.h>
    #include <sys/select.h>
    #include <sys/socket.h>
    #include <unistd.h>
    using NativeSocket = int;
    using SocketLength = socklen_t;
#endif

namespace {

constexpr uint16_t MdnsPort = 5353;
constexpr const char* MdnsIpv4Group = "224.0.0.251";
constexpr const char* ServiceType = "_hexapod._tcp.local.";
constexpr const char* InstanceName = "Hexapod Simulator._hexapod._tcp.local.";
constexpr uint16_t DnsClassIn = 1;
constexpr uint16_t DnsClassCacheFlush = 0x8000;
constexpr uint16_t DnsTypeA = 1;
constexpr uint16_t DnsTypePtr = 12;
constexpr uint16_t DnsTypeTxt = 16;
constexpr uint16_t DnsTypeSrv = 33;
constexpr uint32_t TtlSeconds = 120;

NativeSocket native_socket(MdnsSocket socket)
{
    return static_cast<NativeSocket>(socket);
}

MdnsSocket mdns_socket(NativeSocket socket)
{
    return static_cast<MdnsSocket>(socket);
}

bool valid_socket(MdnsSocket socket)
{
    return socket != InvalidMdnsSocket;
}

int select_width(MdnsSocket socket)
{
#ifdef _WIN32
    (void)socket;
    return 0;
#else
    return socket + 1;
#endif
}

uint16_t read_u16(const unsigned char* data)
{
    return static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8) | data[1]);
}

void write_u16(std::vector<unsigned char>& out, uint16_t value)
{
    out.push_back(static_cast<unsigned char>((value >> 8) & 0xff));
    out.push_back(static_cast<unsigned char>(value & 0xff));
}

void write_u32(std::vector<unsigned char>& out, uint32_t value)
{
    out.push_back(static_cast<unsigned char>((value >> 24) & 0xff));
    out.push_back(static_cast<unsigned char>((value >> 16) & 0xff));
    out.push_back(static_cast<unsigned char>((value >> 8) & 0xff));
    out.push_back(static_cast<unsigned char>(value & 0xff));
}

std::string lowercase(std::string value)
{
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

std::string sanitized_host_name()
{
    std::array<char, 256> buffer = {};
#ifdef _WIN32
    if (gethostname(buffer.data(), static_cast<int>(buffer.size() - 1)) != 0) {
        return "hexapod-simulator";
    }
#else
    if (gethostname(buffer.data(), buffer.size() - 1) != 0) {
        return "hexapod-simulator";
    }
#endif

    std::string host = buffer.data();
    const auto dot = host.find('.');
    if (dot != std::string::npos) host.resize(dot);

    std::string cleaned;
    cleaned.reserve(host.size());
    bool previous_dash = false;
    for (char ch : host) {
        const bool ok = std::isalnum(static_cast<unsigned char>(ch)) != 0;
        if (ok) {
            cleaned.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
            previous_dash = false;
        } else if (!previous_dash && !cleaned.empty()) {
            cleaned.push_back('-');
            previous_dash = true;
        }
    }
    while (!cleaned.empty() && cleaned.back() == '-') cleaned.pop_back();
    return cleaned.empty() ? "hexapod-simulator" : cleaned;
}

std::string host_fqdn(const std::string& host_name)
{
    return host_name + ".local.";
}

void write_name(std::vector<unsigned char>& out, const std::string& name)
{
    std::size_t start = 0;
    while (start < name.size()) {
        const auto dot = name.find('.', start);
        const std::size_t end = dot == std::string::npos ? name.size() : dot;
        if (end > start) {
            const std::size_t length = std::min<std::size_t>(end - start, 63);
            out.push_back(static_cast<unsigned char>(length));
            out.insert(out.end(), name.begin() + static_cast<std::ptrdiff_t>(start),
                       name.begin() + static_cast<std::ptrdiff_t>(start + length));
        }
        if (dot == std::string::npos) break;
        start = dot + 1;
    }
    out.push_back(0);
}

bool parse_name(const unsigned char* data, std::size_t length,
                std::size_t& offset, std::string& name, int depth = 0)
{
    if (depth > 8) return false;

    std::size_t cursor = offset;
    std::string parsed;
    bool jumped = false;
    while (cursor < length) {
        const unsigned char label_length = data[cursor++];
        if (label_length == 0) {
            if (!jumped) offset = cursor;
            name = parsed;
            return true;
        }

        if ((label_length & 0xc0) == 0xc0) {
            if (cursor >= length) return false;
            const uint16_t pointer = static_cast<uint16_t>(((label_length & 0x3f) << 8) | data[cursor++]);
            if (pointer >= length) return false;
            if (!jumped) offset = cursor;
            std::size_t pointed_offset = pointer;
            std::string pointed_name;
            if (!parse_name(data, length, pointed_offset, pointed_name, depth + 1)) return false;
            parsed += pointed_name;
            name = parsed;
            return true;
        }

        if ((label_length & 0xc0) != 0 || cursor + label_length > length) return false;
        parsed.append(reinterpret_cast<const char*>(data + cursor), label_length);
        parsed.push_back('.');
        cursor += label_length;
    }

    return false;
}

struct Question {
    std::string name;
    uint16_t type = 0;
    uint16_t klass = 0;
};

std::vector<Question> parse_questions(const unsigned char* data, std::size_t length)
{
    std::vector<Question> questions;
    if (length < 12) return questions;

    const uint16_t qdcount = read_u16(data + 4);
    std::size_t offset = 12;
    for (uint16_t i = 0; i < qdcount; i++) {
        Question question;
        if (!parse_name(data, length, offset, question.name)) break;
        if (offset + 4 > length) break;
        question.type = read_u16(data + offset);
        question.klass = read_u16(data + offset + 2);
        offset += 4;
        question.name = lowercase(question.name);
        questions.push_back(question);
    }
    return questions;
}

std::vector<uint32_t> local_ipv4_addresses()
{
    std::vector<uint32_t> addresses;
#ifdef _WIN32
    ULONG buffer_size = 15 * 1024;
    std::vector<unsigned char> buffer(buffer_size);
    IP_ADAPTER_ADDRESSES* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
    ULONG result = GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST,
                                        nullptr, adapters, &buffer_size);
    if (result == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(buffer_size);
        adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
        result = GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST,
                                      nullptr, adapters, &buffer_size);
    }
    if (result == NO_ERROR) {
        for (IP_ADAPTER_ADDRESSES* adapter = adapters; adapter; adapter = adapter->Next) {
            if (adapter->OperStatus != IfOperStatusUp) continue;
            if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
            for (IP_ADAPTER_UNICAST_ADDRESS* address = adapter->FirstUnicastAddress;
                 address; address = address->Next) {
                auto* ipv4 = reinterpret_cast<sockaddr_in*>(address->Address.lpSockaddr);
                if (ipv4 && ipv4->sin_family == AF_INET) {
                    addresses.push_back(ipv4->sin_addr.s_addr);
                }
            }
        }
    }
#else
    ifaddrs* interfaces = nullptr;
    if (getifaddrs(&interfaces) == 0) {
        for (ifaddrs* iface = interfaces; iface; iface = iface->ifa_next) {
            if (!iface->ifa_addr || iface->ifa_addr->sa_family != AF_INET) continue;
            const unsigned int flags = iface->ifa_flags;
            if ((flags & IFF_UP) == 0 || (flags & IFF_LOOPBACK) != 0) continue;
            auto* ipv4 = reinterpret_cast<sockaddr_in*>(iface->ifa_addr);
            addresses.push_back(ipv4->sin_addr.s_addr);
        }
        freeifaddrs(interfaces);
    }
#endif

    std::sort(addresses.begin(), addresses.end());
    addresses.erase(std::unique(addresses.begin(), addresses.end()), addresses.end());
    return addresses;
}

void write_record(std::vector<unsigned char>& out,
                  const std::string& name,
                  uint16_t type,
                  uint16_t klass,
                  uint32_t ttl,
                  const std::vector<unsigned char>& rdata)
{
    write_name(out, name);
    write_u16(out, type);
    write_u16(out, klass);
    write_u32(out, ttl);
    write_u16(out, static_cast<uint16_t>(rdata.size()));
    out.insert(out.end(), rdata.begin(), rdata.end());
}

std::vector<unsigned char> encoded_name(const std::string& name)
{
    std::vector<unsigned char> out;
    write_name(out, name);
    return out;
}

std::vector<unsigned char> srv_rdata(uint16_t port, const std::string& target)
{
    std::vector<unsigned char> out;
    write_u16(out, 0);
    write_u16(out, 0);
    write_u16(out, port);
    write_name(out, target);
    return out;
}

std::vector<unsigned char> txt_rdata()
{
    std::vector<unsigned char> out;
    const std::array<std::string, 3> entries = {
        "txtvers=1",
        "app=Proton",
        "path=/"
    };
    for (const std::string& entry : entries) {
        out.push_back(static_cast<unsigned char>(std::min<std::size_t>(entry.size(), 255)));
        out.insert(out.end(), entry.begin(), entry.end());
    }
    return out;
}

std::vector<unsigned char> a_rdata(uint32_t address)
{
    std::vector<unsigned char> out(4);
    std::memcpy(out.data(), &address, out.size());
    return out;
}

std::vector<unsigned char> response_packet(uint16_t query_id,
                                           uint16_t service_port,
                                           const std::string& host_name)
{
    const std::string host = host_fqdn(host_name);
    const std::vector<uint32_t> addresses = local_ipv4_addresses();
    const uint16_t answer_count = static_cast<uint16_t>(3 + addresses.size());

    std::vector<unsigned char> out;
    out.reserve(512);
    write_u16(out, query_id);
    write_u16(out, 0x8400);
    write_u16(out, 0);
    write_u16(out, answer_count);
    write_u16(out, 0);
    write_u16(out, 0);

    write_record(out, ServiceType, DnsTypePtr, DnsClassIn, TtlSeconds,
                 encoded_name(InstanceName));
    write_record(out, InstanceName, DnsTypeSrv, DnsClassIn | DnsClassCacheFlush,
                 TtlSeconds, srv_rdata(service_port, host));
    write_record(out, InstanceName, DnsTypeTxt, DnsClassIn | DnsClassCacheFlush,
                 TtlSeconds, txt_rdata());
    for (uint32_t address : addresses) {
        write_record(out, host, DnsTypeA, DnsClassIn | DnsClassCacheFlush,
                     TtlSeconds, a_rdata(address));
    }

    return out;
}

bool should_answer(const std::vector<Question>& questions, const std::string& host_name)
{
    const std::string service = lowercase(ServiceType);
    const std::string instance = lowercase(InstanceName);
    const std::string host = lowercase(host_fqdn(host_name));

    for (const Question& question : questions) {
        const uint16_t type = question.type;
        const bool any = type == 255;
        if (question.name == service && (any || type == DnsTypePtr)) return true;
        if (question.name == instance && (any || type == DnsTypeSrv || type == DnsTypeTxt)) return true;
        if (question.name == host && (any || type == DnsTypeA)) return true;
    }
    return false;
}

bool wants_unicast_reply(const std::vector<Question>& questions)
{
    return std::any_of(questions.begin(), questions.end(), [](const Question& question) {
        return (question.klass & 0x8000) != 0;
    });
}

void close_socket(MdnsSocket socket_fd)
{
    if (!valid_socket(socket_fd)) return;
#ifdef _WIN32
    closesocket(native_socket(socket_fd));
#else
    close(socket_fd);
#endif
}

} // namespace

MdnsResponder::~MdnsResponder()
{
    stop();
}

bool MdnsResponder::start(int service_port)
{
    if (running_) return true;

#ifdef _WIN32
    WSADATA data = {};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        return false;
    }
    socket_runtime_started_ = true;
#endif

    socket_fd_ = mdns_socket(socket(AF_INET, SOCK_DGRAM, 0));
    if (!valid_socket(socket_fd_)) {
#ifdef _WIN32
        WSACleanup();
        socket_runtime_started_ = false;
#endif
        return false;
    }

    int reuse = 1;
    setsockopt(native_socket(socket_fd_), SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuse), sizeof(reuse));
#if defined(SO_REUSEPORT)
    setsockopt(native_socket(socket_fd_), SOL_SOCKET, SO_REUSEPORT,
               reinterpret_cast<const char*>(&reuse), sizeof(reuse));
#endif

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(MdnsPort);
    if (bind(native_socket(socket_fd_), reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        close_socket(socket_fd_);
        socket_fd_ = InvalidMdnsSocket;
#ifdef _WIN32
        WSACleanup();
        socket_runtime_started_ = false;
#endif
        return false;
    }

    ip_mreq group{};
    group.imr_multiaddr.s_addr = inet_addr(MdnsIpv4Group);
    group.imr_interface.s_addr = htonl(INADDR_ANY);
    setsockopt(native_socket(socket_fd_), IPPROTO_IP, IP_ADD_MEMBERSHIP,
               reinterpret_cast<const char*>(&group), sizeof(group));

    unsigned char ttl = 255;
    setsockopt(native_socket(socket_fd_), IPPROTO_IP, IP_MULTICAST_TTL,
               reinterpret_cast<const char*>(&ttl), sizeof(ttl));

    service_port_ = service_port;
    host_name_ = sanitized_host_name();
    running_ = true;
    thread_ = std::thread(&MdnsResponder::run_loop, this);
    return true;
}

void MdnsResponder::stop()
{
    running_ = false;
    if (valid_socket(socket_fd_)) {
        close_socket(socket_fd_);
        socket_fd_ = InvalidMdnsSocket;
    }
    if (thread_.joinable()) {
        thread_.join();
    }
#ifdef _WIN32
    if (socket_runtime_started_) {
        WSACleanup();
        socket_runtime_started_ = false;
    }
#endif
}

void MdnsResponder::run_loop()
{
    sockaddr_in multicast{};
    multicast.sin_family = AF_INET;
    multicast.sin_port = htons(MdnsPort);
    inet_pton(AF_INET, MdnsIpv4Group, &multicast.sin_addr);

    auto send_response = [&](const sockaddr_in* unicast_target, uint16_t query_id) {
        const auto packet = response_packet(query_id, static_cast<uint16_t>(service_port_), host_name_);
        sendto(native_socket(socket_fd_), reinterpret_cast<const char*>(packet.data()), static_cast<int>(packet.size()), 0,
               reinterpret_cast<const sockaddr*>(&multicast), sizeof(multicast));
        if (unicast_target) {
            sendto(native_socket(socket_fd_), reinterpret_cast<const char*>(packet.data()), static_cast<int>(packet.size()), 0,
                   reinterpret_cast<const sockaddr*>(unicast_target), sizeof(*unicast_target));
        }
    };

    send_response(nullptr, 0);
    auto last_announcement = std::chrono::steady_clock::now();

    while (running_) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(native_socket(socket_fd_), &read_fds);

        timeval timeout{};
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        const int ready = select(select_width(socket_fd_), &read_fds, nullptr, nullptr, &timeout);
        if (!running_) break;

        const auto now = std::chrono::steady_clock::now();
        if (now - last_announcement >= std::chrono::seconds(60)) {
            send_response(nullptr, 0);
            last_announcement = now;
        }

        if (ready <= 0 || !FD_ISSET(socket_fd_, &read_fds)) continue;

        std::array<unsigned char, 1500> buffer = {};
        sockaddr_in peer{};
        SocketLength peer_length = sizeof(peer);
        const int received = static_cast<int>(recvfrom(
            native_socket(socket_fd_),
            reinterpret_cast<char*>(buffer.data()),
            static_cast<int>(buffer.size()),
            0,
            reinterpret_cast<sockaddr*>(&peer),
            &peer_length
        ));
        if (received < 12) continue;

        const uint16_t flags = read_u16(buffer.data() + 2);
        if ((flags & 0x8000) != 0) continue;

        const auto questions = parse_questions(buffer.data(), static_cast<std::size_t>(received));
        if (!should_answer(questions, host_name_)) continue;

        const uint16_t query_id = read_u16(buffer.data());
        const bool unicast_reply = peer.sin_port != htons(MdnsPort)
                                || wants_unicast_reply(questions);
        send_response(unicast_reply ? &peer : nullptr, query_id);
    }
}
