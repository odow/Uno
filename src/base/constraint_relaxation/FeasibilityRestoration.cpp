#include <cassert>
#include "FeasibilityRestoration.hpp"
#include "GlobalizationStrategyFactory.hpp"
#include "SubproblemFactory.hpp"

FeasibilityRestoration::FeasibilityRestoration(const Problem& problem, const std::map<std::string, std::string>& options, bool use_trust_region) :
   ConstraintRelaxationStrategy(SubproblemFactory::create(problem, problem.number_variables, options.at("subproblem"),
         options, use_trust_region)),
   /* create the globalization strategy */
   phase_1_strategy(GlobalizationStrategyFactory::create(options.at("strategy"), options)),
   phase_2_strategy(GlobalizationStrategyFactory::create(options.at("strategy"), options)),
   current_phase(OPTIMALITY) {
}

Iterate FeasibilityRestoration::initialize(Statistics& statistics, const Problem& problem, std::vector<double>& x, Multipliers& multipliers) {
   statistics.add_column("phase", Statistics::int_width, 4);

   /* initialize the subproblem */
   Iterate first_iterate = this->subproblem->generate_initial_iterate(statistics, problem, x, multipliers);
   this->subproblem->compute_residuals(problem, first_iterate, 1.);

   this->phase_1_strategy->initialize(statistics, first_iterate);
   this->phase_2_strategy->initialize(statistics, first_iterate);
   return first_iterate;
}

void FeasibilityRestoration::generate_subproblem(const Problem& problem, Iterate& current_iterate, double trust_region_radius) {
   // simply generate the subproblem
   this->subproblem->generate(problem, current_iterate, problem.objective_sign, trust_region_radius);
}

Direction FeasibilityRestoration::compute_feasible_direction(Statistics& statistics, const Problem& problem, Iterate& current_iterate) {
   // solve the original subproblem
   Direction direction = this->subproblem->compute_direction(statistics, problem, current_iterate);

   if (direction.status != INFEASIBLE) {
      direction.objective_multiplier = problem.objective_sign;
      return direction;
   }
   else {
      ConstraintPartition constraint_partition = direction.constraint_partition;
      // infeasible subproblem: form the feasibility problem
      this->form_feasibility_problem(problem, current_iterate, direction.x, constraint_partition);
      // solve the feasibility subproblem
      direction = this->subproblem->compute_direction(statistics, problem, current_iterate);
      direction.objective_multiplier = 0.;
      direction.constraint_partition = constraint_partition;
      direction.is_relaxed = true;
      return direction;
   }
}

double FeasibilityRestoration::compute_predicted_reduction(const Problem& /*problem*/, Iterate& /*current_iterate*/, const Direction& direction,
      double step_length) {
   // the predicted reduction is simply that of the subproblem (the objective multiplier was set accordingly)
   return this->subproblem->compute_predicted_reduction(direction, step_length);
}

void FeasibilityRestoration::form_feasibility_problem(const Problem& problem, const Iterate& current_iterate, const std::vector<double>&
      phase_2_direction, const ConstraintPartition& constraint_partition) {
   // set the multipliers of the violated constraints
   FeasibilityRestoration::set_restoration_multipliers(this->subproblem->constraints_multipliers, constraint_partition);
   // compute the objective gradient and (possibly) Hessian
   this->subproblem->update_objective_multiplier(problem, current_iterate, 0.);
   this->subproblem->compute_feasibility_linear_objective(current_iterate, constraint_partition);
   this->subproblem->generate_feasibility_bounds(problem, current_iterate.constraints, constraint_partition);
   this->subproblem->set_initial_point(phase_2_direction);
}

Direction FeasibilityRestoration::solve_feasibility_problem(Statistics& statistics, const Problem& problem, Iterate& current_iterate, Direction& direction) {
   assert(this->current_phase == OPTIMALITY && "FeasibilityRestoration is already in the feasibility restoration phase");
   this->form_feasibility_problem(problem, current_iterate, direction.x, direction.constraint_partition);
   return this->subproblem->compute_direction(statistics, problem, current_iterate);
}

