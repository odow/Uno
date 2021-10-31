#include <cassert>
#include "l1Relaxation.hpp"
#include "ingredients/strategy/GlobalizationStrategyFactory.hpp"
#include "ingredients/subproblem/SubproblemFactory.hpp"

l1Relaxation::l1Relaxation(Problem& problem, Subproblem& subproblem, const l1RelaxationParameters& parameters, const Options& options) :
      ConstraintRelaxationStrategy(problem, subproblem),
      number_elastic_variables(l1Relaxation::count_elastic_variables(problem)),
      globalization_strategy(GlobalizationStrategyFactory::create(options.at("strategy"), options)),
      penalty_parameter(parameters.initial_parameter),
      parameters(parameters) {
}

void l1Relaxation::initialize(Statistics& statistics, const Problem& problem, Iterate& first_iterate) {
   statistics.add_column("penalty param.", Statistics::double_width, 4);

   // initialize the subproblem
   this->subproblem.initialize(statistics, problem, first_iterate);

   Subproblem::compute_optimality_conditions(problem, first_iterate, this->penalty_parameter);
   this->globalization_strategy->initialize(statistics, first_iterate);
}

void l1Relaxation::create_current_subproblem(const Problem& problem, Iterate& current_iterate, double trust_region_radius) {
   // scale the derivatives and introduce the elastic variables
   this->subproblem.create_current_subproblem(problem, current_iterate, this->penalty_parameter, trust_region_radius);
   this->add_elastic_variables_to_subproblem();
}

Direction l1Relaxation::compute_feasible_direction(Statistics& statistics, const Problem& problem, Iterate& current_iterate) {
   DEBUG << "penalty parameter: " << this->penalty_parameter << "\n";
   // use Byrd's steering rules to update the penalty parameter and compute descent directions
   Direction direction = this->solve_with_steering_rule(statistics, problem, current_iterate);

   // remove the temporary elastic variables from the direction
   this->remove_elastic_variables_from_direction(problem, direction);
   return direction;
}

Direction l1Relaxation::compute_second_order_correction(const Problem& problem, Iterate& trial_iterate) {
   Direction direction = ConstraintRelaxationStrategy::compute_second_order_correction(problem, trial_iterate);

   // remove the temporary elastic variables from the direction
   this->remove_elastic_variables_from_direction(problem, direction);
   return direction;
}

double l1Relaxation::compute_predicted_reduction(const Problem& problem, Iterate& current_iterate, const Direction& direction, PredictedReductionModel&
predicted_reduction_model, double step_length) {
   // compute the predicted reduction of the l1 relaxation as a postprocessing of the predicted reduction of the subproblem
   if (step_length == 1.) {
      return current_iterate.errors.constraints + predicted_reduction_model.evaluate(step_length);
   }
   else {
      // determine the linearized constraint violation term: c(x_k) + alpha*\nabla c(x_k)^T d
      const auto residual_function = [&](size_t j) {
         const double component_j = current_iterate.constraints[j] + step_length * dot(direction.x, current_iterate.constraints_jacobian[j]);
         return problem.compute_constraint_violation(component_j, j);
      };
      const double linearized_constraint_violation = norm_1(residual_function, problem.number_constraints);
      return current_iterate.errors.constraints - linearized_constraint_violation + predicted_reduction_model.evaluate(step_length);
   }
}

Direction l1Relaxation::solve_feasibility_problem(Statistics& statistics, const Problem& problem, Iterate& current_iterate, const Direction&
/*phase_2_direction*/) {
   assert(0. < this->penalty_parameter && "l1Relaxation: the penalty parameter is already 0");

   Direction direction = this->resolve_subproblem(statistics, problem, current_iterate, 0.);
   // remove the temporary elastic variables
   this->remove_elastic_variables_from_direction(problem, direction);
   return direction;
}

