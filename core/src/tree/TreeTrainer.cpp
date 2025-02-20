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

#include <algorithm>
#include <memory>

#include "commons/DefaultData.h"
#include "tree/TreeTrainer.h"

TreeTrainer::TreeTrainer(std::shared_ptr<RelabelingStrategy> relabeling_strategy,
                         std::shared_ptr<SplittingRuleFactory> splitting_rule_factory,
                         std::shared_ptr<OptimizedPredictionStrategy> prediction_strategy,
                         const TreeOptions& options) :
    relabeling_strategy(relabeling_strategy),
    splitting_rule_factory(splitting_rule_factory),
    prediction_strategy(prediction_strategy),
    options(options) {}

std::shared_ptr<Tree> TreeTrainer::train(Data* data,
                                         const Observations& observations,
                                         RandomSampler& sampler,
                                         const std::vector<size_t>& samples) {
  std::vector<std::vector<size_t>> child_nodes;
  std::vector<std::vector<size_t>> nodes;
  std::vector<size_t> split_vars;
  std::vector<double> split_values;

  child_nodes.push_back(std::vector<size_t>());
  child_nodes.push_back(std::vector<size_t>());
  create_empty_node(child_nodes, nodes, split_vars, split_values);

  std::vector<size_t> new_leaf_samples;

  if (options.get_honesty()) {
    sampler.subsample(samples, 0.5, nodes[0], new_leaf_samples);
  } else {
    nodes[0] = samples;
  }

  std::shared_ptr<SplittingRule> splitting_rule = splitting_rule_factory->create();

  size_t num_open_nodes = 1;
  size_t i = 0;
  while (num_open_nodes > 0) {
    bool is_leaf_node = split_node(i,
                                   splitting_rule,
                                   sampler,
                                   data,
                                   observations,
                                   child_nodes,
                                   nodes,
                                   split_vars,
                                   split_values,
                                   options.get_split_select_weights());
    if (is_leaf_node) {
      --num_open_nodes;
    } else {
      nodes[i].clear();
      ++num_open_nodes;
    }
    ++i;
  }

  auto tree = std::shared_ptr<Tree>(new Tree(0,
      child_nodes,
      nodes,
      split_vars,
      split_values,
      std::vector<size_t>(),
      PredictionValues()));

  if (!new_leaf_samples.empty()) {
    repopulate_leaf_nodes(tree, data, new_leaf_samples);
  }

  PredictionValues prediction_values;
  if (prediction_strategy != NULL) {
    prediction_values = prediction_strategy->precompute_prediction_values(
        tree->get_leaf_samples(), observations);
  }
  tree->set_prediction_values(prediction_values);

  return tree;
}

void TreeTrainer::repopulate_leaf_nodes(std::shared_ptr<Tree> tree,
                                        Data* data,
                                        const std::vector<size_t>& leaf_samples) {
  size_t num_nodes = tree->get_leaf_samples().size();
  std::vector<std::vector<size_t>> new_leaf_nodes(num_nodes);

  std::vector<size_t> leaf_nodes = tree->find_leaf_nodes(data, leaf_samples);

  for (auto& sample : leaf_samples) {
    size_t leaf_node = leaf_nodes.at(sample);
    new_leaf_nodes.at(leaf_node).push_back(sample);
  }
  tree->set_leaf_nodes(new_leaf_nodes);
  tree->prune_empty_leaves();
}

void TreeTrainer::create_split_variable_subset(std::vector<size_t>& result,
                                               RandomSampler& sampler,
                                               Data* data,
                                               const std::vector<double>& split_select_weights) {

  // Always use deterministic variables
  std::vector<size_t> deterministic_vars = options.get_deterministic_vars();
  std::copy(deterministic_vars.begin(), deterministic_vars.end(), std::inserter(result, result.end()));

  // Randomly select an mtry for this tree based on the overall setting.
  size_t num_independent_variables = data->get_num_cols() - options.get_no_split_variables().size();
  size_t mtry_sample = sampler.sample_poisson(options.get_mtry());
  size_t split_mtry = std::max<size_t>(std::min(mtry_sample, num_independent_variables), 1uL);

  // Randomly add non-deterministic variables (according to weights if needed)
  if (split_select_weights.empty()) {
    sampler.draw_without_replacement_skip(result,
                                          data->get_num_cols(),
                                          options.get_no_split_variables(),
                                          split_mtry);
  } else if (split_mtry > result.size()){
    size_t num_draws = split_mtry - result.size();
    sampler.draw_without_replacement_weighted(result,
                                              options.get_split_select_vars(),
                                              num_draws,
                                              split_select_weights);
  }
}

