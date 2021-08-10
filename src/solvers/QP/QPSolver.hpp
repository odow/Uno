#ifndef QPSOLVER_H
#define QPSOLVER_H

#include <vector>
#include "LPSolver.hpp"
#include "Direction.hpp"
#include "CSCSymmetricMatrix.hpp"

/*! \class QPSolver
 * \brief QP solver
 *
 */
class QPSolver : public LPSolver {
public:
   QPSolver() = default;
   ~QPSolver() override = default;
   virtual Direction solve_QP(const std::vector<Range>& variables_bounds, const std::vector<Range>& constraints_bounds, const SparseVector& linear_objective,
         const std::vector<SparseVector>& constraints_jacobian, const CSCSymmetricMatrix& hessian, const std::vector<double>& initial_point) = 0;
   Direction solve_LP(const std::vector<Range>& variables_bounds, const std::vector<Range>& constraints_bounds, const SparseVector& linear_objective,
         const std::vector<SparseVector>& constraints_jacobian, const std::vector<double>& initial_point) override = 0;
};

#endif // QPSOLVER_H
