#include <cmath>
#include <cassert>
#include "BQPDSolver.hpp"

#define BIG 1e30

extern "C" {

/* fortran common block used in bqpd/bqpd.f */
extern struct {
   int kk, ll, kkk, lll, mxws, mxlws;
} wsc_;

/* fortran common for inertia correction in wdotd */
extern struct {
   double alpha;
} kktalphac_;

extern void
bqpd_(int* n, int* m, int* k, int* kmax, double* a, int* la, double* x, double* bl, double* bu, double* f, double* fmin, double* g,
      double* r, double* w, double* e, int* ls, double* alp, int* lp, int* mlp, int* peq, double* ws, int* lws, int* mode, int* ifail,
      int* info, int* iprint, int* nout);
}

/* preallocate a bunch of stuff */
BQPDSolver::BQPDSolver(size_t number_variables, size_t number_constraints, size_t maximum_number_nonzeros, bool quadratic_programming)
      : QPSolver(), n_(number_variables), m_(number_constraints), maximum_number_nonzeros_(maximum_number_nonzeros), lb_(n_ + m_),
      ub_(n_ + m_), use_fortran_(1), jacobian_(n_ * (m_ + 1)), jacobian_sparsity_(n_ * (m_ + 1) + m_ + 3),
      kmax_(quadratic_programming ? 500 : 0), mlp_(1000), mxwk0_(2000000), mxiwk0_(500000), info_(100), alp_(mlp_), lp_(mlp_), ls_(n_ + m_),
      w_(n_ + m_), gradient_solution_(n_), residuals_(n_ + m_), e_(n_ + m_),
      size_hessian_sparsity_(quadratic_programming ? maximum_number_nonzeros + n_ + 3 : 0),
      size_hessian_workspace_(maximum_number_nonzeros_ + kmax_ * (kmax_ + 9) / 2 + 2 * n_ + m_ + mxwk0_),
      size_hessian_sparsity_workspace_(size_hessian_sparsity_ + kmax_ + mxiwk0_), hessian_(size_hessian_workspace_),
      hessian_sparsity_(size_hessian_sparsity_workspace_), k_(0), mode_(COLD_START), iprint_(0), nout_(6), fmin_(-1e20) {
   // active set
   for (size_t i = 0; i < this->n_ + this->m_; i++) {
      this->ls_[i] = i + this->use_fortran_;
   }
}

Direction BQPDSolver::solve_QP(std::vector<Range>& variables_bounds, std::vector<Range>& constraints_bounds, SparseVector& linear_objective,
      std::vector<SparseVector>& constraints_jacobian, CSCMatrix& hessian, std::vector<double>& x) {
   /* Hessian */
   for (size_t i = 0; i < hessian.number_nonzeros; i++) {
      this->hessian_[i] = hessian.matrix[i];
   }
   /* Hessian sparsity */
   this->hessian_sparsity_[0] = hessian.number_nonzeros + 1;
   for (size_t i = 0; i < hessian.number_nonzeros; i++) {
      this->hessian_sparsity_[i + 1] = hessian.row_number[i] + (hessian.fortran_indexing ? 0 : this->use_fortran_);
   }
   for (size_t i = 0; i < hessian.dimension + 1; i++) {
      this->hessian_sparsity_[hessian.number_nonzeros + i + 1] =
            hessian.column_start[i] + (hessian.fortran_indexing ? 0 : this->use_fortran_);
   }

   // if extra variables have been introduced, correct hessian.column_start
   int i = hessian.number_nonzeros + hessian.dimension + 2;
   int last_value = hessian.column_start[hessian.dimension];
   for (size_t j = hessian.dimension; j < this->n_; j++) {
      this->hessian_sparsity_[i] = last_value + (hessian.fortran_indexing ? 0 : this->use_fortran_);
      i++;
   }

   print_vector(DEBUG1, this->hessian_sparsity_, 0, i);
   return this->solve_subproblem_(variables_bounds, constraints_bounds, linear_objective, constraints_jacobian, x);
}

Direction BQPDSolver::solve_LP(std::vector<Range>& variables_bounds, std::vector<Range>& constraints_bounds, SparseVector& linear_objective,
      std::vector<SparseVector>& constraints_jacobian, std::vector<double>& x) {
   return this->solve_subproblem_(variables_bounds, constraints_bounds, linear_objective, constraints_jacobian, x);
}

