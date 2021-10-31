#ifndef SQP_H
#define SQP_H

#include "Subproblem.hpp"
#include "HessianModel.hpp"
#include "solvers/QP/QPSolver.hpp"

class SQP : public Subproblem {
public:
   SQP(const Problem& problem, size_t max_number_variables, size_t number_constraints, const std::string& hessian_model,
         const std::string& QP_solver_name, const std::string& sparse_format, bool use_trust_region);

   void create_current_subproblem(const Problem& problem, Iterate& current_iterate, double objective_multiplier, double trust_region_radius) override;
   void build_objective_model(const Problem& problem, Iterate& current_iterate, double objective_multiplier) override;
   void set_initial_point(const std::vector<double>& point) override;
   Direction solve(Statistics& statistics, const Problem& problem, Iterate& current_iterate) override;
   [[nodiscard]] PredictedReductionModel generate_predicted_reduction_model(const Problem& problem, const Direction& direction) const override;
   [[nodiscard]] size_t get_hessian_evaluation_count() const override;

protected:
   // use pointers to allow polymorphism
   const std::unique_ptr<QPSolver> solver; /*!< Solver that solves the subproblem */
   const std::unique_ptr<HessianModel> hessian_model; /*!< Strategy to evaluate or approximate the
 * Hessian */
   std::vector<double> initial_point;
};

#endif // SQP_H
