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

#ifndef GRF_OPTIMIZEDPREDICTIONSTRATEGY_H
#define GRF_OPTIMIZEDPREDICTIONSTRATEGY_H

#include <unordered_map>
#include <vector>

#include "commons/globals.h"
#include "commons/Observations.h"
#include "prediction/Prediction.h"
#include "prediction/PredictionValues.h"
#include "eigen3/Eigen/Eigen"

/**
 * A prediction strategy defines how predictions are computed over test samples.
 *
 * Unlike the default prediction strategy, an optimized strategy does not predict based on
 * a list of neighboring samples and weights. Instead, it precomputes summary values for each
 * tree and leaf during training, and uses these during prediction. This allows the strategy
 * to avoid duplicate computation for each prediction, such as computing averages, etc.
 *
 */
class OptimizedPredictionStrategy {
public:

 /**
  * The number of values in a prediction, e.g. 1 for regression,
  * or the number of quantiles for quantile forests.
  */
  virtual size_t prediction_length() = 0;

  /**
  * Computes a prediction for a single test sample.
  *
  * average_prediction_values: the 'prediction values' computed during
  *     training, averaged across all leaves this test sample landed in.
  */
  virtual Eigen::VectorXd predict(const std::vector<Eigen::MatrixXd>& average_prediction_values) = 0;

 /**
  * Computes a prediction variance estimate for a single test sample.
  *
  * average_prediction_values: the 'prediction values' computed during training,
  *     averaged across all leaves this test sample landed in.
  * leaf_prediction_values: the individual 'prediction values' for each leaf this test
  *     sample landed in. There will be one entry per tree, even if that tree was OOB or
  *     the leaf was empty.
  * ci_group_size: the size of the tree groups used to train the forest. This
  *     parameter is used when computing within vs. across group variance.
  */
  virtual Eigen::VectorXd compute_variance(
      const std::vector<Eigen::MatrixXd>& average_prediction_values,
      const PredictionValues& leaf_prediction_values,
      uint ci_group_size) = 0;

 /**
  * The number of types of precomputed prediction values. For regression
  * this is 1 (the average outcome), whereas for instrumental forests this
  * is larger, as it includes the average treatment, average instrument etc.
  */
  virtual size_t prediction_value_length() = 0;

 /**
  * This method is called during training on each tree to precompute
  * summary values to be used during prediction.
  *
  * As an example, the regression prediction strategy computes the average outcome in
  * each leaf so that it does not need to recompute these values during every prediction.
  */
  virtual PredictionValues precompute_prediction_values(
      const std::vector<std::vector<size_t>>& leaf_samples,
      const Observations& observations) = 0;
};


#endif //GRF_OPTIMIZEDPREDICTIONSTRATEGY_H
