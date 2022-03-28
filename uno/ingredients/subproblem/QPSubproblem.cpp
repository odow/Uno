#include "QPSubproblem.hpp"
#include "solvers/QP/QPSolverFactory.hpp"

QPSubproblem::QPSubproblem(const Problem& problem, size_t max_number_variables, const Options& options) :
      ActiveSetSubproblem(max_number_variables, problem.number_constraints, NO_SOC, true, norm_from_string(options.at("residual_norm"))),
      // maximum number of Hessian nonzeros = number nonzeros + possible diagonal inertia correction
      solver(QPSolverFactory::create(options.at("QP_solver"), max_number_variables, problem.number_constraints,
            problem.get_hessian_maximum_number_nonzeros()
            + max_number_variables, /* regularization */
            true)),
      // if no trust region is used, the problem should be convexified to guarantee boundedness + descent direction
      hessian_model(HessianModelFactory::create(options.at("hessian_model"), max_number_variables,
            problem.get_hessian_maximum_number_nonzeros() + max_number_variables, options.at("mechanism") != "TR", options)),
      proximal_coefficient(stod(options.at("proximal_coefficient"))) {
}

void QPSubproblem::build_objective_model(const Problem& problem, Iterate& current_iterate, double objective_multiplier) {
   // Hessian
   this->hessian_model->evaluate(problem, current_iterate.x, objective_multiplier, current_iterate.multipliers.constraints);
   this->hessian_model->adjust_number_variables(problem.number_variables);

   // objective gradient
   current_iterate.evaluate_objective_gradient(problem);
   current_iterate.subproblem_evaluations.objective_gradient = current_iterate.problem_evaluations.objective_gradient;
}

void QPSubproblem::build_constraint_model(const Problem& problem, Iterate& current_iterate) {
   // constraints
   current_iterate.evaluate_constraints(problem);

   // constraint Jacobian
   current_iterate.evaluate_constraint_jacobian(problem);

   current_iterate.subproblem_evaluations.constraints = current_iterate.problem_evaluations.constraints;
   current_iterate.subproblem_evaluations.constraint_jacobian = current_iterate.problem_evaluations.constraint_jacobian;
}

Direction QPSubproblem::solve(Statistics& /*statistics*/, const Problem& problem, Iterate& current_iterate) {
   // bounds of the variable displacements
   this->set_variable_displacement_bounds(problem, current_iterate);

   // bounds of the linearized constraints
   this->set_linearized_constraint_bounds(problem, current_iterate.subproblem_evaluations.constraints);

   // compute QP direction
   Direction direction = this->solver->solve_QP(problem.number_variables, problem.number_constraints, this->variable_displacement_bounds,
         this->linearized_constraint_bounds, current_iterate.subproblem_evaluations.objective_gradient,
         current_iterate.subproblem_evaluations.constraint_jacobian, *this->hessian_model->hessian, this->initial_point);
   ActiveSetSubproblem::compute_dual_displacements(problem, current_iterate, direction);
   this->number_subproblems_solved++;
   return direction;
}

PredictedReductionModel QPSubproblem::generate_predicted_reduction_model(const Problem& problem, const Iterate& current_iterate,
      const Direction& direction) const {
   return PredictedReductionModel(-direction.objective, [&]() { // capture this and direction by reference
      // precompute expensive quantities
      const double linear_term = dot(direction.x, current_iterate.subproblem_evaluations.objective_gradient);
      const double quadratic_term = this->hessian_model->hessian->quadratic_product(direction.x, direction.x, problem.number_variables) / 2.;
      // return a function of the step length that cheaply assembles the predicted reduction
      return [=](double step_length) { // capture the expensive quantities by value
         return -step_length * (linear_term + step_length * quadratic_term);
      };
   });
}

size_t QPSubproblem::get_hessian_evaluation_count() const {
   return this->hessian_model->evaluation_count;
}

double QPSubproblem::get_proximal_coefficient() const {
   return this->proximal_coefficient;
}