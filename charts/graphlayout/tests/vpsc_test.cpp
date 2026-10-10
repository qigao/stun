#include "stun/graphlayout/vpsc.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace stun::graphlayout;

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

void close(double actual, double expected, double tolerance, const char* message) {
    if (!std::isfinite(actual) || std::abs(actual - expected) > tolerance)
        throw std::runtime_error(std::string(message) + " got " + std::to_string(actual) +
                                 " expected " + std::to_string(expected));
}

VpscResult solve_checked(const VpscProblem& p) {
    VpscResult r;
    const auto status = solve_vpsc(p, r);
    if (!status) throw std::runtime_error("VPSC solve failure: " + status.message);
    VpscCertificate external;
    require(static_cast<bool>(verify_vpsc(p, r, external)), "external KKT verifier must pass");
    close(r.certificate.objective, external.objective, 1e-8, "audited objective");
    require(r.positions.size() == p.variables.size(), "one position per variable");
    require(r.multipliers.size() == p.constraints.size(), "one multiplier per constraint");
    return r;
}

std::map<std::string, double> by_id(const VpscProblem& p, const VpscResult& r) {
    std::map<std::string, double> result;
    for (std::size_t i = 0; i < p.variables.size(); ++i) result[p.variables[i].id] = r.positions[i];
    return result;
}

void check_witness(const VpscProblem& p, const VpscStatus& status) {
    require(status.error == VpscError::Infeasible, "must be infeasible");
    require(!status.infeasible_cycle.empty(), "infeasibility cycle must be present");
    double gap_sum = 0.0;
    for (std::size_t i = 0; i < status.infeasible_cycle.size(); ++i) {
        const auto& w = status.infeasible_cycle[i];
        require(w.constraint_index < p.constraints.size(), "witness index in range");
        const auto& c = p.constraints[w.constraint_index];
        require(!w.reversed || c.equality, "only equality can reverse arc");
        gap_sum += w.reversed ? -c.gap : c.gap;
        const auto& next = status.infeasible_cycle[(i + 1) % status.infeasible_cycle.size()];
        const auto& next_constraint = p.constraints[next.constraint_index];
        const auto end = w.reversed ? c.left : c.right;
        const auto beginning = next.reversed ? next_constraint.right : next_constraint.left;
        require(end == beginning, "witness arcs form a directed cycle");
    }
    require(gap_sum > 0, "infeasibility witness has strictly positive total gap");
}
}

