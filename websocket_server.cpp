#include "stdafx.h"
#if WITH_WEBSOCKETS
#include "websocket_server.hpp"
#include "websocketpp/websocketpp.hpp"
#include "graphics.hpp"
#include "string_utils.hpp"
#include "json_utils.hpp"
#include "demo_engine.hpp"
#include "logger.hpp"

using namespace std;
using namespace websocketpp;

class echo_server_handler : public server::handler {
public:

  static void on_main_thread(connection_ptr con, std::string str) {
    if (str == "REQ:SYSTEM.FPS") {
      auto obj = JsonValue::create_object();
      obj->add_key_value("system.fps", JsonValue::create_number(GRAPHICS.fps()));
      con->send(print_json(obj), frame::opcode::TEXT);
    } else if (str == "REQ:DEMO.INFO") {
      con->send(print_json(DEMO_ENGINE.get_info()), frame::opcode::TEXT);
    } else {
      JsonValue::JsonValuePtr m = parse_json(str.c_str(), str.c_str() + str.size());
      if (m->has_key("msg")) {
        // msg has type and data fields
        auto d = m->get_by_key("msg");
        auto type = d->get_by_key("type")->get_string();
        auto data = d->get_by_key("data");

        if (type == "time") {
          bool playing = data->get_by_key("is_playing")->get_bool();
          int cur_time = data->get_by_key("cur_time")->get_int();
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
}

#endif