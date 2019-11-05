/*-------------------------------------------------------------------------------
  This file is part of generalized random forest (grf).

  grf is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  grf is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with grf. If not, see <http://www.gnu.org/licenses/>.
 #-------------------------------------------------------------------------------*/

#include <cmath>
#include <string>
#include "prediction/RegressionPredictionStrategy.h"

const size_t RegressionPredictionStrategy::OUTCOME = 0;

size_t RegressionPredictionStrategy::prediction_length() {
    return 4;
}

Eigen::MatrixXd RegressionPredictionStrategy::predict(const std::vector<Eigen::MatrixXd>& average) {
 // std::cout << average.at(OUTCOME) << std::endl;
  return average.at(OUTCOME);
}

Eigen::VectorXd RegressionPredictionStrategy::compute_variance(
    const std::vector<Eigen::MatrixXd>& average,
    const PredictionValues& leaf_values,
    uint ci_group_size) {

  double average_outcome = average.at(OUTCOME)(0,0);

  double num_good_groups = 0;
  double psi_squared = 0;
  double psi_grouped_squared = 0;

  for (size_t group = 0; group < leaf_values.get_num_nodes() / ci_group_size; ++group) {
    bool good_group = true;
    for (size_t j = 0; j < ci_group_size; ++j) {
      if (leaf_values.empty(group * ci_group_size + j)) {
        good_group = false;
      }
    }
    if (!good_group) continue;

    num_good_groups++;

    double group_psi = 0;

    for (size_t j = 0; j < ci_group_size; ++j) {
      size_t i = group * ci_group_size + j;
      double psi_1 = leaf_values.get(i, OUTCOME)(0,0) - average_outcome;

      psi_squared += psi_1 * psi_1;
      group_psi += psi_1;
    }

    group_psi /= ci_group_size;
    psi_grouped_squared += group_psi * group_psi;
  }

  double var_between = psi_grouped_squared / num_good_groups;
  double var_total = psi_squared / (num_good_groups * ci_group_size);

  // This is the amount by which var_between is inflated due to using small groups
  double group_noise = (var_total - var_between) / (ci_group_size - 1);
  
  // A simple variance correction, would be to use:
  // var_debiased = var_between - group_noise.
  // However, this may be biased in small samples; we do an objective
  // Bayes analysis of variance instead to avoid negative values.
  double var_debiased = bayes_debiaser.debias(var_between, group_noise, num_good_groups);

  return Eigen::VectorXd::Constant(1, var_debiased);

  //return predict(average);
}


size_t RegressionPredictionStrategy::prediction_value_length() {
  return 4;
}

PredictionValues RegressionPredictionStrategy::precompute_prediction_values(
    const std::vector<std::vector<size_t>>& leaf_samples,
    const Observations& observations) {
  size_t num_leaves = leaf_samples.size();
  std::vector<std::vector<Eigen::MatrixXd>> values(num_leaves);

  for (size_t i = 0; i < num_leaves; i++) {
    const std::vector<size_t>& leaf_node = leaf_samples.at(i);
    if (leaf_node.empty()) {
      continue;
    }

    std::vector<Eigen::MatrixXd>& averages = values[i];
    averages.resize(1);

   // double average = 0.0;
   // Eigen::MatrixXd mat (1,3);

    Eigen::MatrixXd average = Eigen::MatrixXd::Zero(1, 4);
    for (auto& sample : leaf_node) {
     // mat << observations.get(Observations::OUTCOME, sample);//(0);
      //std::cout << "size: " << mat.size() << std::endl;
      //std::cout << "cols: " << mat.cols() << std::endl;
      //std::cout <<"rows: " << mat.rows() << std::endl;
      //std::cout << "mat:" << mat << std::endl;
      //std::cout << average << std::endl;
       //std::cout << observations.get(Observations::OUTCOME, sample).cols() << std::endl;
      //std::cout << observations.get(Observations::OUTCOME, sample) << std::endl;
      average += observations.get(Observations::OUTCOME, sample);
      
    }
    Eigen::MatrixXd mat (1,4);
    //std::cout << average << std::endl;
    mat << average / leaf_node.size();
    //std::cout << mat << std::endl;
    averages[OUTCOME] = mat;
    
   // std::cout << averages[OUTCOME].cols() << std::endl;

  }

  return PredictionValues(values, num_leaves, 1);
}

