#include <cassert>
#include "Subproblem.hpp"
#include "LinearSolverFactory.hpp"

Subproblem::Subproblem(size_t number_variables, size_t number_constraints) :
      number_variables(number_variables),
      number_constraints(number_constraints),
      variables_bounds(number_variables),
      constraints_multipliers(number_constraints),
      // objective_gradient is a SparseVector
      // constraints_jacobian is a vector of SparseVectors
      constraints_jacobian(number_constraints),
      constraints_bounds(number_constraints),
      number_subproblems_solved(0), subproblem_definition_changed(false) {
}

void Subproblem::evaluate_constraints(const Problem& problem, Iterate& iterate) const {
   iterate.compute_constraints(problem);
}

Iterate Subproblem::generate_initial_iterate(Statistics& /*statistics*/, const Problem& problem, std::vector<double>& x, Multipliers& multipliers) {
   Iterate first_iterate(x, multipliers);
   /* compute the optimality and feasibility measures of the initial point */
   this->evaluate_constraints(problem, first_iterate);
   this->compute_progress_measures(problem, first_iterate);
   return first_iterate;
}

void Subproblem::compute_progress_measures(const Problem& problem, Iterate& iterate) {
   iterate.compute_constraints(problem);
   // feasibility measure: residual of all constraints
   iterate.errors.constraints = problem.compute_constraint_violation(iterate.constraints, L1_NORM);
   // optimality
   iterate.compute_objective(problem);
   iterate.progress = {iterate.errors.constraints, iterate.objective};
}

double Subproblem::push_variable_to_interior(double variable_value, const Range& variable_bounds) {
   double k1 = 1e-2;
   double k2 = 1e-2;

   double perturbation_lb = std::min(k1 * std::max(1., std::abs(variable_bounds.lb)), k2 * (variable_bounds.ub - variable_bounds.lb));
   double perturbation_ub = std::min(k1 * std::max(1., std::abs(variable_bounds.ub)), k2 * (variable_bounds.ub - variable_bounds.lb));
   variable_value = std::max(variable_value, variable_bounds.lb + perturbation_lb);
   variable_value = std::min(variable_value, variable_bounds.ub - perturbation_ub);
   return variable_value;
}

void Subproblem::set_trust_region(const Problem& problem, const Iterate& current_iterate, double trust_region_radius) {
   this->set_variables_bounds(problem, current_iterate, trust_region_radius);
}

void Subproblem::set_variables_bounds(const Problem& problem, const Iterate& current_iterate, double trust_region_radius) {
   /* bounds intersected with trust region  */
   // very important: apply the trust region only on the original variables
   for (size_t i = 0; i < problem.number_variables; i++) {
      double lb = std::max(-trust_region_radius, problem.variables_bounds[i].lb - current_iterate.x[i]);
      double ub = std::min(trust_region_radius, problem.variables_bounds[i].ub - current_iterate.x[i]);
      this->variables_bounds[i] = {lb, ub};
   }
}

void Subproblem::set_constraints_bounds(const Problem& problem, const std::vector<double>& current_constraints) {
   for (size_t j = 0; j < problem.number_constraints; j++) {
      double lb = problem.constraint_bounds[j].lb - current_constraints[j];
      double ub = problem.constraint_bounds[j].ub - current_constraints[j];
      this->constraints_bounds[j] = {lb, ub};
   }
}

void Subproblem::compute_least_square_multipliers(const Problem& problem, Iterate& current_iterate, std::vector<double>& multipliers, double
multipliers_max_size) {
   std::unique_ptr<LinearSolver> linear_solver = LinearSolverFactory::create("MA57");
   Subproblem::compute_least_square_multipliers(problem, current_iterate, multipliers, *linear_solver, multipliers_max_size);
}

void Subproblem::compute_least_square_multipliers(const Problem& problem, Iterate& current_iterate, std::vector<double>& multipliers,
      LinearSolver& solver, double multipliers_max_size) {
   current_iterate.compute_objective_gradient(problem);
   current_iterate.compute_constraints_jacobian(problem);

   /******************************/
   /* build the symmetric matrix */
   /******************************/
   COOMatrix matrix(current_iterate.x.size() + problem.number_constraints, 0, 1);

   /* identity block */
   for (size_t i = 0; i < current_iterate.x.size(); i++) {
      matrix.insert(1., i, i);
   }
   /* Jacobian of general constraints */
   for (size_t j = 0; j < problem.number_constraints; j++) {
      for (const auto[variable_index, derivative]: current_iterate.constraints_jacobian[j]) {
         matrix.insert(derivative, variable_index, current_iterate.x.size() + j);
      }
   }
   DEBUG << "Multipliers estimation: KKT matrix:\n" << matrix << "\n";

   /********************************/
   /* generate the right-hand side */
   /********************************/
   std::vector<double> rhs(current_iterate.x.size() + problem.number_constraints);

   /* objective gradient */
   for (const auto[i, derivative]: current_iterate.objective_gradient) {
      rhs[i] += problem.objective_sign * derivative;
   }
   DEBUG << "LB duals:\n"; print_vector(DEBUG, current_iterate.multipliers.lower_bounds);
   DEBUG << "UB duals:\n"; print_vector(DEBUG, current_iterate.multipliers.upper_bounds);
   /* variable bound constraints */
   for (size_t i = 0; i < current_iterate.x.size(); i++) {
      rhs[i] -= current_iterate.multipliers.lower_bounds[i];
      rhs[i] -= current_iterate.multipliers.upper_bounds[i];
   }
   DEBUG << "Multipliers RHS:\n"; print_vector(DEBUG, rhs);

   // solve the system
   solver.factorize(matrix);
   std::vector<double> solution = solver.solve(matrix, rhs);
   DEBUG << "Solution: "; print_vector(DEBUG, solution);

   // if multipliers too big, discard them. Otherwise, retrieve the least-square multipliers
   if (norm_inf(solution, current_iterate.x.size(), problem.number_constraints) <= multipliers_max_size) {
      for (size_t j = 0; j < problem.number_constraints; j++) {
         multipliers[j] = solution[current_iterate.x.size() + j];
      }
   }
}

