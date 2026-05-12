#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <string>
#include <cctype>

#include "uci.hpp"
#include "engine.hpp"

using namespace TuffFish;

namespace {
std::string trim_copy(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start])))
        ++start;

    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1])))
        --end;

    return s.substr(start, end - start);
}

std::string lowercase_copy(std::string s) {
    for (char& ch: s)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return s;
}

bool has_legal_move(Position& pos) {
    MoveList moves;
    pos.generate_moves(moves);

    Color moving_color = pos.get_side();
    StoredGameState gs(pos);

    for (Move m: moves) {
        pos.make_move(m);
        bool legal = !pos.is_in_check(moving_color);
        pos.undo_move(m, gs);

        if (legal)
            return true;
    }

    return false;
}

std::string score_string(Score score) {
    std::string score_string;
    const int abs_score = std::abs(score);

    if (abs_score >= MAX_CP && abs_score <= MATE_CP) {
        int mate_dist = (MATE_CP - abs_score + 1) / 2;

        score_string = "M" + std::to_string(mate_dist);
        if (score < 0) 
            score_string = "-" + score_string;
    }
    else
        score_string = std::to_string(std::abs(score / 100.0));

    score_string = (score < 0 ? "-" : "+") + score_string;

    return score_string;

}

void run_console_ui() {
    Bitboards::initialize();
    Evaluate::initialize();
    Zobrist::initialize();

    std::cout << "You are now playing TuffFish " << VERSION << "\n";
    std::cout << "Type moves in UCI format (\"e2e4\").\n";

    std::string side_str;
    Color engine_color;

    while (true) {

        std::cout << "Do you want to play white or black? ";
        if (!std::getline(std::cin, side_str)) {
            std::cout << "\nInput closed. Exiting console mode.\n";
            return;
        }

        side_str = lowercase_copy(trim_copy(side_str));

        if (side_str == "white" || side_str == "w") {

            engine_color = BLACK;
            break;
        }

        else if (side_str == "black" || side_str == "b") {

            engine_color = WHITE;
            break;
        }

        else {
            std::cout << "Please choose white or black.\n";
        }
    }

    Position pos;
    pos.set_up_startpos();

    while (true) {

        std::cout << pos << "\n";

        if (!has_legal_move(pos)) {
            if (pos.is_in_check(pos.get_side()))
                std::cout << "Checkmate.\n";
            else
                std::cout << "Stalemate.\n";
            break;
        }

        if (pos.get_side() == engine_color) {

            ConsoleUISearchOutput info;

            std::atomic_bool searching(true);

            std::thread search_thread([&]() {

                console_ui_search(&info, pos, 1000);

                searching.store(false,
                    std::memory_order_relaxed);
            });

            while (searching.load(std::memory_order_relaxed)) {
                if (info.depth.load(std::memory_order_relaxed) == 0) {
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(100));
                    continue;
                }
                std::cout
                    << "\rDepth: "
                    << info.depth.load(std::memory_order_relaxed)

                    << " Score: "
                    << score_string(info.score.load(std::memory_order_relaxed))

                    << " Nodes: "
                    << info.nodes.load(std::memory_order_relaxed)

                    << " NPS: "
                    << info.nps()
                    << "          "
                    << std::flush;

                std::this_thread::sleep_for(
                    std::chrono::milliseconds(100));
            }

            search_thread.join();

            Move bestmove =
                info.move.load(std::memory_order_relaxed);

            if (bestmove == 0) {
                std::cout << "\nNo legal move available.\n";
                break;
            }

            std::cout
                << "\nTuffFish plays: "
                << algebraic(bestmove)
                << "\n";

            pos.make_move(bestmove);
        }

        else {

            std::string move_str;

            MoveList moves;
            pos.generate_moves(moves);

            while (true) {
                bool legal = false;
                std::cout << "Enter move: ";
                if (!std::getline(std::cin, move_str)) {
                    std::cout << "\nInput closed. Exiting console mode.\n";
                    return;
                }

                move_str = trim_copy(move_str);

                for (Move m: moves) {
                    if (algebraic(m) == move_str) {
                        pos.make_move(m);
                        legal = true;
                        break;
                    }
                }

                if (legal) break;
                else std::cout << "Illegal move. Try again.\n";
            }

        }
    }
}

bool use_console_mode(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--console" || arg == "-c")
            return true;
        if (arg == "--uci")
            return false;
    }

    return false;
}
} // namespace

int main(int argc, char* argv[]) {
    if (use_console_mode(argc, argv)) {
        run_console_ui();
        return 0;
    }

    UCI::loop();
    return 0;
}
