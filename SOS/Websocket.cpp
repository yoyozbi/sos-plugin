#include "SOS.h"
#include "json.hpp"
#include "utils/parser.h"


// SEND EVENTS
void SOS::SendEvent(std::string eventName, const json::JSON &jsawn)
{
    json::JSON event;
    event["event"] = eventName;
    event["data"] = jsawn;
    SendWebSocketPayload(event.dump());
}

// WEBSOCKET CODE
void SOS::RunWsServer()
{
    cvarManager->log("[SOS] Starting WebSocket server");

    ws_connections = new ConnectionSet();
    ws_server = new PluginServer();
    
    cvarManager->log("[SOS] Starting asio");
    ws_server->init_asio();
    ws_server->set_open_handler(websocketpp::lib::bind(&SOS::OnWsOpen, this, _1));
    ws_server->set_close_handler(websocketpp::lib::bind(&SOS::OnWsClose, this, _1));
    ws_server->set_message_handler(websocketpp::lib::bind(&SOS::OnWsMsg, this, _1, _2));
    ws_server->set_http_handler(websocketpp::lib::bind(&SOS::OnHttpRequest, this, _1));
    
    cvarManager->log("[SOS] Starting listen on port 49122");
    ws_server->listen(*cvarPort);
    
    cvarManager->log("[SOS] Starting accepting connections");
    ws_server->start_accept();
    ws_server->run();
}

void SOS::StopWsServer()
{
    if (ws_server)
    {
        ws_server->stop();
        ws_server->stop_listening();
        delete ws_server;
        ws_server = nullptr;
    }

    if (ws_connections)
    {
        ws_connections->clear();
        delete ws_connections;
        ws_connections = nullptr;
    }
}

void SOS::OnWsMsg(connection_hdl hdl, PluginServer::message_ptr msg)
{
    this->SendWebSocketPayload(msg->get_payload());
}

void SOS::OnHttpRequest(websocketpp::connection_hdl hdl)
{
    PluginServer::connection_ptr connection = ws_server->get_con_from_hdl(hdl);
    connection->append_header("Content-Type", "application/json");
    connection->append_header("Server", "SOS/1.4.0");

    if (connection->get_resource() == "/init")
    {
        json::JSON data;
        data["event"] = "init";
        data["data"] = "json here";

        connection->set_body(data.dump());
        connection->set_status(websocketpp::http::status_code::ok);

        return;
    }

    connection->set_body("Not found");
    connection->set_status(websocketpp::http::status_code::not_found);
}

void SOS::SendWebSocketPayload(std::string payload)
{
    // broadcast to all connections
    try
    {
        std::string output = payload;

        if (*cvarUseBase64)
        {
            output = base64_encode((const unsigned char*)payload.c_str(), (unsigned int)payload.size());
        }

        for (const connection_hdl& it : *ws_connections)
        {
            ws_server->send(it, output, websocketpp::frame::opcode::text);
        }
    }
    catch (std::exception e)
    {
        cvarManager->log("An error occured sending websocket event: " + std::string(e.what()));
    }
}
