#include "dfa.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>

void DfaSimulation::initialize_parser(cxxopts::Options &options) {
    // Hozzáadjuk azokat a parancssori argumentumokat, amelyeket ez a feladat használ[cite: 3, 4]
    // (Ha a main.cpp-ben az input/output már globálisan definiálva van, akkor innen azok kivehetők)
    options.add_options()
        ("check", "Ellenorzendo szavak", cxxopts::value<std::string>());
}

bool DfaSimulation::is_chosen_problem(const cxxopts::ParseResult &args) {
    // Ez a feladat akkor fut le, ha a "--check" flaget megadták a parancssorban[cite: 4]
    return args.count("check") > 0;
}

static void trim_cr(std::string &s) {
    if (!s.empty() && s.back() == '\r') {
        s.pop_back();
    }
}

int DfaSimulation::run(const cxxopts::ParseResult &args) {
    // Ellenőrizzük a szükséges paraméterek meglétét
    if (args.count("input") == 0 || args.count("output") == 0 || args.count("check") == 0) {
        std::cerr << "Hianyzik egy vagy tobb kotelezo parameter (input, output, check)!" << std::endl;
        return 1;
    }

    std::string input_file = args["input"].as<std::string>();
    std::string output_file = args["output"].as<std::string>();
    std::string check_words_str = args["check"].as<std::string>();

    std::ifstream infile(input_file);
    if (!infile.is_open()) {
        std::cerr << "Nem sikerult megnyitni a bemeneti fajlt: " << input_file << std::endl;
        return 1;
    }

    std::string line;

    // Első sor: Állapotok
    std::getline(infile, line);

    // Második sor: Ábécé
    std::getline(infile, line);

    // Harmadik sor: Kezdőállapot
    std::string start_state;
    std::getline(infile, start_state);
    trim_cr(start_state);

    // Negyedik sor: Végállapotok
    std::getline(infile, line);
    trim_cr(line);
    std::istringstream iss_final(line);
    std::set<std::string> final_states;
    std::string state;
    while (iss_final >> state) {
        final_states.insert(state);
    }

    // Átmenetek beolvasása
    std::map<std::string, std::map<char, std::string>> transitions;
    while (std::getline(infile, line)) {
        trim_cr(line);
        if (line.empty()) continue;
        std::istringstream trans_iss(line);
        std::string from_state, to_state;
        char symbol;
        if (trans_iss >> from_state >> symbol >> to_state) {
            transitions[from_state][symbol] = to_state;
        }
    }
    infile.close();

    // Tesztelendő szavak szétválasztása[cite: 4]
    std::vector<std::string> words;
    std::stringstream ss_words(check_words_str);
    std::string word;
    while (std::getline(ss_words, word, ',')) {
        words.push_back(word);
    }

    // Szimuláció és fájlba írás
    std::ofstream outfile(output_file);
    if (!outfile.is_open()) {
        std::cerr << "Nem sikerult megnyitni a kimeneti fajlt: " << output_file << std::endl;
        return 1;
    }

    for (const auto& w : words) {
        std::string curr_state = start_state;
        bool valid = true;

        for (char c : w) {
            if (transitions[curr_state].count(c)) {
                curr_state = transitions[curr_state][c];
            } else {
                valid = false;
                break;
            }
        }

        if (valid && final_states.count(curr_state)) {
            outfile << "IGEN\n"; //[cite: 3]
        } else {
            outfile << "NEM\n";  //[cite: 3]
        }
    }
    outfile.close();

    return 0; // Hibátlan lefutás
}