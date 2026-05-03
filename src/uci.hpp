#pragma once

#include "types.hpp"
#include "bitboards.hpp"
#include "evaluate.hpp"
#include "engine.hpp"

#include <thread>

namespace TuffFish {
namespace UCI {

    

    inline void info_string(const std::string& msg) {
        std::cout << "info string " << msg << std::endl;
    }
    inline void info_depth(int depth, Score score, uint64_t nodes, uint64_t elapsed, MoveList& pv) {
        std::string score_string;
        if (std::abs(score) >= MAX_CP) {
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

        inline void stop() {
            request_stop();
            if (search_thread.joinable()) {
                info_string("joining search thread");
                search_thread.join();
                info_string("joined search thread");
            }
        }

        void handle_uci() {
            std::cout << "id name TuffChess v0.9.0\n" <<
                         "id author GoobusTheNoobus\n" <<
                         "\nuciok" <<
                         std::endl;    
        }
        void handle_go(std::istringstream& iss) {

            stop();

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
                

                
            }

            if (depth < 1) depth = MAX_PLY;
            if (depth > MAX_PLY) depth = MAX_PLY;

            search_thread = std::thread([depth, time_limit]() mutable {
                Position copy = position;
                try {
                    search(copy, depth, time_limit);
                } catch (const std::exception& e) {
                    std::cout << "Search crashed: " << e.what() << std::endl;
                } catch (...) {
                    std::cout << "Search crashed due to unknown causes" << std::endl;
                }
                
            });
        }
        void handle_position(std::istringstream& iss) {
            stop();

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
        void ucinewgame() {
            
        }


        inline void dispatch(const std::string& cmd, std::istringstream& iss) {
            if (cmd == "uci") {
                handle_uci();
            }
            else if (cmd == "isready") {
                handle_isready();
            }
            else if (cmd == "go") {
                handle_go(iss);
            }
            else if (cmd == "position") {
                handle_position(iss);
            }
            else if (cmd == "ucinewgame") {
                ucinewgame();
            }
            else if (cmd == "d") {
                std::cout << position << std::endl;
            }
            else {
                info_string("invalid command");
            }
        }
    }

    inline void loop() {
        Bitboards::initialize();
        Evaluate::initialize();

        while (true) {
            std::string command;
            if (!std::getline(std::cin, command)) {
                stop();
                break;
            }
            std::istringstream iss(command);
            std::string token;

            if (command.find_first_not_of(" ") == std::string::npos) continue;

            iss >> token;
            if (token == "quit") {
                stop();
                break;
            } else if (token == "stop") {
                stop();
                continue;
            }

            dispatch(token, iss);
        }
    }
}
}