Direction
BQPDSolver::solve_subproblem_(std::vector<Range>& variables_bounds, std::vector<Range>& constraints_bounds, SparseVector& linear_objective,
      std::vector<SparseVector>& constraints_jacobian, std::vector<double>& x) {
   /* initialize wsc_ common block (Hessian & workspace for bqpd) */
   // setting the common block here ensures that several instances of BQPD can run simultaneously
   wsc_.kk = this->maximum_number_nonzeros_;
   wsc_.ll = this->size_hessian_sparsity_;
   wsc_.mxws = this->size_hessian_workspace_;
   wsc_.mxlws = this->size_hessian_sparsity_workspace_;
   kktalphac_.alpha = 0; // inertia control

   DEBUG1 << "objective gradient: ";
   print_vector(DEBUG1, linear_objective);
   for (size_t j = 0; j < constraints_jacobian.size(); j++) {
      DEBUG1 << "gradient c" << j << ": ";
      print_vector(DEBUG1, constraints_jacobian[j]);
   }
   for (size_t i = 0; i < variables_bounds.size(); i++) {
      DEBUG1 << "Δx" << i << " in [" << variables_bounds[i].lb << ", " << variables_bounds[i].ub << "]\n";
   }
   for (size_t j = 0; j < constraints_bounds.size(); j++) {
      DEBUG1 << "linearized c" << j << " in [" << constraints_bounds[j].lb << ", " << constraints_bounds[j].ub << "]\n";
   }

   /* Jacobian */
   int current_index = 0;
   for (const auto[i, derivative]: linear_objective) {
      this->jacobian_[current_index] = derivative;
      this->jacobian_sparsity_[current_index + 1] = i + this->use_fortran_;
      current_index++;
   }
   for (size_t j = 0; j < this->m_; j++) {
      for (const auto[i, derivative]: constraints_jacobian[j]) {
         this->jacobian_[current_index] = derivative;
         this->jacobian_sparsity_[current_index + 1] = i + this->use_fortran_;
         current_index++;
      }
   }
   current_index++;
   this->jacobian_sparsity_[0] = current_index;
   // header
   int size = 1;
   this->jacobian_sparsity_[current_index] = size;
   current_index++;
   size += linear_objective.size();
   this->jacobian_sparsity_[current_index] = size;
   current_index++;
   for (size_t j = 0; j < this->m_; j++) {
      size += constraints_jacobian[j].size();
      this->jacobian_sparsity_[current_index] = size;
      current_index++;
   }

   /* bounds */
   for (size_t i = 0; i < this->n_; i++) {
      this->lb_[i] = (variables_bounds[i].lb == -INFINITY) ? -BIG : variables_bounds[i].lb;
      this->ub_[i] = (variables_bounds[i].ub == INFINITY) ? BIG : variables_bounds[i].ub;
   }
   for (size_t j = 0; j < this->m_; j++) {
      this->lb_[this->n_ + j] = (constraints_bounds[j].lb == -INFINITY) ? -BIG : constraints_bounds[j].lb;
      this->ub_[this->n_ + j] = (constraints_bounds[j].ub == INFINITY) ? BIG : constraints_bounds[j].ub;
   }

   /* call BQPD */
   int mode = (int) this->mode_;
   bqpd_((int*) &this->n_, (int*) &this->m_, &this->k_, &this->kmax_, this->jacobian_.data(), this->jacobian_sparsity_.data(), x.data(),
         this->lb_.data(), this->ub_.data(), &this->f_solution_, &this->fmin_, this->gradient_solution_.data(), this->residuals_.data(),
         this->w_.data(), this->e_.data(), this->ls_.data(), this->alp_.data(), this->lp_.data(), &this->mlp_, &this->peq_solution_,
         this->hessian_.data(), this->hessian_sparsity_.data(), &mode, &this->ifail_, this->info_.data(), &this->iprint_, &this->nout_);

   /* project solution into bounds: it's a ray! */
   for (unsigned int i = 0; i < x.size(); i++) {
      if (x[i] < variables_bounds[i].lb) {
         x[i] = variables_bounds[i].lb;
      }
      else if (variables_bounds[i].ub < x[i]) {
         x[i] = variables_bounds[i].ub;
      }
   }

   Direction direction = this->generate_direction_(x);
   return direction;
}

Direction BQPDSolver::generate_direction_(std::vector<double>& x) {
   Multipliers multipliers(this->n_, this->m_);
   Direction direction(x, multipliers);

   /* active constraints */
   for (size_t j = 0; j < this->n_ - this->k_; j++) {
      size_t index = std::abs(this->ls_[j]) - this->use_fortran_;

      if (index < this->n_) {
         // bound constraint
         if (0 <= this->ls_[j]) { /* lower bound active */
            direction.multipliers.lower_bounds[index] = this->residuals_[index];
            direction.active_set.bounds.at_lower_bound.push_back(index);
         }
         else { /* upper bound active */
            direction.multipliers.upper_bounds[index] = -this->residuals_[index];
            direction.active_set.bounds.at_upper_bound.push_back(index);
         }
      }
      else {
         // general constraint
         int constraint_index = index - this->n_;
         direction.constraint_partition.feasible.push_back(constraint_index);
         direction.constraint_partition.constraint_feasibility[constraint_index] = FEASIBLE;
         if (0 <= this->ls_[j]) { /* lower bound active */
            direction.multipliers.constraints[constraint_index] = this->residuals_[index];
            direction.active_set.constraints.at_lower_bound.push_back(constraint_index);
         }
         else { /* upper bound active */
            direction.multipliers.constraints[constraint_index] = -this->residuals_[index];
            direction.active_set.constraints.at_upper_bound.push_back(constraint_index);
         }
      }
   }

   /* inactive constraints */
   for (size_t j = this->n_ - this->k_; j < this->n_ + this->m_; j++) {
      size_t index = std::abs(this->ls_[j]) - this->use_fortran_;

      if (this->n_ <= index) { // general constraints
         int constraint_index = index - this->n_;
         if (this->residuals_[index] < 0.) { // infeasible constraint
            direction.constraint_partition.infeasible.push_back(constraint_index);
            if (this->ls_[j] < 0) { // upper bound violated
               direction.constraint_partition.constraint_feasibility[constraint_index] = INFEASIBLE_UPPER;
            }
            else { // lower bound violated
               direction.constraint_partition.constraint_feasibility[constraint_index] = INFEASIBLE_LOWER;
            }
         }
         else { // feasible constraint
            direction.constraint_partition.feasible.push_back(constraint_index);
            direction.constraint_partition.constraint_feasibility[constraint_index] = FEASIBLE;
         }
      }
   }
   direction.status = this->int_to_status_(this->ifail_);
   direction.norm = norm_inf(x);
   direction.objective = this->f_solution_;
   return direction;
}

Status BQPDSolver::int_to_status_(int ifail) {
   assert(0 <= ifail && ifail <= 9 && "BQPDSolver.int_to_status: ifail does not belong to [0, 9]");
   Status status = static_cast<Status> (ifail);
   return status;
}