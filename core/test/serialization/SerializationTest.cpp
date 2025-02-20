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

#include <map>
#include <unordered_set>
#include <fstream>
#include <random>

#include "catch.hpp"
#include "serialization/ForestSerializer.h"
#include "serialization/PredictionValuesSerializer.h"
#include "utilities/TestUtilities.h"

TEST_CASE("observations serialize and deserialize correctly", "[observationsSerialization]") {
  size_t test_size = 10;
  Eigen::VectorXd outcomes (test_size);
  outcomes << -9.99984, -7.36924, 5.11211, -0.826997, 0.655345, -5.62082, -9.05911, 3.57729, 3.58593, 8.69386;
  Eigen::VectorXd treatment (test_size);
  treatment << 0, 0, 0, 0, 0, 0, 0, 0, 0, 0;
  Eigen::VectorXd instrument (test_size);
  instrument << 0, 0, 1, 1, 1, 0, 1, 0, 1, 0;
  Observations original_observations = TestUtilities::create_observations(outcomes, treatment, instrument);

  ObservationsSerializer observations_serializer;
  std::stringstream stream;
  observations_serializer.serialize(stream, original_observations);
  Observations observations = observations_serializer.deserialize(stream);

  REQUIRE(observations.get_num_samples() == original_observations.get_num_samples());

  auto original_observations_by_type = original_observations.get_observations_by_type();
  auto observations_by_type = observations.get_observations_by_type();

  REQUIRE(observations_by_type.size() == original_observations_by_type.size());
  for (size_t type = 0; type < original_observations_by_type.size(); ++type) {
    REQUIRE(type < observations_by_type.size());
    REQUIRE(observations_by_type[type].size() == original_observations_by_type[type].size());
  }
}


TEST_CASE("trees serialize and deserialize correctly", "[treeSerialization]") {
  std::shared_ptr<Tree> original_tree(new Tree(0,
      {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}},
      {{0, 1}, {5, 1}, {4, 6, 9}},
      {10, 20, 30, 40},
      {0.5, 0.75, 0.9, 1.1, 1.2},
      {3, 4, 5, 9, 10, 11},
      PredictionValues({{Eigen::MatrixXd::Constant(1, 1, 0), Eigen::MatrixXd::Constant(1, 1, 0),
                                Eigen::MatrixXd::Constant(1, 1, 1)}}, 1, 3)));

  TreeSerializer tree_serializer;
  std::stringstream stream;
  tree_serializer.serialize(stream, original_tree);
  std::shared_ptr<Tree> tree = tree_serializer.deserialize(stream);

  REQUIRE(tree->get_leaf_samples().size() == original_tree->get_leaf_samples().size());
  REQUIRE(tree->get_child_nodes().size() == original_tree->get_child_nodes().size());
  REQUIRE(tree->get_split_vars().size() == original_tree->get_split_vars().size());
  REQUIRE(tree->get_split_values().size() == original_tree->get_split_values().size());
  REQUIRE(tree->get_oob_samples().size() == original_tree->get_oob_samples().size());
}


TEST_CASE("forests serialize and deserialize correctly", "[forestSerialization]") {
  std::vector<std::shared_ptr<Tree>> trees;
  trees.push_back(std::shared_ptr<Tree>(new Tree(0, {{0}}, {{0}}, {0}, {0}, {0}, PredictionValues())));
  trees.push_back(std::shared_ptr<Tree>(new Tree(0, {{1}}, {{1}}, {1}, {1}, {1}, PredictionValues())));

  Eigen::VectorXd outcomes (10);
  outcomes << -9.99984, -7.36924, 5.11211, -0.826997, 0.655345, -5.62082, -9.05911, 3.57729, 3.58593, 8.69386;
  Observations observations = TestUtilities::create_observations(outcomes);

  ForestSerializer forest_serializer;
  std::stringstream stream;
  Forest original_forest(trees, observations, 3);

  forest_serializer.serialize(stream, original_forest);
  Forest forest = forest_serializer.deserialize(stream);

  REQUIRE(forest.get_trees().size() == original_forest.get_trees().size());
  REQUIRE(forest.get_observations().get_num_samples() == original_forest.get_observations().get_num_samples());
}


TEST_CASE("prediction values serialize and deserialize correctly", "[predictionValuesSerialization]") {
  Eigen::MatrixXd mat = Eigen::MatrixXd::Constant(2,2,1);
  std::vector<std::vector<Eigen::MatrixXd>> values = {{mat*1}, {mat*2.8}, {mat*33}, {mat*4}, {mat*-5},
                                                      {mat*-7},{mat*54}, {mat*23}, {mat*-8.7}, {mat*-0.6}};
  PredictionValues prediction_values(values, 10, 1);

  PredictionValuesSerializer prediction_values_serializer;
  std::stringstream stream;

  prediction_values_serializer.serialize(stream, prediction_values);
  PredictionValues prediction_values_deserialized = prediction_values_serializer.deserialize(stream);

  REQUIRE(prediction_values.get_num_nodes() == prediction_values_deserialized.get_num_nodes());
  REQUIRE(prediction_values.get_num_types() == prediction_values_deserialized.get_num_types());
}