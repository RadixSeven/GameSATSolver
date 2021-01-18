#pragma once
#include <map>
#include <string>
#include <vector>

struct LiteralCode {
    unsigned int value;
};

using Clause = std::vector<LiteralCode>;

struct SATInstance {
    /// variables[index_of_variable] = variable_name
    std::vector<std::string> variables;
    /// variable_table[variable_name] = index_of_variable
    std::map<std::string, unsigned> variable_table;
    std::vector<Clause> clauses;
};

enum class LiteralState { normal = 0, negated = 1 };

void add_literal_to_clause(const std::string& variable,
                           const LiteralState state, Clause& clause,
                           SATInstance& instance) {
    unsigned negated = static_cast<unsigned>(state);

    auto& literal_codes = instance.variable_table;
    unsigned variable_code;
    const auto& code_location = literal_codes.find(variable);
    if (code_location == literal_codes.end()) {
        variable_code = instance.variables.size();
        literal_codes[variable] = variable_code;
        instance.variables.push_back(variable);
    } else {
        variable_code = code_location->second;
    }
    LiteralCode encoded_literal;
    encoded_literal.value = variable_code << 1 | negated;
    clause.push_back(encoded_literal);
}

inline void add_clause_to_instance(Clause& clause, SATInstance& instance) {
    instance.clauses.push_back(clause);
}
