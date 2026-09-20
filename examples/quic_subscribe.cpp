// quic_subscribe.cpp
//
// This is a Paho MQTT C++ client, sample application.
//
// This application is an MQTT subscriber using the C++ asynchronous client
// interface over QUIC, employing callbacks to receive messages and status
// updates.
//
// The sample demonstrates:
//  - Connecting to an MQTT server/broker over QUIC (quic://)
//  - Setting SSL/TLS options (QUIC is always TLS-secured)
//  - Subscribing to a topic
//  - Receiving messages through the callback API
//  - Reconnecting and re-subscribing if the connection is lost
//
// Requires the Paho C library built with PAHO_WITH_SSL and PAHO_WITH_QUIC,
// and a QUIC-capable broker such as EMQX on port 14567.
//
// Usage: quic_subscribe [uri] [username] [password]
//
// On a terminal, press Q<Enter> to quit. When stdin is not a TTY (piped
// or /dev/null), the client waits 15 seconds for messages and then exits.
//

/*******************************************************************************
 * Copyright (c) 2026
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v2.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v20.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    QUIC transport example
 *******************************************************************************/

#include <cctype>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#if defined(_WIN32)
    #include <io.h>
    #define MQTTPP_ISATTY(fd) _isatty(fd)
    #define MQTTPP_FILENO(fp) _fileno(fp)
#else
    #include <unistd.h>
    #define MQTTPP_ISATTY(fd) ::isatty(fd)
    #define MQTTPP_FILENO(fp) ::fileno(fp)
#endif

#include "mqtt/async_client.h"

const std::string DFLT_SERVER_URI{"quic://localhost:14567"};
const std::string CLIENT_ID{"quic_subscribe_cpp"};

const std::string TOPIC{"MQTT Examples"};

const int QOS = 1;
const int N_RETRY_ATTEMPTS = 5;
const auto NON_INTERACTIVE_WAIT = std::chrono::seconds(15);

/////////////////////////////////////////////////////////////////////////////

class action_listener : public virtual mqtt::iaction_listener
{
    std::string name_;

    void on_failure(const mqtt::token& tok) override
    {
        std::cout << name_ << " failure";
        if (tok.get_message_id() != 0)
            std::cout << " for token: [" << tok.get_message_id() << "]" << std::endl;
        std::cout << std::endl;
    }

    void on_success(const mqtt::token& tok) override
    {
        std::cout << name_ << " success";
        if (tok.get_message_id() != 0)
            std::cout << " for token: [" << tok.get_message_id() << "]" << std::endl;
        auto top = tok.get_topics();
        if (top && !top->empty())
            std::cout << "\ttoken topic: '" << (*top)[0] << "', ..." << std::endl;
        std::cout << std::endl;
    }

public:
    action_listener(const std::string& name) : name_(name) {}
};

/////////////////////////////////////////////////////////////////////////////

/**
 * Local callback & listener class for use with the client connection.
 * Receives messages and, if the connection is lost, attempts to restore
 * it and re-subscribe to the topic.
 */
class callback : public virtual mqtt::callback, public virtual mqtt::iaction_listener
{
    int nretry_;
    mqtt::async_client& cli_;
    mqtt::connect_options& connOpts_;
    action_listener subListener_;

    void reconnect()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(2500));
        try {
            cli_.connect(connOpts_, nullptr, *this);
        }
        catch (const mqtt::exception& exc) {
            std::cerr << "Error: " << exc.what() << std::endl;
            exit(1);
        }
    }

    void on_failure(const mqtt::token&) override
    {
        std::cout << "Connection attempt failed" << std::endl;
        if (++nretry_ > N_RETRY_ATTEMPTS)
            exit(1);
        reconnect();
    }

    void on_success(const mqtt::token&) override {}

    void connected(const std::string&) override
    {
        std::cout << "Subscribing to topic '" << TOPIC << "' for client " << CLIENT_ID
                  << " using QoS" << QOS << std::endl;

        cli_.subscribe(TOPIC, QOS, nullptr, subListener_);
    }

    void connection_lost(const std::string& cause) override
    {
        std::cout << "\nConnection lost" << std::endl;
        if (!cause.empty())
            std::cout << "\tcause: " << cause << std::endl;

        std::cout << "Reconnecting..." << std::endl;
        nretry_ = 0;
        reconnect();
    }

    void message_arrived(mqtt::const_message_ptr msg) override
    {
        std::cout << "Message arrived" << std::endl;
        std::cout << "\ttopic: '" << msg->get_topic() << "'" << std::endl;
        std::cout << "\tpayload: '" << msg->to_string() << "'\n" << std::endl;
    }

    void delivery_complete(mqtt::delivery_token_ptr) override {}

public:
    callback(mqtt::async_client& cli, mqtt::connect_options& connOpts)
        : nretry_(0), cli_(cli), connOpts_(connOpts), subListener_("Subscription")
    {
    }
};

/////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
    auto serverURI = (argc > 1) ? std::string{argv[1]} : DFLT_SERVER_URI;
    auto userName = (argc > 2) ? std::string{argv[2]} : std::string{};
    auto password = (argc > 3) ? std::string{argv[3]} : std::string{};

    mqtt::async_client cli(serverURI, CLIENT_ID);

    auto sslopts = mqtt::ssl_options_builder()
                       .enable_server_cert_auth(false)
                       .error_handler([](const std::string& msg) {
                           std::cerr << "SSL Error: " << msg << std::endl;
                       })
                       .finalize();

    auto connBuilder = mqtt::connect_options_builder()
                           .clean_session(false)
                           .ssl(std::move(sslopts));

    if (!userName.empty())
        connBuilder.user_name(userName);
    if (!password.empty())
        connBuilder.password(password);

    auto connOpts = connBuilder.finalize();

    callback cb(cli, connOpts);
    cli.set_callback(cb);

    const bool interactive = MQTTPP_ISATTY(MQTTPP_FILENO(stdin));

    try {
        std::cout << "Connecting to the MQTT server '" << serverURI << "' over QUIC..."
                  << std::flush;
        cli.connect(connOpts)->wait();
        std::cout << " OK" << std::endl;
    }
    catch (const mqtt::exception& exc) {
        std::cerr << "\nERROR: Unable to connect to MQTT server: '" << serverURI << "'" << exc
                  << std::endl;
        return 1;
    }

    if (interactive) {
        std::cout << "Press Q<Enter> to quit" << std::endl;
        char ch;
        while (std::cin.get(ch)) {
            if (std::tolower(static_cast<unsigned char>(ch)) == 'q')
                break;
        }
    }
    else {
        std::cout << "stdin is not a terminal; waiting "
                  << NON_INTERACTIVE_WAIT.count() << "s for messages..." << std::endl;
        std::this_thread::sleep_for(NON_INTERACTIVE_WAIT);
    }

    try {
        std::cout << "\nDisconnecting from the MQTT server..." << std::flush;
        cli.disconnect()->wait();
        std::cout << "OK" << std::endl;
    }
    catch (const mqtt::exception& exc) {
        std::cerr << exc << std::endl;
        return 1;
    }

    return 0;
}
