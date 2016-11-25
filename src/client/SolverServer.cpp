//
// Created by Matteo on 22/07/16.
//

#include <unistd.h>
#include <thread>
#include <chrono>
#include "lib/Logger.h"
#include "SolverServer.h"


SolverServer::SolverServer(const net::Address &server) :
        net::Server(),
        server(server),
        solver(nullptr) {
    std::map<std::string, std::string> header;
    header["solver"] = SolverProcess::solver;
    this->server.write(header, "");
    this->add_socket(&this->server);
}

SolverServer::~SolverServer() {
}

void SolverServer::log(uint8_t level, std::string message, std::map<std::string, std::string> *header_solver) {
    //    std::map<std::string, std::string> header;
    //    if (header_solver != nullptr) {
    //        for(auto &pair:header_solver)
    //        header["command"] = "log";
    //        header["level"] = std::to_string(level);
    //        try {
    //            this->server.write(header, message);
    //        } catch (SocketException) { }
    //    }
    if (message.find("\n") != std::string::npos) {
        ::replace(message, "\n", "\n  ");
        message = std::string("\n") + message;
    }
    Logger::log(level, (this->solver ? this->solver->header["name"] + this->solver->header["node"] + ": " : "") +
                       message);
}


bool SolverServer::check_header(std::map<std::string, std::string> &header) {
    if (this->solver == nullptr)
        return false;
    return header["name"] == this->solver->header["name"] && header["node"] == this->solver->header["node"];
}


void SolverServer::handle_close(net::Socket &socket) {
    if (&socket == &this->server) {
        this->log(Logger::INFO, "server closed the connection");
        this->stop_solver();
    } else if (this->solver && &socket == this->solver->reader()) {
        this->log(Logger::ERROR, "solver quit unexpectedly");
        this->solver->header["error"] = "unexpected quit";
        this->server.write(this->solver->header, "");
        this->stop_solver();
    }
}

void SolverServer::handle_exception(net::Socket &socket, net::SocketException &exception) {
    this->log(Logger::WARNING, exception.what());
}

void SolverServer::stop_solver() {
    if (!this->solver)
        return;
    this->del_socket(this->solver->reader());
    delete this->solver;
    this->solver = nullptr;
}

void SolverServer::update_lemmas() {
    if (!this->solver)
        return;
    auto header = this->solver->header;
    header["command"] = "local";
    header["local"] = "lemma_server";
    header["lemma_server"] = this->lemmas_address;
    this->solver->writer()->write(header, "");
}


void
SolverServer::handle_message(net::Socket &socket, std::map<std::string, std::string> &header, std::string &payload) {
    if (socket.get_fd() == this->server.get_fd()) {
        if (header.count("command") != 1) {
            this->log(Logger::WARNING, "unexpected message from server without command");
            return;
        }
        if (header["command"] == "lemmas" && header.count("lemmas") == 1) {
            this->log(Logger::INFO, "new lemma server address: " + header["lemmas"]);
            this->lemmas_address = header["lemmas"];
            this->update_lemmas();
        } else if (header["command"] == "solve") {
            if (this->check_header(header)) {
                return;
            }
            this->stop_solver();
            header.erase("command");
            this->solver = new SolverProcess(header, payload);
            this->update_lemmas();
            this->add_socket(this->solver->reader());
            this->log(Logger::INFO, "start");
        } else if (header["command"] == "stop") {
            if (!this->check_header(header)) {
                return;
            }
            this->log(Logger::INFO, "stop");
            this->stop_solver();
        } else if (this->check_header(header)) {
            this->log(Logger::INFO, header["command"]);
            this->solver->writer()->write(header, payload);
        }
    } else if (this->solver && &socket == this->solver->reader()) {
        if (header.count("status")) {
            this->log(Logger::INFO, std::string("status: ") + header["status"]);
        }
        if (header.count("info")) {
            this->log(Logger::INFO, header["info"]);
            header.erase("info");
        }
        if (header.count("warning")) {
            this->log(Logger::WARNING, header["warning"]);
            header.erase("warning");
        }
        if (header.count("error")) {
            this->log(Logger::ERROR, header["error"]);
            header.erase("error");
        }
        //pprint(header);
        this->server.write(header, payload);
        this->solver->header = header;
    }
}