bool TreeTrainer::split_node(size_t node,
                             std::shared_ptr<SplittingRule> splitting_rule,
                             RandomSampler& sampler,
                             Data* data,
                             const Observations& observations,
                             std::vector<std::vector<size_t>>& child_nodes,
                             std::vector<std::vector<size_t>>& samples,
                             std::vector<size_t>& split_vars,
                             std::vector<double>& split_values,
                             const std::vector<double>& split_select_weights) {

// Select random subset of variables to possibly split at
  std::vector<size_t> possible_split_vars;
  create_split_variable_subset(possible_split_vars, sampler, data, split_select_weights);

// Call subclass method, sets split_vars and split_values
  bool stop = split_node_internal(node,
                                  splitting_rule,
                                  observations,
                                  possible_split_vars,
                                  samples,
                                  split_vars,
                                  split_values);
  if (stop) {
    // Terminal node
    return true;
  }

  size_t split_var = split_vars[node];
  double split_value = split_values[node];

// Create child nodes
  size_t left_child_node = samples.size();
  child_nodes[0][node] = left_child_node;
  create_empty_node(child_nodes, samples, split_vars, split_values);

  size_t right_child_node = samples.size();
  child_nodes[1][node] = right_child_node;
  create_empty_node(child_nodes, samples, split_vars, split_values);

  // For each sample in node, assign to left or right child
  // Ordered: left is <= splitval and right is > splitval
  for (auto& sample : samples[node]) {
    if (data->get(sample, split_var) <= split_value) {
      samples[left_child_node].push_back(sample);
    } else {
      samples[right_child_node].push_back(sample);
    }
  }

  // No terminal node
  return false;
}

bool TreeTrainer::split_node_internal(size_t node,
                                      std::shared_ptr<SplittingRule> splitting_rule,
                                      const Observations& observations,
                                      const std::vector<size_t>& possible_split_vars,
                                      std::vector<std::vector<size_t>>& samples,
                                      std::vector<size_t>& split_vars,
                                      std::vector<double>& split_values) {
  // Check node size, stop if maximum reached
  if (samples[node].size() <= options.get_min_node_size()) {
    split_values[node] = -1.0;
    return true;
  }

  // Check if node is pure and set split_value to estimate and stop if pure
  bool pure = true;
  double pure_value = 0;
  for (size_t i = 0; i < samples[node].size(); ++i) {
    size_t sample = samples[node][i];
    Eigen::VectorXd value = observations.get(Observations::OUTCOME, sample);
    if (i != 0 && value[0] != pure_value) {
      pure = false;
      break;
    }
    pure_value = value[0];
  }

  if (pure) {
    split_values[node] = -1.0;
    return true;
  }

  std::unordered_map<size_t, Eigen::VectorXd> responses_by_sample = relabeling_strategy->relabel(
      samples[node], observations);

  bool stop = responses_by_sample.empty() ||
              splitting_rule->find_best_split(node,
                                              possible_split_vars,
                                              responses_by_sample,
                                              samples,
                                              split_vars,
                                              split_values);

  if (stop) {
    split_values[node] = -1.0;
    return true;
  }
  return false;
}

void TreeTrainer::create_empty_node(std::vector<std::vector<size_t>>& child_nodes,
                                    std::vector<std::vector<size_t>>& samples,
                                    std::vector<size_t>& split_vars,
                                    std::vector<double>& split_values) {
  child_nodes[0].push_back(0);
  child_nodes[1].push_back(0);
  samples.push_back(std::vector<size_t>());
  split_vars.push_back(0);
  split_values.push_back(0);
}
