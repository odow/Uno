#include <gtest/gtest.h>
#include <math.h>
 
double squareRoot(const double a) {
    double b = sqrt(a);
    if(b != b) { // NaN check
        return -1.0;
    }
    else{
        return sqrt(a);
    }
}

// https://www.eriksmistad.no/getting-started-with-google-test-on-ubuntu/

TEST(SquareRootTest, Positive) { 
    ASSERT_EQ(squareRoot(36.0), 6.);
    ASSERT_EQ(squareRoot(324.0), 18.);
    ASSERT_EQ(squareRoot(645.16), 25.4);
    ASSERT_EQ(squareRoot(0.0), 0.);
}
 
TEST(SquareRootTest, Negative) {
    ASSERT_EQ(squareRoot(-15.0), -1.);
    ASSERT_EQ(squareRoot(-0.2), -1.);
}
 
int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

//void test_factorization() {
//    int fortran_indexing = 1;
//    int n = 2;
//    COOMatrix coo_matrix(n, fortran_indexing);
//    coo_matrix.add_term(6050.0001, 0, 0);
//    coo_matrix.add_term(-2774, 0, 1);
//    coo_matrix.add_term(1e-4, 1, 1);
//    
//    MA57Solver solver;
//    MA57Factorization factorization = solver.factorize(coo_matrix);
//    std::cout << "Dimension: " << coo_matrix.dimension << "\n";
//    std::cout << "Singular ? " << factorization.matrix_is_singular() << "\n";
//    std::cout << "Rank ? " << factorization.rank() << "\n";
//    std::cout << "Negative eigenvalues ? " << factorization.number_negative_eigenvalues() << "\n";
//}

//void test_mask_matrix() {
//    int n = 4;
//    UnoMatrix matrix(n, 0);
//
//    // Column 0
//    matrix.insert(1., 0, 0);
//    // Column 1
//    matrix.insert(2., 0, 1);
//    matrix.insert(3., 1, 1);
//    // Column 2
//    matrix.insert(4., 0, 2);
//    matrix.insert(5., 1, 2);
//    matrix.insert(6., 2, 2);
//    // Column 3
//    matrix.insert(7., 0, 3);
//    matrix.insert(8., 1, 3);
//    matrix.insert(9., 2, 3);
//    matrix.insert(10., 3, 3);
//    std::cout << "Original matrix: " << matrix << "\n";
//
//    std::unordered_map<int, int> mask;
//    mask[0] = 0;
//    mask[2] = 1;
//
//
//    COOMatrix coo_matrix = matrix.to_COO(mask);
//    std::cout << "COO reduced matrix:\n" << coo_matrix;
//
//    CSCMatrix csc_matrix = matrix.to_CSC(mask);
//    std::cout << "CSC reduced matrix:\n" << csc_matrix;
//}

// C++ problem

//double f(std::vector<double> x) {
//    return x[0];
//}
//
//std::vector<double> f_gradient(std::vector<double> x) {
//    std::vector<double> gradient(1);
//    gradient[0] = 1.;
//    return gradient;
//}

//void test_cpp() {
//    CppProblem problem("my_problem", 1, 0, f, f_gradient);
//    std::vector<double> x = {123.};
//    double f_x = f(x);
//    std::cout << "f(x) = " << f_x << "\n";
//}

// Matrix

//void test_matrix() {
//    int fortran_indexing = 0;
//    ArgonotMatrix matrix(3, fortran_indexing);
//    matrix.add_term(1., 0, 0);
//    matrix.add_term(2., 0, 2);
//    matrix.add_term(3., 1, 2);
//    matrix.add_term(4., 2, 0);
//    matrix.add_term(5., 2, 1);
//    matrix.add_term(6., 2, 2);
//    CSCMatrix csc_matrix = matrix.to_CSC();
//    std::cout << "Before\n" << csc_matrix;
//    
//    csc_matrix = csc_matrix.add_identity_multiple(100.);
//    
//    std::cout << "After\n" << csc_matrix;
//}

// sparse vector

//void dense_to_sparse(const std::vector<double>& input) {
   //// create
   //std::vector<std::pair<int, double>> output;
   //output.reserve(input.size());
   //for (size_t i = 0; i < input.size(); i++) {
      //if (input[i] != 0.) {
         //output.emplace_back(i, input[i]);
      //}
   //}
   //// print
   //for (auto& [i, xi]: output) {
      //std::cout << "Index " << i << ", value " << xi << "\n";
   //}
//}

//void test_sparse_vector() {
   //std::vector<double> input{0, 1, 2, 0, 0, 0, 3};
   //dense_to_sparse(input);
//}


//// predicted reduction

//struct PredictedReduction {
   //std::function<double (double)> partial_step{nullptr};

   //double evaluate(const Problem& problem, const Direction& direction, double step_length);
//};

//double PredictedReduction::evaluate(const Problem& problem, const Direction& direction, double step_length) {
   //if (step_length == 1.) {
      //return -direction.objective;
   //}
   //else {
      //if (this->partial_step == nullptr) {
         //double complicated_stuff = problem.objective(direction.x);
         //std::cout << "COMPLICATED STUFF COMPUTED ONCE\n";
         //// construct a lambda that captures by value
         //this->partial_step = [=](double step_length) {
            //return -step_length*complicated_stuff;
         //};
      //}
      //return this->partial_step(step_length);
   //}
//}

//double f(const std::vector<double>& x) {
    //return 2*x[0];
//}

//void f_gradient(const std::vector<double>& /*x*/, std::vector<double>& gradient) {
    //gradient[0] = 1.;
//}

//void test_cpp() {
    //CppProblem<1, 0, 0> problem("my_problem", f, f_gradient);
    //std::vector<double> x = {123.};
    //double f_x = f(x);
    //std::cout << "f(x) = " << f_x << "\n";
//}
