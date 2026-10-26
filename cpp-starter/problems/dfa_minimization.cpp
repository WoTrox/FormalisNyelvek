#include "dfa_minimization.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <queue>
#include <algorithm>
#include <cctype>

void DfaMinimization::initialize_parser(cxxopts::Options &options) {
    options.add_options()
        ("mini", "Determinisztikus automata minimalizalasa");
}

bool DfaMinimization::is_chosen_problem(const cxxopts::ParseResult &args) {
    return args.count("mini") > 0;
}

static void trim_cr(std::string &s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) {
        s.pop_back();
    }
}

static std::string to_s_name(const std::string &q_name) {
    if (!q_name.empty() && q_name[0] == 'q') {
        return "s" + q_name.substr(1);
    }
    if (!q_name.empty() && q_name[0] == 's') {
        return q_name;
    }
    return "s" + q_name;
}

static bool state_cmp(const std::string &a, const std::string &b) {
    size_t ia = 0;
    while (ia < a.size() && !isdigit(static_cast<unsigned char>(a[ia]))) ia++;
    size_t ib = 0;
    while (ib < b.size() && !isdigit(static_cast<unsigned char>(b[ib]))) ib++;

    if (ia > 0 && ib > 0 && a.substr(0, ia) == b.substr(0, ib) && ia < a.size() && ib < b.size()) {
        try {
            long long na = std::stoll(a.substr(ia));
            long long nb = std::stoll(b.substr(ib));
            if (na != nb) return na < nb;
        } catch (...) {
            // Ha túl nagy szám lenne, esik vissza lexikografikusra
        }
    }
    return a < b;
}

int DfaMinimization::run(const cxxopts::ParseResult &args) {
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
    std::vector<std::string> states;
    std::set<std::string> valid_states;
    std::string state;
    while (iss_states >> state) {
        if (!valid_states.count(state)) {
            valid_states.insert(state);
            states.push_back(state);
        }
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
    std::set<std::string> final_states;
    while (iss_final >> state) {
        if (!valid_states.count(state)) {
            std::cerr << "A vegallapot nem szerepel az ervenyes allapotok kozott: " << state << std::endl;
            return 1;
        }
        final_states.insert(state);
    }

    // További sorok: Átmenetek
    std::map<std::string, std::map<std::string, std::string>> transitions;
    while (std::getline(infile, line)) {
        trim_cr(line);
        if (line.empty()) continue;
        std::istringstream trans_iss(line);
        std::string from_state, trans_sym, to_state;
        if (trans_iss >> from_state >> trans_sym >> to_state) {
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
            transitions[from_state][trans_sym] = to_state;
        }
    }
    infile.close();

    int n = static_cast<int>(states.size());
    std::map<std::string, int> state_to_idx;
    for (int i = 0; i < n; ++i) {
        state_to_idx[states[i]] = i;
    }

    // Táblázatkitöltéses minimalizáló algoritmus (Moore / Hopcroft jellegű pár-megjelölés)
    std::vector<std::vector<bool>> marked(n, std::vector<bool>(n, false));

    // 1. lépés: Végállapot és nem végállapot párok megjelölése
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            bool f_i = final_states.count(states[i]) > 0;
            bool f_j = final_states.count(states[j]) > 0;
            if (f_i != f_j) {
                marked[i][j] = marked[j][i] = true;
            }
        }
    }

    // 2. lépés: Megkülönböztethetőség terjesztése
    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                if (marked[i][j]) continue;

                for (const auto &c_sym : alphabet) {
                    bool has_i = transitions[states[i]].count(c_sym) > 0;
                    bool has_j = transitions[states[j]].count(c_sym) > 0;

                    if (has_i != has_j) {
                        marked[i][j] = marked[j][i] = true;
                        changed = true;
                        break;
                    }

                    if (has_i && has_j) {
                        const std::string &next_i = transitions[states[i]][c_sym];
                        const std::string &next_j = transitions[states[j]][c_sym];
                        int ni = state_to_idx[next_i];
                        int nj = state_to_idx[next_j];
                        if (marked[ni][nj]) {
                            marked[i][j] = marked[j][i] = true;
                            changed = true;
                            break;
                        }
                    }
                }
            }
        }
    }

    // 3. lépés: Ekvivalenciaosztályok meghatározása (összevonások)
    std::vector<bool> visited(n, false);
    std::map<std::string, std::string> state_to_merged;
    std::vector<std::string> new_states;

    for (int i = 0; i < n; ++i) {
        if (visited[i]) continue;

        std::vector<std::string> comp;
        std::queue<int> q;
        q.push(i);
        visited[i] = true;

        while (!q.empty()) {
            int u = q.front();
            q.pop();
            comp.push_back(states[u]);

            for (int v = 0; v < n; ++v) {
                if (!visited[v] && !marked[u][v]) {
                    visited[v] = true;
                    q.push(v);
                }
            }
        }

        // Az összevont állapot neve a komponens legkisebb elemének neve 's' betűvel
        std::string rep = *std::min_element(comp.begin(), comp.end(), state_cmp);
        std::string new_name = to_s_name(rep);
        new_states.push_back(new_name);

        for (const auto &st : comp) {
            state_to_merged[st] = new_name;
        }
    }

    // Új állapotok rendezése a specifikáció szerint
    std::sort(new_states.begin(), new_states.end(), state_cmp);

    // Kezdőállapot meghatározása
    std::string new_start_state = state_to_merged[start_state];

    // Végállapotok meghatározása (nem duplikálva, rendezve)
    std::vector<std::string> new_final_states;
    for (const auto &ns : new_states) {
        bool is_final = false;
        for (const auto &st : states) {
            if (state_to_merged[st] == ns && final_states.count(st)) {
                is_final = true;
                break;
            }
        }
        if (is_final) {
            new_final_states.push_back(ns);
        }
    }

    // Új átmenetek meghatározása (duplikátumok nélkül)
    std::map<std::string, std::map<std::string, std::string>> merged_transitions;
    for (const auto &st : states) {
        for (const auto &c_sym : alphabet) {
            if (transitions[st].count(c_sym)) {
                std::string u = state_to_merged[st];
                std::string v = state_to_merged[transitions[st][c_sym]];
                merged_transitions[u][c_sym] = v;
            }
        }
    }

    // Eredmény kiírása
    std::ofstream outfile(output_file);
    if (!outfile.is_open()) {
        std::cerr << "Nem sikerult megnyitni a kimeneti fajlt: " << output_file << std::endl;
        return 1;
    }

    // 1. sor: Állapotok
    for (size_t i = 0; i < new_states.size(); ++i) {
        if (i > 0) outfile << " ";
        outfile << new_states[i];
    }
    outfile << "\n";

    // 2. sor: Bemeneti ábécé elemei
    for (size_t i = 0; i < alphabet.size(); ++i) {
        if (i > 0) outfile << " ";
        outfile << alphabet[i];
    }
    outfile << "\n";

    // 3. sor: Kezdőállapot
    outfile << new_start_state << "\n";

    // 4. sor: Végállapotok
    for (size_t i = 0; i < new_final_states.size(); ++i) {
        if (i > 0) outfile << " ";
        outfile << new_final_states[i];
    }
    outfile << "\n";

    // További sorok: Átmenetek ábécé sorrendben
    for (const auto &u : new_states) {
        for (const auto &c_sym : alphabet) {
            if (merged_transitions[u].count(c_sym)) {
                outfile << u << " " << c_sym << " " << merged_transitions[u][c_sym] << "\n";
            }
        }
    }
    outfile.close();

    return 0;
}