void Subproblem::compute_feasibility_linear_objective(const Iterate& current_iterate, const ConstraintPartition& constraint_partition) {
   /* objective function: sum of gradients of infeasible constraints */
   this->objective_gradient.clear();
   for (int j: constraint_partition.infeasible) {
      for (const auto [i, derivative]: current_iterate.constraints_jacobian[j]) {
         if (constraint_partition.constraint_feasibility[j] == INFEASIBLE_LOWER) {
            this->objective_gradient[i] -= derivative;
         }
         else {
            this->objective_gradient[i] += derivative;
         }
      }
   }
}

void Subproblem::generate_feasibility_bounds(const Problem& problem, const std::vector<double>& current_constraints, const ConstraintPartition&
constraint_partition) {
   for (size_t j = 0; j < problem.number_constraints; j++) {
      double lb, ub;
      if (constraint_partition.constraint_feasibility[j] == INFEASIBLE_LOWER) {
         lb = -INFINITY;
         ub = problem.constraint_bounds[j].lb - current_constraints[j];
      }
      else if (constraint_partition.constraint_feasibility[j] == INFEASIBLE_UPPER) {
         lb = problem.constraint_bounds[j].ub - current_constraints[j];
         ub = INFINITY;
      }
      else { // FEASIBLE
         lb = problem.constraint_bounds[j].lb - current_constraints[j];
         ub = problem.constraint_bounds[j].ub - current_constraints[j];
      }
      this->constraints_bounds[j] = {lb, ub};
   }
}

double Subproblem::compute_first_order_error(const Problem& problem, Iterate& iterate, double objective_multiplier) {
   std::vector<double> lagrangian_gradient = iterate.lagrangian_gradient(problem, objective_multiplier, iterate.multipliers);
   return norm_1(lagrangian_gradient);
}

/* complementary slackness error. Use abs/1e-8 to safeguard */
double Subproblem::compute_complementarity_error(const Problem& problem, Iterate& iterate, const Multipliers& multipliers) {
   double error = 0.;
   /* bound constraints */
   for (size_t i = 0; i < problem.number_variables; i++) {
      if (-INFINITY < problem.variables_bounds[i].lb) {
         error += std::abs(multipliers.lower_bounds[i] * (iterate.x[i] - problem.variables_bounds[i].lb));
      }
      if (problem.variables_bounds[i].ub < INFINITY) {
         error += std::abs(multipliers.upper_bounds[i] * (iterate.x[i] - problem.variables_bounds[i].ub));
      }
   }
   /* constraints */
   iterate.compute_constraints(problem);
   for (size_t j = 0; j < problem.number_constraints; j++) {
      double multiplier_j = multipliers.constraints[j];
      if (iterate.constraints[j] < problem.constraint_bounds[j].lb) {
         // violated lower: the multiplier is 1 at optimum
         error += std::abs((1. - multiplier_j) * (problem.constraint_bounds[j].lb - iterate.constraints[j]));
      }
      else if (problem.constraint_bounds[j].ub < iterate.constraints[j]) {
         // violated upper: the multiplier is -1 at optimum
         error += std::abs((1. + multiplier_j) * (iterate.constraints[j] - problem.constraint_bounds[j].ub));
      }
      else if (-INFINITY < problem.constraint_bounds[j].lb && 0. < multiplier_j) {
         error += std::abs(multiplier_j * (iterate.constraints[j] - problem.constraint_bounds[j].lb));
      }
      else if (problem.constraint_bounds[j].ub < INFINITY && multiplier_j < 0.) {
         error += std::abs(multiplier_j * (iterate.constraints[j] - problem.constraint_bounds[j].ub));
      }
   }
   return error;
}

double Subproblem::compute_constraint_violation(const Problem& problem, const Iterate& iterate) const {
   return problem.compute_constraint_violation(iterate.constraints, L1_NORM);
}

void Subproblem::compute_errors(const Problem& problem, Iterate& iterate, double objective_multiplier) const {
   iterate.compute_constraints(problem);
   iterate.errors.constraints = this->compute_constraint_violation(problem, iterate);
   // compute the KKT error only if the objective multiplier is positive
   iterate.errors.KKT = Subproblem::compute_first_order_error(problem, iterate, 0. < objective_multiplier ? objective_multiplier : 1.);
   iterate.errors.FJ = Subproblem::compute_first_order_error(problem, iterate, 0.);
   iterate.errors.complementarity = Subproblem::compute_complementarity_error(problem, iterate, iterate.multipliers);
}

Direction Subproblem::compute_second_order_correction(const Problem& /*problem*/, Iterate& /*trial_iterate*/) {
   assert(false && "Subproblem::compute_second_order_correction");
}