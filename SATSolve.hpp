#pragma once
#include <array>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

/// Index of a variable in an instance
struct VariableIndex {
    unsigned int value;
};

/// Variables are either unassigned, true, or false
enum class LiteralAssignment { unassigned = 0, true_ = 1, false_ = 2 };

struct LiteralCode {
    unsigned int value;

    VariableIndex variable() const { return VariableIndex{value >> 1}; }

    bool is_negated() const { return value & 1; }

    /// Return the literal that would be falsified by assigning value_assigned
    /// the variable at index var.
    ///
    /// \param var The index of the variable assigned
    /// \param value_assigned The value assigned (cannot be unassigned)
    /// \return The code for the literal that would be falsified
    static constexpr LiteralCode literal_falsified_by_assignment(
        VariableIndex var, LiteralAssignment value_assigned) {
        return value_assigned == LiteralAssignment::true_
                   ? LiteralCode{var.value << 1 | 1}
                   : LiteralCode{var.value << 1};
    }
};

/// Clause cannot be empty
using Clause = std::vector<LiteralCode>;

struct SATInstance {
    /// variables[index_of_variable] = variable_name
    std::vector<std::string> variables;
    /// variable_table[variable_name] = index_of_variable
    std::map<std::string, unsigned> variable_table;
    /// This should not be empty
    std::vector<Clause> clauses;
};

enum class LiteralState { normal = 0, negated = 1 };

void add_literal_to_clause(const std::string& variable,
                           const LiteralState state, Clause& clause,
                           SATInstance& instance) {
    auto negated = static_cast<unsigned>(state);

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
    LiteralCode encoded_literal{variable_code << 1 | negated};
    clause.push_back(encoded_literal);
}

inline void add_clause_to_instance(Clause& clause, SATInstance& instance) {
    instance.clauses.push_back(clause);
}

std::string literal_to_string(const LiteralCode l,
                              const SATInstance& instance) {
    std::string s{l.value & 1 ? "~" : ""};
    s += instance.variables[l.value >> 1];
    return s;
}

std::string clause_to_string(const Clause& clause,
                             const SATInstance& instance) {
    std::string s;
    auto cur = clause.begin();
    if (cur == clause.end()) {
        return s;
    }
    s += literal_to_string(*cur++, instance);
    while (cur != clause.end()) {
        s += " ";
        s += literal_to_string(*cur++, instance);
    }
    return s;
}

/// Watchers is a list of pointers to the clauses that are watching the literal
/// We assume that the clauses are not deleted, that is that their list is
/// not altered. Further, these are not_null, but not_null is not available in
/// the standard library ... so I leave them un-annotated.
using Watchers = std::deque<const Clause*>;

/// The (partial) assignment of values to variables used as
/// a workspace for finding a satisfying assignment
class Assignment {
    std::vector<LiteralAssignment> literal_assignments;

   public:
    LiteralAssignment& operator[](VariableIndex l) {
        return literal_assignments.at(l.value);
    }
    LiteralAssignment operator[](VariableIndex l) const {
        return literal_assignments.at(l.value);
    }
    bool empty() const { return literal_assignments.empty(); }
    bool operator<(const Assignment& rhs) const {
        return std::lexicographical_compare(
            literal_assignments.begin(), literal_assignments.end(),
            rhs.literal_assignments.begin(), rhs.literal_assignments.end());
    }
};

std::ostream& operator<<(std::ostream& out, LiteralAssignment litA) {
    switch (litA) {
        case LiteralAssignment::unassigned:
            return out << "None";
        case LiteralAssignment::true_:
            return out << "True";
        case LiteralAssignment::false_:
            return out << "False";
    }
}

std::string assignment_to_string(const Assignment& assignment,
                                 const SATInstance& instance) {
    std::ostringstream out;
    if (assignment.empty()) {
        return std::string{"{No assignments}"};
    }
    out << "{";
    out << instance.variables[0] << "==" << assignment[VariableIndex{0}];
    for (VariableIndex var{1}; var.value < instance.variables.size();
         ++var.value) {
        out << ", " << instance.variables[var.value] << "==" << assignment[var];
    }
    out << "}";
    return out.str();
}

class WatchList {
    const SATInstance& instance;
    std::vector<Watchers> watchers;

