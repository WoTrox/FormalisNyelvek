#include <iostream>
#include <string>
#include <vector>

#include "cxxopts.hpp"

// Add your own problems here
#include "problems/dfa.hpp"
#include "problems/sum.hpp"
#include "problems/nfa_to_dfa.hpp"

int runProblem(int argc, char* argv[]) {
    std::vector<Problem *> problems;
    problems.push_back(new SumProblem());
    problems.push_back(new DfaSimulation());
    problems.push_back(new NfaToDfa());

    cxxopts::Options options("project", "Run the specific problem");

    options.add_options()
        ("i,input", "Input file name", cxxopts::value<std::string>())
        ("o,output", "Output file name", cxxopts::value<std::string>())
        ("h,help", "Print usage");
    
    for (Problem *p : problems) {
        p->initialize_parser(options);
    }

    cxxopts::ParseResult args = options.parse(argc, argv);

    for (Problem *p : problems) {
        if (p->is_chosen_problem(args)) {
            return p->run(args);
        }
    }

    std::cout << options.help() << std::endl;
    return 0;
}

int main(int argc, char* argv[]) {
    try {
        return runProblem(argc, argv);
    } catch (const cxxopts::exceptions::exception &e) {
        std::cerr << "Error parsing options: " << e.what() << std::endl;
        return 1;
    }
}
