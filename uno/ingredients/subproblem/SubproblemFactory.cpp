// Copyright (c) 2018-2023 Charlie Vanaret
// Licensed under the MIT license. See LICENSE file in the project directory for details.

#include "SubproblemFactory.hpp"
#include "ingredients/subproblem/active_set/QPSubproblem.hpp"
#include "ingredients/subproblem/active_set/LPSubproblem.hpp"
#include "ingredients/subproblem/interior_point/PrimalDualInteriorPointSubproblem.hpp"
#include "solvers/QP/QPSolverFactory.hpp"
#include "solvers/linear/SymmetricIndefiniteLinearSolverFactory.hpp"

std::unique_ptr<Subproblem> SubproblemFactory::create(Statistics& statistics, size_t max_number_variables, size_t max_number_constraints,
      size_t max_number_hessian_nonzeros, const Options& options) {
   const std::string subproblem_type = options.get_string("subproblem");
   // active-set methods
   if (subproblem_type == "QP") {
      return std::make_unique<QPSubproblem>(statistics, max_number_variables, max_number_constraints, max_number_hessian_nonzeros, options);
   }
   else if (subproblem_type == "LP") {
      return std::make_unique<LPSubproblem>(max_number_variables, max_number_constraints, options);
   }
   // interior-point method
   else if (subproblem_type == "primal_dual_interior_point") {
      return std::make_unique<PrimalDualInteriorPointSubproblem>(statistics, max_number_variables, max_number_constraints,
            max_number_hessian_nonzeros, options);
   }
   throw std::invalid_argument("Subproblem method " + subproblem_type + " is not supported");
}

std::vector<std::string> SubproblemFactory::available_strategies() {
   std::vector<std::string> strategies{};
   if (not QPSolverFactory::available_solvers().empty()) {
      strategies.emplace_back("QP");
      strategies.emplace_back("LP");
   }
   if (not SymmetricIndefiniteLinearSolverFactory::available_solvers().empty()) {
      strategies.emplace_back("primal-dual interior-point");
   }
   return strategies;
}