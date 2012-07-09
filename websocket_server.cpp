#include "stdafx.h"
#if WITH_WEBSOCKETS
#include "websocket_server.hpp"
#include "threading.hpp"
#include "graphics.hpp"
#include "string_utils.hpp"
#include "json_utils.hpp"
#include "demo_engine.hpp"
#include "logger.hpp"
#include "app.hpp"

extern "C" {
#include "sha1.h"
}

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Crypt32.lib")
using namespace std;

WebSocketServer *WebSocketServer::_instance;

static string get_value(const char *str, const char *key) {
  string res;
  if (const char *s = strstr(str, key)) {
    int len = strlen(key);
    const char *start = s + len;
    if (const char *end = strstr(start, "\r\n")) {
      res = string(start, end - start);
    }
  }
  return res;
}

#pragma pack(push, 1)
struct WebSocketMessageHeader {

  enum OpCode {
    kOpMessageContinuation    = 0x0,
    kOpTextMessage            = 0x1,
    kOpBinaryMessage          = 0x2,
    kOpConnectionClose        = 0x8,
    kOpPing                   = 0x9,
    kOpPong                   = 0xa,
  };

  union {
    struct {
      uint16_t OP_CODE : 4;
      uint16_t RSV1 : 1;
      uint16_t RSV2 : 1;
      uint16_t RSV3 : 1;
      uint16_t FIN : 1;
      uint16_t LEN : 7;
      uint16_t MASK : 1;
    } bits;
    uint16_t short_header;
  };

#pragma warning(suppress: 4200)
  char data[0];

  uint64_t GetMessageLength() const {
    return sizeof(WebSocketMessageHeader) + GetPayloadOffset() + GetPayloadLength();
  }

  size_t GetPayloadOffset() const {
    size_t ofs = offsetof(WebSocketMessageHeader, data);
    ofs += bits.MASK ? sizeof(uint32_t) : 0;
    ofs += bits.LEN <= 125 ? 0 : bits.LEN == 126 ? sizeof(uint16_t) : sizeof(uint64_t);
    return ofs;
  }

  uint64_t GetPayloadLength() const {
    if (bits.LEN < 125 )
      return bits.LEN;
    else if (bits.LEN == 126)
      return *(uint16_t*)data;
    else if (bits.LEN == 127)
      return *(uint64_t*)data;
    return -1;
  }

  uint8_t *GetMask() const {
    assert(bits.MASK);
    return (uint8_t*)&data[GetPayloadOffset() - offsetof(WebSocketMessageHeader, data) - 4];
  }

  OpCode GetOpCode() const { return (OpCode)bits.OP_CODE; }

  bool IsFinal() const { return bits.FIN; }
  bool IsMasked() const { return bits.MASK; }

};
#pragma pack(pop)

static string websocket_handshake(const char *msg) {

  const char *get_str = strstr(msg, "GET /servicename HTTP/1.1");
  string host = get_value(msg, "Host: ");
  const char *upgrade_str = strstr(msg, "Upgrade: websocket");
  const char *conn_str = strstr(msg, "Connection: Upgrade");
  string sec = get_value(msg, "Sec-WebSocket-Key: ");
  string origin = get_value(msg, "Origin: ");

  // compute response to challenge
  const char *SpecifcationGUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  SHA1Context ctx;
  SHA1Reset(&ctx);
  SHA1Input(&ctx, (const uint8_t *)sec.data(), sec.size());
  SHA1Input(&ctx, (const uint8_t *)SpecifcationGUID, strlen(SpecifcationGUID));
  uint8_t digest[20];
  SHA1Result(&ctx, digest);
  string response = base64encode(digest, sizeof(digest));
  return "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " + 
    response + "\r\n\r\n";
}


static void websocket_frame(const char *payload, int len, vector<char> *msg) {

  // we always use a mask
  int hdr_size = sizeof(WebSocketMessageHeader) + sizeof(uint32_t);

  WebSocketMessageHeader *hdr = nullptr;

  if (len <= 125) {
    msg->resize(hdr_size + len);
    hdr = (WebSocketMessageHeader *)msg->data();
    hdr->bits.LEN = len;

  } else if (len <= 65535) {
    hdr_size += sizeof(uint16_t);
    msg->resize(hdr_size + len);
    hdr = (WebSocketMessageHeader *)msg->data();
    hdr->bits.LEN = 126;
    *(uint16_t *)&hdr->data = len;

  } else {
    hdr_size += sizeof(uint64_t);
    msg->resize(hdr_size + len);
    hdr = (WebSocketMessageHeader *)msg->data();
    hdr->bits.LEN = 127;
    *(uint64_t *)&hdr->data = len;
  }

  hdr->bits.MASK = 1;
  hdr->bits.FIN = 1;
  hdr->bits.OP_CODE = WebSocketMessageHeader::kOpTextMessage;

  uint8_t *mask = hdr->GetMask();
  for (int i = 0; i < 4; ++i)
    mask[i] = i+1;

  char *encoded = msg->data() + hdr->GetPayloadOffset();
  for (int i = 0; i < len; ++i) {
    encoded[i] = payload[i] ^ mask[i%4];
  }
}

