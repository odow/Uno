// Copyright (c) 2018-2024 Charlie Vanaret
// Licensed under the MIT license. See LICENSE file in the project directory for details.

#ifndef UNO_SYMMETRICMATRIX_H
#define UNO_SYMMETRICMATRIX_H

#include <functional>
#include <cassert>
#include <memory>
#include "SparseStorage.hpp"
#include "SparseStorageFactory.hpp"

namespace uno {
   template <typename IndexType, typename ElementType>
   class SymmetricMatrix {
   public:
      using index_type = IndexType;
      using value_type = ElementType;

      explicit SymmetricMatrix(std::unique_ptr<SparseStorage<IndexType, ElementType>> sparse_storage);
      SymmetricMatrix(size_t dimension, size_t capacity, bool use_regularization, const std::string& sparse_format);
      ~SymmetricMatrix() = default;

      void reset() { this->sparse_storage->reset(); }
      [[nodiscard]] size_t dimension() const { return this->sparse_storage->get_number_rows(); }
      void set_dimension(size_t new_dimension) {
         this->sparse_storage->set_number_rows(new_dimension);
         this->sparse_storage->set_number_columns(new_dimension);
      }
      [[nodiscard]] size_t number_nonzeros() const { return this->sparse_storage->get_number_nonzeros(); }
      [[nodiscard]] size_t capacity() const { return this->sparse_storage->get_capacity(); }

      template <typename Vector1, typename Vector2>
      ElementType quadratic_product(const Vector1& x, const Vector2& y) const;

      // build the matrix incrementally
      void insert(ElementType term, IndexType row_index, IndexType column_index) {
         this->sparse_storage->insert(term, row_index, column_index);
      }
      void finalize_column(IndexType column_index) { this->sparse_storage->finalize_column(column_index); }
      [[nodiscard]] ElementType smallest_diagonal_entry(size_t max_dimension) const;
      void set_regularization(const std::function<ElementType(IndexType /*index*/)>& regularization_function) {
         this->sparse_storage->set_regularization(regularization_function);
      }

      const SparseStorage<IndexType, ElementType>* get_sparse_storage() const { return this->sparse_storage.get(); }

      typename SparseStorage<IndexType, ElementType>::iterator begin() const {
         return this->sparse_storage->begin();
      }
      typename SparseStorage<IndexType, ElementType>::iterator end() const {
         return this->sparse_storage->end();
      }

      static SymmetricMatrix<IndexType, ElementType> zero(size_t dimension) {
         return {dimension, 0, false, "COO"};
      }

      void print(std::ostream& stream) const { this->sparse_storage->print(stream); }
      template <typename Index, typename Element>
      friend std::ostream& operator<<(std::ostream& stream, const SymmetricMatrix<Index, Element>& matrix);

   protected:
      std::unique_ptr<SparseStorage<IndexType, ElementType>> sparse_storage;
   };

   // implementation
   template <typename IndexType, typename ElementType>
   SymmetricMatrix<IndexType, ElementType>::SymmetricMatrix(std::unique_ptr<SparseStorage<IndexType, ElementType>> sparse_storage) :
         sparse_storage(std::move(sparse_storage)) { }

   template <typename IndexType, typename ElementType>
   SymmetricMatrix<IndexType, ElementType>::SymmetricMatrix(size_t dimension, size_t capacity, bool use_regularization, const std::string& sparse_format) :
         sparse_storage(SparseStorageFactory<IndexType, ElementType>::create(sparse_format, dimension, dimension, capacity, use_regularization)) { }

   template <typename IndexType, typename ElementType>
   // TODO fix. We need to scan through all the columns
   ElementType SymmetricMatrix<IndexType, ElementType>::smallest_diagonal_entry(size_t max_dimension) const {
      ElementType smallest_entry = INF<ElementType>;
      for (const auto [row_index, column_index, element]: *this) {
         if (row_index == column_index && row_index < max_dimension) {
            smallest_entry = std::min(smallest_entry, element);
         }
      }
      if (smallest_entry == INF<ElementType>) {
         smallest_entry = ElementType(0);
      }
      return smallest_entry;
   }

   template <typename IndexType, typename ElementType>
   template <typename Vector1, typename Vector2>
   ElementType SymmetricMatrix<IndexType, ElementType>::quadratic_product(const Vector1& x, const Vector2& y) const {
      static_assert(std::is_same_v<typename Vector1::value_type, ElementType>);
      static_assert(std::is_same_v<typename Vector2::value_type, ElementType>);
      assert(x.size() == y.size() && "SymmetricMatrix::quadratic_product: the two vectors x and y do not have the same size");

      ElementType result = ElementType(0);
      for (const auto [row_index, column_index, element]: *this) {
         if (row_index == column_index) {
            // diagonal term
            result += element * x[row_index] * y[row_index];
         }
         else {
            // off-diagonal term
            result += element * (x[row_index] * y[column_index] + x[column_index] * y[row_index]);
         }
      }
      return result;
   }

   template <typename Index, typename Element>
   std::ostream& operator<<(std::ostream& stream, const SymmetricMatrix<Index, Element>& matrix) {
      stream << "Dimension: " << matrix.dimension() << ", number of nonzeros: " << matrix.number_nonzeros() << '\n';
      matrix.print(stream);
      return stream;
   }
} // namespace

#endif // UNO_SYMMETRICMATRIX_H