bool FeasibilityRestoration::is_acceptable(Statistics& statistics, const Problem& problem, Iterate& current_iterate, Iterate& trial_iterate,
      const Direction& direction, double step_length) {
   // check if subproblem definition changed
   if (this->subproblem->subproblem_definition_changed) {
      this->phase_2_strategy->reset();
      this->subproblem->subproblem_definition_changed = false;
      this->subproblem->compute_progress_measures(problem, current_iterate);
   }
   double step_norm = step_length * direction.norm;

   bool accept = false;
   if (step_norm == 0.) {
      accept = true;
   }
   else {
      /* possibly go from 1 (restoration) to phase 2 (optimality) */
      if (!direction.is_relaxed && this->current_phase == FEASIBILITY_RESTORATION) {
         // TODO && this->filter_optimality->accept(trial_iterate.progress.feasibility, trial_iterate.progress.objective))
         this->current_phase = OPTIMALITY;
         DEBUG << "Switching from restoration to optimality phase\n";
         this->subproblem->compute_progress_measures(problem, current_iterate);
      }
      /* possibly go from phase 2 (optimality) to 1 (restoration) */
      else if (direction.is_relaxed && this->current_phase == OPTIMALITY) {
         this->current_phase = FEASIBILITY_RESTORATION;
         DEBUG << "Switching from optimality to restoration phase\n";
         this->phase_2_strategy->notify(current_iterate);
         this->phase_1_strategy->reset();
         this->compute_infeasibility_measures(problem, current_iterate, direction.constraint_partition);
         this->phase_1_strategy->notify(current_iterate);
      }

      if (this->current_phase == FEASIBILITY_RESTORATION) {
         // if restoration phase, recompute progress measures of trial point
         this->compute_infeasibility_measures(problem, trial_iterate, direction.constraint_partition);
      }
      else {
         this->subproblem->compute_progress_measures(problem, trial_iterate);
      }

      // evaluate the predicted reduction
      double predicted_reduction = this->compute_predicted_reduction(problem, current_iterate, direction, step_length);
      // pick the current strategy
      GlobalizationStrategy& strategy = (this->current_phase == OPTIMALITY) ? *this->phase_2_strategy : *this->phase_1_strategy;
      // invoke the globalization strategy for acceptance
      accept = strategy.check_acceptance(statistics, current_iterate.progress, trial_iterate.progress,
            direction.objective_multiplier, predicted_reduction);
   }

   if (accept) {
      statistics.add_statistic("phase", (int) direction.is_relaxed ? FEASIBILITY_RESTORATION : OPTIMALITY);
      if (direction.is_relaxed) {
         /* correct multipliers for infeasibility problem */
         FeasibilityRestoration::set_restoration_multipliers(trial_iterate.multipliers.constraints, direction.constraint_partition);
      }
      // compute the residuals
      trial_iterate.compute_objective(problem);
      this->subproblem->compute_residuals(problem, trial_iterate, direction.objective_multiplier);
   }
   return accept;
}

void FeasibilityRestoration::set_restoration_multipliers(std::vector<double>& constraints_multipliers, const ConstraintPartition&
constraint_partition) {
   for (int j: constraint_partition.infeasible) {
      if (constraint_partition.constraint_feasibility[j] == INFEASIBLE_LOWER) {
         constraints_multipliers[j] = 1.;
      }
      else { // constraint_partition.constraint_feasibility[j] == INFEASIBLE_UPPER
         constraints_multipliers[j] = -1.;
      }
   }
   // otherwise, leave the multiplier as it is
}

void FeasibilityRestoration::compute_infeasibility_measures(const Problem& problem, Iterate& iterate, const ConstraintPartition& constraint_partition) {
   iterate.compute_constraints(problem);
   // feasibility measure: residual of all constraints
   double feasibility = problem.compute_constraint_violation(iterate.constraints, L1_NORM);
   // optimality measure: residual of linearly infeasible constraints
   double objective = problem.compute_constraint_violation(iterate.constraints, constraint_partition.infeasible, L1_NORM);
   iterate.progress = {feasibility, objective};
}