bool l1Relaxation::is_acceptable(Statistics& statistics, const Problem& problem, Iterate& current_iterate, Iterate& trial_iterate,
      const Direction& direction, PredictedReductionModel& predicted_reduction_model, double step_length) {
   // check if subproblem definition changed
   if (this->subproblem.subproblem_definition_changed) {
      this->globalization_strategy->reset();
      this->subproblem.subproblem_definition_changed = false;
      this->subproblem.compute_progress_measures(problem, current_iterate);
   }

   bool accept = false;
   if (direction.norm == 0.) {
      accept = true;
   }
   else {
      this->subproblem.compute_progress_measures(problem, trial_iterate);

      // compute the predicted reduction (both the subproblem and the l1 relaxation strategy contribute)
      const double predicted_reduction = this->compute_predicted_reduction(problem, current_iterate, direction, predicted_reduction_model, step_length);
      // invoke the globalization strategy for acceptance
      accept = this->globalization_strategy->check_acceptance(statistics, current_iterate.progress, trial_iterate.progress,
            this->penalty_parameter, predicted_reduction);
   }
   if (accept) {
      statistics.add_statistic("penalty param.", this->penalty_parameter);
      Subproblem::compute_optimality_conditions(problem, trial_iterate, direction.objective_multiplier);
   }
   return accept;
}

Direction l1Relaxation::solve_with_steering_rule(Statistics& statistics, const Problem& problem, Iterate& current_iterate) {
   // stage a: compute the step within trust region
   Direction direction = this->solve_subproblem(statistics, problem, current_iterate);

   // penalty update: if penalty parameter is already 0, no need to decrease it
   if (0. < this->penalty_parameter) {
      // check infeasibility
      double linearized_residual = this->compute_linearized_constraint_residual(direction.x);
      DEBUG << "Linearized residual mk(dk): " << linearized_residual << "\n\n";

      // if problem had to be relaxed
      if (linearized_residual != 0.) {
         const double current_penalty_parameter = this->penalty_parameter;

         // stage c: compute the lowest possible constraint violation (penalty = 0)
         DEBUG << "Compute ideal solution (param = 0):\n";
         Direction direction_lowest_violation = this->resolve_subproblem(statistics, problem, current_iterate, 0.);
         const double residual_lowest_violation = this->compute_linearized_constraint_residual(direction_lowest_violation.x);
         DEBUG << "Ideal linearized residual mk(dk): " << residual_lowest_violation << "\n\n";

         if (!(0. < current_iterate.errors.constraints && residual_lowest_violation == current_iterate.errors.constraints)) {
            // compute the ideal error (with a zero penalty parameter)
            const double error_lowest_violation = l1Relaxation::compute_error(problem, current_iterate, direction_lowest_violation.multipliers, 0.);
            DEBUG << "Ideal error: " << error_lowest_violation << "\n";
            if (error_lowest_violation == 0.) {
               // stage f: update the penalty parameter
               this->penalty_parameter = 0.;
               direction = direction_lowest_violation;
            }
            else {
               // stage f: update the penalty parameter
               const double updated_penalty_parameter = this->penalty_parameter;
               const double term = error_lowest_violation / std::max(1., current_iterate.errors.constraints);
               this->penalty_parameter = std::min(this->penalty_parameter, term * term);
               if (this->penalty_parameter < updated_penalty_parameter) {
                  if (this->penalty_parameter == 0.) {
                     direction = direction_lowest_violation;
                  }
                  else {
                     direction = this->resolve_subproblem(statistics, problem, current_iterate, this->penalty_parameter);
                  }
               }

               // decrease penalty parameter to satisfy 2 conditions
               bool condition1 = false, condition2 = false;
               while (!condition2) {
                  if (!condition1) {
                     // stage d: reach a fraction of the ideal decrease
                     if ((residual_lowest_violation == 0. && linearized_residual == 0) || (residual_lowest_violation != 0. &&
                     current_iterate.errors.constraints - linearized_residual >= this->parameters.epsilon1 *
                     (current_iterate.errors.constraints - residual_lowest_violation))) {
                        condition1 = true;
                        DEBUG << "Condition 1 is true\n";
                     }
                  }
                  // stage e: further decrease penalty parameter if necessary
                  if (condition1 && current_iterate.errors.constraints - direction.objective >=
                                    this->parameters.epsilon2 * (current_iterate.errors.constraints - direction_lowest_violation.objective)) {
                     condition2 = true;
                     DEBUG << "Condition 2 is true\n";
                  }
                  if (!condition2) {
                     this->penalty_parameter /= this->parameters.decrease_factor;
                     if (this->penalty_parameter < 1e-10) {
                        this->penalty_parameter = 0.;
                        condition2 = true;
                     }
                     else {
                        DEBUG << "\nAttempting to solve with penalty parameter " << this->penalty_parameter << "\n";
                        direction = this->resolve_subproblem(statistics, problem, current_iterate, this->penalty_parameter);
   
                        linearized_residual = this->compute_linearized_constraint_residual(direction.x);
                        DEBUG << "Linearized residual mk(dk): " << linearized_residual << "\n\n";
                     }
                  }
               }
            } // end else
         }

         if (this->penalty_parameter < current_penalty_parameter) {
            DEBUG << "\n*** Penalty parameter updated to " << this->penalty_parameter << "\n";
            this->globalization_strategy->reset();
         }
      }
   }
   return direction;
}