struct ClientData {
  SOCKET socket;
  HANDLE close_event;
};

DWORD WINAPI client_thread(void *param) {

  const int BUF_SIZE = 32 * 1024;
  char *buf = (char *)_alloca(BUF_SIZE);

  ClientData cd = *(ClientData *)param;
  DEFER([=]() { closesocket(cd.socket); });

  int ret = recv(cd.socket, buf, BUF_SIZE, 0);
  if (ret < 0) {
    LOG_ERROR_LN("Error in recv: %d", ret);
    return 1;
  }

  string reply = websocket_handshake(buf);
  if (reply.empty()) {
    LOG_ERROR_LN("Error creating websocket handshake");
    return 1;
  }

  int send_result = send(cd.socket, reply.data(), reply.size(), 0);
  if (send_result < 0) {
    LOG_ERROR_LN("Error second handshake: %d", send_result);
    return 1;
  }

  // create an accept event
  HANDLE events[2];
  events[0] = cd.close_event;
  events[1] = CreateEvent(NULL, FALSE, FALSE, NULL);
  DEFER([&]() {CloseHandle(events[1]);});

  while (true) {

    WSAOVERLAPPED overlapped;
    ZeroMemory(&overlapped, sizeof(overlapped));
    overlapped.hEvent = events[1];

    // Receive until the peer shuts down the connection
    WSABUF wsabuf;
    wsabuf.buf = buf;
    wsabuf.len = BUF_SIZE;

    DWORD bytes_read = 0;
    DWORD flags = 0;
    if (WSARecv(cd.socket, &wsabuf, 1, &bytes_read, &flags, &overlapped, NULL) == SOCKET_ERROR) {
      int err = WSAGetLastError();
      if (err != WSA_IO_PENDING) {
        LOG_ERROR_LN("Error in recv: %d", WSAGetLastError());
        return 1;
      }
    }

    int idx = WSAWaitForMultipleEvents(ELEMS_IN_ARRAY(events), events, FALSE, INFINITE, FALSE);
    if (idx == WSA_WAIT_EVENT_0)
      break;

    BOOL res = WSAGetOverlappedResult(cd.socket, &overlapped, &bytes_read, FALSE, &flags);

    WebSocketMessageHeader *header = (WebSocketMessageHeader *)buf;

    switch (header->GetOpCode()) {

      case WebSocketMessageHeader::kOpTextMessage: {
        // decode the message, and send it to the app
        int len = (int)header->GetPayloadLength();
        char *decoded = new char[len];
        const uint8_t *mask = header->GetMask();
        uint8_t *data = (uint8_t *)(buf + header->GetPayloadOffset());
        for (int i = 0; i < len; ++i) {
          decoded[i] = data[i] ^ mask[i%4];
        }
        DISPATCHER.invoke(FROM_HERE, 
          threading::kMainThread, 
          std::bind(&App::add_network_msg, &App::instance(), cd.socket, decoded, len));
        break;
      }

      case WebSocketMessageHeader::kOpConnectionClose:
        return 0;

      default:
        LOG_WARNING_LN("Unhandled websocket opcode: %d", header->GetOpCode());
    }
  }

  return 0;
}

WebSocketThread::WebSocketThread() 
  : BlockingThread(threading::kWebSocketThread, nullptr)
{
}

