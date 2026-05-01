#pragma once

#include "types.hpp"
#include "bitboards.hpp"
#include "evaluate.hpp"
#include "engine.hpp"

#include <functional>
#include <unordered_map>
#include <thread>

namespace TuffChess {
namespace UCI {

    inline void info_string(const std::string& msg) {
        std::cout << "info string " << msg << std::endl;
    }
    inline void info_depth(int depth, Score score, uint64_t nodes, uint64_t elapsed, MoveList& pv) {
        std::string score_string;
        if (std::abs(score) > MAX_CP) {
            int mate_dist = (MATE_CP - std::abs(score) + 1) / 2;

            if (score < 0) 
                mate_dist = -mate_dist;

            score_string = "mate " + std::to_string(mate_dist);
        }
        else
            score_string = "cp " + std::to_string(score);

        elapsed = std::max<uint64_t>(elapsed, 1);

        std::cout << "info depth " << depth << " score " << score_string << " nodes " << nodes << " nps " << (nodes * 1000 / elapsed) << " time " << elapsed << " pv ";

        for (Move m: pv) {
            std::cout << algebraic(m) << " ";
        }

        std::cout << std::endl;
    }

    namespace {

        Position position;
        std::thread search_thread;

        void handle_uci() {
            std::cout << "id name TuffChess v0.9.0\n" <<
                         "id author GoobusTheNoobus\n" <<
                         std::endl;    
        }
        void handle_go(std::istringstream& iss) {

            request_stop();
            if (search_thread.joinable()) search_thread.join();

            std::string token;

            int depth = 32;
            int movetime = 0;

            int winc = 0;
            int binc = 0;
            int wtime = 0;
            int btime = 0;
            
            while (iss >> token) {
                if (token == "depth") {
                    iss >> depth;
                } else if (token == "movetime") {
                    iss >> movetime;
                } else if (token == "wtime") {
                    iss >> wtime;
                } else if (token == "btime") {
                    iss >> btime;
                } else if (token == "winc") {
                    iss >> winc;
                } else if (token == "binc") {
                    iss >> binc;
                } else if (token == "perft") {
                    iss >> depth;

                    perft_divide(position, depth);

                    return;
                }
            }

            int time_limit = 0;

            // time_limit = 10000;

            // Movetime parameter has priority over other
            if (movetime > 0) {
                time_limit = movetime;
                
            }

            else if (wtime > 0 || btime > 0) {
                int our_time = position.get_side() == WHITE ? wtime : btime;
                int our_inc  = position.get_side() == WHITE ? winc : binc;

                time_limit = std::min(our_time / 20 + our_inc / 2, our_time);
                time_limit = std::min(time_limit, 10000);

                
            }

            if (depth < 1) depth = MAX_PLY;
            if (depth > MAX_PLY) depth = MAX_PLY;

            search_thread = std::thread([depth, time_limit](){
                search(position, depth, time_limit);
            });
        }
        void handle_position(std::istringstream& iss) {
            request_stop();
            if (search_thread.joinable()) search_thread.join();

            std::string token;

            iss >> token;

            if (token == "startpos") {
                position.parse_fen(START_FEN);

                iss >> token;
            } else if (token == "fen") {
                std::string fen;

                while ((iss >> token) && token != "moves") {
                    fen += token + " ";
                }

                position.parse_fen(fen);
                iss >> token;
            }

            if (token == "moves") {
                while (iss >> token) {
                    position.make_move(token);
                }
            }
        }
        void handle_isready() {
            std::cout << "readyok" << std::endl;
        }


        std::unordered_map<std::string, std::function<void(std::istringstream&)>> handlers = {
            {"uci", [](std::istringstream&) {handle_uci(); }},
            {"isready", [](std::istringstream&){handle_isready(); }},
            {"go", [](std::istringstream& iss){handle_go(iss); }},
            {"position", [](std::istringstream& iss){handle_position(iss); }}
        };
    }

    inline void loop() {
        Bitboards::initialize();
        Evaluate::initialize();

        while (true) {
            std::string command;
            std::getline(std::cin, command);
            std::istringstream iss(command);
            std::string token;

            if (command.find_first_not_of(" ") == std::string::npos) continue;

            iss >> token;

            auto it = handlers.find(token);
            if (it != handlers.end()) it->second(iss);
            else {
                if (token == "quit") {
                    request_stop();
                    if (search_thread.joinable()) search_thread.join();
                    break;
                } else if (token == "stop") {
                    request_stop();
                    if (search_thread.joinable()) search_thread.join();
                }
                else {
                    continue;
                }
            }

        }
    }
}
}


