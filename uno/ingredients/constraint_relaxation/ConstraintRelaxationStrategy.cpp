#include "ConstraintRelaxationStrategy.hpp"

ConstraintRelaxationStrategy::ConstraintRelaxationStrategy(Subproblem& subproblem): subproblem(subproblem),
number_variables(this->subproblem.number_variables), number_constraints(this->subproblem.number_constraints) {
}

size_t ConstraintRelaxationStrategy::count_elastic_variables(const Problem& problem) {
   size_t number_variables = 0;
   for (size_t j = 0; j < problem.number_constraints; j++) {
      if (-INFINITY < problem.constraint_bounds[j].lb) {
         number_variables++;
      }
      if (problem.constraint_bounds[j].ub < INFINITY) {
         number_variables++;
      }
   }
   return number_variables;
}

void ConstraintRelaxationStrategy::generate_elastic_variables(const Problem& problem, ElasticVariables& elastic_variables) {
   // generate elastic variables p and n on the fly to relax the constraints
   size_t elastic_index = problem.number_variables;
   for (size_t j = 0; j < problem.number_constraints; j++) {
      if (-INFINITY < problem.constraint_bounds[j].lb) {
         // nonpositive variable n that captures the negative part of the constraint violation
         elastic_variables.negative.insert(j, elastic_index);
         elastic_index++;
      }
      if (problem.constraint_bounds[j].ub < INFINITY) {
         // nonnegative variable p that captures the positive part of the constraint violation
         elastic_variables.positive.insert(j, elastic_index);
         elastic_index++;
      }
   }
}

void ConstraintRelaxationStrategy::set_elastic_bounds_in_subproblem(const Problem& problem, size_t number_elastic_variables) const {
   for (size_t i = problem.number_variables; i < problem.number_variables + number_elastic_variables; i++) {
      this->subproblem.variables_bounds[i] = {0., INFINITY};
   }
}

void ConstraintRelaxationStrategy::add_elastic_variables_to_subproblem(const ElasticVariables& elastic_variables) {
   // add the positive elastic variables
   elastic_variables.positive.for_each([&](size_t j, size_t i) {
      this->subproblem.objective_gradient[i] = 1.;
      this->subproblem.constraints_jacobian[j][i] = -1.;
   });
   elastic_variables.negative.for_each([&](size_t j, size_t i) {
      this->subproblem.objective_gradient[i] = 1.;
      this->subproblem.constraints_jacobian[j][i] = 1.;
   });
   /*
   for (const auto& [j, i]: elastic_variables.positive) {
      this->subproblem.objective_gradient[i] = 1.;
      this->subproblem.constraints_jacobian[j][i] = -1.;
   }
   // add the negative elastic variables
   for (const auto& [j, i]: elastic_variables.negative) {
      this->subproblem.objective_gradient[i] = 1.;
      this->subproblem.constraints_jacobian[j][i] = 1.;
   }
    */
}

Direction ConstraintRelaxationStrategy::compute_second_order_correction(const Problem& problem, Iterate& trial_iterate) {
   return this->subproblem.compute_second_order_correction(problem, trial_iterate);
}

int ConstraintRelaxationStrategy::get_hessian_evaluation_count() const {
   return this->subproblem.get_hessian_evaluation_count();
}

int ConstraintRelaxationStrategy::get_number_subproblems_solved() const {
   return this->subproblem.number_subproblems_solved;
}

void ConstraintRelaxationStrategy::register_accepted_iterate(Iterate& iterate) {
   this->subproblem.register_accepted_iterate(iterate);
}