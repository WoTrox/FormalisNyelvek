#include "nfa_to_dfa.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <queue>
#include <algorithm>

void NfaToDfa::initialize_parser(cxxopts::Options &options) {
    options.add_options()
        ("det", "Determinizalni kell az adott automatat");
}

bool NfaToDfa::is_chosen_problem(const cxxopts::ParseResult &args) {
    return args.count("det") > 0;
}

static void trim_cr(std::string &s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) {
        s.pop_back();
    }
}

int NfaToDfa::run(const cxxopts::ParseResult &args) {
    if (args.count("input") == 0 || args.count("output") == 0) {
        std::cerr << "Hianyzik egy vagy tobb kotelezo parameter (input, output)!" << std::endl;
        return 1;
    }

    std::string input_file = args["input"].as<std::string>();
    std::string output_file = args["output"].as<std::string>();

    std::ifstream infile(input_file);
    if (!infile.is_open()) {
        std::cerr << "Nem sikerult megnyitni a bemeneti fajlt: " << input_file << std::endl;
        return 1;
    }

    std::string line;

    // 1. sor: Állapotok
    if (!std::getline(infile, line)) {
        std::cerr << "Hianyos bemeneti fajl: nincsenek allapotok!" << std::endl;
        return 1;
    }
    trim_cr(line);
    std::istringstream iss_states(line);
    std::set<std::string> valid_states;
    std::string state;
    while (iss_states >> state) {
        valid_states.insert(state);
    }

    // 2. sor: Bemeneti ábécé elemei
    if (!std::getline(infile, line)) {
        std::cerr << "Hianyos bemeneti fajl: hianyzik az abece!" << std::endl;
        return 1;
    }
    trim_cr(line);
    std::istringstream iss_alpha(line);
    std::vector<std::string> alphabet;
    std::set<std::string> valid_alphabet;
    std::string sym;
    while (iss_alpha >> sym) {
        if (!valid_alphabet.count(sym)) {
            valid_alphabet.insert(sym);
            alphabet.push_back(sym);
        }
    }
    // Az ábécét rendezzük, hogy a kimenet és az átmenetek ábécé sorrendben legyenek
    std::sort(alphabet.begin(), alphabet.end());

    // 3. sor: Kezdőállapot
    std::string start_state;
    if (!std::getline(infile, start_state)) {
        std::cerr << "Hianyos bemeneti fajl: hianyzik a kezdöallapot!" << std::endl;
        return 1;
    }
    trim_cr(start_state);
    std::istringstream iss_start(start_state);
    if (!(iss_start >> start_state)) {
        std::cerr << "Ervenytelen kezdöallapot!" << std::endl;
        return 1;
    }
    if (!valid_states.count(start_state)) {
        std::cerr << "A kezdöallapot nem szerepel az ervenyes allapotok kozott: " << start_state << std::endl;
        return 1;
    }

    // 4. sor: Végállapotok
    if (!std::getline(infile, line)) {
        std::cerr << "Hianyos bemeneti fajl: hianyoznak a vegallapotok!" << std::endl;
        return 1;
    }
    trim_cr(line);
    std::istringstream iss_final(line);
    std::set<std::string> nfa_final_states;
    while (iss_final >> state) {
        if (!valid_states.count(state)) {
            std::cerr << "A vegallapot nem szerepel az ervenyes allapotok kozott: " << state << std::endl;
            return 1;
        }
        nfa_final_states.insert(state);
    }

    // További sorok: Átmenetek
    std::map<std::string, std::map<std::string, std::set<std::string>>> nfa_transitions;
    while (std::getline(infile, line)) {
        trim_cr(line);
        if (line.empty()) continue;
        std::istringstream trans_iss(line);
        std::string from_state, trans_sym, to_state;
        if (trans_iss >> from_state >> trans_sym >> to_state) {
            // Ellenőrzés: állapotok és jelek valóban szerepelnek-e az érvényes elemek között
            if (!valid_states.count(from_state)) {
                std::cerr << "Ervenytelen forrasallapot az atmenetben: " << from_state << std::endl;
                return 1;
            }
            if (!valid_alphabet.count(trans_sym)) {
                std::cerr << "Ervenytelen jel az atmenetben: " << trans_sym << std::endl;
                return 1;
            }
            if (!valid_states.count(to_state)) {
                std::cerr << "Ervenytelen celallapot az atmenetben: " << to_state << std::endl;
                return 1;
            }
            nfa_transitions[from_state][trans_sym].insert(to_state);
        }
    }
    infile.close();

    // Determinizálás részhalmaz-konstrukcióval (BFS)
    std::vector<std::set<std::string>> dfa_states;
    std::map<std::set<std::string>, int> state_to_id;
    std::vector<std::map<std::string, int>> dfa_transitions;

    std::set<std::string> start_set = {start_state};
    dfa_states.push_back(start_set);
    state_to_id[start_set] = 0;
    dfa_transitions.emplace_back();

    std::queue<int> q;
    q.push(0);

    while (!q.empty()) {
        int u = q.front();
        q.pop();

        const auto current_set = dfa_states[u];

        for (const auto &c_sym : alphabet) {
            std::set<std::string> target_set;
            for (const auto &st : current_set) {
                auto it_st = nfa_transitions.find(st);
                if (it_st != nfa_transitions.end()) {
                    auto it_sym = it_st->second.find(c_sym);
                    if (it_sym != it_st->second.end()) {
                        for (const auto &dest : it_sym->second) {
                            target_set.insert(dest);
                        }
                    }
                }
            }

            if (!target_set.empty()) {
                int v;
                auto it_find = state_to_id.find(target_set);
                if (it_find == state_to_id.end()) {
                    v = static_cast<int>(dfa_states.size());
                    dfa_states.push_back(target_set);
                    state_to_id[target_set] = v;
                    dfa_transitions.emplace_back();
                    q.push(v);
                } else {
                    v = it_find->second;
                }
                dfa_transitions[u][c_sym] = v;
            }
        }
    }

    // Eredmény kiírása
    std::ofstream outfile(output_file);
    if (!outfile.is_open()) {
        std::cerr << "Nem sikerult megnyitni a kimeneti fajlt: " << output_file << std::endl;
        return 1;
    }

    // 1. sor: Állapotok s-sel jelölve
    for (size_t i = 0; i < dfa_states.size(); ++i) {
        if (i > 0) outfile << " ";
        outfile << "s" << i;
    }
    outfile << "\n";

    // 2. sor: Bemeneti ábécé elemei
    for (size_t i = 0; i < alphabet.size(); ++i) {
        if (i > 0) outfile << " ";
        outfile << alphabet[i];
    }
    outfile << "\n";

    // 3. sor: Kezdőállapot
    outfile << "s0\n";

    // 4. sor: Végállapotok
    bool first_final = true;
    for (size_t i = 0; i < dfa_states.size(); ++i) {
        bool is_final = false;
        for (const auto &st : dfa_states[i]) {
            if (nfa_final_states.count(st)) {
                is_final = true;
                break;
            }
        }
        if (is_final) {
            if (!first_final) outfile << " ";
            outfile << "s" << i;
            first_final = false;
        }
    }
    outfile << "\n";

    // További sorok: Átmenetek ábécé sorrendben
    // Sorrend: előbb a csomópontok szerint (s0, s1, ...), majd az él címkéje szerint
    for (size_t u = 0; u < dfa_states.size(); ++u) {
        for (const auto &c_sym : alphabet) {
            auto it = dfa_transitions[u].find(c_sym);
            if (it != dfa_transitions[u].end()) {
                outfile << "s" << u << " " << c_sym << " s" << it->second << "\n";
            }
        }
    }
    outfile.close();

    return 0;
}
