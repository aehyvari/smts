//
// Created by Matteo on 21/07/16.
//

#ifndef CLAUSE_SHARING_PROCESSSOLVER_H
#define CLAUSE_SHARING_PROCESSSOLVER_H

#include <atomic>
#include <mutex>
#include "lib/lib.h"


enum Status {
    unknown, sat, unsat
};

struct Task {
    const enum {
        incremental, resume
    } command;
    std::string smtlib;
    uint8_t partitions;
};


class SolverProcess : public Process {
private:
    std::shared_ptr<net::Socket> lemmas;
    std::mutex lemmas_mtx;
    static const uint8_t lemmas_errors_max = 3;
    uint8_t lemmas_errors;

    net::Pipe pipe;
    std::string instance;

    void main() {
        this->init();
        this->report();
        std::thread t([&] {
            std::map<std::string, std::string> header;
            std::string payload;
            while (true) {
                // receives both commands from the server(forwarded by SolverServer) and from SolverServer.
                // if command=local the answer is built here and delivered to SolverServer
                // otherwise the solver is interrupted and the message forwarder through pipe
                this->reader()->read(header, payload);
                if (!header.count("command"))
                    continue;
                if (header["command"] == "local" && header.count("local")) {
                    if (header["local"] == "report")
                        this->report();
                    else if (header["local"] == "lemma_server" && header.count("lemma_server")) {
                        std::lock_guard<std::mutex> lock(this->lemmas_mtx);
                        if (!header["lemma_server"].size())
                            this->lemmas.reset();
                        else
                            try {
                                this->lemmas.reset(new net::Socket(header["lemma_server"]));
                            } catch (net::SocketException &ex) {
                                header = this->header;
                                header["error"] = std::string("lemma server connection failed: ") + ex.what();
                                this->report(header, "");
                            }
                    }
                    continue;
                }
                this->interrupt();
                this->pipe.writer()->write(header, payload);
            }
        });
        this->solve();
    }

    // here the module can edit class fields
    void init();

    // class field are read only
    void solve();


    void partition(uint8_t);

    // async interrupt the solver
    void interrupt();

    void report(const std::map<std::string, std::string> header, const std::string payload) {
        this->writer()->write(header, payload);
    }

    void report() {
        this->report(this->header, "");
    }

    void report(Status status) {
        auto header = this->header;
        if (status == Status::sat)
            header["status"] = "sat";
        else if (status == Status::unsat)
            header["status"] = "unsat";
        else
            header["status"] = "unknown";
        this->report(header, "");

    }

    void report(const std::vector<std::string> &partitions, const char *error = nullptr) {
        auto header = this->header;
        std::stringstream payload;
        if (error != nullptr)
            header["error"] = error;
        ::join(payload, "\n", partitions);
        header["partitions"] = std::to_string(partitions.size());
        this->report(header, payload.str());
    }

    Task wait() {
        std::map<std::string, std::string> header;
        std::string payload;
        this->pipe.reader()->read(header, payload);
        if (header["command"] == "incremental") {
            return Task{
                    .command=Task::incremental,
                    .smtlib=payload
            };
        }
        if (header["command"] == "partition" && header.count("partitions") == 1) {
            this->partition((uint8_t) atoi(header["partitions"].c_str()));
        }
        return Task{
                .command=Task::resume
        };
    }

    void lemma_push(const std::vector<net::Lemma> &lemmas) {
        if (!is_sharing() || lemmas.size() == 0 || this->lemmas_errors >= SolverProcess::lemmas_errors_max)
            return;

        std::lock_guard<std::mutex> _l(this->lemmas_mtx);

        auto header = this->header;
        std::stringstream payload;

        payload << lemmas;

        header["separator"] = "a";
        header["lemmas"] = std::to_string(lemmas.size());

        try {
            this->lemmas->write(header, payload.str());
        } catch (net::SocketException &ex) {
            this->lemmas_errors++;
            header["error"] = std::string("lemma push failed: ") + ex.what();
            this->report(header, "");
        }
    }

    void lemma_pull(std::vector<net::Lemma> &lemmas) {
        if (!is_sharing() || this->lemmas_errors >= SolverProcess::lemmas_errors_max)
            return;

        std::lock_guard<std::mutex> _l(this->lemmas_mtx);

        auto header = this->header;
        std::string payload;

        try {
            this->lemmas->write(header, "");
            this->lemmas->read(header, payload, 2000);
        } catch (net::SocketException &ex) {
            this->lemmas_errors++;
            header["error"] = std::string("lemma pull failed: ") + ex.what();
            this->report(header, "");
            return;
        } catch (net::SocketTimeout &) {
            this->lemmas_errors++;
            header["warning"] = "lemma pull failed: timeout";
            this->report(header, "");
            return;
        }

        if (header["name"] != this->header["name"]
            || header["node"] != this->header["node"]
            || header.count("separator") == 0)
            return;

        std::istringstream is(payload);
        is >> lemmas;
    }

public:
    SolverProcess(std::map<std::string, std::string> header,
                  std::string instance) :
            lemmas_errors(0),
            instance(instance),
            header(header) {
        this->start();
    }

    std::map<std::string, std::string> header;

    bool is_sharing() {
        return this->lemmas != nullptr;
    }

    static const char *solver;
};

#endif //CLAUSE_SHARING_PROCESSSOLVER_H
