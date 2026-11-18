//	This file is part of the continued Journey MMORPG client
//	Copyright (C) 2015-2019  Daniel Allendorf, Ryan Payton
//
//	This program is free software: you can redistribute it and/or modify
//	it under the terms of the GNU Affero General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU Affero General Public License for more details.
//
//	You should have received a copy of the GNU Affero General Public License
//	along with this program.  If not, see <https://www.gnu.org/licenses/>.
#include "Session.h"

#include "../Configuration.h"
#include "PacketError.h"

namespace ms {
Session::Session(std::unique_ptr<Forwarder> packet_forwarder) :
    packet_forwarder_(std::move(packet_forwarder)),
    length_(0),
    pos_(0),
    is_connected_(false) {}

Session::~Session() {
    if (is_connected_) {
        socket_.close();
    }
}

bool Session::init(const char *host, const char *port) {
    // Connect to the server
    pending_.clear();
    generation_++;
    is_connected_ = socket_.open(host, port);

    if (is_connected_) {
        // Read keys necessary for communicating with the server
        cryptography_ = { socket_.get_buffer() };
    }

    return is_connected_;
}

std::string Session::readHostIP(GLFMDisplay* pApp) {
    std::string filePath = std::string(((ANativeActivity*)glfmGetAndroidActivity(pApp))->externalDataPath) + "/hostip.txt";
    FILE* file = fopen(filePath.c_str(), "rb");
    std::string hostIP;

    if (file != nullptr) {
        char buffer[256]; // Buffer to store read data
        size_t bytesRead = fread(buffer, 1, sizeof(buffer), file);
        fclose(file);

        // Ensure the read operation succeeded
        if (bytesRead > 0) {
            // Convert the buffer to a string
            hostIP.assign(buffer, bytesRead);
        }
    }

    // maple-offline: the server always runs inside the app.
    while (!hostIP.empty() && (hostIP.back() == '\n' || hostIP.back() == '\r' || hostIP.back() == ' '))
        hostIP.pop_back();
    if (hostIP.empty())
        hostIP = "127.0.0.1";
    return hostIP;
}

Error Session::init(GLFMDisplay *pApp) {
//    std::string HOST = Setting<ServerIP>::get().load();
    std::string HOST = readHostIP(pApp);
    std::string PORT = Setting<ServerPort>::get().load();

    if (!init(HOST.c_str(), PORT.c_str())) {
        return Error::CONNECTION;
    }

    return Error::NONE;
}

void Session::reconnect(const char *address, const char *port) {
    // Close the current connection and open a new one
    bool success = socket_.close();

    if (success) {
        init(address, port);
    } else {
        is_connected_ = false;
    }
}

void Session::process(const int8_t *bytes, size_t available) {
    // maple-offline: TCP may split or join packets anywhere, including inside
    // the 4-byte header. Keep every received byte and only cut out packets
    // that are complete; the old code dropped short tails and lost sync.
    pending_.insert(pending_.end(), bytes, bytes + available);

    size_t offset = 0;

    while (pending_.size() - offset >= HEADER_LENGTH) {
        const int8_t *head = pending_.data() + offset;
        size_t length = cryptography_.check_length(head);

        if (length > MAX_PACKET_LENGTH) {
            std::cout << "Invalid packet length " << length << ", dropping stream" << std::endl;
            pending_.clear();
            return;
        }

        if (pending_.size() - offset < HEADER_LENGTH + length) {
            break;
        }

        memcpy(buffer_, head + HEADER_LENGTH, length);
        cryptography_.decrypt(buffer_, length);

        uint32_t generation = generation_;

        try {
            packet_forwarder_->forward(buffer_, length);
        } catch (const PacketError &err) {
            std::cout << err.what() << std::endl;
        }

        if (generation != generation_) {
            // The handler reconnected (e.g. SERVER_IP -> channel server).
            // pending_ now belongs to the new connection; stop here.
            return;
        }

        offset += HEADER_LENGTH + length;
    }

    if (offset > 0) {
        pending_.erase(pending_.begin(), pending_.begin() + offset);
    }
}

void Session::write(int8_t *packet_bytes, size_t packet_length) {
    if (!is_connected_) {
        return;
    }

    int8_t header[HEADER_LENGTH];
    cryptography_.create_header(header, packet_length);
    cryptography_.encrypt(packet_bytes, packet_length);

    socket_.dispatch(header, HEADER_LENGTH);
    socket_.dispatch(packet_bytes, packet_length);
}

void Session::read() {
    // Check if a packet has arrived. Handle if data is sufficient: 4 bytes
    // (header) + 2 bytes (opcode) = 6 bytes.
    size_t result = socket_.receive(&is_connected_);

    if (result > 0) {
        process(socket_.get_buffer(), result);
    }
}

void Session::reconnect() {
    std::string HOST = Setting<ServerIP>::get().load();
    std::string PORT = Setting<ServerPort>::get().load();

    reconnect(HOST.c_str(), PORT.c_str());
}

bool Session::is_connected() const {
    return is_connected_;
}
}  // namespace ms