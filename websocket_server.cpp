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
    return GetPayloadOffset() + GetPayloadLength();
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
      return ntohs(*(uint16_t*)data);
    else if (bits.LEN == 127)
      return *(uint64_t*)data;
    return -1;
  }

  size_t GetMaskOffset() const {
    KASSERT(bits.MASK);
    return GetPayloadOffset() - sizeof(uint32_t);
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

static uint64_t hton64(uint64_t x) {
  return ((uint64_t)htonl(x & 0xffffffff)) << 32 | htonl(x >> 32);
}

static void websocket_frame(const char *payload, int len, vector<char> *msg) {

  int hdr_size = sizeof(WebSocketMessageHeader)/* + sizeof(uint32_t)*/;

  WebSocketMessageHeader *hdr = nullptr;

  if (len <= 125) {
    msg->resize(hdr_size + len);
    hdr = (WebSocketMessageHeader *)msg->data();
    ZeroMemory(hdr, hdr_size);
    hdr->bits.LEN = len;

  } else if (len <= 65535) {
    hdr_size += sizeof(uint16_t);
    msg->resize(hdr_size + len);
    hdr = (WebSocketMessageHeader *)msg->data();
    ZeroMemory(hdr, hdr_size);
    hdr->bits.LEN = 126;
    *(uint16_t *)&hdr->data = htons(len);

  } else {
    hdr_size += sizeof(uint64_t);
    msg->resize(hdr_size + len);
    hdr = (WebSocketMessageHeader *)msg->data();
    ZeroMemory(hdr, hdr_size);
    hdr->bits.LEN = 127;
    *(uint64_t *)&hdr->data = hton64(len);
  }

  hdr->bits.MASK = 0;
  hdr->bits.FIN = 1;
  hdr->bits.OP_CODE = WebSocketMessageHeader::kOpTextMessage;

  memcpy(msg->data() + hdr->GetPayloadOffset(), payload, len);
}

DWORD WINAPI WebSocketThread::client_thread(void *param) {

  const int BUF_SIZE = 16 * 1024;
  char *buf = (char *)_alloca(BUF_SIZE);

  ClientData cd = *(ClientData *)param;

  // When we go out of scope, close the socket, and delete ourselves from the thread list
  DEFER([=]() { 
    closesocket(cd.socket); 
    SCOPED_CS(cd.self->_thread_cs);
    auto it = std::find(RANGE(cd.self->_client_threads), cd);
    cd.self->_client_threads.erase(it);
  });

  HANDLE async_event = CreateEvent(NULL, FALSE, FALSE, NULL);
  DEFER([=] { CloseHandle(async_event); });
  WSAEventSelect(cd.socket, async_event, FD_READ);
  WaitForSingleObject(async_event, INFINITE);

  int ret = recv(cd.socket, buf, BUF_SIZE, 0);
  if (ret < 0) {
    return WSAGetLastError();
  }

  string reply = websocket_handshake(buf);
  if (reply.empty())
    return 1;

  int send_result = send(cd.socket, reply.data(), reply.size(), 0);
  if (send_result < 0)
    return WSAGetLastError();

  // create an accept event
  HANDLE events[2];
  events[0] = cd.close_event;
  events[1] = CreateEvent(NULL, FALSE, FALSE, NULL);
  DEFER([&]() {CloseHandle(events[1]);});

  while (true) {

    WSAOVERLAPPED overlapped;
    ZeroMemory(&overlapped, sizeof(overlapped));
    overlapped.hEvent = events[1];

    WSABUF wsabuf;
    wsabuf.buf = buf;
    wsabuf.len = BUF_SIZE;

    DWORD bytes_read = 0;
    DWORD flags = 0;
    if (WSARecv(cd.socket, &wsabuf, 1, &bytes_read, &flags, &overlapped, NULL) == SOCKET_ERROR) {
      int err = WSAGetLastError();
      if (err != WSA_IO_PENDING)
        return err;
    }

    int idx = WSAWaitForMultipleEvents(ELEMS_IN_ARRAY(events), events, FALSE, INFINITE, FALSE);
    // break if the cancel event was signaled
    if (idx == WSA_WAIT_EVENT_0)
      break;

    if (WSAGetOverlappedResult(cd.socket, &overlapped, &bytes_read, FALSE, &flags) && bytes_read > 0) {
      WebSocketMessageHeader *header = (WebSocketMessageHeader *)buf;

      switch (header->GetOpCode()) {

      case WebSocketMessageHeader::kOpTextMessage: {
        // decode the message, and send it to the app
        int len = (int)header->GetPayloadLength();
        char *decoded = new char[len];
        const char *mask = buf + header->GetMaskOffset();
        const char *data = buf + header->GetPayloadOffset();
        for (int i = 0; i < len; ++i) {
          decoded[i] = data[i] ^ mask[i%4];
        }
        DISPATCHER.invoke(FROM_HERE, 
          threading::kMainThread, 
          std::bind(&App::process_network_msg, &App::instance(), cd.socket, decoded, len));
        break;
      }

      case WebSocketMessageHeader::kOpConnectionClose:
        return 0;

      default:
        LOG_WARNING_LN("Unhandled websocket opcode: %d", header->GetOpCode());
      }
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

  // set socket to non-blocking
  ULONG p = 1;
  ioctlsocket(listen_socket, FIONBIO, &p);

  HANDLE events[] = {
    _cancel_event,
    CreateEvent(NULL, TRUE, FALSE, NULL),
    _callbacks_added
  };

  WSAEventSelect(listen_socket, events[1], FD_ACCEPT);

  while (true) {
    if (listen(listen_socket, SOMAXCONN) == SOCKET_ERROR)
      return WSAGetLastError();

    int res = WaitForMultipleObjects(3, events, FALSE, INFINITE);

    if (res == WAIT_OBJECT_0) {
      // cancel event
      break;

    } else if (res == WAIT_OBJECT_0 + 1) {
      // accept event
      SOCKET client_socket = accept(listen_socket, NULL, NULL);
      if (client_socket != INVALID_SOCKET) {
        DWORD thread_id;
        ClientData cd;
        cd.self = this;
        cd.close_event = _cancel_event;
        cd.socket = client_socket;
        cd.thread = CreateThread(NULL, 0, client_thread, &cd, 0, &thread_id);
        {
          SCOPED_CS(_thread_cs);
          _client_threads.push_back(cd);
        }
      }
      ResetEvent(events[1]);

    } else if (res == WAIT_OBJECT_0 + 2) {
      process_deferred();
    }
  }

  vector<HANDLE> threads;
  for (size_t i = 0; i < _client_threads.size(); ++i)
    threads.push_back(_client_threads[i].thread);

  WaitForMultipleObjects(threads.size(), threads.data(), TRUE, INFINITE);
  return 0;
}

void WebSocketThread::send_msg(SOCKET receiver, const char *msg, int len) {

  vector<char> frame;
  websocket_frame(msg, len, &frame);
  if (receiver == 0) {
    // multicast
    SCOPED_CS(_thread_cs);
    for (size_t i = 0; i < _client_threads.size(); ++i) {
      int res = send(_client_threads[i].socket, frame.data(), frame.size(), 0);
    }

  } else {
    int res = send(receiver, frame.data(), frame.size(), 0);
  }
  delete [] msg;
}

void WebSocketThread::stop() {
  join();
}

bool WebSocketServer::create() {
  KASSERT(!_instance);
  _instance = new WebSocketServer();
  return _instance->_thread.start();
}

bool WebSocketServer::close() {
  KASSERT(_instance);
  _instance->_thread.stop();
  delete exch_null(_instance);
  return true;
}

void WebSocketServer::send_msg(SOCKET receiver, const char *msg, int len) {
  DISPATCHER.invoke(FROM_HERE, threading::kWebSocketThread, 
    std::bind(&WebSocketThread::send_msg, &_thread, receiver, msg, len));
}

WebSocketServer& WebSocketServer::instance() {
  KASSERT(_instance);
  return *_instance;
}

#endif