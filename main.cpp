#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include "SATSolve.hpp"

void parse_line_and_add_clause(SATInstance& instance, const std::string& line) {
    trace("parse_line_and_add_clause");
    std::stringstream line_stream(line);
    std::string literal_str;

    Clause clause;
    while (std::getline(line_stream, literal_str, ' ')) {
        if (literal_str.empty()) {
            continue;
        }

        // The variable is negated if it starts with ~
        LiteralState state = literal_str[0] == '~' ? LiteralState::negated
                                                   : LiteralState::normal;

        // Remove ~ if necessary
        std::string variable;
        auto start = state == LiteralState::negated
                         ? std::next(literal_str.begin())
                         : literal_str.begin();
        std::copy(start, literal_str.end(), std::back_inserter(variable));

        add_literal_to_clause(variable, state, clause, instance);
    }
    add_clause_to_instance(clause, instance);
}

// Strip whitespace from both ends of line
// Note that this can break on UTF-8 input ...
void strip_whitespace(std::string& line) {
    trace("strip_whitespace");
    while (not line.empty() and std::isspace(line.back())) {
        line.pop_back();
    }
    while (not line.empty() and std::isspace(line.front())) {
        line.erase(line.begin());
    }
}

SATInstance sat_instance_from_file(std::istream& file) {
    trace("sat_instance_from_file");
    SATInstance instance;
    std::string line;
    while (std::getline(file, line, '\n')) {
        strip_whitespace(line);
        // Skip blank lines and comments
        if ((not line.empty()) and line[0] != '#') {
            parse_line_and_add_clause(instance, line);
        }
    }
    return instance;
}

std::ostream& operator<<(std::ostream& o, const SATInstance& i) {
    trace("operator<< SATInstance");
    for (const auto& c : i.clauses) {
        const auto& success = o << clause_to_string(c, i) << std::endl;
        if (not success) {
            break;
        }
    }
    return o;
}

int main() {
    /// Accepts input in CNF with one clause per line
    ///
    /// For example: (A or not B) and (B or C) is represented as
    ///
    /// A ~B
    /// B  C
    auto instance = sat_instance_from_file(std::cin);
    std::cerr << "Read instance!" << std::endl;
    std::cerr << instance << std::endl;
    auto assignments = solve(instance);
    std::cerr << "Solutions:" << std::endl;
    for (const auto& assignment : assignments) {
        std::cout << assignment_to_string(assignment, instance) << std::endl;
    }
    return 0;
}
