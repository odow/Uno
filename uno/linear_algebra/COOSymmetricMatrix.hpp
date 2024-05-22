// Copyright (c) 2018-2024 Charlie Vanaret
// Licensed under the MIT license. See LICENSE file in the project directory for details.

#ifndef UNO_COOSYMMETRICMATRIX_H
#define UNO_COOSYMMETRICMATRIX_H

#include <ostream>
#include <cassert>
#include "SymmetricMatrix.hpp"
#include "tools/Infinity.hpp"

/*
 * Coordinate list
 * https://en.wikipedia.org/wiki/Sparse_matrix#Coordinate_list_(COO)
 */
template <typename IndexType, typename ElementType>
class COOSymmetricMatrix: public SymmetricMatrix<IndexType, ElementType> {
public:
   class View {
   public:
      View(const COOSymmetricMatrix<IndexType, ElementType>& matrix, size_t start, size_t end): matrix(matrix), start(start), end(end) {
         assert(start <= end && "COOSymmetricMatrix::view: start > end");
         assert(end <= matrix.number_nonzeros && "COOSymmetricMatrix::view: end > NNZ");
      }

   protected:
      const COOSymmetricMatrix<IndexType, ElementType>& matrix;
      size_t start;
      size_t end;
   };

   COOSymmetricMatrix(size_t dimension, size_t original_capacity, bool use_regularization);

   void reset() override;
   void insert(ElementType element, IndexType row_index, IndexType column_index) override;
   void finalize_column(IndexType /*column_index*/) override { /* do nothing */ }
   [[nodiscard]] ElementType smallest_diagonal_entry() const override;
   void set_regularization(const std::function<ElementType(IndexType index)>& regularization_function) override;

   [[nodiscard]] const IndexType* row_indices_pointer() const { return this->row_indices.data(); }
   [[nodiscard]] const IndexType* column_indices_pointer() const { return this->column_indices.data(); }

   [[nodiscard]] View view(size_t start, size_t end) const { return {*this, start, end}; }

   void print(std::ostream& stream) const override;

   static COOSymmetricMatrix<IndexType, ElementType> zero(size_t dimension);

protected:
   std::vector<IndexType> row_indices;
   std::vector<IndexType> column_indices;

   void initialize_regularization();

   // iterator functions
   [[nodiscard]] std::tuple<size_t, size_t, ElementType> dereference_iterator(size_t column_index, size_t nonzero_index) const override;
   void increment_iterator(size_t& column_index, size_t& nonzero_index) const override;
};

// implementation

template <typename IndexType, typename ElementType>
COOSymmetricMatrix<IndexType, ElementType>::COOSymmetricMatrix(size_t dimension, size_t original_capacity, bool use_regularization):
      SymmetricMatrix<IndexType, ElementType>(dimension, original_capacity, use_regularization) {
   this->row_indices.reserve(this->capacity);
   this->column_indices.reserve(this->capacity);

   // initialize regularization terms
   if (this->use_regularization) {
      this->initialize_regularization();
   }
}

template <typename IndexType, typename ElementType>
void COOSymmetricMatrix<IndexType, ElementType>::reset() {
   // empty the matrix
   SymmetricMatrix<IndexType, ElementType>::reset();
   this->row_indices.clear();
   this->column_indices.clear();

   // initialize regularization terms
   if (this->use_regularization) {
      this->initialize_regularization();
   }
}

template <typename IndexType, typename ElementType>
void COOSymmetricMatrix<IndexType, ElementType>::insert(ElementType element, IndexType row_index, IndexType column_index) {
   assert(this->number_nonzeros <= this->row_indices.size() && "The COO matrix doesn't have a sufficient capacity");
   this->entries.push_back(element);
   this->row_indices.push_back(row_index);
   this->column_indices.push_back(column_index);
   this->number_nonzeros++;
}

template <typename IndexType, typename ElementType>
ElementType COOSymmetricMatrix<IndexType, ElementType>::smallest_diagonal_entry() const {
   ElementType smallest_entry = INF<ElementType>;
   for (size_t index: Range(this->number_nonzeros)) {
      if (this->row_indices[index] == this->column_indices[index]) {
         smallest_entry = std::min(smallest_entry, this->entries[index]);
      }
   }
   // if no explicit diagonal term, it is 0
   if (smallest_entry == INF<ElementType>) {
      smallest_entry = ElementType(0);
   }
   return smallest_entry;
}

template <typename IndexType, typename ElementType>
void COOSymmetricMatrix<IndexType, ElementType>::set_regularization(const std::function<ElementType(IndexType /*index*/)>& regularization_function) {
   assert(this->use_regularization && "You are trying to regularize a matrix where regularization was not preallocated.");

   // the regularization terms (that lie at the start of the entries vector) can be directly modified
   for (size_t row_index: Range(this->dimension)) {
      const ElementType element = regularization_function(IndexType(row_index));
      this->entries[row_index] = element;
   }
}

template <typename IndexType, typename ElementType>
void COOSymmetricMatrix<IndexType, ElementType>::print(std::ostream& stream) const {
   for (const auto [row_index, column_index, element]: *this) {
      stream << "m(" << row_index << ", " << column_index << ") = " << element << '\n';
   }
}

template <typename IndexType, typename ElementType>
void COOSymmetricMatrix<IndexType, ElementType>::initialize_regularization() {
   // introduce elements at the start of the entries
   for (size_t row_index: Range(this->dimension)) {
      this->insert(ElementType(0), IndexType(row_index), IndexType(row_index));
   }
}

template <typename IndexType, typename ElementType>
std::tuple<size_t, size_t, ElementType> COOSymmetricMatrix<IndexType, ElementType>::dereference_iterator(size_t /*column_index*/, size_t nonzero_index) const {
   return {this->row_indices[nonzero_index], this->column_indices[nonzero_index], this->entries[nonzero_index]};
}

template <typename IndexType, typename ElementType>
void COOSymmetricMatrix<IndexType, ElementType>::increment_iterator(size_t& column_index, size_t& nonzero_index) const {
   nonzero_index++;
   // if end reached
   if (nonzero_index == this->number_nonzeros) {
      column_index = this->dimension;
   }
}

template <typename IndexType, typename ElementType>
COOSymmetricMatrix<IndexType, ElementType> COOSymmetricMatrix<IndexType, ElementType>::zero(size_t dimension) {
   return {dimension, 0, false};
}

#endif // UNO_COOSYMMETRICMATRIX_H
