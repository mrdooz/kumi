#include "stdafx.h"
#if WITH_WEBSOCKETS
#include "websocket_server.hpp"
#include "websocketpp/websocketpp.hpp"
#include "graphics.hpp"
#include "string_utils.hpp"
#include "json_writer.hpp"
#include "demo_engine.hpp"
#include "logger.hpp"

using namespace std;
using namespace websocketpp;

const char *skip_whitespace(const char *start, const char *end) {
  for (int i = 0; i < end - start; ++i) {
    if (!isspace((int)start[i]))
      return start + i;
  }
  return nullptr;
}

const char *skip_delim(const char *start, const char *end, char delim) {
  while (start != end) {
    if (*start++ == delim) {
      while (*start == delim && start != end)
        start++;
      return start == end ? nullptr : start;
    }
  }
  return nullptr;
}

bool between_delim(const char *start, const char *end, char delim, const char **key_start, const char **key_end) {
  int i;
  int len = end - start;
  for (i = 0; i < len; ++i) {
    if (start[i] == delim) {
      ++i;
      *key_start = start + i;
      for (; i < len; ++i) {
        if (start[i] == delim) {
          *key_end = start + i;
          return true;
        }
      }
    }
  }
  return false;
}

JsonValue::JsonValuePtr parse_json(const char *start, const char *end) {
  const char *s = skip_whitespace(start, end);

  switch (*s) {
    case '{': {
      if (s = skip_whitespace(s + 1, end)) {
        const char *key_start, *key_end;
        if (between_delim(s, end, '"', &key_start, &key_end)) {
          string key(key_start, key_end - key_start);
          s = skip_delim(key_end, end, ':');
          auto a = JsonValue::create_object();
          a->add_key_value(key, parse_json(s, end));
          return a;
        }

      }
    }

      break;
    case '[':
      break;
  }

  return JsonValue::JsonValuePtr();
}

class echo_server_handler : public server::handler {
public:
  void on_message(connection_ptr con, message_ptr msg) {
    const string &str = msg->get_payload();
    if (str == "SYSTEM.FPS") {
      auto obj = JsonValue::create_object();
      obj->add_key_value("system.fps", JsonValue::create_number(GRAPHICS.fps()));
      JsonWriter w;
      con->send(w.print(obj), frame::opcode::TEXT);
    } else if (str == "DEMO.INFO") {
      con->send(DEMO_ENGINE.get_info(), frame::opcode::TEXT);
      //con->send(msg->get_payload(),msg->get_opcode());
    } else {
      JsonValue::JsonValuePtr a = parse_json(str.c_str(), str.c_str() + str.size());
      LOG_WARNING_LN("Unknown websocket message: %s", str);
    }
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
}

#endif