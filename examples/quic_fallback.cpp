// quic_fallback.cpp
//
// This is a Paho MQTT C++ client, sample application.
//
// MQTT over QUIC with fallback to TLS, using connect_options::servers().
// The client tries quic:// first and falls back to ssl:// when the QUIC
// connection cannot be established (for example if UDP is blocked).
//
// A blocked or dead QUIC port fails by timeout, not immediately: expect
// up to connect_timeout per URI before fallback. Setting an explicit MQTT
// version avoids a second attempt per URI with MQTT 3.1.
//
// The sample demonstrates:
//  - Connecting with an ordered list of server URIs
//  - QUIC-first, TLS fallback
//  - Publishing a message after whichever URI succeeds
//
// Requires the Paho C library built with PAHO_WITH_SSL and PAHO_WITH_QUIC.
//
// Usage: quic_fallback [quic_uri] [ssl_uri] [username] [password]
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

const std::string DFLT_QUIC_URI{"quic://localhost:14567"};
const std::string DFLT_SSL_URI{"ssl://localhost:8883"};
const std::string DFLT_CLIENT_ID{"quic_fallback_cpp"};

const std::string TOPIC{"MQTT Examples"};
const std::string PAYLOAD{"Hello World!"};

const int QOS = 1;
const auto TIMEOUT = std::chrono::seconds(10);

using namespace std;
using namespace std::chrono;

/////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
    string quicURI = (argc > 1) ? string{argv[1]} : DFLT_QUIC_URI;
    string sslURI = (argc > 2) ? string{argv[2]} : DFLT_SSL_URI;
    string userName = (argc > 3) ? string{argv[3]} : string{};
    string password = (argc > 4) ? string{argv[4]} : string{};

    cout << "Trying " << quicURI << ", falling back to " << sslURI << endl;

    // Create against the first URI; servers() supplies the full try-list.
    mqtt::async_client client(quicURI, DFLT_CLIENT_ID);

    auto sslopts = mqtt::ssl_options_builder()
                       .enable_server_cert_auth(false)
                       .error_handler([](const string& msg) {
                           cerr << "SSL Error: " << msg << endl;
                       })
                       .finalize();

    auto servers = mqtt::string_collection::create({quicURI, sslURI});

    auto connBuilder = mqtt::connect_options_builder()
                           .mqtt_version(MQTTVERSION_3_1_1)
                           .connect_timeout(10s)
                           .clean_session()
                           .servers(servers)
                           .ssl(std::move(sslopts));

    if (!userName.empty())
        connBuilder.user_name(userName);
    if (!password.empty())
        connBuilder.password(password);

    auto connopts = connBuilder.finalize();

    try {
        cout << "\nConnecting..." << endl;
        auto rsp = client.connect(connopts)->get_connect_response();
        cout << "  ...OK via " << rsp.get_server_uri() << endl;

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
