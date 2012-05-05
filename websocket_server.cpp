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

class echo_server_handler : public server::handler {
public:
  void on_message(connection_ptr con, message_ptr msg) {
    const string &str = msg->get_payload();
    if (str == "SYSTEM.FPS") {
      auto obj = JsonValue::create_object();
      obj->add_key_value("system.fps", JsonValue::create_number(GRAPHICS.fps()));
      con->send(obj->print(4), frame::opcode::TEXT);
    } else if (str == "DEMO.INFO") {
      con->send(DEMO_ENGINE.get_info(), frame::opcode::TEXT);
      //con->send(msg->get_payload(),msg->get_opcode());
    } else {
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