UINT WebSocketThread::blocking_run(void *param) {

  WSADATA data;
  int res;
  if (res = WSAStartup(MAKEWORD(2,2), &data))
    return res;
  DEFER([]{ WSACleanup(); } );

  struct addrinfo *result = nullptr, *ptr = nullptr, hints;
  ZeroMemory(&hints, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_flags = AI_PASSIVE;

  if (res = getaddrinfo(NULL, "9002", &hints, &result))
    return res;
  DEFER([=]{ freeaddrinfo(result); });

  SOCKET listen_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
  if (listen_socket == INVALID_SOCKET)
    return 1;
  DEFER([=]{ closesocket(listen_socket); });

  if (res = ::bind(listen_socket, result->ai_addr, (int)result->ai_addrlen))
    return res;

  ULONG p = 1;
  ioctlsocket(listen_socket, FIONBIO, &p);

  while (WaitForSingleObject(_cancel_event, 0) != WAIT_OBJECT_0) {
    if (listen(listen_socket, SOMAXCONN) == SOCKET_ERROR) {
      return 1;
    }

    SOCKET client_socket = INVALID_SOCKET;
    while (client_socket == INVALID_SOCKET) {
      client_socket = accept(listen_socket, NULL, NULL);
      if (WaitForSingleObject(_cancel_event, 0) == WAIT_OBJECT_0)
        break;
      if (client_socket == INVALID_SOCKET) {
        int r = WSAGetLastError();
        if (r != WSAEWOULDBLOCK) {
          return 1;
        }
        process_deferred();
        Sleep(1000);
      }
    }

    if (client_socket != INVALID_SOCKET) {
      DWORD thread_id;
      ClientData cd;
      cd.close_event = _cancel_event;
      cd.socket = client_socket;
      HANDLE h = CreateThread(NULL, 0, client_thread, &cd, 0, &thread_id);
      _client_threads.push_back(h);
    }
  }

  WaitForMultipleObjects(_client_threads.size(), _client_threads.data(), TRUE, INFINITE);
  return 0;
}

void WebSocketThread::send_msg(SOCKET receiver, const char *msg, int len) {

  if (strncmp(msg, "REQ:SYSTEM.FPS", len) == 0) {
    auto obj = JsonValue::create_object();
    obj->add_key_value("system.fps", JsonValue::create_number(GRAPHICS.fps()));
    obj->add_key_value("system.ms", JsonValue::create_number(APP.frame_time()));
    string str = print_json(obj);

    vector<char> frame;
    websocket_frame(str.c_str(), str.size(), &frame);
    send(receiver, frame.data(), frame.size(), 0);
  }

}

void WebSocketThread::stop() {
  join();
}

bool WebSocketServer::create() {
  assert(!_instance);
  _instance = new WebSocketServer();
  return _instance->_thread.start();
}

bool WebSocketServer::close() {
  assert(_instance);
  _instance->_thread.stop();
  delete exch_null(_instance);
  return true;
}

void WebSocketServer::send_msg(SOCKET receiver, const char *msg, int len) {
  DISPATCHER.invoke(FROM_HERE, threading::kWebSocketThread, 
    std::bind(&WebSocketThread::send_msg, &_thread, receiver, msg, len));
}

WebSocketServer& WebSocketServer::instance() {
  assert(_instance);
  return *_instance;
}

#if 0

class echo_server_handler : public server::handler {
public:

  static void on_main_thread(connection_ptr con, std::string str) {
    if (str == "REQ:SYSTEM.FPS") {
      auto obj = JsonValue::create_object();
      obj->add_key_value("system.fps", JsonValue::create_number(GRAPHICS.fps()));
      obj->add_key_value("system.ms", JsonValue::create_number(APP.frame_time()));
      con->send(print_json(obj), frame::opcode::TEXT);
    } else if (str == "REQ:DEMO.INFO") {
      con->send(print_json(DEMO_ENGINE.get_info()), frame::opcode::TEXT);
    } else {
      JsonValue::JsonValuePtr m = parse_json(str.c_str(), str.c_str() + str.size());
      if ((*m)["msg"]) {
        // msg has type and data fields
        auto d = (*m)["msg"];
        auto type = (*d)["type"]->to_string();
        auto data = (*d)["data"];

        if (type == "time") {
          bool playing = (*data)["is_playing"]->to_bool();
          int cur_time = (*data)["cur_time"]->to_int();
          DEMO_ENGINE.set_pos(cur_time);
          DEMO_ENGINE.set_paused(!playing);

        } else if (type == "demo") {
          DEMO_ENGINE.update(data);
        }
      } else {
        LOG_WARNING_LN("Unknown websocket message: %s", str);
      }
    }
  }

  void on_message(connection_ptr con, message_ptr msg) {
    DISPATCHER.invoke(FROM_HERE, threading::kMainThread, boost::bind(&echo_server_handler::on_main_thread, con, msg->get_payload()));
  }
};

struct WebSocketThread::Impl {
  Impl() : _server(nullptr) {}
  websocketpp::server *_server;
};

WebSocketThread::WebSocketThread() 
  : BlockingThread(threading::kWebSocketThread, nullptr)
  , _impl(nullptr)
{
}

UINT WebSocketThread::blocking_run(void *data) {

  unsigned short port = 9002;
  _impl = new Impl;

  try {       
    websocketpp::server::handler::ptr h(new echo_server_handler());
    _impl->_server = new websocketpp::server(h);

    _impl->_server->alog().unset_level(websocketpp::log::alevel::ALL);
    _impl->_server->elog().unset_level(websocketpp::log::elevel::ALL);

    _impl->_server->alog().set_level(websocketpp::log::alevel::CONNECT);
    _impl->_server->alog().set_level(websocketpp::log::alevel::DISCONNECT);

    _impl->_server->elog().set_level(websocketpp::log::elevel::RERROR);
    _impl->_server->elog().set_level(websocketpp::log::elevel::FATAL);

    _impl->_server->listen(port);
  } catch (std::exception& e) {
    std::cerr << "Exception: " << e.what() << std::endl;
  }

  SAFE_DELETE(_impl->_server);
  SAFE_DELETE(_impl);

  return 0;
}

void WebSocketThread::stop() {
  if (_impl && _impl->_server)
    _impl->_server->stop();
  join();
}

#endif


#endif