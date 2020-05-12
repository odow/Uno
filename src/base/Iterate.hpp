#ifndef ITERATE_H
#define ITERATE_H

#include <ostream>
#include <vector>
#include <map>
#include "Problem.hpp"
#include "Constraint.hpp"
#include "Matrix.hpp"

enum OptimalityStatus {
    NOT_OPTIMAL = 0,
    KKT_POINT, /* feasible stationary point */
    FJ_POINT, /* infeasible stationary point */
    FEASIBLE_SMALL_STEP,
    INFEASIBLE_SMALL_STEP
};

struct Residuals {
    double constraints;
    double KKT;
    double FJ;
    double complementarity;
};

std::ostream& operator<<(std::ostream &stream, OptimalityStatus& status);

/*! \class Iterate
 * \brief Optimization iterate
 *
 *  Point and its evaluations during an optimization process
 */
class Iterate {
public:
    /*!
     *  Constructor
     */
    Iterate(std::vector<double>& x, Multipliers& multipliers);

    std::vector<double> x; /*!< \f$\mathbb{R}^n\f$ primal variables */
    Multipliers multipliers; /*!< \f$\mathbb{R}^n\f$ Lagrange multipliers/dual variables */

    // functions
    double objective; /*!< Objective value */
    bool is_objective_computed;

    std::vector<double> constraints; /*!< Constraint values (size \f$m)\f$ */
    bool are_constraints_computed;
    
    std::map<int, double> objective_gradient; /*!< Sparse Jacobian of the objective */
    bool is_objective_gradient_computed; /*!< Flag that indicates if the objective gradient has already been computed */

    std::vector<std::map<int, double> > constraints_jacobian; /*!< Sparse Jacobian of the constraints */
    bool is_constraints_jacobian_computed; /*!< Flag that indicates if the constraint Jacobian has already been computed */

    CSCMatrix hessian; /*!< Sparse Lagrangian Hessian */
    bool is_hessian_computed; /*!< Flag that indicates if the Hessian has already been computed */

    // status and measures
    OptimalityStatus status;

    //double constraint_residual; /*!< Constraint residual */
    //double KKT_residual;
    //double complementarity_residual;
    Residuals residuals;
    //bool are_residuals_computed;

    double feasibility_measure;
    double optimality_measure;

    void compute_objective(Problem& problem);
    void compute_constraints(Problem& problem);
    void set_constraint_residual(double constraint_residual);
    void compute_objective_gradient(Problem& problem);
    void set_objective_gradient(std::map<int, double>& objective_gradient);
    void compute_constraints_jacobian(Problem& problem);
    std::vector<double> lagrangian_gradient(Problem& problem, double objective_multiplier, Multipliers& multipliers);

    /*!
     *  Compute the Hessian in a lazy way: the Hessian is computed only when required and stored
     *  in CSC (Compressed Sparse Column)
     */
    void compute_hessian(Problem& problem, double objective_multiplier, std::vector<double>& constraint_multipliers);

    friend std::ostream& operator<<(std::ostream &stream, Iterate& iterate);
};

#endif // ITERATE_H