Direction l1Relaxation::solve_subproblem(Statistics& statistics, const Problem& problem, Iterate& current_iterate) {
   Direction direction = this->subproblem.solve(statistics, problem, current_iterate);
   if (direction.constraint_partition.has_value()) {
      const ConstraintPartition& constraint_partition = direction.constraint_partition.value();
      assert(constraint_partition.infeasible.empty() && "solve_subproblem: infeasible constraints found, although direction is feasible");
   }
   direction.objective_multiplier = this->penalty_parameter;
   DEBUG << "\n" << direction;

   // remove the temporary elastic variables
   this->remove_elastic_variables_from_subproblem();
   return direction;
}

Direction l1Relaxation::resolve_subproblem(Statistics& statistics, const Problem& problem, Iterate& current_iterate, double objective_multiplier) {
   this->subproblem.build_objective_model(problem, current_iterate, objective_multiplier);
   this->add_elastic_variables_to_subproblem();

   Direction direction = this->subproblem.solve(statistics, problem, current_iterate);
   if (direction.constraint_partition.has_value()) {
      const ConstraintPartition& constraint_partition = direction.constraint_partition.value();
      assert(constraint_partition.infeasible.empty() && "resolve_subproblem: infeasible constraints found, although direction is feasible");
   }
   direction.objective_multiplier = objective_multiplier;
   DEBUG << "\n" << direction;

   // remove the temporary elastic variables
   this->remove_elastic_variables_from_subproblem();
   return direction;
}

size_t l1Relaxation::get_max_number_variables(const Problem& problem) {
   return problem.number_variables + l1Relaxation::count_elastic_variables(problem);
}

double l1Relaxation::compute_linearized_constraint_residual(std::vector<double>& direction) const {
   double residual = 0.;
   // l1 residual of the linearized constraints: sum of elastic variables
   auto add_variable_contribution = [&](size_t i) {
      residual += direction[i];
   };
   this->elastic_variables.positive.for_each_value(add_variable_contribution);
   this->elastic_variables.negative.for_each_value(add_variable_contribution);
   return residual;
}

// measure that combines KKT error and complementarity error
double l1Relaxation::compute_error(const Problem& problem, Iterate& iterate, Multipliers& multipliers, double current_penalty_parameter) {
   // complementarity error
   double error = Subproblem::compute_complementarity_error(problem, iterate, multipliers);
   // KKT error
   iterate.evaluate_lagrangian_gradient(problem, current_penalty_parameter, multipliers);
   error += norm_1(iterate.lagrangian_gradient);
   return error;
}