   public:
    explicit WatchList(const SATInstance& instance)
        : instance{instance}, watchers{2 * instance.variables.size()} {
        for (const auto& clause : instance.clauses) {
            Watchers& clauses_watching_first_value =
                watchers.at(clause.front().value);
            clauses_watching_first_value.push_back(&clause);
        }
    }

    Watchers& operator[](LiteralCode l) { return watchers.at(l.value); }
    const Watchers& operator[](LiteralCode l) const {
        return watchers.at(l.value);
    }

    void dump(std::ostream& out) {
        for (LiteralCode lit{0}; lit.value < watchers.size(); ++lit.value) {
            out << "Watching " << literal_to_string(lit, instance) << ":";
            for (const Clause& clause : instance.clauses) {
                out << " [" << clause_to_string(clause, instance) << "]";
            }
            out << std::endl;
        }
    }

    /// Falsify the literal in this watchlist and return true if the
    /// assignment remains satisfiable or false if you must backtrack.
    ///
    /// Makes any clause watching false_literal watch something else.
    ///
    /// Assumes false_literal was just assigned.
    ///
    /// Restores the invariant that all watched literals are either
    /// not assigned yet, or they have been assigned true.
    ///
    /// \param literal The literal to falsify
    /// \param assignment The assignment in which the literal
    ///                   was falsified
    /// \param debug_stream If non-null, print debugging / verbose solution info
    /// \return true if the assignment is potentially satisfiable and
    ///              false otherwise
    bool assignment_is_satisfiable_and_falsify_literal(
        const LiteralCode false_literal, const Assignment& assignment,
        std::ostream* debug_stream = nullptr) {
        auto& watching_false_literal = operator[](false_literal);
        while (not watching_false_literal.empty()) {
            const Clause& clause = *watching_false_literal.front();
            bool found_alternative = false;
            for (const LiteralCode alternative : clause) {
                const auto v = alternative.variable();
                const bool negated_alt = alternative.is_negated();
                const auto assigned = assignment[v];
                if (assigned == LiteralAssignment::unassigned or
                    (assigned == LiteralAssignment::true_ and negated_alt) or
                    (assigned == LiteralAssignment::false_ and
                     not negated_alt)) {
                    found_alternative = true;
                    watching_false_literal.pop_front();
                    operator[](alternative).push_back(&clause);
                    break;
                }
            }

            if (not found_alternative) {
                if (debug_stream) {
                    dump(*debug_stream);
                    (*debug_stream)
                        << "Assignment: "
                        << assignment_to_string(assignment, instance)
                        << std::endl
                        << "Contradicted clause: "
                        << clause_to_string(clause, instance) << std::endl;
                }
                return false;
            }
        }
        return true;
    }
};

constexpr std::array<LiteralAssignment, 2> false_true{LiteralAssignment::false_,
                                                      LiteralAssignment::true_};

void solve_helper(const SATInstance& instance, WatchList& watchList,
                  Assignment& assignment,
                  VariableIndex first_unassigned_variable,
                  std::set<Assignment>& satisfying_assignments,
                  std::ostream* debug_stream) {
    if (first_unassigned_variable.value == instance.variables.size()) {
        satisfying_assignments.insert(assignment);
        return;
    }

    for (const LiteralAssignment a : false_true) {
        if (debug_stream) {
            (*debug_stream)
                << "Trying "
                << instance.variables[first_unassigned_variable.value] << " = "
                << a;
        }
        assignment[first_unassigned_variable] = a;
        if (watchList.assignment_is_satisfiable_and_falsify_literal(
                LiteralCode::literal_falsified_by_assignment(
                    first_unassigned_variable, a),
                assignment, debug_stream)) {
            solve_helper(instance, watchList, assignment,
                         VariableIndex{first_unassigned_variable.value + 1},
                         satisfying_assignments, debug_stream);
        }
    }

    assignment[first_unassigned_variable] = LiteralAssignment::unassigned;
}

std::set<Assignment> solve(const SATInstance& instance,
                           std::ostream* debug_stream = nullptr) {
    WatchList watch_list{instance};
    std::set<Assignment> satisfying_assignments;
    Assignment workspace_assignment;
    solve_helper(instance, watch_list, workspace_assignment, VariableIndex{0},
                 satisfying_assignments, debug_stream);
    return satisfying_assignments;
}