int main() {
    try {
        {
            VpscProblem p;
            VpscResult r = solve_checked(p);
            require(r.positions.empty() && r.multipliers.empty(), "empty model works");
        }
        {
            VpscProblem p{{{"left", 0.0, 1.0}, {"right", 0.0, 1.0}}, {{0, 1, 10.0}}};
            const auto r = solve_checked(p);
            close(r.positions[0], -5.0, 1e-7, "simple weighted projection left");
            close(r.positions[1], 5.0, 1e-7, "simple weighted projection right");
            close(r.multipliers[0], 5.0, 1e-7, "positive KKT multiplier");
            close(r.certificate.objective, 25.0, 1e-6, "objective");
            close(r.certificate.dual_lower_bound, 25.0, 1e-6, "dual lower bound");
            require(r.sweeps > 0, "constrained solve uses coordinate steps");
        }
        {
            // Weighted analytic minimizer: w_left=1, w_right=3, gap=8.
            VpscProblem p{{{"a", 0.0, 1.0}, {"b", 0.0, 3.0}}, {{0, 1, 8.0}}};
            const auto r = solve_checked(p);
            close(r.positions[0], -6.0, 1e-7, "weighted left");
            close(r.positions[1], 2.0, 1e-7, "weighted right");
            close(r.multipliers[0], 6.0, 1e-7, "weighted KKT lambda");
        }
        {
            VpscProblem p{{{"a", 0.0, 1.0}, {"b", 0.0, 1.0}, {"c", 0.0, 1.0}},
                          {{0, 1, 5.0}, {1, 2, 5.0}}};
            const auto r = solve_checked(p);
            close(r.positions[0], -5.0, 1e-5, "chain left");
            close(r.positions[1], 0.0, 1e-5, "chain center");
            close(r.positions[2], 5.0, 1e-5, "chain right");
        }
        {
            VpscProblem p{{{"a", 3.0, 1.0}, {"b", 8.0, 1.0}}, {{0, 1, 5.0}}};
            const auto r = solve_checked(p);
            close(r.positions[0], 3.0, 0.0, "feasible desires unchanged");
            close(r.positions[1], 8.0, 0.0, "feasible desires unchanged");
            close(r.multipliers[0], 0.0, 0.0, "feasible initial multiplier zero");
            require(r.sweeps == 0, "initial desires satisfy KKT");
        }
        {
            VpscProblem p{{{"a", 0, 1}, {"b", 0, 1}}, {{0, 1, 12.0, true}}};
            const auto r = solve_checked(p);
            close(r.positions[0], -6.0, 1e-7, "exact equality left");
            close(r.positions[1], 6.0, 1e-7, "exact equality right");
        }
        {
            VpscProblem p{{{"a", -100, 1}, {"b", 100, 1}}, {{0, 1, -10.0, true}}};
            const auto r = solve_checked(p);
            close(r.positions[1] - r.positions[0], -10.0, 1e-7, "negative equality gap");
            require(r.multipliers[0] < 0.0, "equality dual is unrestricted");
        }
        {
            // Feasible cycle with non-positive total gap.
            VpscProblem p{{{"a", 0, 1}, {"b", 0, 1}}, {{0, 1, 4.0}, {1, 0, -5.0}}};
            const auto r = solve_checked(p);
            require(r.positions[1] - r.positions[0] >= 4.0 - 1e-6, "feasible cycle left-right");
            require(r.positions[1] - r.positions[0] <= 5.0 + 1e-6, "feasible cycle right-left");
        }
        {
            VpscProblem p{{{"a", 0, 1}, {"b", 0, 1}}, {{0, 1, 4.0}, {1, 0, 2.0}}};
            VpscResult r;
            const auto s = solve_vpsc(p, r);
            check_witness(p, s);
            require(r.positions.empty() && r.multipliers.empty(), "no partial success for infeasible case");
        }
        {
            VpscProblem p{{{"a", 0, 1}, {"b", 0, 1}}, {{0, 1, 3.0, true}, {1, 0, 1.0}}};
            VpscResult r;
            check_witness(p, solve_vpsc(p, r));
        }
        {
            VpscProblem p{{{"a", 0, 1}}, {{0, 0, 1.0}}};
            VpscResult r;
            check_witness(p, solve_vpsc(p, r));
            p.constraints[0].gap = 0.0;
            require(static_cast<bool>(solve_vpsc(p, r)), "self inequality with zero gap is feasible");
            p.constraints[0].gap = 1.0;
            p.constraints[0].equality = true;
            check_witness(p, solve_vpsc(p, r));
        }
        {
            // Independent oracle for repeated identity-preserving reordering.
            VpscProblem original{{{"a", 0, 1}, {"b", 0, 1}, {"c", 0, 1}},
                                 {{0, 1, 4}, {1, 2, 5}, {0, 2, 10}}};
            VpscProblem reordered{{original.variables[2], original.variables[0], original.variables[1]},
                                  {{1, 2, 4}, {1, 0, 10}, {2, 0, 5}}};
            const auto a = solve_checked(original);
            const auto b = solve_checked(reordered);
            const auto pa = by_id(original, a);
            const auto pb = by_id(reordered, b);
            require(pa.size() == pb.size(), "permutation cardinality");
            for (const auto& v : pa) close(v.second, pb.at(v.first), 1e-8, "permutation invariant");
        }
        {
            VpscProblem p{{{"a", 0, 1}, {"b", 0, 1}}, {{0, 1, 10}}};
            auto r = solve_checked(p);
            r.positions[0] += 0.5;
            VpscCertificate audit;
            require(verify_vpsc(p, r, audit).error == VpscError::CertificateFailed,
                    "tampered primal vector rejected");
            r = solve_checked(p);
            r.multipliers[0] = -1.0;
            require(verify_vpsc(p, r, audit).error == VpscError::CertificateFailed,
                    "tampered dual vector rejected");
            r = solve_checked(p);
            r.multipliers[0] = -1e-12;
            require(verify_vpsc(p, r, audit).error == VpscError::CertificateFailed,
                    "even a tiny negative inequality multiplier invalidates the dual lower bound");
            r = solve_checked(p);
            r.certificate.objective = -1234; // must not be trusted by verifier
            require(static_cast<bool>(verify_vpsc(p, r, audit)), "certificate recomputed independently");
        }
        {
            VpscProblem p{{{"a", 0, 1}, {"b", 0, 1}, {"c", 0, 1}},
                          {{0, 1, 10}, {1, 2, 10}}};
            VpscOptions options; options.max_coordinate_updates = 1;
            VpscResult out;
            const auto s = solve_vpsc(p, out, options);
            require(s.error == VpscError::IterationLimit, "iteration limit typed error");
            require(out.positions.empty() && out.multipliers.empty(), "iteration failure atomic");
            options.max_coordinate_updates = 30;
            options.max_feasibility_checks = 1;
            require(solve_vpsc(p, out, options).error == VpscError::CapacityExceeded,
                    "feasibility budget typed error");
        }
        {
            VpscProblem p{{{"a", 0, 1}, {"a", 0, 1}}, {}};
            VpscResult out;
            require(solve_vpsc(p, out).error == VpscError::DuplicateVariableId, "duplicate ids rejected");
            p.variables[1].id = "b";
            p.variables[1].weight = 0;
            require(solve_vpsc(p, out).error == VpscError::InvalidVariable, "zero weights rejected");
            p.variables[1].weight = 1;
            p.constraints.push_back({0, 2, 1.0});
            require(solve_vpsc(p, out).error == VpscError::InvalidConstraint, "dangling constraints rejected");
            p.constraints.clear();
            VpscOptions opt; opt.max_variables = 1;
            require(solve_vpsc(p, out, opt).error == VpscError::CapacityExceeded, "variable capacity bound");
            opt.max_variables = 10; opt.tolerance = -1.0;
            require(solve_vpsc(p, out, opt).error == VpscError::InvalidOptions, "invalid tolerance rejected");
            require(out.positions.empty(), "error clears prior output");
        }
        {
            // A mathematically sub-resolution equality cycle must not be
            // presented as a definitively validated infeasibility witness.
            VpscProblem p{{{"a", 0, 1}, {"b", 0, 1}, {"c", 0, 1}},
                          {{0, 1, 0.1, true}, {1, 2, 0.2, true},
                           {2, 0, -0.30000000000000004, true}}};
            VpscResult r;
            const auto status = solve_vpsc(p, r);
            require(status.error == VpscError::InvalidNumerics,
                    "near-zero positive cycle reported as numerical ambiguity");
            require(status.infeasible_cycle.empty(), "ambiguous cycle is not a verified witness");
            require(r.positions.empty(), "ambiguous result returns no partial positions");
        }
        {
            // Deterministic property exercise: all constraints are generated
            // from a feasible integral witness, including equality constraints.
            std::uint32_t state = 0x13579bdfu;
            const auto next = [&state]() -> std::uint32_t {
                state = 1664525u * state + 1013904223u;
                return state;
            };
            for (int repeat = 0; repeat < 45; ++repeat) {
                VpscProblem p;
                const std::size_t n = 2 + next() % 7;
                std::vector<double> witness(n);
                for (std::size_t i = 0; i < n; ++i) {
                    witness[i] = static_cast<int>(next() % 41) - 20;
                    p.variables.push_back({"property_" + std::to_string(i),
                                          static_cast<double>(static_cast<int>(next() % 61) - 30),
                                          static_cast<double>(1 + next() % 5)});
                }
                for (std::size_t j = 0; j < 2 * n; ++j) {
                    std::size_t left = next() % n;
                    std::size_t right = next() % n;
                    if (left == right) right = (right + 1) % n;
                    const bool equality = next() % 7 == 0;
                    const double gap = witness[right] - witness[left] -
                                       (equality ? 0.0 : static_cast<double>(1 + next() % 10));
                    p.constraints.push_back({left, right, gap, equality});
                }
                const auto r = solve_checked(p);
                for (const auto& v : r.positions)
                    require(std::isfinite(v), "random feasible witness result finite");
            }
        }
        {
            // Hard pin is an exact coordinate, not an enormous soft weight.
            VpscProblem p{{{"anchor", 0, 1, true}, {"free", 0, 1}}, {{0, 1, 10}}};
            const auto r = solve_checked(p);
            close(r.positions[0], 0.0, 0.0, "hard pin exact");
            close(r.positions[1], 10.0, 1e-7, "hard pin one-sided projection");
            close(r.multipliers[0], 10.0, 1e-7, "pin KKT multiplier");
            close(r.certificate.objective, 50.0, 1e-6, "pinned objective excludes anchor");
            auto corrupted = r;
            corrupted.positions[0] += 1e-12;
            VpscCertificate audit;
            require(verify_vpsc(p, corrupted, audit).error == VpscError::CertificateFailed,
                    "pinned coordinate never moves, even within a tolerance");
        }
        {
            // An unconstrained free variable is not translated when another
            // component contains a pinned constraint.
            VpscProblem p{{{"anchor", 0, 1, true}, {"free", 0, 1},
                           {"isolated", -70, 1}}, {{0, 1, 10}}};
            const auto r = solve_checked(p);
            close(r.positions[1], 10.0, 1e-7, "anchored constrained value");
            close(r.positions[2], -70.0, 0, "isolated free variable stays at its desire");
        }
        {
            VpscProblem p{{{"anchor", 2, 1, true}, {"free", 0, 1},
                           {"other", 12, 1, true}},
                          {{0, 1, 5}, {1, 2, 5}}};
            const auto r = solve_checked(p);
            close(r.positions[0], 2, 0, "first fixed value");
            close(r.positions[1], 7, 1e-7, "exact free equality between fixed bounds");
            close(r.positions[2], 12, 0, "second fixed value");
        }
        {
            VpscProblem p{{{"anchor", 0, 1, true}, {"other", 5, 1, true}},
                          {{0, 1, 10}}};
            VpscResult result;
            const auto status = solve_vpsc(p, result);
            require(status.error == VpscError::Infeasible, "conflicting pins give Infeasible");
            require(result.positions.empty(), "infeasible pinned output transactional");
            bool found_pin_arc = false;
            for (const auto& arc : status.infeasible_cycle) {
                if (arc.fixed_variable) {
                    require(arc.constraint_index < p.variables.size(), "pin witness names original variable");
                    found_pin_arc = true;
                } else {
                    require(arc.constraint_index < p.constraints.size(), "separation witness names constraint");
                }
            }
            require(found_pin_arc, "infeasible pin cycle contains hard-coordinate witness");
        }
        std::cout << "vpsc: all checks passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "vpsc: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
