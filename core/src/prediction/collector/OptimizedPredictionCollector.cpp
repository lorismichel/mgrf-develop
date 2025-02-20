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

#include "prediction/collector/OptimizedPredictionCollector.h"

OptimizedPredictionCollector::OptimizedPredictionCollector(std::shared_ptr<OptimizedPredictionStrategy> strategy,
                                                           uint ci_group_size):
    strategy(strategy),
    ci_group_size(ci_group_size) {}

std::vector<Prediction> OptimizedPredictionCollector::collect_predictions(const Forest& forest,
                                                                          Data* prediction_data,
                                                                          const std::vector<std::vector<size_t>>& leaf_nodes_by_tree,
                                                                          const std::vector<std::vector<bool>>& trees_by_sample) {
  size_t num_trees = forest.get_trees().size();
  size_t num_samples = prediction_data->get_num_rows();

  std::vector<Prediction> predictions;
  predictions.reserve(num_samples);

  for (size_t sample = 0; sample < num_samples; ++sample) {
    std::vector<Eigen::MatrixXd> average_value;
    std::vector<std::vector<Eigen::MatrixXd>> leaf_values;
    if (ci_group_size > 1) {
      leaf_values.resize(num_trees);
    }

    // Create a list of weighted neighbors for this sample.
    uint num_leaves = 0;
    for (size_t tree_index = 0; tree_index < forest.get_trees().size(); ++tree_index) {
      if (!trees_by_sample.empty() && !trees_by_sample[sample][tree_index]) {
        continue;
      }

      const std::vector<size_t>& leaf_nodes = leaf_nodes_by_tree.at(tree_index);
      size_t node = leaf_nodes.at(sample);

      std::shared_ptr<Tree> tree = forest.get_trees()[tree_index];
      const PredictionValues& prediction_values = tree->get_prediction_values();

      if (!prediction_values.empty(node)) {
        num_leaves++;
        add_prediction_values(node, prediction_values, average_value);
        if (ci_group_size > 1) {
          leaf_values[tree_index] = prediction_values.get_values(node);
        }
      }
    }

    // If this sample has no neighbors, then return placeholder predictions. Note
    // that this can only occur when honesty is enabled, and is expected to be rare.
    if (num_leaves == 0) {
      Eigen::VectorXd temp = Eigen::VectorXd::Constant(strategy->prediction_length(), NAN);
      predictions.push_back(Prediction(temp));
      continue;
    }

    normalize_prediction_values(num_leaves, average_value);

    Eigen::VectorXd point_prediction = strategy->predict(average_value);
    Eigen::VectorXd variance_estimate;
    if (ci_group_size > 1) {
      PredictionValues prediction_values(leaf_values, num_trees, strategy->prediction_value_length());
      variance_estimate = strategy->compute_variance(average_value, prediction_values, ci_group_size);
    }

    Prediction prediction(point_prediction, variance_estimate);
    validate_prediction(sample, prediction);
    predictions.push_back(prediction);
  }
  return predictions;
}

void OptimizedPredictionCollector::add_prediction_values(size_t node,
    const PredictionValues& prediction_values,
    std::vector<Eigen::MatrixXd>& combined_average) {
  if (combined_average.empty()) {
    combined_average.resize(prediction_values.get_num_types());
    Eigen::MatrixXd prototype;
    for (size_t type = 0; type < prediction_values.get_num_types(); ++type) {
      prototype = prediction_values.get(node, type);
      combined_average[type] = Eigen::MatrixXd::Zero(prototype.rows(), prototype.cols());
    }
  }

  for (size_t type = 0; type < prediction_values.get_num_types(); ++type) {
    combined_average[type] += prediction_values.get(node, type);
  }
}

void OptimizedPredictionCollector::normalize_prediction_values(size_t num_leaves,
    std::vector<Eigen::MatrixXd>& combined_average) {
  for (Eigen::MatrixXd& value : combined_average) {
    value /= num_leaves;
  }
}

void OptimizedPredictionCollector::validate_prediction(size_t sample, Prediction prediction) {
  size_t prediction_length = strategy->prediction_length();
  if (prediction.size() != prediction_length) {
    throw std::runtime_error("Prediction for sample " + std::to_string(sample) +
                             " did not have the expected length.");
  }
}