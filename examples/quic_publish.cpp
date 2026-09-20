// quic_publish.cpp
//
// This is a Paho MQTT C++ client, sample application.
//
// It's an example of how to connect to an MQTT broker over QUIC, and then
// send messages as an MQTT publisher using the C++ asynchronous client
// interface.
//
// The sample demonstrates:
//  - Connecting to an MQTT server/broker over QUIC (quic://)
//  - Setting SSL/TLS options (QUIC is always TLS-secured)
//  - Publishing messages
//  - Using asynchronous tokens
//
// Requires the Paho C library built with PAHO_WITH_SSL and PAHO_WITH_QUIC,
// and a QUIC-capable broker such as EMQX on port 14567.
//
// Usage: quic_publish [uri] [username] [password]
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

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

#include "mqtt/async_client.h"

const std::string DFLT_SERVER_URI{"quic://localhost:14567"};
const std::string DFLT_CLIENT_ID{"quic_publish_cpp"};

const std::string TOPIC{"MQTT Examples"};
const std::string PAYLOAD{"Hello World!"};

const int QOS = 1;
const auto TIMEOUT = std::chrono::seconds(10);

/////////////////////////////////////////////////////////////////////////////

class callback : public virtual mqtt::callback
{
public:
    void connection_lost(const std::string& cause) override
    {
        std::cout << "\nConnection lost" << std::endl;
        if (!cause.empty())
            std::cout << "\tcause: " << cause << std::endl;
    }

    void delivery_complete(mqtt::delivery_token_ptr tok) override
    {
        std::cout << "\tDelivery complete for token: " << (tok ? tok->get_message_id() : -1)
                  << std::endl;
    }
};

/////////////////////////////////////////////////////////////////////////////

using namespace std;

int main(int argc, char* argv[])
{
    string serverURI = (argc > 1) ? string{argv[1]} : DFLT_SERVER_URI;
    string userName = (argc > 2) ? string{argv[2]} : string{};
    string password = (argc > 3) ? string{argv[3]} : string{};

    cout << "Initializing for server '" << serverURI << "'..." << endl;
    mqtt::async_client client(serverURI, DFLT_CLIENT_ID);

    callback cb;
    client.set_callback(cb);

    // QUIC is always TLS-secured. Server certificate verification is
    // disabled here for a local/dev broker; set a trust store for
    // production.
    auto sslopts = mqtt::ssl_options_builder()
                       .enable_server_cert_auth(false)
                       .error_handler([](const string& msg) {
                           cerr << "SSL Error: " << msg << endl;
                       })
                       .finalize();

    auto connBuilder = mqtt::connect_options_builder().clean_session().ssl(std::move(sslopts));

    if (!userName.empty())
        connBuilder.user_name(userName);
    if (!password.empty())
        connBuilder.password(password);

    auto connopts = connBuilder.finalize();

    cout << "  ...OK" << endl;

    try {
        cout << "\nConnecting over QUIC..." << endl;
        mqtt::token_ptr conntok = client.connect(connopts);
        cout << "Waiting for the connection..." << endl;
        conntok->wait();
        cout << "  ...OK" << endl;

        cout << "\nSending message..." << endl;
        auto msg = mqtt::make_message(TOPIC, PAYLOAD, QOS, false);
        client.publish(msg)->wait_for(TIMEOUT);
        cout << "  ...OK" << endl;

        cout << "\nDisconnecting..." << endl;
        client.disconnect()->wait();
        cout << "  ...OK" << endl;
    }
    catch (const mqtt::exception& exc) {
        cerr << exc.what() << endl;
        return 1;
    }

    return 0;
}
